// Read-only observations of the actual SystemServer diagnostic response.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import {normalizeMultiplexedConsole} from './reverseandroid-console-proof.mjs';
export function observeSystemServerDiagnostic(bytes){
  const {before,stdout,lines,removed}=normalizeMultiplexedConsole(bytes);
  const begin=lines.indexOf('REVERSE_PTS_SS_DIAG_BEGIN');
  const codes=lines.flatMap((line,index)=>{
    const m=/^REVERSE_PTS_SS_DIAG RC_([0-9]+)$/.exec(line);
    return m?[{index,code:Number(m[1])}]:[];
  });
  return {schema:'reverseandroid-actual-SystemServer-diagnostic-console-v1',
    raw_console_SHA256:before,raw_console_bytes:bytes.length,
    normalized_stdout_SHA256:createHash('sha256').update(Buffer.from(stdout,'latin1')).digest('hex'),
    removed_complete_kernel_records:removed,actual_diagnostic_begin_seen:begin>=0,
    actual_response_codes:codes.map(x=>x.code),
    diagnostic_completed:begin>=0&&codes.some(x=>x.index>begin&&x.code===0)&&codes.every(x=>x.code===0),
    diagnostic_failed:codes.some(x=>x.code!==0),original_console_bytes_unchanged:true,
    original_C46_or_boot_or_UI_gates_modified:false,boot_or_UI_completion_claimed:false};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(resolve(process.argv[1])).href){
  const [file,out]=process.argv.slice(2);assert.ok(file&&out);
  const result=observeSystemServerDiagnostic(readFileSync(file));
  writeFileSync(out,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
}
