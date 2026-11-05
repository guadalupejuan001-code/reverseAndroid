const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),cp=require('node:child_process');
const [runtime,probe,outArg]=process.argv.slice(2),out=path.resolve(outArg),root=path.resolve(__dirname,'../..');fs.mkdirSync(out,{recursive:true});
const context=vm.createContext({xiHTML(){},document:{addEventListener(){}}});vm.runInContext(fs.readFileSync(path.join(root,'reverse-os-android/app/src/main/assets/workbench/quantum-workbench.js'),'utf8'),context);
const program=(kind,p)=>context.quantumProgram(kind,p),run=(...args)=>cp.execFileSync(runtime,args,{encoding:'utf8',stdio:['ignore','pipe','pipe']});
const base=path.join(out,'base.rvp'),state=path.join(out,'result.rvp'),source=path.join(out,'program.xi');run('init',base);
let checks=0,maxError=0;const cases=[];
function near(a,b){const error=Math.abs(a-b);assert.ok(error<2e-10,`${a} != ${b}`);maxError=Math.max(maxError,error);checks++;}
function execute(kind,parameters){fs.writeFileSync(source,program(kind,parameters));return run('run',source,base,state).split('\n').filter(s=>s.startsWith('{')).map(s=>JSON.parse(s));}
// Independent dense two-qubit oracle: directly exponentiate ZZ and X using
// cos/sin, instead of replaying the native H/CX/T gate decomposition.
function oracle(j,h,steps){
  let s=Array.from({length:4},()=>[.5,0]);const theta=Math.PI/8;
  for(let step=0;step<steps;step++){
    s=s.map(([re,im],k)=>{const a=-theta*j*(((k&1)^((k>>1)&1))?-1:1),c=Math.cos(a),t=Math.sin(a);return [re*c-im*t,re*t+im*c];});
    for(let q=0;q<2;q++){
      const old=s,c=Math.cos(theta*h),t=Math.sin(theta*h);
      s=old.map(([re,im],k)=>{const [otherRe,otherIm]=old[k^(1<<q)];return [c*re+t*otherIm,c*im-t*otherRe];});
    }
  }
  const zz=s.reduce((sum,[re,im],k)=>sum+(re*re+im*im)*(((k&1)^((k>>1)&1))?-1:1),0);
  const x=q=>s.reduce((sum,[re,im],k)=>{const [r,i]=s[k^(1<<q)];return sum+re*r+im*i;},0);
  return [zz,x(0),x(1)];
}
const phase=execute('phase',{}).filter(r=>r.type==='expectation').map(r=>r.value);near(phase[0],Math.SQRT1_2);near(phase[1],Math.SQRT1_2);near(phase[2],0);
for(const coupling of [-3,-1,0,1,3])for(const field of [-2,0,1,3])for(const steps of [1,2,4]){
  const parameters={coupling,field,steps},records=execute('ising',parameters),actual=records.filter(r=>r.type==='expectation').map(r=>r.value),expected=oracle(coupling,field,steps);
  assert.equal(actual.length,3);checks++;actual.forEach((v,i)=>near(v,expected[i]));cases.push({parameters,observables:actual});
}
const large=execute('register',{qubits:4096}),profile=JSON.parse(cp.execFileSync(probe,[state],{encoding:'utf8'}));
assert.equal(profile.active_qubits,4096);assert.equal(profile.components,4096);assert.equal(profile.largest_component,1);checks+=3;
assert.equal(large.find(r=>r.type==='quantum_stats').T_instructions,4096);checks++;
for(const p of [{qubits:0},{qubits:4097},{qubits:1.5}]){assert.throws(()=>program('register',p));checks++;}
for(const p of [{coupling:4},{field:Infinity},{steps:17},{steps:0}]){assert.throws(()=>program('ising',p));checks++;}
const capContext=vm.createContext({window:{}});vm.runInContext(fs.readFileSync(path.join(root,'reverse-os-android/app/src/main/assets/workbench/core-capabilities.js'),'utf8'),capContext);
for(const key of ['max_active_qubits','max_component_qubits','max_state_bytes','max_address_digits']){assert.equal(capContext.window.REVERSE_CAPABILITIES[key],profile[key]);checks++;}
const report={checks,independent_dense_oracle_cases:cases.length,max_observable_error:maxError,independent_qubits_verified:4096,T_instructions_verified:4096,profile,scope:'Actual XI runtime; independent analytic two-qubit product-formula oracle. Not a claim of arbitrary 4096-qubit entangled simulation.',cases};
fs.writeFileSync(path.join(out,'results.json'),JSON.stringify(report,null,2)+'\n');console.log('QUANTUM_WORKBENCH_NATIVE_PASS',checks,'error',maxError);
