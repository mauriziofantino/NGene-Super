/* Summarize the per-run timelines produced by replay-matrix.ps1. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = process.argv[2] ?? 'build/f6';
const results = [];
for (const directory of fs.readdirSync(root)) {
  const match = /^trial(\d+)-worker(\d+)-budget(\d+)$/.exec(directory);
  if (!match) continue;
  const timeline = JSON.parse(fs.readFileSync(path.join(root, directory, 'timeline.json'), 'utf8'));
  const running = timeline.filter(point => point.state === 'running');
  assert(running.length > 0, directory);
  const last = timeline.at(-1);
  const active = point => Number(point.gps.pool_active);
  const executing = point => Number(point.gps.pool_executing);
  const max = (records, field) => Math.max(...records.map(field));
  const firstSearch = system => running.find(point => Number(point[system].searches) > 0)?.sample ?? '';
  const firstConfirmation = system =>
    running.find(point => Array.from(point[system].results ?? []).some(result => result.confirmed))?.sample ?? '';
  const fix = running.find(point => point.pvt.available);
  results.push([
    ...match.slice(1).map(Number),
    last.sample, last.elapsed.toFixed(3), (last.cpu / last.elapsed).toFixed(3),
    max(timeline, point => point.ring), last.gaps, last.drops, last.discarded,
    max(running, point => Number(point.gps.searches)), max(running, point => Number(point.galileo.searches)),
    firstSearch('gps'), firstSearch('galileo'), firstConfirmation('gps'), firstConfirmation('galileo'),
    max(running, active), max(running, executing),
    running.filter(point => active(point) > executing(point)).length, running.length,
    max(running, point => point.tracking.filter(channel => channel.pll).length),
    fix?.pvt.sample ?? '',
  ]);
}
assert(results.length > 0, 'No replay timelines');
results.sort((a, b) => a[0] - b[0] || a[1] - b[1] || a[2] - b[2]);
console.log('trial,tracking_workers,acq_budget,processed_pairs,wall_seconds,process_cpu_cores,ring_peak_blocks,' +
  'gaps,drops,discarded,gps_searches,galileo_searches,first_gps_observed_sample,first_galileo_observed_sample,' +
  'first_gps_confirmation_observed_sample,first_galileo_confirmation_observed_sample,' +
  'pool_active_peak,pool_executing_peak,polls_with_claimed_nonexecuting_jobs,running_polls,peak_pll,first_pvt_sample');
for (const row of results) console.log(row.join(','));
