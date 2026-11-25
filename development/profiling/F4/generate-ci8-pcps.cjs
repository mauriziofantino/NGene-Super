/* Substitute only the three FFT calls inside coarse integer PCPS and code preparation. */
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');

const output = process.argv[2];
assert(output, 'Pass the build directory generated/pcps-profile');
const source = fs.readFileSync('src/dsp/search.c', 'utf8');
const start = source.indexOf('int NgSearchIntegerCode(');
const end = source.indexOf('/** Integer transforms', start);
assert(start >= 0 && end > start);
const coarse = source.slice(start, end);
assert.equal(coarse.split('NgSearchFftExecuteInt32(').length, 4);
const probe = path.resolve(__dirname, 'ci8_pcps_probe.inc').replaceAll('\\', '/');
const folder = path.join(output, 'dsp');
fs.mkdirSync(folder, {recursive: true});
fs.writeFileSync(path.join(folder, 'search.c'), source.slice(0, start) +
  '#include "' + probe + '"\n' +
  coarse.replaceAll('NgSearchFftExecuteInt32(', 'Ci8PcpsTransform(') + source.slice(end));
fs.writeFileSync(path.join(output, 'profile_generated.h'), '/* F4 CI8 PCPS probe */\n');
