/* Compare independent Doppler rows with an ordered, serial decision pass. */
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const {execFileSync} = require('node:child_process');

const [executable, capture, output] = process.argv.slice(2);
assert(executable && capture && output, 'Pass batch executable, Classic diagnostic CI8 file and output folder');
const budgets = [0, 1, 2, 4];
const diagnostic = [
  {id: 'rf-gps', signal: 'gps', fs: 8183800, n: 4096, prn: 5, confirmed: '1'},
  {id: 'rf-galileo', signal: 'galileo', fs: 8183800, n: 16384, prn: 7, confirmed: '1'}
];
const edge = [
  {id: 'edge-gps', signal: 'gps', fs: 4096000, n: 4096, prn: 7, seed: 20261121, amplitude: 0.46, confirmed: '0'},
  {id: 'edge-galileo', signal: 'galileo', fs: 4096000, n: 16384, prn: 7, seed: 20261124, amplitude: 0.21, confirmed: '0'}
];
const profiles = ['native', 'adaptive-madd'];

function trial(test, profile, budget, map) {
  const env = {...process.env, NGENE_F4_PCPS_PROFILE: profile,
    NGENE_F4_PCPS_BATCH: budget ? 'rows' : 'direct',
    NGENE_F4_PCPS_REPLAY: test.seed ? '' : path.resolve(capture),
    NGENE_F4_PCPS_OFFSET: '0', NGENE_F4_PCPS_FIRST_SAMPLE: '0',
    NGENE_F4_PCPS_PRN: String(test.prn), NGENE_F4_PCPS_SEED: String(test.seed ?? 20261121),
    NGENE_F4_PCPS_AMPLITUDE: String(test.amplitude ?? 12)};
  if (map) env.NGENE_F4_PCPS_MAP = map;
  else delete env.NGENE_F4_PCPS_MAP;
  const args = [test.signal, test.fs, test.n, 0, 1, budget || 1, 1].map(String);
  const [fixture, verify, batch] = execFileSync(path.resolve(executable), args,
    {env, encoding: 'utf8', timeout: 90000}).trim().split(/\r?\n/).map(line => line.split(','));
  assert.equal(fixture[0], 'fixture');
  assert.equal(verify[0], 'verify');
  assert.equal(batch[0], 'batch');
  assert.equal(Number(batch[1]), budget);
  assert.equal(batch.length, 7);
  assert.equal(verify[10], test.confirmed);
  const measurements = batch.slice(2).map(Number);
  assert(measurements.every(value => Number.isFinite(value) && value >= 0));
  const rows = test.signal === 'gps' ? 16 : 116;
  assert.equal(measurements[4], budget ? (rows + 1) * test.n * 8 : 0);
  return {fixture: fixture.at(-1), verify, measurements};
}

function checkParity(test, profile, folder) {
  const map = path.join(folder, 'map.bin');
  let reference;
  for (const budget of budgets) {
    try {
      const result = trial(test, profile, budget, map);
      const bytes = fs.readFileSync(map);
      const rows = test.signal === 'gps' ? 17 : 117;
      assert.equal(bytes.length, rows * test.n * 8);
      reference ??= {...result, bytes};
      assert.equal(result.fixture, reference.fixture);
      assert.deepEqual(result.verify, reference.verify);
      assert(bytes.equals(reference.bytes), 'Power rows differ for ' + test.id);
    } finally {
      if (fs.existsSync(map)) fs.unlinkSync(map);
    }
  }
  console.error('Exact map and decisions: ' + test.id + '/' + profile);
  return reference;
}

async function main() {
  const hash = crypto.createHash('sha256');
  for await (const chunk of fs.createReadStream(capture)) hash.update(chunk);
  assert.equal(hash.digest('hex'), '90c65dd542de8eee1b411fe60df6ce24e12374b6c82261ff5feef213491168b1');
  fs.mkdirSync(output, {recursive: true});
  const folder = fs.mkdtempSync(path.join(path.resolve(output), 'maps-'));
  try {
    const references = new Map();
    for (const test of diagnostic)
      for (const profile of profiles)
        references.set(test.id + '/' + profile, checkParity(test, profile, folder));
    for (const test of edge) checkParity(test, 'adaptive-madd', folder);
    console.log('round,case,profile,workers,fixture_hash,map_hash,confirmed,prepare_s,search_s,cpu_s,fill_s,rows_bytes');
    for (let round = 1; round <= 4; ++round) {
      const order = round % 2 ? budgets : [...budgets].reverse();
      for (const test of diagnostic) {
        for (const profile of profiles) {
          const ref = references.get(test.id + '/' + profile);
          for (const budget of order) {
            const result = trial(test, profile, budget);
            assert.equal(result.fixture, ref.fixture);
            assert.deepEqual(result.verify, ref.verify);
            console.log([round, test.id, profile, budget || 'direct', result.fixture, result.verify[2],
              result.verify[10], ...result.measurements].join(','));
          }
        }
      }
      console.error('Completed timing round ' + round + '/4');
    }
  } finally {
    fs.rmdirSync(folder);
  }
}
main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});

