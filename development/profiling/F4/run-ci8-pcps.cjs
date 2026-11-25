/* Run the serial F4 PCPS probe on paired captures and compare complete coarse maps. */
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const {execFile} = require('node:child_process');
const {promisify} = require('node:util');

const [exe, directory, campaign, manifest] = process.argv.slice(2);
assert(exe && directory && (campaign === '--rf' ? manifest :
  !manifest && (!campaign || campaign === '--near-threshold')),
  'Pass the executable, scratch directory and optionally --near-threshold or --rf <manifest>');
const rf = campaign === '--rf';
fs.mkdirSync(directory, {recursive: true});
const scratch = fs.mkdtempSync(path.join(path.resolve(directory), 'maps-'));
const run = promisify(execFile);
const profiles = rf ? ['wide', 'adaptive', 'native']
  : campaign ? ['wide', 'adaptive'] : ['wide', 'fixed', 'adaptive'];
const seeds = [20261121, 20261122];
const amplitudes = [0, 0.25, 0.5, 1, 12];

function casesFor(signal) {
  if (!campaign) return seeds.flatMap(seed => amplitudes.map(amplitude => ({signal, seed, amplitude})));

  const signalLevels = signal === 'gps' ? [0.39, 0.43, 0.46, 0.50] : [0.17, 0.19, 0.21, 0.24];
  const signalCases = Array.from({length: 4}, (_, k) => 20261121 + k)
    .flatMap(seed => signalLevels.map(amplitude => ({signal, seed, amplitude})));
  const noiseCases = Array.from({length: 8}, (_, k) => ({signal, seed: 20261125 + k, amplitude: 0}));
  return [...signalCases, ...noiseCases];
}

async function replayCases() {
  const {sources} = JSON.parse(fs.readFileSync(manifest, 'utf8'));
  const cases = [];
  const ids = new Set();
  for (const source of sources) {
    const {id, rate, ifHz, firstSample, offsets, prns} = source;
    assert(/^[a-z0-9-]+$/.test(id) && !ids.has(id), 'Invalid capture identifier');
    ids.add(id);
    assert(Number.isSafeInteger(rate) && rate >= 4096000 && rate <= 20000000 &&
      Number.isSafeInteger(ifHz) && Math.abs(ifHz) < rate / 2 &&
      Number.isSafeInteger(firstSample) && firstSample >= 0, 'Invalid stream metadata');
    const file = path.resolve(path.dirname(manifest), source.file);
    const bytes = fs.statSync(file).size;
    const hash = crypto.createHash('sha256');
    for await (const chunk of fs.createReadStream(file)) hash.update(chunk);
    assert.equal(hash.digest('hex'), source.sha256, id + ' SHA-256');
    assert.equal(bytes % 2, 0, 'Capture must contain complete CI8 pairs');
    assert.equal(new Set(offsets).size, offsets.length, 'Duplicate replay offsets');
    for (const signal of ['gps', 'galileo']) {
      const count = Math.ceil(rate * (signal === 'gps' ? 56 : 224) / 1000);
      assert.equal(new Set(prns[signal]).size, prns[signal].length, 'Duplicate PRNs');
      for (const prn of prns[signal]) {
        assert(Number.isInteger(prn) && prn >= 1 && prn <= 32, 'Invalid replay PRN');
        for (const offset of offsets) {
          assert(Number.isSafeInteger(offset) && offset >= 0 && offset + count <= bytes / 2 &&
            Number.isSafeInteger(firstSample + offset), 'Replay window outside capture');
          cases.push({signal, id, rate, ifHz, prn, offset, origin: firstSample + offset, file});
        }
      }
    }
  }
  assert(cases.length > 0, 'Manifest has no replay windows');
  return cases;
}

function hexFloat(text) {
  const match = /^(-?)0x([0-9a-f])(?:\.([0-9a-f]+))?p([+-]\d+)$/i.exec(text);
  assert(match, text);
  const [, sign, whole, fraction = '', exponent] = match;
  const mantissa = parseInt(whole + fraction, 16) / 16 ** fraction.length;
  return (sign ? -1 : 1) * mantissa * 2 ** Number(exponent);
}

