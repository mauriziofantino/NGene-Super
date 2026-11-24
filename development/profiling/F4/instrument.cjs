/* Add stage timers to the generated F3 search source for single-worker runs. */
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const directory = process.argv[2];
assert(directory, 'Pass the generated PCPS profile directory');
const file = path.join(directory, 'dsp/search.c');
let source = fs.readFileSync(file, 'utf8');
assert(!source.includes('F4_STAGE'), 'Regenerate the F3 profile before adding F4 timers');

function instrument(start, end, mode, anchors) {
  const first = source.indexOf(start);
  const last = source.indexOf(end, first);
  assert(first >= 0 && last > first);
  let body = source.slice(first, last);
  const timers = [
    'double begin = NgMonotonicSeconds();',
    'double permutation = NgMonotonicSeconds();',
    'double butterflies = NgMonotonicSeconds();',
    'double finish = NgMonotonicSeconds();\n' +
      '    fprintf(stderr, "F4_STAGE,' + mode + ',%zu,%d,%.9g,%.9g,%.9g\\n",\n' +
      '            ' + (mode === 'float' ? 'size' : 'n') + ', inverse, permutation - begin,\n' +
      '            butterflies - permutation, finish - butterflies);',
  ];
  anchors.forEach((anchor, index) => {
    assert.equal(body.split("\n" + anchor).length, 2, anchor);
    body = body.replace('\n' + anchor, '\n    ' + timers[index] + '\n' + anchor);
  });
  source = source.slice(0, first) + body + source.slice(last);
}

instrument('int NgSearchFftExecuteInt32(', 'int NgSearchIntegerCode(', 'int32', [
  '    for (k = 0; k < n; ++k) {',
  '    for (width = 2;',
  '    if (inverse)\n',
  '    return NG_OK;',
]);
instrument('void NgSearchFftExecute(', '/** Recurrent carrier', 'float', [
  '    for (k = 0; k < size; ++k) {',
  '    if (half == 2) {',
  '    if (inverse) {\n        float scale',
  '}\n\n',
]);
fs.writeFileSync(file, '#include <stdio.h>\n' + source);

