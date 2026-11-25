/* Generate the offline row-batch entry points from both production strategies. */
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');

const destination = path.join(process.argv[2], 'acquisition');
fs.mkdirSync(destination, {recursive: true});
for (const signal of ['gps', 'galileo']) {
  let source = fs.readFileSync('src/acquisition/' + signal + '.c', 'utf8').replace(/\r\n/g, '\n');
  source = source.replace(/^#include "(noise_policy|galileo_codes)\.h"$/gm, '#include "acquisition/$1.h"');
  const search = source.indexOf('static int Search(');
  const original = source.slice(search);
  const frequency = /double frequency = ([^;]+);/.exec(original);
  assert(search >= 0 && frequency && frequency[1].includes('bin'));
  const fill = '    if (!(threshold > 0))\n        return NG_OK;';
  const row = '        if (Coarse(search, prn, frequency, config->squareCarrier, config->fftMode) != NG_OK)';
  for (const anchor of [fill, row]) assert.equal(original.split(anchor).length, 2, anchor);
  const body = original.replace(fill, fill + '\n    const int batched = F4BatchEnabled();\n' +
      '    if (batched && F4BatchFill(F4CoarseFrequency, prn, config->squareCarrier,\n' +
      '                              config->fftMode, search->power, search->size) != NG_OK)\n' +
      '        return NgFail(error, "F4 coarse batch failed");')
    .replace(frequency[0], 'double frequency = F4CoarseFrequency(bin);')
    .replace(row, '        int coarse = batched ? F4BatchRead(bin, search->power, search->size) :\n' +
      '            Coarse(search, prn, frequency, config->squareCarrier, config->fftMode);\n' +
      '        if (coarse != NG_OK)');
  fs.writeFileSync(path.join(destination, signal + '.c'), '#include "pcps_batch.h"\n' +
    source.slice(0, search) + 'static double F4CoarseFrequency(unsigned bin)\n{\n' +
    '    return ' + frequency[1] + ';\n}\n\n' + body);
}