async function measure(test, profile) {
  const {signal, seed, amplitude} = test;
  const prefix = rf ? [signal, test.id, test.offset, test.prn] : [signal, seed, amplitude];
  const inputEnv = rf ? {
    NGENE_F4_PCPS_REPLAY: test.file, NGENE_F4_PCPS_OFFSET: String(test.offset),
    NGENE_F4_PCPS_FIRST_SAMPLE: String(test.origin), NGENE_F4_PCPS_PRN: String(test.prn)
  } : {
    NGENE_F4_PCPS_REPLAY: '', NGENE_F4_PCPS_SEED: String(seed),
    NGENE_F4_PCPS_AMPLITUDE: String(amplitude)
  };
  const n = signal === 'gps' ? 4096 : 16384;
  const rows = signal === 'gps' ? 16 : 116;
  const dump = path.join(scratch, [...prefix, profile].join('-') + '.bin');
  try {
    const args = [signal, test.rate ?? 4096000, n, test.ifHz ?? 0, 1, 1, 1].map(String);
    const {stdout} = await run(path.resolve(exe), args, {
      env: {...process.env, ...inputEnv, NGENE_F4_PCPS_PROFILE: profile, NGENE_F4_PCPS_MAP: dump}
    });
    const [fixture, verify] = stdout.trim().split(/\r?\n/).map(line => line.split(','));
    assert.equal(fixture[0], 'fixture');
    assert.equal(verify[0], 'verify');
    const [, , hash, , threshold, coarseHz, coarseChips, ratio, finalHz, finalChips,
      confirmed, successes] = verify;
    const data = fs.readFileSync(dump);
    assert.equal(data.length, (rows + 1) * n * 8);
    let noise = 0, peak = 0;
    for (let k = 0; k < n * 8; k += 8) noise += data.readDoubleLE(k);
    for (let k = n * 8; k < data.length; k += 8) peak = Math.max(peak, data.readDoubleLE(k));
    assert(Math.abs(peak / hexFloat(threshold) - hexFloat(ratio)) < 1e-10);
    return {fixture: fixture.at(-1), hash, data, start: n * 8, noise: noise / (n * 8), peak,
      threshold: hexFloat(threshold), coarseHz: hexFloat(coarseHz), coarseChips: hexFloat(coarseChips),
      ratio: hexFloat(ratio), finalHz: hexFloat(finalHz), finalChips: hexFloat(finalChips),
      confirmed, successes};
  } finally {
    if (fs.existsSync(dump)) fs.unlinkSync(dump);
  }
}

async function fixture(test) {
  const {signal, seed, amplitude, id, offset, origin, prn} = test;
  const prefix = rf ? [signal, id, offset, origin, prn] : [signal, seed, amplitude];
  const rows = [];
  let reference;
  for (const profile of profiles) {
    const m = await measure(test, profile);
    reference ??= m;
    assert.equal(m.fixture, reference.fixture);
    let signalPower = 0, errorPower = 0;
    for (let k = m.start; k < m.data.length; k += 8) {
      const wide = reference.data.readDoubleLE(k);
      signalPower += wide ** 2;
      errorPower += (m.data.readDoubleLE(k) - wide) ** 2;
    }
    rows.push([...prefix, profile, m.fixture, m.hash, m.noise, m.threshold,
      m.peak, m.ratio, Number(m.ratio > 1), m.coarseHz, m.coarseChips, m.finalHz, m.finalChips,
      m.confirmed, m.successes, Math.sqrt(errorPower / signalPower)].join(','));
  }
  return rows;
}

async function main() {
  const cases = rf ? await replayCases() : ['gps', 'galileo'].flatMap(casesFor);
  const results = [];
  const jobs = Math.min(4, os.availableParallelism());
  for (let k = 0; k < cases.length; k += jobs) {
    const batch = await Promise.allSettled(cases.slice(k, k + jobs).map(fixture));
    for (const result of batch) {
      if (result.status === 'rejected') throw result.reason;
      results.push(result.value);
    }
    if (campaign) console.error(`Completed ${results.length}/${cases.length} paired fixtures`);
  }
  const inputColumns = rf ? 'signal,capture,offset,first_sample,prn' : 'signal,seed,amplitude';
  console.log(inputColumns + ',profile,fixture_hash,map_hash,noise_per_period,threshold,' +
    'peak_power,peak_ratio,coarse_detected,coarse_hz,coarse_chips,final_hz,final_chips,' +
    'confirmed,successes,map_relative_rmse_vs_wide');
  for (const rows of results) console.log(rows.join('\n'));
}
main().catch(error => {
  console.error(error);
  process.exitCode = 1;
}).finally(() => fs.rmdirSync(scratch));

