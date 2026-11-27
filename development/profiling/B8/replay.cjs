/* Cold receiver replays and a loop discontinuity through the production HTTP API. */
const fs = require('node:fs');
const path = require('node:path');
const net = require('node:net');
const {spawn, execFileSync} = require('node:child_process');
const {createHash} = require('node:crypto');
const {setTimeout: delay} = require('node:timers/promises');
const assert = require('node:assert/strict');

function cpuSeconds(pid, ticksPerSecond) {
  const stat = fs.readFileSync(`/proc/${pid}/stat`, 'utf8');
  const fields = stat.slice(stat.lastIndexOf(')') + 2).trim().split(/\s+/);
  return (Number(fields[11]) + Number(fields[12])) / ticksPerSecond;
}

async function sha256(file) {
  const hash = createHash('sha256');
  for await (const block of fs.createReadStream(file)) hash.update(block);
  return hash.digest('hex');
}

async function localPort() {
  const server = net.createServer();
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  const port = server.address().port;
  await new Promise(resolve => server.close(resolve));
  return port;
}

function writeJson(file, value) {
  fs.writeFileSync(file, JSON.stringify(value, null, 2) + '\n', {flag: 'wx'});
}

function ecef({latitude_deg, longitude_deg, height_m}) {
  assert([latitude_deg, longitude_deg, height_m].every(Number.isFinite), 'Invalid position');
  const lat = latitude_deg * Math.PI / 180;
  const lon = longitude_deg * Math.PI / 180;
  const e2 = 6.6943799901413165e-3;
  const radius = 6378137 / Math.sqrt(1 - e2 * Math.sin(lat)**2);
  return [(radius + height_m) * Math.cos(lat) * Math.cos(lon),
    (radius + height_m) * Math.cos(lat) * Math.sin(lon),
    (radius * (1 - e2) + height_m) * Math.sin(lat)];
}

const key = fix => `${fix.epoch}:${fix.sample}`;
const usesBeiDou = fix => fix.sky?.some(sat => sat.used === true && sat.constellation === 3);
const usesThreeSystems = fix => [1, 2, 3].every(system =>
  fix.sky?.some(sat => sat.used === true && sat.constellation === system));
function compare(fixes, reference, staticPosition) {
  const errors = [];
  for (const fix of fixes) {
    const other = reference.get(key(fix)) || staticPosition;
    if (!other) continue;
    const origin = ecef(other);
    errors.push({epoch: fix.epoch, sample: fix.sample,
      error_m: Math.hypot(...ecef(fix).map((value, index) => value - origin[index]))});
  }
  return errors;
}

