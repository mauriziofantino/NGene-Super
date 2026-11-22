/* Aggregate Release measurements using process medians for paired comparisons. */
const fs = require('node:fs');
const assert = require('node:assert/strict');
const sorted = values => [...values].sort((a, b) => a - b);
function median(values) {
  const valuesSorted = sorted(values);
  const middle = Math.floor(valuesSorted.length / 2);
  return valuesSorted.length % 2 ? valuesSorted[middle] :
    (valuesSorted[middle - 1] + valuesSorted[middle]) / 2;
}
const p95 = values => sorted(values)[Math.ceil(values.length * 0.95) - 1];
const profiles = new Map();
const hashes = new Map();

const bytes = fs.readFileSync(process.argv[2]);
const text = bytes[0] === 0xff && bytes[1] === 0xfe ? bytes.toString('utf16le') :
  bytes.toString('utf8');
for (const line of text.trim().split(/\r?\n/)) {
  const [stage, repeat, signal, rate, grid, ifHz, mode, workers, , kind, ...values] = line.split(',');
  const key = [signal, rate, grid, ifHz, mode, workers].join(',');
  if (!profiles.has(key)) profiles.set(key, [[], [], [], []]);
  const stages = profiles.get(key);
  const runs = stages[Number(stage)];
  const trial = (runs[Number(repeat)] ??= {prepare: [], cancel: [], gap: []});
  if (kind === 'fixture') {
    const hash = values.at(-1);
    if (!hashes.has(key)) hashes.set(key, hash);
    assert.equal(hash, hashes.get(key), key);
  } else if (kind === 'memory') {
    const [create, allocated, before, afterCreate, afterPrepare] = values.map(Number);
    Object.assign(trial, {create, allocated, rssCreate: afterCreate - before,
      rssPrepare: afterPrepare - before});
  } else if (kind === 'time') {
    const [, , elapsed, cancel, gap] = values.map(Number);
    trial.prepare.push(elapsed);
    trial.cancel.push(cancel);
    trial.gap.push(gap);
  }
}

console.log('signal,Fs,N,IF,mode,workers,stage,create_ms,allocated_bytes,rss_create_bytes,' +
  'rss_warm_bytes,prepare_ms,cancel_us,cancel_p95_us,max_gap_p95_ms,prepare_ratio_prev,' +
  'prepare_ratio_min,prepare_ratio_max');
for (const [key, stages] of profiles) {
  let previous;
  for (const [stage, runs] of stages.entries()) {
    assert.equal(runs.length, 4, key);
    const workers = Number(key.split(',').at(-1));
    assert(runs.every(run => run && run.prepare.length === workers * 12 && run.allocated), key);
    const prepare = runs.map(run => median(run.prepare));
    const ratios = previous ? prepare.map((value, index) => value / previous[index]) : [1];
    const result = [median(runs.map(run => run.create)) * 1000,
      median(runs.map(run => run.allocated)),
      median(runs.map(run => run.rssCreate)),
      median(runs.map(run => run.rssPrepare)), median(prepare) * 1000,
      median(runs.map(run => median(run.cancel))) * 1e6,
      p95(runs.flatMap(run => run.cancel)) * 1e6,
      p95(runs.flatMap(run => run.gap)) * 1000,
      median(ratios), Math.min(...ratios), Math.max(...ratios)];
    console.log([key, stage, ...result.map(value => value.toFixed(3))].join(','));
    previous = prepare;
  }
}
