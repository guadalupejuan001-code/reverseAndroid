import test from 'node:test';import assert from 'node:assert/strict';import {continuationWatchdogMs} from '../tools/android_profile_resume.mjs';
test('the documented three fifteen-minute defaults fit the actual supervisor bound',()=>{const n=continuationWatchdogMs();assert.ok(n>=600000+3*900000+120000);assert.ok(n<=3600000);});
test('four ten-minute cycles reserve startup and final publication without exceeding one hour',()=>{const n=continuationWatchdogMs(600000,4);assert.ok(n>=600000+4*600000+120000);assert.ok(n<=3600000);});
test('overlong and malformed schedules are rejected before source I/O',()=>{for(const [wall,cycles]of[[900000,4],[0,1],[NaN,3],[900001,1],[1,0],[1,5],[1,1.5]])assert.throws(()=>continuationWatchdogMs(wall,cycles));});
