/* Run independently compiled Release variants in balanced order, one process per profile. */
const {spawnSync} = require('node:child_process');
const assert = require('node:assert/strict');
const path = require('node:path');

const directory = process.argv[2] || 'build/msvc-x64-multisdr/Release';
const mode = process.argv[3] || 'timing';
const systems = [['gps', 4096], ['galileo', 16384]];
const orders = [[0, 1, 3, 2], [1, 2, 0, 3], [2, 3, 1, 0], [3, 0, 2, 1]];

function run(stage, repeat, args) {
  const executable = path.join(directory, 'ngene_prepare_' + stage + '.exe');
  const child = spawnSync(executable, args.map(String), {encoding: 'utf8', windowsHide: true});
  assert.equal(child.status, 0, executable + ' ' + args.join(' ') + '\n' + child.stderr);
  const records = child.stdout.trimEnd().split(/\r?\n/);
  for (const record of records) console.log([stage, repeat, ...args, record].join(','));
  return records.filter(record => /^(fixture|verify),/.test(record)).join('\n');
}

if (mode === 'timing') {
  for (const [signal, n] of systems)
    for (const ifHz of [0, 38500])
      for (const arithmetic of [0, 1, 2])
        for (const workers of [1, 4])
          for (const [repeat, order] of orders.entries())
            for (const stage of order) run(stage, repeat, [signal, 8183800, n, ifHz, arithmetic, workers, 0]);
} else if (mode === 'verify') {
  const cases = systems.flatMap(([signal, n]) => [0, 38500].map(ifHz => [signal, 8183800, n, ifHz, 1]));
  cases.push(['gps', 8183800, 4096, 38500, 2], ['gps', 8183800, 4096, 0, 3],
    ['galileo', 2048000, 8192, -38500, 1], ['galileo', 2048000, 4096, 38500, 2],
    ['galileo', 4096000, 16384, 0, 3]);
  for (const [signal, rate, n, ifHz, scenario] of cases) {
    for (const arithmetic of [0, 1, 2]) {
      const args = [signal, rate, n, ifHz, arithmetic, 1, scenario];
      const expected = run(0, 0, args);
      for (const stage of [1, 2, 3]) assert.equal(run(stage, 0, args), expected, args.join(' '));
    }
  }
} else {
  throw new Error('Expected timing or verify');
}
