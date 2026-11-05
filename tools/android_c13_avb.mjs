// Read-only, pinned C13 boot handoff. No guest instructions or snapshots are edited.
// Binary contract: AOSP libavb/avb_vbmeta_image.h and avb_chain_partition_descriptor.h.
import {createHash,createPublicKey,verify,constants} from 'node:crypto';
import {readFile,writeFile,open,mkdir} from 'node:fs/promises';
import {createReadStream} from 'node:fs';
import {resolve,join} from 'node:path';
import {pathToFileURL} from 'node:url';
const need=(ok,message)=>{if(!ok)throw Error(message);};
export const sha=b=>createHash('sha256').update(b).digest('hex');
export const PIN={diskBytes:8738832384,diskSha:'28303ac7b2832cbde4e1c56551bf81816b55bc946cc75a444f2605316ed97e1c',rootSha:'4109182481af95c749aa8e1a55aee87607a9d1cc6f16397b2a00154d7f10e492'};
export const PARTITIONS=[['boot_a',2048,67108864],['init_boot_a',133120,8388608],['vbmeta_a',17057792,12288],['vbmeta_system_a',17059840,4096],['vbmeta_system_dlkm_a',17061888,4096],['vbmeta_vendor_dlkm_a',17063936,4096]];
const number=n=>{const v=Number(n);need(Number.isSafeInteger(v)&&v>=0,'Unsafe AVB extent');return v;};
const range=(b,at,n)=>{need(Number.isSafeInteger(at)&&Number.isSafeInteger(n)&&at>=0&&n>=0&&at<=b.length&&n<=b.length-at,'AVB extent outside block');return b.subarray(at,at+n);};
export function parseVbmeta(input){
  need(Buffer.isBuffer(input)&&input.length>=256&&input.length<=1048576,'Invalid AVB input size');
  const h=input.subarray(0,256),u=at=>number(h.readBigUInt64BE(at));
  need(h.toString('ascii',0,4)==='AVB0','Invalid AVB magic');
  need(h.readUInt32BE(4)===1&&h.readUInt32BE(8)<=3,'Unsupported AVB version');
  need(h.readUInt32BE(120)===0,'AVB verification must remain enabled');
  need(h.subarray(176).every(x=>x===0),'Nonzero reserved AVB header');
  const a=u(12),b=u(20);need(a%64===0&&b%64===0,'Unaligned AVB blocks');
  const raw=range(input,0,256+a+b),auth=range(raw,256,a),aux=range(raw,256+a,b);
  const algorithm=h.readUInt32BE(28);need(algorithm>=1&&algorithm<=6,'Signed RSA AVB required');
  const hashAlg=algorithm<=3?'sha256':'sha512',bits=[2048,4096,8192][(algorithm-1)%3];
  const digest=range(auth,u(32),u(40)),signature=range(auth,u(48),u(56)),key=range(aux,u(64),u(72));
  range(aux,u(80),u(88));
  need(digest.length===(hashAlg==='sha256'?32:64)&&signature.length===bits/8,'Invalid AVB hash or signature size');
  need(u(32)+digest.length<=u(48)||u(48)+signature.length<=u(32),'Overlapping AVB authentication ranges');
  need(key.length===8+bits/4&&key.readUInt32BE(0)===bits,'Invalid AVB RSA key extent');
  const modulus=key.subarray(8,8+bits/8),n=BigInt('0x'+modulus.toString('hex'));
  need(modulus[0]>=128&&(n&1n)===1n,'Invalid AVB RSA modulus');
  const low=n&0xffffffffn,inv=BigInt(key.readUInt32BE(4));
  need((low*inv&0xffffffffn)===0xffffffffn,'Invalid AVB RSA Montgomery inverse');
  need(BigInt('0x'+key.subarray(8+bits/8).toString('hex'))===(1n<<BigInt(2*bits))%n,'Invalid AVB RSA Montgomery square');
  const signed=Buffer.concat([h,aux]);need(createHash(hashAlg).update(signed).digest().equals(digest),'AVB authenticated hash mismatch');
  const publicKey=createPublicKey({format:'jwk',key:{kty:'RSA',n:modulus.toString('base64url'),e:'AQAB'}});
  need(verify(hashAlg,signed,{key:publicKey,padding:constants.RSA_PKCS1_PADDING},signature),'AVB signature mismatch');
  const descriptors=range(aux,u(96),u(104)),chains=[];
  for(let at=0;at<descriptors.length;){
    need(descriptors.length-at>=16,'Truncated AVB descriptor');
    const tag=descriptors.readBigUInt64BE(at),length=number(descriptors.readBigUInt64BE(at+8));
    need(length%8===0,'Unaligned AVB descriptor');const d=range(descriptors,at,16+length);
    if(tag===4n){
      need(d.length>=92,'Truncated AVB chain descriptor');
      const location=d.readUInt32BE(16),pn=d.readUInt32BE(20),kn=d.readUInt32BE(24),flags=d.readUInt32BE(28);
      need(location>0&&flags===0,'Unsupported chain rollback location or slot flags');
      need(d.subarray(32,92).every(x=>x===0),'Nonzero reserved chain bytes');
      const name=range(d,92,pn).toString('utf8');need(/^[a-z][a-z0-9_]{0,35}$/.test(name),'Unsafe AVB chain name');
      const expectedKey=range(d,92+pn,kn);need(kn>=520&&kn<=2056,'Invalid chain key length');
      chains.push({name,location,key:expectedKey});
    }
    at+=16+length;
  }
  return {raw,key,publicKey,signed,signature,hashAlg,algorithm,chains,rollback:h.readBigUInt64BE(112).toString(),rollbackLocation:h.readUInt32BE(124)};
}
export function deriveHandoff(images){
  need(images instanceof Map&&images.size===6,'Exactly six original AVB members required');
  const root=images.get('vbmeta_a');need(root&&sha(parseVbmeta(root).raw)===PIN.rootSha,'Wrong pinned original root vbmeta');
  const visited=new Set(),locations=new Set([0]),ordered=[];
  const visit=(name,expectedKey=null,depth=0)=>{
    need(depth<=8&&!visited.has(name),'Repeated or cyclic AVB chain');visited.add(name);
    need(images.has(name),'Missing chained vbmeta: '+name);const image=parseVbmeta(images.get(name));
    if(expectedKey){need(image.key.equals(expectedKey),'AVB chained public key mismatch: '+name);need(image.rollbackLocation===0,'Child rollback location belongs in parent descriptor');}
    ordered.push({name,image});
    for(const child of image.chains){need(!locations.has(child.location),'Duplicate AVB rollback location');locations.add(child.location);visit(child.name+'_a',child.key,depth+1);}
  };
  visit('vbmeta_a');need(visited.size===images.size,'Unreferenced AVB image');
  const hashAlg=ordered[0].image.hashAlg,hasher=createHash(hashAlg);let bytes=0;
  for(const {image} of ordered){hasher.update(image.raw);bytes+=image.raw.length;}
  const digest=hasher.digest('hex');need(bytes===22912&&digest==='92f4ce73ee87fb244fee21aee3de3ddef2fd52d7631e12fc850be8348f3c9ed3','Not the exact original six-image AVB chain');const args=`androidboot.vbmeta.size=${bytes} androidboot.vbmeta.hash_alg=${hashAlg} androidboot.vbmeta.digest=${digest}`;
  return {schema:'reverse-c13-avb-handoff-v1',disk_sha256:PIN.diskSha,disk_bytes:PIN.diskBytes,hash_algorithm:hashAlg,vbmeta_bytes:bytes,vbmeta_digest:digest,bootargs:args,chain:ordered.map(({name,image})=>({name,vbmeta_bytes:image.raw.length,vbmeta_sha256:sha(image.raw),public_key_sha256:sha(image.key),rollback_index:image.rollback,signature_verified:true})),chain_public_keys_verified:true,verification_disabled:false,hashtree_disabled:false,partition_payloads_verified:false,hardware_root_of_trust:false,rollback_protection_emulated:false,guest_execution_performed:false};
}
export async function readDirectory(directory){
  const images=new Map();for(const [name] of PARTITIONS)images.set(name,await readFile(join(directory,name+'.vbmeta')));return images;
}
export async function readOriginalDisk(path){
  const f=await open(path,'r');try{
    need((await f.stat()).size===PIN.diskBytes,'Wrong original disk size');
    const hasher=createHash('sha256');for await(const block of createReadStream(path))hasher.update(block);need(hasher.digest('hex')===PIN.diskSha,'Wrong original disk hash');
    async function read(at,n){need(n>=0&&n<=1048576&&at>=0&&at+n<=PIN.diskBytes,'Disk range rejected');const b=Buffer.alloc(n);need((await f.read(b,0,n,at)).bytesRead===n,'Incomplete disk read');return b;}
    const images=new Map();for(const [name,lba,size] of PARTITIONS){
      let offset=0,footerSize=null;
      if(!name.startsWith('vbmeta')){const footer=await read(lba*512+size-64,64);need(footer.toString('ascii',0,4)==='AVBf'&&footer.readUInt32BE(4)===1,'Invalid AVB footer');offset=number(footer.readBigUInt64BE(20));footerSize=number(footer.readBigUInt64BE(28));need(number(footer.readBigUInt64BE(12))<=offset&&offset+footerSize<=size-64,'Invalid footer extent');}
      const header=await read(lba*512+offset,256),extent=256+number(header.readBigUInt64BE(12))+number(header.readBigUInt64BE(20));need(offset+extent<=size&&(footerSize===null||extent===footerSize),'AVB partition extent mismatch');
      images.set(name,await read(lba*512+offset,extent));
    }return images;
  }finally{await f.close();}
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  try{const [mode,input,output]=process.argv.slice(2);need(['--disk','--directory'].includes(mode)&&input&&output,'Expected --disk disk.raw|--directory metadata-dir output-dir');
    const images=await(mode==='--disk'?readOriginalDisk(input):readDirectory(input)),result=deriveHandoff(images);await mkdir(output,{recursive:true});
    await writeFile(join(output,'handoff.json'),JSON.stringify(result,null,2)+'\n',{flag:'wx'});
    await writeFile(join(output,'avb-bootargs.txt'),result.bootargs,{flag:'wx'});
    for(const [name,b] of images)await writeFile(join(output,name+'.vbmeta'),b,{flag:'wx'});
    console.log(JSON.stringify(result,null,2));
  }catch(e){console.error(e.message);process.exitCode=1;}
}
