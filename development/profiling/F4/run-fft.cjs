/* Compare every output word, then time paired Release binaries without stage timers. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {spawnSync} = require('node:child_process');

const [action, directory = 'build/f4'] = process.argv.slice(2);
const variants = ['before', 'after'];
const sizes = [1024, 2048, 4096, 8192, 16384];
const vectors = 8;

function run(variant, args = []) {
  const extension = process.platform === 'win32' ? '.exe' : '';
  const binary = path.resolve(directory, 'ngene_int32_fft_' + variant + extension);
  const result = spawnSync(binary, args, {encoding: 'utf8', windowsHide: true});
  assert.equal(result.status, 0, result.error?.message || result.stderr);
  return result.stdout.trim();
}

if (action === 'verify') {
  const files = variants.map(variant => {
    const file = path.join(directory, variant + '.bin');
    run(variant, [file]);
    return fs.readFileSync(file);
  });
  let offset = 0;
  for (const n of sizes) {
    for (let inverse = 0; inverse <= 1; ++inverse) {
      for (let vector = 0; vector < vectors; ++vector) {
        for (let k = 0; k < 2 * n; ++k, offset += 4) {
          assert.equal(files[1].readInt32LE(offset), files[0].readInt32LE(offset),
            'N=' + n + ' inverse=' + inverse + ' vector=' + vector + ' component=' + k);
        }
      }
      console.log([n, inverse, vectors, 2 * n * vectors, 0].join(','));
    }
  }
  for (const file of files) assert.equal(file.length, offset);
} else if (action === 'timing') {
  console.log('variant,pair,N,inverse,transforms,seconds_per_transform');
  for (let pair = 0; pair < 24; ++pair) {
    const order = pair % 2 ? [...variants].reverse() : variants;
    for (const variant of order)
      for (const line of run(variant).split(/\r?\n/))
        console.log([variant, pair, line].join(','));
  }
} else if (action === 'summary') {
  const records = fs.readFileSync(directory, 'utf8').trim().split(/\r?\n/).slice(1);
  const timings = new Map();
  function median(values) {
    const sorted = [...values].sort((a, b) => a - b);
    const middle = Math.floor(sorted.length / 2);
    return sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
  }
  for (const record of records) {
    const [variant, pair, n, inverse, , seconds] = record.split(',');
    const key = [n, inverse].join(',');
    if (!timings.has(key)) timings.set(key, {before: [], after: []});
    timings.get(key)[variant][Number(pair)] = Number(seconds);
  }
  assert.equal(timings.size, sizes.length * 2);
  console.log('N,inverse,before_us,after_us,median_ratio,min_ratio,max_ratio');
  for (const [key, {before, after}] of timings) {
    assert.equal(before.length, 24);
    assert.equal(after.length, 24);
    const ratios = Array.from(after, (seconds, pair) => {
      assert(Number.isFinite(seconds) && before[pair] > 0);
      return seconds / before[pair];
    });
    const stats = [1e6 * median(before), 1e6 * median(after), median(ratios),
      Math.min(...ratios), Math.max(...ratios)];
    console.log([key, ...stats.map(value => value.toFixed(4))].join(','));
  }
} else {
  throw new Error('Expected verify, timing, or summary');
}
