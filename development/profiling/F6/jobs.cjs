/* Read the acquisition-job records from a replay's stderr. */
const assert = require('node:assert/strict');
const fs = require('node:fs');

const [file, referenceFile, deadlineArg] = process.argv.slice(2);
const deadlineMs = deadlineArg === undefined ? undefined : Number(deadlineArg);
assert(file, 'Usage: node jobs.cjs stderr.txt [reference-stderr.txt] [deadline-ms]');
assert(deadlineMs === undefined || (Number.isFinite(deadlineMs) && deadlineMs > 0));

function read(filename) {
  const jobs = new Map();
  for (const line of fs.readFileSync(filename, 'utf8').split(/\r?\n/)) {
    if (!line.startsWith('f6_job,')) continue;
    const fields = line.split(',');
    assert.equal(fields.length, 14, line);
    const [, system, epoch, sample, prn, status, published, confirmed,
      doppler, code, peak, claim, permit, total] = fields;
    const key = [system, epoch, sample, prn].join(':');
    assert(!jobs.has(key), 'Duplicate job ' + key);
    const delays = [claim, permit, total].map(value => 1000 * Number(value));
    assert(delays.every(value => Number.isFinite(value) && value >= 0), line);
    jobs.set(key, {system, published: published === '1',
      decision: [status, confirmed, doppler, code, peak].join(','), delays});
  }
  assert(jobs.size > 0, 'No F6 jobs in ' + filename);
  return jobs;
}

const jobs = read(file);
function p95(values) {
  const ordered = [...values].sort((a, b) => a - b);
  return ordered[Math.ceil(ordered.length * 0.95) - 1];
}
for (const system of new Set([...jobs.values()].map(job => job.system))) {
  const records = [...jobs.values()].filter(job => job.system === system);
  const published = records.filter(job => job.published);
  console.log(JSON.stringify({
    system, claimed: records.length, published: published.length,
    claimP95Ms: p95(published.map(job => job.delays[0])),
    permitP95Ms: p95(published.map(job => job.delays[1])),
    decisionP95Ms: p95(published.map(job => job.delays[2])),
    missedDeadline: deadlineMs === undefined ? undefined :
      published.filter(job => job.delays[2] > deadlineMs).length,
  }));
}
if (referenceFile) {
  const reference = read(referenceFile);
  const keys = records => [...records].filter(([, job]) => job.published).map(([key]) => key);
  const common = keys(reference).filter(key => jobs.get(key)?.published);
  console.log(JSON.stringify({
    common: common.length,
    referenceOnly: keys(reference).length - common.length,
    candidateOnly: keys(jobs).length - common.length,
    changed: common.filter(key => reference.get(key).decision !== jobs.get(key).decision),
  }));
}