async function replay(executable, profile, capture, directory, seconds, loop, requireB1c, ticks,
                      startSeconds) {
  fs.mkdirSync(directory);
  const port = await localPort();
  const base = `http://127.0.0.1:${port}/api/v1/`;
  const stdio = ['ignore', fs.openSync(path.join(directory, 'stdout.txt'), 'wx'),
    fs.openSync(path.join(directory, 'stderr.txt'), 'wx')];
  const child = spawn(executable, ['--web', '--config', profile, '--replay', capture,
    '--port', String(port), '--output-dir', directory], {stdio, windowsHide: true});
  stdio.slice(1).forEach(fd => fs.closeSync(fd));
  let token, launchError;
  child.on('error', error => { launchError = error; });
  const exited = new Promise(resolve => child.once('close', resolve));
  async function request(route, body) {
    const options = {signal: AbortSignal.timeout(3000)};
    if (body !== undefined) Object.assign(options, {method: 'POST',
      headers: {'Content-Type': 'application/json', 'X-NGene-Token': token},
      body: JSON.stringify(body)});
    const response = await fetch(base + route, options);
    if (!response.ok) throw Error(`${route}: ${response.status} ${await response.text()}`);
    return response.json();
  }

  const timeline = [];
  try {
    let capability;
    const startup = Date.now() + 10000;
    while (!capability && Date.now() < startup) {
      if (launchError) throw launchError;
      assert(child.exitCode === null, `Receiver exited (see ${directory}/stderr.txt)`);
      try { capability = await request('bootstrap'); } catch { await delay(100); }
    }
    assert(capability, 'Receiver startup timed out');
    token = capability.token;
    if (requireB1c)
      assert.equal(capability.beidou_b1c_operational, true, 'Receiver has no operational B1C chain');
    const config = await request('config');
    const rate = Number(/^sample_rate_hz=(\d+)$/m.exec(config.ini)?.[1]);
    const limit = rate * seconds;
    const startSample = rate * startSeconds;
    assert(Number.isSafeInteger(limit) && limit > 0, 'Invalid replay sample limit');
    assert(Number.isSafeInteger(startSample), 'Invalid starting sample');
    const ini = config.ini.replace(/^start_sample=\d+$/m, `start_sample=${startSample}`)
      .replace(/^sample_limit=\d+$/m, `sample_limit=${limit}`)
      .replace(/^loop=(true|false)$/m, `loop=${loop}`)
      .replace(/^realtime=(true|false)$/m, 'realtime=true');
    await request('config', {revision: config.revision, ini});
    const effective = await request('config');
    assert(effective.ini.includes(`start_sample=${startSample}\n`) &&
      effective.ini.includes(`sample_limit=${limit}\n`) &&
      effective.ini.includes(`loop=${loop}\n`) && effective.ini.includes('realtime=true\n'),
    'Replay range or pacing did not apply');
    fs.writeFileSync(path.join(directory, 'effective.ini'), effective.ini, {flag: 'wx'});
    const startCpu = ticks ? cpuSeconds(child.pid, ticks) : null;
    const startWall = process.hrtime.bigint();
    await request('receiver/start', {revision: effective.revision});
    const deadline = Date.now() + (loop ? 2 * seconds + 30 : seconds + 30) * 1000;
    let stoppedLoop = false;
    let status;
    do {
      status = await request('status');
      const tracking = status.tracking.filter(channel => channel.prn).map(
        ({channel_id, constellation, prn, active, pll, epoch, tracking_start_sample,
          end_sample, integrations, tow_valid, valid_subframes, ephemeris_valid,
          navigation_boundary_sample}) =>
          ({channel_id, constellation, prn, active, pll, epoch, tracking_start_sample,
            end_sample, integrations, tow_valid, valid_subframes, ephemeris_valid,
            navigation_boundary_sample}));
      const acquisitionFields = ['acquisition', 'galileo_acquisition', 'beidou_acquisition'];
      const confirmed = Object.fromEntries(acquisitionFields
        .map(name => [name, (status[name]?.results || []).filter(result => result.confirmed)]));
      const acquisition = Object.fromEntries(acquisitionFields.filter(name => status[name])
        .map(name => {
          const {searches, active_searches, current_prns} = status[name];
          return [name, {searches, active_searches, current_prns}];
        }));
      timeline.push({state: status.state, sample: status.processed_samples,
        inputQueuedSamples: status.input_queued_samples, acquisition,
        gaps: status.gap_events, drops: status.known_dropped_samples, tracking, confirmed, pvt: status.pvt});
      assert(status.state !== 'error', status.message);
      if (status.state === 'stopped' && Number(status.processed_samples) > 0) break;
      if (loop && !stoppedLoop && Number(status.processed_samples) >= limit + Math.floor(limit / 2)) {
        await request('receiver/stop', {revision: effective.revision});
        stoppedLoop = true;
      }
      await delay(200);
    } while (Date.now() < deadline);
    assert.equal(status.state, 'stopped', 'Replay did not stop');
    const wallSeconds = Number(process.hrtime.bigint() - startWall) / 1e9;
    const receiverCpu = ticks ? cpuSeconds(child.pid, ticks) - startCpu : null;
    assert.equal(status.actual_sample_rate_hz, rate, 'Unexpected source sample rate');
    if (loop) {
      assert(stoppedLoop && Number(status.gap_events) === 1, 'Expected one loop discontinuity');
    } else {
      assert.equal(Number(status.processed_samples), limit, 'Replay did not consume the full range');
      assert.equal(Number(status.gap_events), 0, 'Unexpected discontinuity in the cold run');
    }
    const solutions = new Map(timeline.filter(point => Number(point.sample) > 0)
      .map(point => [key(point.pvt), point.pvt]));
    const fixes = [...solutions.values()].filter(fix => fix.available);
    const epochs = [...new Set([...solutions.values()].map(solution => solution.epoch))].map(epoch => {
      const epochFixes = fixes.filter(fix => fix.epoch === epoch);
      const samples = epochFixes.map(fix => Number(fix.sample));
      const beidou = epochFixes.filter(usesBeiDou);
      const start = Number(epoch) * limit;
      const gaps = samples.slice(1).map((sample, index) => (sample - samples[index]) / rate);
      return {epoch, fixes: samples.length, ttff_signal_s: samples.length ? (samples[0] - start) / rate : null,
        beidou_fixes: beidou.length, three_system_fixes: epochFixes.filter(usesThreeSystems).length,
        beidou_ttff_signal_s: beidou.length ? (Number(beidou[0].sample) - start) / rate : null,
        max_fix_spacing_signal_s: gaps.length ? Math.max(...gaps) : null};
    });
    const latest = timeline.at(-1);
    return {platform: capability.os, architecture: capability.architecture, version: capability.version,
      beidou_operational: capability.beidou_b1c_operational === true, rate, limit,
      file_start_sample: startSample,
      processed: latest.sample, gaps: latest.gaps, drops: latest.drops, epochs, fixes,
      wall_seconds: wallSeconds, receiver_cpu_seconds: receiverCpu,
      receiver_mean_cores: receiverCpu === null ? null : receiverCpu / wallSeconds,
      beidou_fixes: fixes.filter(usesBeiDou).length};
  } finally {
    try {
      writeJson(path.join(directory, 'timeline.json'), timeline);
    } finally {
      if (child.exitCode === null && token) {
        await request('shutdown', {}).catch(() => {});
        await Promise.race([exited, delay(2000, undefined, {ref: false})]);
      }
      if (child.exitCode === null) child.kill();
      await exited;
    }
  }
}

