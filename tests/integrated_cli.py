#!/usr/bin/env python3
"""Large XI -> InverseMemory/Ether -> RVPC2 -> continued execution."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument("exe");parser.add_argument("out");parser.add_argument("legacy",nargs="?");parser.add_argument("--hard",action="store_true");args=parser.parse_args()
exe=Path(args.exe).resolve();out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
def run(*args,ok=True):
    p=subprocess.run([str(exe),*map(str,args)],text=True,capture_output=True,timeout=90)
    assert (p.returncode==0)==ok,(args,p.stdout,p.stderr)
    return [json.loads(x) for x in p.stdout.splitlines() if x.strip()]
results=[]
for n in [128,256,512,1000]:
    zero=out/f'zero-{n}.rvp';state=out/f'state-{n}.rvp';continued=out/f'continued-{n}.rvp'
    run('init',zero)
    far=[q for q in [0,127,255,511,999] if q<n]
    program='h 0\n'+''.join(f'cx {q-1} {q}\n' for q in range(1,n))
    program+=''.join(f't {q}\n' for q in far)
    program+=f'cx 0 {n-1}\ncx 0 {n-1}\nh_all\nt 0\ntdg 0\nh_all\n'
    program+=f'mem_store 9 42\nmem_store 10 17\nmem_bind 88 {n-8}\nseed 918\nexpect Z:0 Z:{n-1}\nmem_read 88\nmem_read 9\nmem_stats\nqstats\nstats\n'
    path=out/f'program-{n}.xi';path.write_text(program)
    rows=run('run',path,zero,state)
    quantum=[r for r in rows if r.get('type')=='quantum_stats'][0]
    assert abs(quantum['norm']-1)<2e-9 and quantum['reduced_components']==1
    assert [r for r in rows if r.get('type')=='expectation'][0]['value']>1-2e-9
    assert not [r for r in rows if r.get('address')=='88'][0]['deterministic']
    assert [r for r in rows if r.get('address')=='9'][0]['value']==42
    revision=[r for r in rows if r.get('type')=='stats'][0]['revision']
    mem_revision=[r for r in rows if r.get('type')=='memory_stats'][0]['revision']
    assert state.read_bytes()[:5]==b'RVPC2'
    inspect=out/'restore.xi';inspect.write_text(f'mem_stats\nstats\nexpect Z:0 Z:{n-1}\nmem_read 9\nmeasure 0\nx 0\nqstats\n')
    observed=run('run',inspect,state,continued)
    assert observed[0]['revision']==mem_revision and observed[1]['revision']==revision
    again=out/f'again-{n}.rvp';same=run('run',inspect,state,again)
    assert observed==same and again.read_bytes()==continued.read_bytes(),'restored RNG/continuation is not reproducible'
    keep=hashlib.sha256(state.read_bytes()).hexdigest()
    bad=out/'failure.xi';bad.write_text('mem_store 9 99\nt 0\nmeasure 0\nnot_an_instruction\n')
    old=continued.read_bytes();run('run',bad,state,continued,ok=False)
    assert continued.read_bytes()==old and hashlib.sha256(state.read_bytes()).hexdigest()==keep
    corrupt=bytearray(state.read_bytes());corrupt[-11]^=1;damaged=out/'damaged.rvp';damaged.write_bytes(corrupt)
    run('run',inspect,damaged,continued,ok=False);assert continued.read_bytes()==old
    results.append({'qubits':n,'far_operands':far,'snapshot_bytes':state.stat().st_size,'core_revision':revision,'memory_revision':mem_revision,'coefficients':quantum['coefficients'],'continued_RNG_identical':True,'failed_run_preserved_files':True})
# Import a legacy RVPC1 envelope with its original MXIM1 payload, then migrate.
# An optional previous CLI generates the actual old file, rather than guessing
# internal tableau/RNG formats.
if args.legacy:
    old_exe=Path(args.legacy).resolve();legacy_dir=out/'legacy';p=subprocess.run([str(old_exe),'demo',str(legacy_dir)],capture_output=True,timeout=30);assert p.returncode==0
    src=legacy_dir/'initial.rvp';assert src.read_bytes()[:5]==b'RVPC1'
    inspect=out/'legacy.xi';inspect.write_text('mem_read 7\nmem_read 9\n')
    migrated=out/'migrated.rvp';rows=run('run',inspect,src,migrated)
    assert [r['value'] for r in rows]==[5,5] and migrated.read_bytes()[:5]==b'RVPC2'
if args.hard:
    n=1000;zero=out/'hard-zero.rvp';state=out/'hard-graph.rvp';run('init',zero)
    graph=''.join(f'h {q}\n' for q in range(n))+''.join(f'cz {q-1} {q}\n' for q in range(1,n))+''.join(f'cz {q} {q+n//2}\n' for q in range(n//2))+''.join(f't {q}\n' for q in range(16))
    graph+='mem_store 9 42\nmem_bind 88 992\nseed 917\nexpect X:0 Z:1 Z:500\nqstats\n'
    program=out/'hard-graph.xi';program.write_text(graph);rows=run('run',program,zero,state)
    qs=rows[-1];assert qs['coefficients']==65536 and abs(qs['norm']-1)<2e-9
    follow=out/'hard-continue.xi';follow.write_text('tdg 0\ncx 0 999\ncx 0 999\nexpect X:0 Z:1 Z:500\nqstats\nmem_read 9\n')
    continued=out/'hard-continued.rvp';a=run('run',follow,state,continued);assert abs(a[0]['value']-1)<2e-9 and a[-1]['value']==42
    original=hashlib.sha256(state.read_bytes()).hexdigest();destination=hashlib.sha256(continued.read_bytes()).hexdigest()
    bad=out/'hard-over-budget.xi';bad.write_text('mem_store 9 99\nt 16\n')
    run('run',bad,state,continued,ok=False)
    assert hashlib.sha256(state.read_bytes()).hexdigest()==original and hashlib.sha256(continued.read_bytes()).hexdigest()==destination
    results.append({'qubits':1000,'family':'distributed_graph_16_T','coefficients':65536,'snapshot_bytes':state.stat().st_size,'persistent_state_sha256':original,'17th_T_rejected_without_overwriting_files':True})
print(json.dumps({'suite':'integrated_large_XI_persistence','cases':results,'legacy_migration_checked':bool(args.legacy),'hard_graph_checked':args.hard},indent=2))
