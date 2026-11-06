/* Real HTTP service and browser, linked to calibration_web_fixture.c only.
 * No hardware is opened; native measurement/cancellation has its separate C contract. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(fn, label) {
  for (let k = 0; k < 500; ++k) { const result = await fn(); if (result) return result; await delay(20); }
  throw Error('Timeout: ' + label);
}
(async () => {
  const executable = path.resolve(process.env.NGENE_EXECUTABLE || 'build/msvc-x64-multisdr/Release/ngene_web_calibration_fixture.exe');
  const root = fs.mkdtempSync(path.resolve('build/calibration-ui-'));
  const port = process.env.NGENE_TEST_PORT || '8141', base = `http://127.0.0.1:${port}`;
  const child = spawn(executable, ['--web', '--port', port, '--output-dir', path.join(root, 'profile')], {stdio:['ignore','pipe','pipe']});
  let log = '', browser; child.stdout.on('data', data => log += data); child.stderr.on('data', data => log += data);
  const exit = new Promise(resolve => child.once('exit', resolve));
  try {
    const bootstrap = await until(async () => {
      if (child.exitCode !== null) throw Error(log);
      try { return await (await fetch(base + '/api/v1/bootstrap')).json(); } catch { return false; }
    }, 'startup');
    assert(bootstrap.hackrf, 'Build the fixture with the HackRF adapter enabled');
    const get = async route => (await fetch(base + '/api/v1/' + route)).json();
    const post = async (route, body, expected) => {
      const response = await fetch(base + '/api/v1/' + route, {method:'POST', headers:{'Content-Type':'application/json','X-NGene-Token':bootstrap.token}, body:JSON.stringify(body)});
      assert.equal(response.status, expected); return response.json();
    };
    browser = await chromium.launch({headless:true, executablePath:process.env.BROWSER_EXECUTABLE, args:['--no-sandbox']});
    const page = await browser.newPage({viewport:{width:1440,height:1100}}), errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(base);
    await until(async () => await page.locator('#connection').textContent() === 'Connected', 'connection');
    await page.locator('#showConfiguration').click();
    await page.locator('[data-settings="source"]').click();
    await page.locator('#hackrfPreset').click();
    assert.equal(await page.locator('[name^="conditioning."]').count(), 0);
    assert.equal(await page.locator('#analysis_spectrum_source, #analysis_histogram_source').count(), 0);
    await page.locator('[name="hackrf.vga_gain_db"]').selectOption('6');
    await page.locator('#calibrateGains').click();
    await until(async () => !(await page.locator('#cancelCalibration').isDisabled()), 'cancel becomes available');
    assert.equal(await page.locator('#exit').isDisabled(), false, 'Service exit must remain available');
    assert.equal(await page.locator('[name="hackrf.vga_gain_db"]').isDisabled(), true, 'Only owned source controls are locked');
    const running = await get('frontend/calibrate'), config = await get('config');
    assert.equal(running.state, 'running');
    const response = await Promise.race([get('status'), delay(3000).then(() => { throw Error('Status blocked by calibration'); })]);
    assert.equal(response.state, 'stopped');
    await post('receiver/start', {revision:config.revision}, 409);
    await post('config', {revision:config.revision, ini:config.ini}, 409);
    await post('frontend/calibrate', {revision:config.revision, ini:config.ini}, 409);
    await post('frontend/calibrate/cancel', {job_id:'999999'}, 409);
    await page.screenshot({path:path.join(root, 'calibration-progress.png'), fullPage:true});
    await page.locator('#cancelCalibration').click();
    await until(async () => (await get('frontend/calibrate')).state === 'cancelled', 'worker cancellation');
    await until(async () => !(await page.locator('#calibrateGains').isDisabled()), 'controls restored');
    assert.equal(await page.locator('[name="hackrf.vga_gain_db"]').inputValue(), '6');
    await page.locator('[name="hackrf.vga_gain_db"]').selectOption('2');
    await page.locator('#calibrateGains').click();
    await until(async () => (await page.locator('#calibrationStatus').textContent()).includes('No setting passed'), 'rejection details');
    assert.equal(await page.locator('[name="hackrf.vga_gain_db"]').inputValue(), '2', 'Rejected gains must not change the draft');
    assert.match(await page.locator('#calibrationStatus').textContent(), /weak [1-9]/);
    await page.locator('[name="hackrf.vga_gain_db"]').selectOption('0');
    await page.locator('#calibrateGains').click();
    await until(async () => await page.locator('[name="hackrf.vga_gain_db"]').inputValue() === '28', 'accepted draft');
    assert.equal(await page.locator('[name="hackrf.lna_gain_db"]').inputValue(), '24');
    assert.equal((await get('config')).revision, config.revision, 'A suggestion must not save a profile');
    assert.match(await page.locator('#calibrationStatus').textContent(), /GNSS sensitivity is not measured/);
    await page.locator('[name="hackrf.vga_gain_db"]').selectOption('4');
    await page.locator('#calibrateGains').click();
    await until(async () => (await page.locator('#calibrationStatus').textContent()).includes('Simulated SDR transport failure'), 'failed worker');
    assert.equal(await page.locator('[name="hackrf.vga_gain_db"]').inputValue(), '4');
    await page.locator('[name="hackrf.vga_gain_db"]').selectOption('6');
    await page.locator('#calibrateGains').click();
    await until(async () => !(await page.locator('#cancelCalibration').isDisabled()), 'second long job');
    page.once('dialog', dialog => dialog.accept());
    await page.locator('#exit').click();
    assert.equal(await Promise.race([exit, delay(5000).then(() => { throw Error('Shutdown failed to cancel/join calibration'); })]), 0);
    assert.deepEqual(errors, []);
    console.log('PASS: async progress, responsive HTTP/exit, ownership guards, cancellation, rejection details, draft-only suggestion, worker error and shutdown. ' + root);
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill(); await exit; }
    fs.writeFileSync(path.join(root, 'service.log'), log);
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
