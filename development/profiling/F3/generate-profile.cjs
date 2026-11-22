/* Insert timers into build-local copies of the operative PCPS loops. */
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const output = process.argv[2];
assert(output, 'Pass the build directory generated/pcps-profile');

function read(file) {
  return fs.readFileSync(path.join('src', file), 'utf8').replace(/\r\n/g, '\n');
}

function write(file, contents) {
  const target = path.join(output, file);
  fs.mkdirSync(path.dirname(target), {recursive: true});
  fs.writeFileSync(target, contents);
}

function replace(source, anchor, replacement) {
  const first = source.indexOf(anchor);
  assert(first >= 0 && source.indexOf(anchor, first + anchor.length) < 0, anchor);
  return source.replace(anchor, replacement);
}

for (const signal of ['gps', 'galileo']) {
  const file = 'acquisition/' + signal + '.c';
  const source = read(file).replace(/^#include "(noise_policy|galileo_codes)\.h"$/gm,
    '#include "acquisition/$1.h"');
  const begin = source.indexOf('static int Coarse(');
  const end = source.indexOf('/** Mix ', begin);
  assert(begin >= 0 && end > begin);
  let body = source.slice(begin, end);
  body = replace(body, '    NgSearchCarrier(search->carrier, search->size,',
    '    double mark = NgMonotonicSeconds();\n    NgSearchCarrier(search->carrier, search->size,');
  body = replace(body, 'doppler, 0, square);\n', 'doppler, 0, square);\n' +
    '    NgPcpsProfilePart(PCPS_CARRIER, &mark);\n');
  body = replace(body, '        memcpy(search->work,', '        mark = NgMonotonicSeconds();\n' +
    '        memcpy(search->work,');
  for (const [anchor, stage] of [
    ['        NgSearchMix(search->work, search->carrier, search->size);', 'ROTATE'],
    ['        NgSearchFftExecute(search->fft, search->work, 0);', 'FORWARD'],
    ['        NgSearchFftExecute(search->fft, search->work, 1);', 'INVERSE'],
  ]) body = replace(body, anchor, anchor + '\n        NgPcpsProfilePart(PCPS_' + stage + ', &mark);');
  body = replace(body, '        NgSearchFftExecute(search->fft, search->work, 1);',
    '        NgPcpsProfilePart(PCPS_PRODUCT, &mark);\n' +
    '        NgSearchFftExecute(search->fft, search->work, 1);');
  body = replace(body, '            search->power[k] += i * i + q * q;\n        }',
    '            search->power[k] += i * i + q * q;\n        }\n' +
    '        NgPcpsProfilePart(PCPS_POWER, &mark);');
  write(file, '#include "pcps_lab.h"\n#include "platform/platform.h"\n' +
    source.slice(0, begin) + body + source.slice(end));
}

const source = read('dsp/search.c');
const begin = source.indexOf('int NgSearchIntegerPcps(');
const end = source.indexOf('/** Integer transforms', begin);
assert(begin >= 0 && end > begin);
let body = source.slice(begin, end);
body = replace(body, '    n = plan->size;\n', '    n = plan->size;\n' +
  '    double mark = NgMonotonicSeconds();\n');
const forward = '    if (mode == NG_SEARCH_FFT_INTEGER_SCALED) {\n' +
  '        if (NgFftExecute(plan->integerPlan, n, plan->integerInput,\n' +
  '                         plan->integerOutput, error) != NG_OK)\n' +
  '            return NG_ERROR;\n    } else if (NgSearchFftExecuteInt32';
body = replace(body, forward, '    NgPcpsProfilePart(PCPS_ROTATE, &mark);\n' + forward);
body = replace(body, '        return NG_ERROR;\n    for (k = 0;', '        return NG_ERROR;\n' +
  '    NgPcpsProfilePart(PCPS_FORWARD, &mark);\n    for (k = 0;');
body = replace(body, '    while ((maximum >> shift)', '    NgPcpsProfilePart(PCPS_PRODUCT, &mark);\n' +
  '    while ((maximum >> shift)');
body = replace(body, '    if (mode == NG_SEARCH_FFT_INTEGER_SCALED) {\n        if (NgFftExecute(plan->integerPlan, n, plan->integerInput,\n                         plan->integerOutput, error) != NG_OK)\n            return NG_ERROR;\n        restore',
  '    NgPcpsProfilePart(PCPS_PACK, &mark);\n' +
  '    if (mode == NG_SEARCH_FFT_INTEGER_SCALED) {\n        if (NgFftExecute(plan->integerPlan, n, plan->integerInput,\n                         plan->integerOutput, error) != NG_OK)\n            return NG_ERROR;\n        restore');
body = body.replace(/^        restore = ldexp/gm,
  '        NgPcpsProfilePart(PCPS_INVERSE, &mark);\n        restore = ldexp');
body = replace(body, '    return NG_OK;', '    NgPcpsProfilePart(PCPS_POWER, &mark);\n    return NG_OK;');
write('dsp/search.c', '#include "pcps_lab.h"\n#include "platform/platform.h"\n' +
  source.slice(0, begin) + body + source.slice(end));
write('profile_generated.h', '/* Generated PCPS profiling sources are present. */\n');
