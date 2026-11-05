#!/usr/bin/env node
// Independent full-system oracle; QEMU is a test dependency only.
import {spawnSync} from 'node:child_process';
import {mkdtempSync,readFileSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {resolve,join} from 'node:path';
const [binaryArg,elfArg]=process.argv.slice(2);
if(!binaryArg||!elfArg)throw Error('Usage: node test.mjs reverse_guest pairs.elf');
const binary=resolve(binaryArg),elf=resolve(elfArg),directory=mkdtempSync(join(tmpdir(),'reverse-pairs-'));
const run=(args)=>{
  const p=spawnSync(binary,args,{encoding:'utf8',timeout:30000});
  if(p.status!==0)throw Error(p.stderr||'REVERSE command failed');
  return JSON.parse(p.stdout);
};
try{
  const initial=join(directory,'initial.rvg'),whole=join(directory,'whole.rvg');
  const part=join(directory,'part.rvg'),resumed=join(directory,'resumed.rvg');
  run(['system-init',elf,initial]);
  const direct=run(['run',initial,whole,'10000']);
  if(direct.stdout!=='REVERSE: EXCLUSIVE PAIR OK\n'||direct.status!=='exited'||
     direct.guest.exceptions!==0)throw Error('Own paired-exclusive ELF witness failed');
  const first=run(['run',initial,part,'5']);
  const saved=run(['inspect',part]);
  const next=run(['run',part,resumed,'10000']);
  if(first.stdout+next.stdout!==direct.stdout||!readFileSync(whole).equals(readFileSync(resumed)))
    throw Error('A live paired reservation did not survive exact RVG restoration');
  const oracle=spawnSync('qemu-system-aarch64',
    ['-M','virt','-cpu','cortex-a53','-m','128M','-nographic','-monitor','none',
     '-kernel',elf,'-no-reboot'],{encoding:'utf8',timeout:30000});
  if(oracle.status!==0||oracle.stdout!==direct.stdout)
    throw Error('Independent QEMU system result differs: '+oracle.stderr);
  console.log(JSON.stringify({status:'independent_clang_qemu_system_pair_oracle_passed',
    stdout:direct.stdout,exact_live_monitor_resume:true,
    saved_instructions:saved.instructions,android_boot:false}));
}finally{rmSync(directory,{recursive:true,force:true});}
