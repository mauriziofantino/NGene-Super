const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const [directory] = process.argv.slice(2);
assert(directory, 'Usage: node acquisition-trace.cjs <replay-run-directory>');
const timeline = JSON.parse(fs.readFileSync(path.join(directory, 'timeline.json'), 'utf8'));
const jobs = new Map();
const satellites = new Map();

function satellite(system, epoch, prn) {
  const key = [system, epoch, prn].join(':');
  if (!satellites.has(key))
    satellites.set(key, {system, epoch, prn: Number(prn), decisions: [], integrations: []});
  return satellites.get(key);
}

for (const line of fs.readFileSync(path.join(directory, 'stderr.txt'), 'utf8').split(/\r?\n/)) {
  if (!line.startsWith('f6_job,')) continue;
  const fields = line.split(',');
  assert.equal(fields.length, 14, line);
  const [, system, epoch, origin, prn, status, published, confirmed,
    doppler, phase, peak, claim, permit, elapsed] = fields;
  const decision = {
    origin, status: Number(status), published: published === '1', confirmed: confirmed === '1',
    dopplerHz: Number(doppler), codePhaseChips: Number(phase), peakRatio: Number(peak),
    waitToClaimMs: Number(claim) * 1000, waitForPermitMs: Number(permit) * 1000,
    timeToDecisionMs: Number(elapsed) * 1000
  };
  const key = [system, epoch, prn, origin].join(':');
  assert(!jobs.has(key), 'Duplicate acquisition decision ' + key);
  jobs.set(key, decision);
  satellite(system, epoch, prn).decisions.push(decision);
}

const systems = ['gps', 'galileo', 'beidou'];
const searchFields = ['acquisition', 'galileo_acquisition', 'beidou_acquisition'];
const assignments = new Set();
for (const point of timeline) {
  for (const [index, field] of searchFields.entries()) {
    for (const result of point.confirmed[field] || []) {
      const key = [systems[index], result.epoch, result.prn, result.first_sample].join(':');
      const decision = jobs.get(key);
      if (decision && decision.firstConfirmationObservedAt === undefined)
        decision.firstConfirmationObservedAt = point.sample;
    }
  }
  for (const channel of point.tracking) {
    if (BigInt(channel.integrations) === 0n) continue;
    const system = systems[channel.constellation - 1];
    assert(system, 'Unknown constellation ' + channel.constellation);
    const key = [system, channel.epoch, channel.prn, channel.tracking_start_sample].join(':');
    if (assignments.has(key)) continue;
    assignments.add(key);
    satellite(system, channel.epoch, channel.prn).integrations.push({
      trackingStartSample: channel.tracking_start_sample, firstObservedAt: point.sample,
      integrationsAtObservation: channel.integrations
    });
  }
}

console.log(JSON.stringify({
  traceRecords: jobs.size,
  processedSamples: timeline.at(-1)?.sample,
  satellites: [...satellites.values()]
}, null, 2));
