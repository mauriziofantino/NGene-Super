/* Paired Windows Release PCPS runs. Modes 0, 1, and 2 are float, int32, and Q14. */
const {spawnSync} = require('node:child_process');
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');
const [action, location = 'build/msvc-x64-multisdr/Release'] = process.argv.slice(2);
const sizes = [['gps', 4096], ['galileo', 16384]];
const variants = ['before', 'after'];
const modes = [0, 1, 2];
const rate = 4096000;

function run(exe, variant, repeat, args) {
  const binary = path.join(location, exe + variant + '.exe');
  const call = spawnSync(binary, args.map(String), {encoding: 'utf8', windowsHide: true});
  assert.equal(call.status, 0, binary + ' ' + args.join(' ') + '\n' + call.stderr);
  const records = call.stdout.trimEnd().split(/\r?\n/);
  for (const record of records) console.log([variant, repeat, ...args, record].join(','));
  return records.filter(record => /^(fixture|verify),/.test(record)).join('\n');
}

function median(samples) {
  const sorted = [...samples].sort((a, b) => a - b);
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
}

function summarize(file) {
  const bytes = fs.readFileSync(file);
  const text = bytes[0] === 0xff && bytes[1] === 0xfe ? bytes.toString('utf16le') :
    bytes.toString('utf8');
  const profiles = new Map();
  for (const line of text.trim().split(/\r?\n/)) {
    const [variant, repeat, signal, sampleRate, n, ifHz, mode, workers, , kind, ...values] =
      line.split(',');
    const key = [signal, sampleRate, n, ifHz, mode, workers].join(',');
    if (!profiles.has(key)) profiles.set(key, {before: [], after: []});
    const run = (profiles.get(key)[variant][Number(repeat)] ??= {row: [], search: [], cancel: []});
    if (kind === 'fixture') run.hash = values.at(-1);
    if (kind === 'allocated') run.bytes = Number(values[0]);
    if (kind === 'row' || kind === 'search' || kind === 'cancel') run[kind].push(Number(values.at(-1)));
  }
  console.log('signal,Fs,N,IF,mode,workers,before_row_ms,after_row_ms,row_ratio,' +
    'row_ratio_min,row_ratio_max,before_search_ms,after_search_ms,search_ratio,' +
    'search_ratio_min,search_ratio_max,before_cancel_p95_ms,after_cancel_p95_ms,before_bytes,after_bytes');
  for (const [key, {before, after}] of profiles) {
    const workers = Number(key.split(',').at(-1));
    assert.equal(before.length, 4);
    assert.equal(after.length, 4);
    for (const result of [...before, ...after]) {
      assert.equal(result.hash, before[0].hash);
      assert.equal(result.row.length, workers * 12);
      assert.equal(result.cancel.length, workers * 24);
      assert.equal(result.search.length, workers);
    }
    function timings(metric) {
      const a = before.map(run => median(run[metric]));
      const b = after.map(run => median(run[metric]));
      const ratios = b.map((value, i) => value / a[i]);
      return [1e3 * median(a), 1e3 * median(b), median(ratios),
        Math.min(...ratios), Math.max(...ratios)];
    }
    function cancelP95(runs) {
      const sorted = runs.flatMap(run => run.cancel).sort((a, b) => a - b);
      return sorted[Math.ceil(0.95 * sorted.length) - 1] * 1e3;
    }
    const stats = [...timings('row'), ...timings('search'), cancelP95(before), cancelP95(after),
      before[0].bytes, after[0].bytes];
    console.log([key, ...stats.map(value => value.toFixed(4))].join(','));
  }
}

if (action === 'profile') {
  for (const variant of process.argv.slice(4).length ? process.argv.slice(4) : variants)
    for (const [system, n] of sizes)
      for (const mode of modes)
        run('ngene_pcps_profile_', variant, 0, [system, rate, n, 38500, mode, 1, 4]);
} else if (action === 'timing') {
  const [onlySystem, onlyMode, onlyWorkers] = process.argv.slice(4);
  for (const [system, n] of sizes)
    for (const mode of modes)
      for (const workers of [1, 4]) {
        if ((onlySystem && onlySystem !== system) || (onlyMode && Number(onlyMode) !== mode) ||
            (onlyWorkers && Number(onlyWorkers) !== workers)) continue;
        for (let repeat = 0; repeat < 4; ++repeat)
          for (const variant of repeat % 2 === 0 ? variants : [...variants].reverse())
            run('ngene_pcps_', variant, repeat, [system, rate, n, 38500, mode, workers, 5]);
      }
} else if (action === 'verify') {
  const cases = sizes.flatMap(([signal, n]) => [0, 38500].map(ifHz => [signal, 8183800, n, ifHz, 1]));
  cases.push(['gps', 8183800, 4096, 38500, 2], ['gps', 8183800, 4096, 0, 3],
    ['galileo', 2048000, 8192, -38500, 1], ['galileo', 2048000, 4096, 38500, 2],
    ['galileo', 4096000, 16384, 0, 3]);
  for (const [signal, fsHz, n, ifHz, scenario] of cases)
    for (const mode of modes) {
      const args = [signal, fsHz, n, ifHz, mode, 1, scenario];
      const expected = run('ngene_pcps_', 'before', 0, args);
      assert.equal(run('ngene_pcps_', 'after', 0, args), expected, args.join(' '));
    }
} else if (action === 'summary') {
  summarize(location);
} else {
  throw new Error('Expected profile, timing, verify, or summary');
}
