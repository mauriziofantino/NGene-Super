/* Validate saved evidence and summarize repeat-to-repeat timing ranges. */
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const root=__dirname;
const lines=file=>fs.readFileSync(path.join(root,'results',file),'utf8').replace(/^\uFEFF/,'').trim().split(/\r?\n/);
const a=lines('windows-x64-run1.csv'),b=lines('windows-x64-run2.csv');
const numeric=l=>/^(quality|harmonic|sensitivity),/.test(l)&&!l.includes(',mode,')&&!l.includes(',case,');
const verify=lines('windows-x64-verify.csv'),reference=lines('windows-x64-reference.csv');
assert.deepEqual(verify.slice(1),reference.slice(1),'Explicit SIMD and reference numerical results differ');
assert.deepEqual(a.filter(numeric),b.filter(numeric),'Repeated deterministic numerical measurements differ');
for(const run of [a,b]) {
  assert(run.some(l=>l.startsWith('# PASS:')));
  assert.equal(run.filter(l=>/^timing,[^,]+,16000,/.test(l)).length,5);
  assert.equal(run.filter(l=>/^parallel,[^,]+,[124],/.test(l)).length,12);
  assert.equal(run.filter(l=>/^stream,ring_/.test(l)).length,3);
  for(const line of run.filter(l=>/^sensitivity,[^,]+,\d/.test(l))) {
    const [,mode,,trials,loss]=line.split(',');assert.equal(Number(trials),1024);assert(Number.isFinite(Number(loss)));
    if(mode==='iq16_q15_simd')assert(Math.abs(Number(loss))<0.01);
    if(mode==='iq8_q15_simd')assert(Math.abs(Number(loss))<0.05);
    if(mode==='iq8_xor_square')assert(Number(loss)>0.5&&Number(loss)<2);
    if(mode==='sign1_xor_square')assert(Number(loss)>1.5&&Number(loss)<4.5);
  }
}
for(const prefix of ['timing','stream']) {
  const rows=[...a,...b].filter(l=>l.startsWith(prefix+',')&&!l.includes(',mode,')).map(l=>l.split(','));
  const modes=[...new Set(rows.map(r=>r[1]))];
  for(const mode of modes) {
    const values=rows.filter(r=>r[1]===mode&&(prefix==='stream'||r[2]==='16000')).map(r=>Number(r[3]));
    console.log(`${prefix} ${mode}: ${Math.min(...values).toFixed(4)}–${Math.max(...values).toFixed(4)} ${prefix==='stream'?'ms':'us'}`);
  }
}
console.log('PASS: saved native/reference numerical parity; repeated numeric results; expected timing/concurrency/memory rows; sensitivity bounds.');
