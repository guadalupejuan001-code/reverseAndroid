// R2 supervisor, unchanged, with C11's actual initial instruction counter.
import {superviseNativeCycle} from './android_supervised_cycle.mjs';
import {PIN} from './android_c12_audit.mjs';
const [root,separator,command,...args]=process.argv.slice(2);
if(separator!=='--'||!command)throw Error('Expected output-directory -- command arguments...');
const result=await superviseNativeCycle({command,args,outputDirectory:root,
  initialInstructions:PIN.instructions,startupMs:180000,noProgressMs:90000,wallMs:600000,graceMs:30000,
  onStatus:s=>process.stderr.write('REVERSE_C12_SUPERVISOR '+JSON.stringify(s)+'\n')});
console.log(JSON.stringify(result));if(result.status!=='process_completed')process.exitCode=2;
