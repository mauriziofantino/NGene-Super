/* End-to-end optional GPS search: real controller, ring, worker, API and browser form. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const testGalileo = process.env.NGENE_TEST_GALILEO === '1';
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(fn, label) {
  for (let k = 0; k < 300; k++) { const value = await fn(); if (value) return value; await delay(25); }
  throw Error('Timeout: ' + label);
}
(async () => {
  const root = path.resolve('build/web-acquisition-' + Date.now());
  fs.mkdirSync(root, {recursive: true}); fs.mkdirSync(path.join(root, 'profile'));
  const chips = [], iq = Buffer.alloc(2 * 2400000);
  const galileoCode = fs.readFileSync(path.resolve('tests/data/galileo-e1b-prn7.bin'));
  let g1 = 1023, g2 = 1023, random = 177;
  for (let k = 0; k < 1023; k++) {
    chips.push(((g1 >> 9) ^ g2 ^ (g2 >> 7)) & 1 ? 1 : -1);
    const f1 = ((g1 >> 2) ^ (g1 >> 9)) & 1;
    const f2 = ((g2 >> 1) ^ (g2 >> 2) ^ (g2 >> 5) ^ (g2 >> 7) ^ (g2 >> 8) ^ (g2 >> 9)) & 1;
    g1 = ((g1 << 1) | f1) & 1023; g2 = ((g2 << 1) | f2) & 1023;
  }
  const noise = () => { random = (Math.imul(random, 1664525) + 1013904223) >>> 0; return 20 * (random / 4294967296 - 0.5); };
  for (let k = 0; k < iq.length / 2; k++) {
    const t = k / 2400000, chip = chips[Math.floor((231.3 + t * 1023000 * (1 + 1234 / 1575420000)) % 1023)];
    const angle = 2 * Math.PI * 1234 * t;
    const phase = (123.42 + t * 1023000 * (1 + 369 / 1575420000)) % 4092;
    const galChip = galileoCode.readInt8(Math.floor(phase)) * (phase % 1 < .5 ? 1 : -1);
    const galAngle = 2 * Math.PI * 369 * t;
    const galI = testGalileo ? 4 * galChip * Math.cos(galAngle) : 0;
    const galQ = testGalileo ? 4 * galChip * Math.sin(galAngle) : 0;
    iq.writeInt8(Math.round((testGalileo ? 5 : 2) * chip * Math.cos(angle) + galI + noise()), 2 * k);
    iq.writeInt8(Math.round((testGalileo ? 5 : 2) * chip * Math.sin(angle) + galQ + noise()), 2 * k + 1);
  }
  const input = path.join(root, 'gps.cs8').replaceAll('\\', '/'); fs.writeFileSync(input, iq);
  const configPath = path.join(root, 'gps.ini');
  fs.writeFileSync(configPath, `[frontend]\ntype=file\nsample_rate_hz=2400000\ncenter_frequency_hz=1575420000\nsignal_frequency_hz=1575420000\n[receiver]\nrun_seconds=0\n[replay]\npath=${input}\nrealtime=true\nloop=true\n[acquisition]\nenabled=true\nfirst_prn=7\nlast_prn=8\nsquare_carrier=false\nxor_correlation=true\n`);
  if (testGalileo) fs.appendFileSync(configPath, '[galileo_acquisition]\nenabled=false\nfirst_prn=7\nlast_prn=8\nsquare_carrier=false\nxor_correlation=true\n');
  const port = Number(process.env.NGENE_TEST_PORT || 19528), base = `http://127.0.0.1:${port}`;
  const child = spawn(path.resolve(process.env.NGENE_EXECUTABLE || 'build/ngene'),
    ['--web', '--port', String(port), '--config', configPath, '--output-dir', path.join(root, 'profile')],
    {stdio: ['ignore', 'pipe', 'pipe']});
  let log = '', browser;
  child.stdout.on('data', data => log += data); child.stderr.on('data', data => log += data);
  const exited = new Promise(resolve => child.once('exit', resolve));
  try {
    const bootstrap = await until(async () => {
      if (child.exitCode !== null) throw Error(log);
      try { return await (await fetch(base + '/api/v1/bootstrap')).json(); } catch { return false; }
    }, 'startup');
    const get = async route => (await fetch(base + '/api/v1/' + route)).json();
    const post = async (route, body) => {
      const response = await fetch(base + '/api/v1/' + route, {method: 'POST',
        headers: {'Content-Type': 'application/json', 'X-NGene-Token': bootstrap.token}, body: JSON.stringify(body)});
      const reply = await response.json(); assert(response.ok, JSON.stringify(reply)); return reply;
    };
    assert.equal(bootstrap.gps_acquisition, true);
    let config = await get('config'); assert.match(config.ini, /xor_correlation=true/);
    browser = await chromium.launch({headless: true,
      ...(process.env.BROWSER_EXECUTABLE ? {executablePath: process.env.BROWSER_EXECUTABLE} : {})});
    const page = await browser.newPage({viewport: {width: 1440, height: 1000}}), errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(base); await until(async () => (await page.locator('#connection').textContent()) === 'Connected', 'GUI');
    await page.locator('.workspace-nav-item[data-view="acquisition"]').click();
    await page.locator('[data-open-settings="acquisition"]').first().click();
    await page.locator('[name="acquisition.enabled"]').waitFor({state: 'visible'});
    assert.equal(await page.locator('[name="acquisition.first_prn"]').inputValue(), '7');
    assert.equal(await page.locator('[name="acquisition.xor_correlation"]').inputValue(), 'true');
    assert.equal(await page.locator('[name="acquisition.enabled"]').isEnabled(), true);
    if (testGalileo) {
      assert.equal(bootstrap.galileo_acquisition, true);
      const enable = page.locator('[name="galileo_acquisition.enabled"]');
      assert.equal(await enable.inputValue(), 'false');
      await enable.selectOption('true');
      await page.locator('#apply').click();
      await until(async () => (await get('config')).revision !== config.revision, 'Galileo profile applied');
      config = await get('config');
      assert.match(config.ini, /\[galileo_acquisition\]\s+enabled=true/);
    }
    await post('receiver/start', {revision: config.revision});
    const found = await until(async () => {
      const status = await get('status'); return status.acquisition?.results.find(result => result.confirmed);
    }, 'GPS confirmation').catch(async error => { throw Error(error.message + JSON.stringify(await get('status'))); });
    assert.equal(found.prn, 7); assert(Math.abs(found.doppler_hz - 1234) < 35);
    assert(Math.abs(found.code_phase_chips - 231.3) < 0.6); assert.equal(found.successes, 3);
    await page.locator('.workspace-nav-item[data-view="acquisition"]').click();
    await until(async () => (await page.locator('[data-acquisition-results]').textContent()).includes('Confirmed'), 'result table');
    await until(async () => (await page.locator('.caf-details').first().textContent()).includes('GPS 7'), 'completed search space');
    const space = await get('acquisition/search-space?prn=7');
    assert.equal(space.valid, true); assert.equal(space.prn, 7); assert.equal(space.confirmed, true);
    assert.equal(space.power.length, 16); assert.equal(space.power[0].length, 1024);
    assert.equal(space.doppler_hz[0], -5000); assert.equal(space.doppler_hz[15], 5000);
    assert(Math.abs(Math.max(...space.power.flat()) - space.peak_ratio) < 1e-4);
    assert.equal((await fetch(base + '/api/v1/acquisition/search-space?prn=33')).status, 400);
    assert.equal((await get('acquisition/search-space?prn=6')).valid, false);
    await page.locator('[data-search-space] canvas[data-mode="2d"]').waitFor();
    await until(async () => (await get('status')).acquisition.results.some(result => result.prn === 8), 'absent satellite round');
    const absent = await get('acquisition/search-space?prn=8');
    assert.equal(absent.valid, true); assert.equal(absent.confirmed, false); if (!testGalileo) assert(absent.peak_ratio < 1);
    await until(async () => await page.locator('[data-search-space] select option[value="8"]').count(), 'absent satellite picker');
    await page.locator('[data-search-space] select').selectOption('8');
    await until(async () => (await page.locator('.caf-details').first().textContent()).includes(absent.peak_ratio > 1 ? 'GPS 8 · Coarse candidate, not confirmed' : 'GPS 8 · Below coarse threshold'), 'noise map');
    await page.screenshot({path: path.join(root, 'acquisition-no-detection.png'), fullPage: true});
    await page.locator('[data-search-space] select').selectOption('7');
    await until(async () => (await page.locator('.caf-details').first().textContent()).includes('GPS 7'), 'signal map selected');
    const assertViews = async stacked => {
      const plots = page.locator('[data-search-space] canvas');
      assert.equal(await plots.count(), 2);
      await until(async () => (await plots.evaluateAll(nodes => nodes.every(node => node.dataset.prn === '7'))), 'both GPS 7 views');
      const datasets = await plots.evaluateAll(nodes => nodes.map(node => ({...node.dataset})));
      for (const field of ['prn','capture','codeRange','dopplerRange','peak'])
        assert.equal(datasets[0][field], datasets[1][field], 'Shared ' + field);
      const a = await plots.nth(0).boundingBox(), b = await plots.nth(1).boundingBox();
      assert(a.width > 200 && b.width > 200);
      assert(stacked ? b.y >= a.y + a.height : Math.abs(a.y - b.y) < 1 && b.x >= a.x + a.width);
    };
    await assertViews(false);
    await page.screenshot({path: path.join(root, 'acquisition-3d.png'), fullPage: true});
    await page.setViewportSize({width: 390, height: 844});
    await assertViews(true);
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
    await page.screenshot({path: path.join(root, 'acquisition-mobile.png'), fullPage: true});
    await page.setViewportSize({width: 1440, height: 1000});
    await assertViews(false);
    await page.screenshot({path: path.join(root, 'acquisition.png'), fullPage: true});
    if (testGalileo) {
      await until(async () => (await get('status')).galileo_acquisition.results.some(result => result.prn === 7 && result.confirmed), 'Galileo confirmation');
      const picker = page.getByLabel('Acquisition signal', {exact:true});
      await picker.selectOption('galileo');
      await until(async () => (await page.locator('.caf-details').first().textContent()).includes('Galileo 7'), 'Galileo selected map');
      const galMap = await get('acquisition/search-space?signal=galileo&prn=7');
      assert.equal(galMap.valid,true); assert.equal(galMap.confirmed,true);
      assert.equal(galMap.code_period_chips,4092); assert.equal(galMap.power.length,116);
      assert.equal(galMap.doppler_hz[115],5005);
      await until(async () => page.locator('[data-search-space] canvas').evaluateAll(nodes => nodes.every(node => node.dataset.signal === 'galileo' && node.dataset.codeRange === '0:4092' && node.dataset.dopplerRange === '-5000:5005')), 'Galileo synchronized plots');
      assert(await page.locator('[data-satellite="Galileo:E1B:7"]').count());
      await page.screenshot({path:path.join(root,'galileo.png'),fullPage:true});
      await picker.selectOption('gps'); await assertViews(false);
      await page.getByLabel('Search space satellite',{exact:true}).selectOption('8');
      await picker.selectOption('galileo');
      assert.equal(await page.getByLabel('Search space satellite',{exact:true}).inputValue(),'7');
      await picker.selectOption('gps');
      assert.equal(await page.getByLabel('Search space satellite',{exact:true}).inputValue(),'8');
      await page.getByLabel('Search space satellite',{exact:true}).selectOption('7');
    }
    await post('receiver/stop', {revision: config.revision});
    await until(async () => (await get('status')).state === 'stopped', 'stop');
    const stoppedSpace = await get('acquisition/search-space?prn=7');
    assert.equal(stoppedSpace.valid, true, 'Completed maps survive receiver stop');
    assert.equal(stoppedSpace.prn, 7);
    await until(async () => await page.locator('.acquisition-acquired').count(), 'green confirmed satellite');
    const acquiredColour = await page.locator('.acquisition-acquired').evaluate(node => getComputedStyle(node).backgroundColor);
    assert.equal(acquiredColour, 'rgb(18, 53, 47)');
    if (testGalileo) {
      assert.equal((await get('acquisition/search-space?signal=galileo&prn=7')).valid,true,'Galileo map survives stop');
      assert.equal((await fetch(base + '/api/v1/acquisition/search-space?signal=invalid&prn=7')).status,400);
      await page.locator('[data-open-settings="acquisition"]').first().click();
      await until(async () => await page.locator('[name="acquisition.enabled"]').isEnabled(), 'settings unlocked');
      await page.locator('[name="acquisition.enabled"]').selectOption('false');
      await page.locator('#apply').click();
      await until(async () => (await get('config')).revision !== config.revision, 'Galileo-only profile');
      config = await get('config');
      assert.match(config.ini, /\[acquisition\]\s+enabled=false/);
      await post('receiver/start',{revision:config.revision});
      await until(async () => (await get('status')).galileo_acquisition.results.some(result => result.confirmed), 'Galileo-only confirmation');
      assert.equal((await get('status')).acquisition.enabled,false);
      assert.deepEqual((await get('status')).acquisition.results,[]);
      await page.locator('.workspace-nav-item[data-view="acquisition"]').click();
      await page.getByLabel('Acquisition signal',{exact:true}).selectOption('galileo');
      await until(async () => page.locator('[data-search-space] canvas').evaluateAll(nodes => nodes.every(node => node.dataset.signal === 'galileo')), 'Galileo-only plots');
      await post('receiver/stop',{revision:config.revision});
      await until(async () => (await get('status')).state === 'stopped','Galileo-only stop');

    }
    assert.deepEqual(errors, []);
    await post('shutdown', {}); assert.equal(await exited, 0);
    console.log((testGalileo ? 'PASS: mixed GPS/Galileo and Galileo-only UI activation, persistence, results, plot switching and stop. ' : 'PASS: GPS form,') + ' persisted modes, worker confirmation, provenance, result table and stop. ' + root);
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill('SIGTERM'); await exited; }
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
