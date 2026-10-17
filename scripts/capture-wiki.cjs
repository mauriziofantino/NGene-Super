/* Capture the real receiver GUI with a deterministic replay fixture; Node/Playwright are
 * optional documentation tools, never receiver dependencies. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const net = require('node:net');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const executable = path.resolve(root, process.env.NGENE_EXECUTABLE || 'build/ngene');
const images = path.join(root, 'docs/wiki/images');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(predicate, message, timeout = 10000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) { if (await predicate()) return; await delay(50); }
  throw new Error(`Timed out: ${message}`);
}
(async () => {
  assert(fs.existsSync(executable), `Missing executable: ${executable}`);
  fs.mkdirSync(path.join(root, 'build'), {recursive:true});
  const fixture = fs.mkdtempSync(path.join(root, 'build/wiki-capture-'));
  const input = path.join(fixture, 'documentation-demo.cs8');
  const relativeInput = path.relative(root, input).replaceAll('\\', '/');
  const pairs = 200000, iq = Buffer.alloc(pairs * 2);
  // Two unmodulated tones, explicitly an illustration rather than a satellite recording.
  for (let n = 0; n < pairs; n++) {
    const phase = 2 * Math.PI * n;
    iq.writeInt8(Math.round(35 * Math.cos(phase / 10) + 12 * Math.cos(phase / 17)), n * 2);
    iq.writeInt8(Math.round(35 * Math.sin(phase / 10) + 12 * Math.sin(phase / 17)), n * 2 + 1);
  }
  fs.writeFileSync(input, iq, {flag:'wx'});
  const probe = net.createServer();
  await new Promise((resolve, reject) => { probe.once('error', reject); probe.listen(0, '127.0.0.1', resolve); });
  const port = probe.address().port;
  await new Promise(resolve => probe.close(resolve));
  const base = `http://127.0.0.1:${port}`;
  const child = spawn(executable, ['--web', '--port', String(port), '--output-dir', path.join(fixture, 'profile')], {cwd:root, stdio:['ignore', 'pipe', 'inherit']});
  const exit = new Promise(resolve => child.once('exit', resolve));
  child.stdout.resume();
  let browser, token;
  try {
    await until(async () => {
      assert.equal(child.exitCode, null, 'Receiver exited before startup');
      try { token = (await (await fetch(`${base}/api/v1/bootstrap`)).json()).token; return Boolean(token); }
      catch { return false; }
    }, 'receiver startup');
    const get = async route => (await fetch(`${base}/api/v1/${route}`)).json();
    const post = async (route, body) => {
      const response = await fetch(`${base}/api/v1/${route}`, {method:'POST', headers:{'Content-Type':'application/json', 'X-NGene-Token':token}, body:JSON.stringify(body)});
      const result = await response.json();
      assert(response.ok, `${route}: ${JSON.stringify(result)}`);
      return result;
    };
    const config = await get('config');
    await post('config', {revision:config.revision, ini:`[receiver]\nmode=capture\nrun_seconds=0\nidle_timeout_ms=3000\n[frontend]\ntype=file\nsample_rate_hz=100000\ncenter_frequency_hz=1575420000\nsignal_frequency_hz=1575420000\n[replay]\nformat=CI8\npath=${relativeInput}\nrealtime=true\nloop=true\n[conditioning]\nmodule=bypass\n[buffer]\nwindow_samples=1000\ningress_blocks=128\nring_blocks=16\noverflow_policy=stop\n[recording]\nenabled=false\n[analysis]\nspectrum_enabled=true\nhistogram_enabled=true\nfft_size=2048\naverages=4\nupdate_hz=15\nhistogram_pairs=4096\nhistogram_update_hz=4\n`});
    browser = await chromium.launch({headless:true, ...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE} : {}), args:['--no-sandbox']});
    const page = await browser.newPage({viewport:{width:1440, height:1100}, locale:'en-GB'});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(base);
    await until(() => page.locator('#connection').textContent().then(text => text === 'Connected'), 'GUI connection');
    await page.locator('#showConfiguration').click();
    await page.locator('[data-settings="display"]').click();
    await page.locator('#waterfallRows').fill('32');
    await page.locator('#waterfallRows').dispatchEvent('change');
    await page.locator('#waterfallRate').fill('15');
    await page.locator('#waterfallRate').dispatchEvent('change');
    await page.locator('#showObservation').click();
    await page.locator('#refresh').selectOption('33');
    await page.locator('#start').click();
    await until(() => page.locator('#runState').textContent().then(text => text === 'running'), 'running replay');
    try {
      await until(() => page.evaluate(async () => (await import('/app.js')).diagnosticSnapshot().history.length >= 32), 'waterfall history', 30000);
    } catch (error) {
      const status = await get('status');
      console.error('Capture diagnostic state:', JSON.stringify({state:status.state, message:status.message, processed:status.processed_samples, analysis:status.analysis?.config, sequence:status.analysis?.sequence_id, browser:await page.evaluate(async () => (await import('/app.js')).diagnosticSnapshot()), errors}));
      throw error;
    }
    fs.mkdirSync(images, {recursive:true});
    await page.locator('#refresh').selectOption('250');
    const plots = await page.locator('#spectrumPanel').boundingBox();
    await page.setViewportSize({width:1440, height:Math.ceil(plots.y + plots.height + 16)});
    await delay(150);
    await page.screenshot({path:path.join(images, 'receiver-dashboard.png')});
    await page.locator('#waterfallMode').selectOption('3d');
    await delay(300);
    await page.locator('#spectrumPanel').screenshot({path:path.join(images, 'waterfall-3d.png')});
    await page.locator('#stop').click();
    await until(() => page.locator('#runState').textContent().then(text => text === 'stopped'), 'stopped replay');
    await page.locator('#showConfiguration').click();
    await page.locator('[data-settings="source"]').click();
    await page.screenshot({path:path.join(images, 'configuration.png'), fullPage:true});
    await page.locator('#browseReplay').click();
    await until(() => page.locator('#pickerDirectory').inputValue().then(Boolean), 'initial browser folder');
    await page.locator('#pickerDirectory').fill(fixture);
    await page.locator('#pickerNavigation button[type="submit"]').click();
    try {
      await until(() => page.locator('#pickerEntries').textContent().then(text => text.includes('documentation-demo.cs8')), 'file browser entries');
    } catch (error) {
      console.error('Picker:', await page.locator('#pickerNotice').textContent(), await page.locator('#pickerDirectory').inputValue(), await page.locator('#pickerEntries').textContent());
      throw error;
    }
    await page.locator('#pickerEntries tr').filter({hasText:'documentation-demo.cs8'}).getByRole('button', {name:'Select', exact:true}).click();
    await until(() => page.locator('#pickerUse').isEnabled(), 'file metadata inspection');
    // Capture only the file list; absolute machine paths in the preview stay out of
    // published assets. No pixels, text or application state are altered for the capture.
    const list = await page.locator('.picker-list').boundingBox();
    await page.screenshot({path:path.join(images, 'replay-file-picker.png'), clip:list});
    assert.deepEqual(errors, [], 'Unexpected browser errors');
    console.log('Captured four real GUI screenshots using deterministic CI8 replay. No RF hardware or GNSS solution is implied.');
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null && token) {
      await fetch(`${base}/api/v1/shutdown`, {method:'POST', headers:{'Content-Type':'application/json', 'X-NGene-Token':token}, body:'{}'}).catch(() => {});
    }
    if (child.exitCode === null) await Promise.race([exit, delay(5000)]);
    if (child.exitCode === null) child.kill();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
