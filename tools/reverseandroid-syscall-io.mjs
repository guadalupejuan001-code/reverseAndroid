// Bounded, read-only syscall pathname and file-descriptor context.
import assert from 'node:assert/strict';
const hx=value=>'0x'+value.toString(16),kernel=value=>value>=0xffff000000000000n;

export function inspectTaskFd(api,taskPointer,fd,filePath) {
  const {read,type,members,task}=api;
  assert.ok(Number.isSafeInteger(fd)&&fd>=0&&fd<1048576,'Invalid bounded descriptor number');
  assert.ok(kernel(taskPointer));
  const files=read(taskPointer+BigInt(task.files.offset));assert.ok(kernel(files),'Missing task files');
  const ff=members(type('files_struct')),table=read(files+BigInt(ff.fdt.offset));assert.ok(kernel(table));
  const tf=members(type('fdtable')),capacity=Number(read(table+BigInt(tf.max_fds.offset),4));
  assert.ok(capacity>0&&capacity<=1048576&&fd<capacity,'Descriptor outside BTF fdtable bounds');
  const array=read(table+BigInt(tf.fd.offset));assert.ok(kernel(array));
  const pointer=read(array+BigInt(fd)*8n);
  const result={read_only:true,kernel_BTF_layout_verified:true,fd,table_capacity:capacity,open:pointer!==0n};
  if(!pointer)return result;
  assert.ok(kernel(pointer),'Invalid file pointer');
  const fields=members(type('file'));result.file=hx(pointer);
  result.position_signed=BigInt.asIntN(64,read(pointer+BigInt(fields.f_pos.offset))).toString();
  result.file_mode_raw=hx(read(pointer+BigInt(fields.f_mode.offset),4));
  result.file_flags_raw=hx(read(pointer+BigInt(fields.f_flags.offset),4));
  try{result.path=filePath(pointer);}catch(error){result.path_unverified=error.message;}
  if(fields.f_inode)try{
    const inode=read(pointer+BigInt(fields.f_inode.offset));assert.ok(kernel(inode));
    const inf=members(type('inode'));result.inode=hx(inode);
    result.inode_number=read(inode+BigInt(inf.i_ino.offset)).toString();
    result.inode_mode_raw=hx(read(inode+BigInt(inf.i_mode.offset),2));
    if(inf.i_size)result.inode_size_signed=BigInt.asIntN(64,read(inode+BigInt(inf.i_size.offset))).toString();
  }catch(error){result.inode_unverified=error.message;}
  return result;
}

export function inspectSavedSyscallIO({api,taskPointer,memory,syscall,registers,originalX0,TBI0,filePath}) {
  const result={read_only:true,syscall_number:syscall,argument0_source:'pt_regs.orig_x0',original_argument0:hx(originalX0)};
  if([63,64,67,68,62,29].includes(syscall)) {
    assert.ok(originalX0>=0n&&originalX0<1048576n,'Invalid saved descriptor argument');
    result.descriptor=inspectTaskFd(api,taskPointer,Number(originalX0),filePath);
  }
  if([56,78,79,48,291].includes(syscall)) {
    const raw=registers[1],address=TBI0?raw&0x00ffffffffffffffn:raw;
    assert.ok(address>0n&&address<0x1000000000000n,'Invalid user pathname pointer');
    const bytes=[];let terminated=false;
    for(let n=0;n<4096;n++) {
      const part=memory.read(address+BigInt(n),1);assert.equal(part.length,1,'Short pathname memory read');
      const byte=part[0];
      if(byte===0){terminated=true;break;}bytes.push(byte);
    }
    assert.ok(terminated,'Unterminated bounded syscall pathname');
    const body=Buffer.from(bytes);result.path_argument={raw_address:hx(raw),address:hx(address),bytes:body.length,raw_hex:body.toString('hex'),display_utf8:body.toString('utf8'),directory_fd_signed:BigInt.asIntN(32,originalX0).toString()};
    const proc=body.toString('utf8').match(/^\/proc\/self\/fd\/([0-9]{1,7})$/);
    if(proc)result.proc_self_fd_target=inspectTaskFd(api,taskPointer,Number(proc[1]),filePath);
  }
  return result;
}
