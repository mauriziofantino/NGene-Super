/* Summarize the published F6 jobs from an F4 scheduled replay. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const directory = process.argv[2];
assert(directory, 'Pass a scheduled replay output directory');
const [header, ...lines] = fs.readFileSync(path.join(directory, 'runs.csv'), 'utf8')
  .trim().replaceAll('"', '').split(/\r?\n/);
const fields = header.split(',');
const runs = lines.map(line => Object.fromEntries(line.split(',').map((value, i) => [fields[i], value])));
assert(runs.length > 0 && runs.length % 3 === 0);
const keys = job => [job.signal, job.epoch, job.sample, job.prn].join(':');

function readJobs(run) {
  const tag = `round${run.round}-budget${run.budget}-${run.profile}`;
  const log = fs.readFileSync(path.join(directory, tag, 'stderr.txt'), 'utf8');
  const jobs = [];
  for (const line of log.split(/\r?\n/)) {
    if (!line.startsWith('f6_job,')) continue;
    const values = line.split(',');
    assert.equal(values.length, 14, line);
    const [, signal, epoch, sample, prn, status, published, confirmed, doppler, phase, peak, claim, permit, total] = values;
    const delays = [claim, permit, total].map(value => 1000 * Number(value));
    assert(delays.every(delay => Number.isFinite(delay) && delay >= 0), line);
    jobs.push({signal, epoch, sample, prn, published, confirmed, doppler, phase, delays, decision: [status, confirmed, doppler, phase, peak].join(",")});
  }
  assert(jobs.length > 0, tag);
  assert.equal(new Set(jobs.map(keys)).size, jobs.length, 'Duplicate trace key: ' + tag);
  return jobs;
}

function p95(values) {
  const ordered = values.sort((a, b) => a - b);
  return ordered.length ? ordered[Math.ceil(ordered.length * 0.95) - 1].toFixed(3) : '';
}

const groups = new Map();
for (const run of runs) {
  const tag = run.round + '/' + run.budget;
  if (!groups.has(tag)) groups.set(tag, new Map());
  const group = groups.get(tag);
  assert(!group.has(run.profile), 'Duplicate replay: ' + tag);
  group.set(run.profile, {run, jobs: readJobs(run)});
}
const simdComparison = runs.some(run => run.profile === 'adaptive-avx2');
console.log('round,budget,profile,pairs,wall_seconds,cpu_seconds,signal,claimed,published,confirmed,' +
  'p95_claim_ms,p95_permit_ms,p95_decision_ms,common_native,confirmed_diff,estimate_diff' +
  (simdComparison ? ',common_adaptive,decision_diff_adaptive' : ''));
for (const group of groups.values()) {
  assert.equal(group.size, 3);
  const publishedIndex = profile => new Map(group.get(profile).jobs
    .filter(job => job.published === '1').map(job => [keys(job), job]));
  const native = publishedIndex('native');
  const adaptive = simdComparison ? publishedIndex('adaptive') : null;
  for (const {run, jobs} of group.values()) {
    for (const signal of ['gps', 'galileo']) {
      const claimed = jobs.filter(job => job.signal === signal);
      const published = claimed.filter(job => job.published === '1');
      const confirmed = published.filter(job => job.confirmed === '1');
      const common = published.filter(job => native.has(keys(job)));
      const changes = common.filter(job => job.confirmed !== native.get(keys(job)).confirmed);
      const estimates = confirmed.filter(job => {
        const reference = native.get(keys(job));
        return reference?.confirmed === '1' &&
          (job.doppler !== reference.doppler || job.phase !== reference.phase);
      });
      const compared = run.profile === 'native' ? ['', '', ''] : [common.length, changes.length, estimates.length];
      const scalarCommon = run.profile === 'adaptive-avx2' ?
        published.filter(job => adaptive.has(keys(job))) : [];
      const scalarComparison = run.profile === 'adaptive-avx2' ? [scalarCommon.length,
        scalarCommon.filter(job => job.decision !== adaptive.get(keys(job)).decision).length] : ['', ''];
      console.log([run.round, run.budget, run.profile, run.pairs, run.wall_seconds, run.cpu_seconds,
        signal, claimed.length, published.length, confirmed.length,
        ...[0, 1, 2].map(k => p95(published.map(job => job.delays[k]))), ...compared, ...(simdComparison ? scalarComparison : [])].join(','));
    }
  }
}

