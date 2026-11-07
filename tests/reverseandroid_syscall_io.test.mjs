import test from 'node:test';
import assert from 'node:assert/strict';
import {inspectTaskFd,inspectSavedSyscallIO} from '../tools/reverseandroid-syscall-io.mjs';

function fixture() {
  const base=0xffff000000000000n,task=base+0x1000n,files=base+0x2000n,table=base+0x3000n,array=base+0x4000n,file=base+0x5000n,inode=base+0x6000n;
  const words=new Map(),put=(at,value)=>words.set(at.toString(),BigInt(value));
  put(task,files);put(files+8n,table);put(table,4);put(table+8n,array);put(array+16n,0);put(array+24n,file);
  put(file,12);put(file+8n,0);put(file+12n,1);put(file+16n,inode);put(inode,45);put(inode+8n,0x81a4);put(inode+16n,1000);
  const fields=map=>Object.fromEntries(Object.entries(map).map(([name,offset])=>[name,{offset}]));
  const layouts={files_struct:fields({fdt:8}),fdtable:fields({max_fds:0,fd:8}),file:fields({f_pos:0,f_flags:8,f_mode:12,f_inode:16}),inode:fields({i_ino:0,i_mode:8,i_size:16})};
  const api={read(at){assert.ok(words.has(at.toString()),'Unmapped kernel word');return words.get(at.toString());},type(name){assert.ok(layouts[name]);return layouts[name];},members(t){return t;},task:{files:{offset:0}}};
  const filePath=pointer=>{assert.equal(pointer,file);return '/system/framework/Test.jar';};
  return {api,task,words,put,array,file,filePath};
}

test('bounded BTF descriptor context keeps file position and identity without reading its contents',()=>{
  const {api,task,words,filePath}=fixture(),before=new Map(words),r=inspectTaskFd(api,task,3,filePath);
  assert.equal(r.path,'/system/framework/Test.jar');assert.equal(r.position_signed,'12');assert.equal(r.inode_size_signed,'1000');assert.deepEqual(words,before);
  assert.equal(inspectTaskFd(api,task,2,filePath).open,false);
  for(const fd of [-1,4,1048576])assert.throws(()=>inspectTaskFd(api,task,fd,filePath));
});

test('uses original saved syscall argument rather than a possible return value',()=>{
  const {api,task,filePath}=fixture();
  const r=inspectSavedSyscallIO({api,taskPointer:task,memory:null,syscall:63,registers:[24n],originalX0:3n,TBI0:true,filePath});
  assert.equal(r.descriptor.fd,3);
});

test('normalizes a tagged readlink pathname, signed 32-bit dirfd and its proc/self/fd target',()=>{
  const {api,task,filePath}=fixture(),bytes=Buffer.from('/proc/self/fd/3\0');
  const memory={read(at,n){const i=Number(at-0x5000n);assert.ok(i>=0&&i+n<=bytes.length);return bytes.subarray(i,i+n);}};
  const r=inspectSavedSyscallIO({api,taskPointer:task,memory,syscall:78,registers:[0n,0xb400000000005000n],originalX0:0xffffff9cn,TBI0:true,filePath});
  assert.equal(r.path_argument.display_utf8,'/proc/self/fd/3');assert.equal(r.path_argument.directory_fd_signed,'-100');assert.equal(r.proc_self_fd_target.path,'/system/framework/Test.jar');
});

test('rejects kernel pathname pointers, unterminated strings and short reads',()=>{
  const {api,task,filePath}=fixture();const options={api,taskPointer:task,syscall:78,originalX0:0n,TBI0:true,filePath};
  assert.throws(()=>inspectSavedSyscallIO({...options,registers:[0n,0xffff000000005000n],memory:null}));
  assert.throws(()=>inspectSavedSyscallIO({...options,registers:[0n,0x5000n],memory:{read:()=>Buffer.from('x')}}),/Unterminated/);
  assert.throws(()=>inspectSavedSyscallIO({...options,registers:[0n,0x5000n],memory:{read:()=>Buffer.alloc(0)}}),/Short/);
});
