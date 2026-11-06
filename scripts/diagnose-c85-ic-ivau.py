#!/usr/bin/env python3
"""Reproduce the exact original Android IC IVAU fault observation without CPU execution.

Uses a small index of original RFP pages; does not create dense guest RAM.
Requires the whole exact checkpoint and the independent full observer report.
This is supplemental diagnosis, never a runtime decoder or admission replacement.
"""
from pathlib import Path
import hashlib,json,struct,sys

assert len(sys.argv)==3, 'Usage: diagnose-c85-ic-ivau.py SOURCE_DIRECTORY NEW_OUTPUT.json'
SOURCE=Path(sys.argv[1]).resolve()
OUTPUT=Path(sys.argv[2]).resolve()
assert not OUTPUT.exists(), 'Observation output must be a new file'
SHA='acff30448cdaf5fc5f8f79b5716120f4357c7968f8ae9e4468c00474677a75dd'
KERNEL='f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8'
RAM=0x40000000
f=(SOURCE/'current.rvg').open('rb')
assert hashlib.file_digest(f,'sha256').hexdigest()==SHA
f.seek(0);h=f.read(360);assert h[:8]==b'RVG3\r\n\x1a\n';v=struct.unpack_from('<44Q',h,8)
image,core,mem,system,fp=v[39:44];assert 360+image+core+mem+system+fp+8==f.seek(0,2)
f.seek(360);rvi=f.read(48);assert rvi[:8]==b'RVI3\r\n\x1a\n';k,tree,rd,args,dev=struct.unpack_from('<5Q',rvi,8)
assert 48+k+tree+rd+args+dev==image
kernel_at=408;f.seek(kernel_at);kernel=f.read(k);assert hashlib.sha256(kernel).hexdigest()==KERNEL
kernel_address=RAM+struct.unpack_from('<Q',kernel,8)[0]
backings=[(kernel_address,k,kernel_at),(RAM+v[0]//2,rd,kernel_at+k+tree),(RAM+v[0]-2097152,tree,kernel_at+k)]
machine_at=360+image+core+mem;f.seek(machine_at);machine=f.read(system)
m=lambda i:struct.unpack_from('<Q',machine,i*8)[0]
assert m(0)==7 and m(11)&63==25
index={};f.seek(360+image+core)
assert f.readline(100).startswith(b'MXIM2 ') and f.readline(100)==b'MXIM3 0\n'
while f.tell()<machine_at:
    at=f.tell();line=f.readline(16385);assert len(line)<=16384 and f.tell()<=machine_at
    if line==b'\n':continue
    fields=line.split(b' ',2);assert len(fields)==3 and fields[0]==b'P'
    page=int(fields[1]);assert page not in index;index[page]=(at,len(line))
cache={}
def delta_page(page):
    if page in cache:return cache[page]
    data=bytearray(4096)
    if page in index:
        at,n=index[page];f.seek(at);fields=f.read(n).rstrip(b'\n').split(b' ',3)
        mask=bytes.fromhex(fields[2].decode());values=bytes.fromhex(fields[3].decode())
        assert len(mask)==512 and sum(x.bit_count() for x in mask)==len(values)
        j=0
        for i in range(4096):
            if mask[i>>3]&(1<<(i&7)):data[i]=values[j];j+=1
    cache[page]=data
    return data
def physical(address,n=8):
    assert RAM<=address and address+n<=RAM+v[0]
    out=bytearray(n)
    for i in range(n):
        a=address+i;out[i]=delta_page(a//4096)[a%4096]
        for start,length,offset in backings:
            if start<=a<start+length:f.seek(offset+a-start);out[i]^=f.read(1)[0];break
    return bytes(out)
def p64(address):return int.from_bytes(physical(address),'little')
def translate(address,ttbr,tcr,upper):
    bits=64-((tcr>>(16 if upper else 0))&63);assert 25<=bits<=48
    assert ((tcr>>14)&3)==0 and ((tcr>>30)&3)==2
    if tcr&(1<<(38 if upper else 37)):address=(address|0xff00000000000000) if upper else address&0x00ffffffffffffff
    va_mask=(1<<bits)-1
    assert (address|va_mask)==(1<<64)-1 if upper else address&~va_mask==0
    assert not tcr&(1<<(23 if upper else 7))
    table=ttbr&0x0000fffffffff000
    for level in range(4-((bits-12+8)//9),4):
        shift=12+(3-level)*9;d=p64(table+((address>>shift)&511)*8);assert d&1
        if level<3 and d&2:table=d&0x0000fffffffff000;continue
        mask=(1<<shift)-1
        assert level>0 and (level<3 or d&2) and not d&(mask&~4095) and d&(1<<10)
        return (d&0x0000fffffffff000&~mask)|(address&mask)
    raise AssertionError('Untranslated address')
def kernel_read(address,n=8):return int.from_bytes(physical(translate(address,m(13),m(11),True),n),'little')

# BTF decoding uses the exact immutable kernel and the original observer's
# pinned BTF location. Only byte-aligned structure fields are accepted.
btf=27670332;assert struct.unpack_from('<H',kernel,btf)[0]==0xeb9f
assert struct.unpack_from('<I',kernel,btf+4)[0]==24
ts,tl,ss,sl=struct.unpack_from('<4I',kernel,btf+8);ts+=btf+24;ss+=btf+24
def string(off):
    assert 0<=off<sl;end=kernel.index(0,ss+off);assert end<ss+sl
    return kernel[ss+off:end].decode()
extras={1:4,2:0,3:12,4:12,5:12,6:8,7:0,8:0,9:0,10:0,11:0,12:0,13:8,14:4,15:12,16:0,17:4,18:0,19:12}
types=[None];at=ts
while at<ts+tl:
    name,info,t=struct.unpack_from('<3I',kernel,at);kind=(info>>24)&31;count=info&65535;assert kind in extras
    size=12+extras[kind]*(count if kind in (4,5,6,13,15,19) else 1)
    assert at+size<=ts+tl
    types.append({'name':string(name),'info':info,'kind':kind,'type':t,'count':count,'at':at});at+=size
def resolved(t):
    seen=set()
    while t['kind'] in (8,9,10,11,18):
        assert t['type'] not in seen;seen.add(t['type']);t=types[t['type']]
    return t
def members(t,base=0,depth=0):
    t=resolved(t);assert depth<8 and t['kind'] in (4,5);result={}
    for i in range(t['count']):
        name,ident,bits=struct.unpack_from('<3I',kernel,t['at']+12+12*i)
        off=bits&0xffffff if t['info']>>31 else bits;name=string(name)
        if name:result[name]={'bits':base*8+off,'type':ident}
        else:result.update(members(types[ident],base+off//8,depth+1))
    return result
def layout(name):
    matches=[t for t in types[1:] if t['kind']==4 and t['name']==name];assert matches
    layouts=[members(t) for t in matches];signature=lambda fields:{k:v['bits'] for k,v in fields.items()}
    assert all(t['type']==matches[0]['type'] for t in matches)
    assert all(signature(x)==signature(layouts[0]) for x in layouts)
    return layouts[0]
task,mm=layout('task_struct'),layout('mm_struct')
def offset(fields,key):assert fields[key]['bits']%8==0;return fields[key]['bits']//8
pointer=0xffffff80224a2880
assert kernel_read(pointer+offset(task,'pid'),4)==3386
assert kernel_read(pointer+offset(task,'tgid'),4)==3386
assert physical(translate(pointer+offset(task,'comm'),m(13),m(11),True),16).rstrip(b'\0')==b'system_server'
mm_pointer=kernel_read(pointer+offset(task,'mm'));assert mm_pointer>=0xffff000000000000
pgd=kernel_read(mm_pointer+offset(mm,'pgd'));pgd_physical=translate(pgd,m(13),m(11),True)
def user(address,n):
    return b''.join(physical(translate(address+i,pgd_physical,m(11),False),1) for i in range(n))
original=json.loads((SOURCE/'threads-with-diagnostics-readonly.json').read_text())
assert original['checkpoint_sha256']==SHA and original['instructions']==v[4] and original['input_unchanged']
assert original['original_observer_file_unchanged'] and original['observer_source_git_blob_sha1']=='9f72798c437126cb0cbb2b678a5ec80ea81b251c'
leader=next(t for g in original['groups'] if g['pid']==3386 for t in g['threads'] if t['pid']==3386)
frame=leader['saved_user_frame'];assert user(int(frame['pc']['address'],16),64).hex()==frame['pc_code_le_hex']
assert kernel_read(pointer+offset(task,'ptrace'),4)==int(leader['signals']['ptrace_raw'])
fault=0x7544ce7e64;code=user(fault-32,96)
result={'schema':'reverseandroid-readonly-original-fault-code-v1','checkpoint_SHA256':SHA,'instructions':v[4],'whole_checkpoint_and_original_kernel_SHA256_verified':True,'kernel_BTF_layout_and_task_identity_verified':True,'PID':3386,'original_full_observer_crypto_code_and_ptrace_matched':True,'actual_user_pgd_physical':hex(pgd_physical),'fault_address_from_actual_RAM_log':hex(fault),'code_start':hex(fault-32),'actual_code_le_hex':code.hex(),'actual_fault_word':hex(int.from_bytes(code[32:36],'little')),'fault_physical_address':hex(translate(fault,pgd_physical,m(11),False)),'CPU_instructions_executed':0,'source_bytes_unchanged':True,'independent_read_only_reader_not_a_runtime_or_promotion_source':True}
result['actual_SCTLR_EL1']=hex(m(10));result['actual_SCTLR_UCI_bit26']=bool(m(10)&(1<<26))
f.seek(0);assert hashlib.file_digest(f,'sha256').hexdigest()==SHA
with OUTPUT.open('x') as out:out.write(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
