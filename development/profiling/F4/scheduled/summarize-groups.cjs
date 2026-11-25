/* Aggregate the group replay with foreground load and exact matched decisions. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const directory = process.argv[2];
assert(directory);

function csv(file) {
  const [header, ...rows] = fs.readFileSync(file, 'utf8').trim().replaceAll('"', '').split(/\r?\n/);
  const columns = header.split(',');
  return rows.map(line => Object.fromEntries(line.split(',').map((value, k) => [columns[k], value])));
}

function p95(values) {
  return values.length ? values.sort((a, b) => a - b)[Math.ceil(values.length * 0.95) - 1] : '';
}

function read(run) {
  const folder = path.join(directory, run.output);
  const lines = fs.readFileSync(path.join(folder, 'stderr.txt'), 'utf8').trim().split(/\r?\n/);
  const rows = name => lines.filter(line => line.startsWith(name + ',')).map(line => line.split(','));
  const jobs = rows('f6_job');
  const pairs = rows('f4_pair');
  const tracking = rows('f4_tracking');
  const workload = rows('f4_work');
  const cancel = rows('f4_cancel');
  assert(jobs.length > 0 && jobs.length === pairs.length && tracking.length === 1 && cancel.length === 2);
  assert(jobs.every(job => job[5] === '0' || job[5] === '2'), 'Failed search in ' + run.output);
  const published = jobs.filter(job => job[6] === '1');
  const index = new Map(published.map(job => [job.slice(1, 5).join(':'), job.slice(5, 11).join(',')]));
  assert.equal(index.size, published.length, run.output);
  const pairStarts = pairs.reduce((sum, row) => sum + Number(row[5]), 0);
  const pairCompletes = pairs.reduce((sum, row) => sum + Number(row[6]), 0);
  assert(pairStarts >= pairCompletes);
  if (run.mode === 'direct') assert.equal(pairStarts, 0);
  if (run.fixed_tracking === '1')
    assert(workload.length === 1 && Number(workload[0][1]) > 0, 'Missing fixed tracking work');
  const ambient = csv(path.join(folder, 'ambient.csv'));
  return {run, published, index, tracking: tracking[0], workload: workload[0], cancel, pairStarts, pairCompletes,
    pairCpu: pairs.reduce((sum, row) => sum + Number(row[7]), 0),
    ambient95: p95(ambient.map(row => Number(row.other_busy_cores)))};
}

const results = csv(path.join(directory, 'runs.csv')).map(read);
assert(results.length > 0);
const fixed = results.filter(result => result.run.fixed_tracking === '1');
if (fixed.length) {
  assert.equal(new Set(fixed.map(result => result.workload.join(','))).size, 1,
    'Fixed tracking work changed across runs');
}
const tracksWork = results.some(result => result.workload);
const comparesReuse = results.some(result => result.run.mode === 'reuse');
console.log('round,profile,mode,pairs,cpu_s,other_cores,other_cores_p95,peak_working_set_bytes,' +
  'published_gps,published_galileo,confirmed_gps,confirmed_galileo,p95_gps_ms,p95_galileo_ms,' +
  'helper_groups,helper_completed,helper_cpu_s,tracking_windows,tracking_over_1ms,tracking_p95_us,' +
  'tracking_max_us,stop_pending_gps,stop_pending_galileo,stop_gps_ms,stop_galileo_ms,' +
  'common_direct,changes_direct' + (comparesReuse ? ',common_pair,changes_pair' : '') +
  (tracksWork ? ',fixed_tracking,tracking_jobs,tracking_work_hash' : ''));
for (const m of results) {
  const {run, published} = m;
  const groups = ['gps', 'galileo'].map(signal => published.filter(job => job[1] === signal));
  const counts = groups.map(jobs => jobs.length);
  const confirms = groups.map(jobs => jobs.filter(job => job[7] === '1').length);
  const delay = groups.map(jobs => p95(jobs.map(job => Number(job[13]) * 1000)));
  const cancellation = ['gps', 'galileo'].map(signal => m.cancel.find(row => row[1] === signal));
  const direct = results.find(x => x.run.round === run.round && x.run.profile === run.profile &&
    x.run.mode === 'direct');
  assert(direct);
  const common = run.mode !== 'direct' ? [...m.index].filter(([key]) => direct.index.has(key)) : [];
  const changes = common.filter(([key, value]) => direct.index.get(key) !== value);
  const pair = run.mode === 'reuse' ? results.find(x => x.run.round === run.round &&
    x.run.profile === run.profile && x.run.mode === 'pair') : null;
  const commonPair = pair ? [...m.index].filter(([key]) => pair.index.has(key)) : [];
  const comparePair = pair ? [commonPair.length,
    commonPair.filter(([key, value]) => pair.index.get(key) !== value).length] : ['', ''];
  console.log([run.round, run.profile, run.mode, run.pairs, run.cpu_s, run.other_cores, m.ambient95,
    run.peak_working_set_bytes, ...counts, ...confirms, ...delay, m.pairStarts, m.pairCompletes, m.pairCpu,
    ...m.tracking.slice(1), ...cancellation.map(row => row[2]),
    ...cancellation.map(row => Number(row[3]) * 1000),
    run.mode !== 'direct' ? common.length : '', run.mode !== 'direct' ? changes.length : '',
    ...(comparesReuse ? comparePair : []),
    ...(tracksWork ? [run.fixed_tracking, ...(m.workload?.slice(1) ?? ['', ''])] : [])].join(','));
}