async function main() {
  const args = process.argv.slice(2);
  const startArgs = args.filter(arg => arg.startsWith('--start-seconds='));
  assert(startArgs.length <= 1, 'Specify at most one starting offset');
  const startSeconds = startArgs.length ? Number(args.splice(args.indexOf(startArgs[0]), 1)[0]
    .slice('--start-seconds='.length)) : 0;
  assert(Number.isSafeInteger(startSeconds) && startSeconds >= 0, 'Invalid starting offset');
  const requireB1c = args.at(-1) === '--require-b1c';
  if (requireB1c) args.pop();
  const single = args.at(-1) === '--single';
  if (single) args.pop();
  const [program, config, input, output, duration, truth] = args;
  const seconds = Number(duration);
  assert(program && config && input && output && Number.isInteger(seconds) && seconds >= 2,
    'Usage: node replay.cjs EXECUTABLE PROFILE CAPTURE NEW_OUTPUT SECONDS [REFERENCE_JSON] [--single | --require-b1c] [--start-seconds=N]');
  assert(!(single && requireB1c), 'Full B1C qualification requires both cold starts and a gap');
  assert(!requireB1c || truth, 'B1C qualification requires an independent position reference');
  const ticks = process.platform === 'linux' ?
    Number(execFileSync('getconf', ['CLK_TCK'], {encoding: 'utf8'})) : null;
  assert(ticks === null || (Number.isFinite(ticks) && ticks > 0), 'Invalid Linux clock tick rate');
  const [executable, profile, capture] = [program, config, input].map(file => fs.realpathSync(file));
  const directory = path.resolve(output);
  fs.mkdirSync(directory);
  const metadata = [capture + '.meta.ini', capture.replace(/_\d{6}\.[^.]+$/, '.meta.ini')]
    .find(file => file !== capture && fs.existsSync(file));
  const reference = truth ? JSON.parse(fs.readFileSync(truth, 'utf8')) : {};
  const referenceEpochs = new Map((reference.epochs || []).map(fix => [key(fix), fix]));
  writeJson(path.join(directory, 'inputs.json'), {
    executable, executable_sha256: await sha256(executable), profile, profile_sha256: await sha256(profile),
    capture, capture_sha256: await sha256(capture),
    metadata: metadata ? {path: metadata, sha256: await sha256(metadata), ini: fs.readFileSync(metadata, 'utf8')} : null,
    seconds, start_seconds: startSeconds, reference_sha256: truth ? await sha256(truth) : null
  });
  const runs = [];
  for (const name of single ? ['cold-1'] : ['cold-1', 'cold-2', 'loop-gap']) {
    const run = await replay(executable, profile, capture, path.join(directory, name), seconds,
      name === 'loop-gap', requireB1c, ticks, startSeconds);
    runs.push({name, ...run, reference_errors_m: compare(run.fixes, referenceEpochs, reference.position)});
    console.log(name, run.processed, 'samples,', run.gaps, 'gaps,', run.fixes.length, 'observed fixes');
  }
  const coldReference = new Map(runs[0].fixes.map(fix => [key(fix), fix]));
  const missing = [];
  if (requireB1c) for (const run of runs) {
    for (const epoch of run.name === 'loop-gap' ? ['0', '1'] : ['0']) {
      const mixed = run.fixes.filter(fix => String(fix.epoch) === epoch && usesThreeSystems(fix));
      if (!compare(mixed, referenceEpochs, reference.position).length)
        missing.push(`${run.name}, epoch ${epoch}: no observed three-system fix with a reference`);
    }
  }
  writeJson(path.join(directory, 'summary.json'), {runs, missing_required_evidence: missing,
    cold_position_differences_m: single ? [] : compare(runs[1].fixes, coldReference)});
  assert.equal(missing.length, 0, missing.join('\n'));
}

main().catch(error => { console.error(error); process.exitCode = 1; });
