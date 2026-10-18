/* Actual embedded receiver, independently generated GPS I/Q and production browser UI.
 * Requires Node >=18 and Playwright on the test host; no simulated receiver endpoints. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const net = require('node:net');
const os = require('node:os');
const path = require('node:path');
const sleep = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));

async function until(predicate, message, timeout = 6000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    const value = await predicate();
    if (value) return value;
    await sleep(20);
  }
  throw new Error(`Timed out: ${message}`);
}

/* Test-side shift registers and independent full-rate sampling do not import DSP code. */
function gpsPrn3() {
  const g1 = Array(10).fill(1), g2 = Array(10).fill(1), chips = [];
  for (let n = 0; n < 1023; ++n) {
    const a = g1[2] ^ g1[9], b = g2[1] ^ g2[2] ^ g2[5] ^ g2[7] ^ g2[8] ^ g2[9];
    chips.push((g1[9] ^ g2[3] ^ g2[7]) ? 1 : -1);
    g1.pop(); g1.unshift(a); g2.pop(); g2.unshift(b);
  }
  let hash = 2166136261;
  for (const chip of chips) hash = Math.imul((hash ^ (chip & 255)) >>> 0, 16777619) >>> 0;
  assert.equal(hash, 0xb41e7fe0, 'PRN 3 chip order and polarity match original NGene');
  return chips;
}

function gpsFixture() {
  const rate = 2046000, delay = 509, doppler = 350, digitalIf = 24000;
  const chips = gpsPrn3(), data = Buffer.alloc(rate * 2);
  let random = 0x13579bdf;
  const uniform = () => {
    random ^= random << 13; random ^= random >>> 17; random ^= random << 5;
    return ((random >>> 0) + 0.5) / 4294967296;
  };
  const clamp = number => Math.max(-128, Math.min(127, Math.round(number)));
  for (let n = 0; n < rate; ++n) {
    const codeSample = ((n - delay) % 2046 + 2046) % 2046;
    const amplitude = 35 * chips[Math.floor(codeSample / 2)];
    const phase = 2 * Math.PI * (digitalIf + doppler) * n / rate + 0.37;
    const radius = 6 * Math.sqrt(-2 * Math.log(uniform())), angle = 2 * Math.PI * uniform();
    data.writeInt8(clamp(amplitude * Math.cos(phase) + radius * Math.cos(angle)), 2 * n);
    data.writeInt8(clamp(amplitude * Math.sin(phase) + radius * Math.sin(angle)), 2 * n + 1);
  }
  return {data, rate, delay, doppler, digitalIf};
}

function circularDistance(a, b, period) {
  const distance = Math.abs(a - b) % period;
  return Math.min(distance, period - distance);
}

(async () => {
  const executable = path.resolve(process.env.NGENE_EXECUTABLE || 'build/ngene');
  assert(fs.existsSync(executable), `Receiver executable not found: ${executable}`);
  const probe = net.createServer();
  await new Promise((resolve, reject) => {
    probe.once('error', reject);
    probe.listen(Number(process.env.NGENE_TEST_PORT || 0), '127.0.0.1', resolve);
  });
  const port = String(probe.address().port);
  await new Promise(resolve => probe.close(resolve));
  const temporaryRoot = fs.realpathSync.native(fs.mkdtempSync(path.join(os.tmpdir(), 'ngene-gps-ui-')));
  const input = path.join(temporaryRoot, 'independent-gps-prn3-ci8.cs8');
  const profile = path.join(temporaryRoot, 'profile');
  const fixture = gpsFixture();
  fs.writeFileSync(input, fixture.data, {flag:'wx'});
  const childEnvironment = {...process.env};
  const schema = JSON.parse(fs.readFileSync(path.join(__dirname, '../web/module-parameters.json'), 'utf8'));
  if (process.env.NGENE_TEST_SCHEMA) {
    const step = schema.modules.flatMap(m => m.groups).flatMap(g => g.fields).find(f => f.key === 'acquisition.doppler_step_hz');
    step.label = 'User-library Doppler spacing';
    childEnvironment.NGENE_MODULE_SCHEMA = path.join(temporaryRoot, 'custom-modules.json');
    fs.writeFileSync(childEnvironment.NGENE_MODULE_SCHEMA, JSON.stringify(schema));
  } else delete childEnvironment.NGENE_MODULE_SCHEMA;
  const base = `http://127.0.0.1:${port}`;
  const child = spawn(executable, ['--web', '--port', port, '--output-dir', profile],
    {stdio:['ignore', 'pipe', 'pipe'], env:childEnvironment});
  const exit = new Promise(resolve => child.once('exit', resolve));
  let output = '', browser;
  child.stdout.on('data', chunk => { output = (output + chunk).slice(-10000); });
  child.stderr.on('data', chunk => { output = (output + chunk).slice(-10000); });
  try {
    const bootstrap = await until(async () => {
      assert.equal(child.exitCode, null, `Receiver exited before startup: ${output}`);
      try { return await (await fetch(`${base}/api/v1/bootstrap`)).json(); }
      catch { return false; }
    }, 'isolated receiver service startup');
    const get = async route => {
      const response = await fetch(`${base}/api/v1/${route}`);
      assert.equal(response.status, 200, `GET ${route}`);
      return response.json();
    };
    const post = async (route, body) => {
      const response = await fetch(`${base}/api/v1/${route}`, {method:'POST',
        headers:{'Content-Type':'application/json', 'X-NGene-Token':bootstrap.token}, body:JSON.stringify(body)});
      const result = await response.json();
      assert(response.ok, `POST ${route}: ${response.status} ${JSON.stringify(result)}`);
      return result;
    };
    await post('config', {revision:(await get('config')).revision, ini:`
[receiver]
mode=capture
run_seconds=0
idle_timeout_ms=3000
[frontend]
type=file
sample_rate_hz=${fixture.rate}
center_frequency_hz=${1575420000 - fixture.digitalIf}
signal_frequency_hz=1575420000
[replay]
format=CI8
path=${input}
realtime=true
loop=false
[conditioning]
module=bypass
[buffer]
window_samples=2046
ingress_blocks=128
ring_blocks=16
overflow_policy=stop
[recording]
enabled=false
directory=${path.join(temporaryRoot, 'captures')}
prefix=gps_ui_test
queue_blocks=128
[analysis]
spectrum_enabled=true
histogram_enabled=false
fft_size=1024
averages=1
update_hz=10
histogram_pairs=1024
histogram_bins=32
histogram_update_hz=10
[acquisition]
enabled=true
gps_prn_mask=4
gps_channels=1
fft_size=1024
doppler_max_hz=1000
doppler_step_hz=500
code_step_chips=0.5
false_alarm_probability=0.00001
noncoherent_sums=2
confirmation_attempts=4
confirmation_successes=3
refinement_doppler_span_hz=250
refinement_doppler_step_hz=50
refinement_code_span_chips=1
refinement_code_step_samples=1
`});
    browser = await chromium.launch({headless:true, args:['--no-sandbox'],
      ...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE} : {})});
    const page = await browser.newPage({viewport:{width:1512, height:1100}});
    const errors = [], failedResponses = [], profileWrites = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', event => { if (event.type() === 'error') errors.push(event.text()); });
    page.on('response', response => {
      if (response.url().startsWith(base) && response.status() >= 400)
        failedResponses.push(`${response.status()} ${response.url()}`);
    });
    page.on('request', request => {
      if (request.method() === 'POST' && request.url() === `${base}/api/v1/config`)
        profileWrites.push(request.postData());
    });
    const document = await page.goto(base);
    const csp = document.headers()['content-security-policy'];
    assert(csp && !csp.includes("'unsafe-eval'"), 'The real receiver keeps its production CSP');
    await until(() => page.locator('#connection').textContent().then(text => text === 'Connected'), 'UI connected');
    assert.equal(bootstrap.gps_acquisition, true, 'The receiver advertises the compiled GPS module');
    const acquisitionDot = page.locator('[data-view="acquisition"] .workspace-nav-dot');
    await until(() => acquisitionDot.getAttribute('class').then(classes => /\bavailable\b/.test(classes)),
      'sidebar acquisition availability follows the connected receiver capability');
    assert.match(await acquisitionDot.getAttribute('title'), /GPS.*acquisition available/i);
    const field = name => page.locator(`#configForm [name="${name}"]`);
    const select = async view => page.locator(`[data-view="${view}"]`).first().click();

    // Header switches change actual spectrum computation before acquisition starts.
    await page.locator('#showSpectrum').uncheck();
    await page.locator('#showWaterfall').uncheck();
    await until(async () => !(await get('analysis/config')).analysis.spectrum_enabled,
      'both RF switches disable runtime PSD computation');
    await select('setup');
    await page.locator('[data-settings="receiver"]').click();
    assert.equal(await field('acquisition.enabled').inputValue(), 'true');
    assert.equal(await field('acquisition.gps_prn_mask').inputValue(), '4');
    assert.equal(await field('acquisition.gps_channels').inputValue(), '1');
    assert(await field('acquisition.enabled').isEnabled(), 'GPS enable is an operational receiver parameter');
    const galileo = page.locator('[data-reference-param="Number of supported Galileo channels"]');
    assert(await galileo.isDisabled(), 'Galileo remains a disabled reference parameter');
    await page.locator('[data-settings="acquisition"]').click();
    const dopplerStep = field('acquisition.doppler_step_hz');
    assert(await dopplerStep.isVisible() && await dopplerStep.isEnabled(), 'GPS search controls are operational');
    assert.deepEqual(await get('module-schema'), schema, 'The receiver serves its actual module descriptor');
    if (process.env.NGENE_TEST_SCHEMA)
      assert.match(await dopplerStep.locator('..').textContent(), /User-library Doppler spacing/);
    assert.equal(await field('acquisition.detector').inputValue(), 'pfa');
    assert.equal(await field('acquisition.search_method').inputValue(), 'ngene_fft');
    await field('acquisition.confirmation').selectOption('none');
    assert(await field('acquisition.confirmation_attempts').isHidden());
    assert(await field('acquisition.confirmation_successes').isHidden());
    assert(await field('acquisition.confirmation_successes').isDisabled());
    await field('acquisition.confirmation').selectOption('m_of_n');
    assert(await field('acquisition.confirmation_successes').isVisible());
    const beforeValidation = await get('config');
    const invalidSave = async (input, value, expectedMessage, restoredValue) => {
      const requests = profileWrites.length;
      await input.fill(value); await input.blur();
      await until(() => input.getAttribute('aria-invalid').then(value => value === 'true'),
        'invalid acquisition value has inline field feedback');
      await page.locator('#apply').click();
      const feedback = page.locator(`[id="${await input.getAttribute('id')}_error"]`);
      assert.match(await feedback.textContent(), expectedMessage);
      assert.match(await page.locator('#configMessage').getAttribute('class'), /\berror\b/);
      await sleep(50);
      assert.equal(profileWrites.length, requests, 'Invalid acquisition settings must not issue a profile POST');
      assert.deepEqual(await get('config'), beforeValidation, 'Invalid local edits preserve the saved receiver configuration');
      await input.fill(restoredValue); await input.blur();
      await until(() => page.locator('#configForm [aria-invalid="true"]').count().then(count => count === 0),
        'restoring the acquisition value clears dependent validation errors');
    };
    await invalidSave(field('acquisition.confirmation_successes'), '5', /M successes.*N attempts/i, '3');
    await invalidSave(dopplerStep, '0', /positive step/i, '500');
    await invalidSave(field('acquisition.code_step_chips'), '0.001', /delay grid.*65536/i, '0.5');
    await dopplerStep.fill('250'); await dopplerStep.blur();
    await page.locator('#apply').click();
    await until(async () => /doppler_step_hz=250(?:\r?\n|$)/.test((await get('config')).ini),
      'GUI save reaches the actual acquisition profile');
    const saved = await get('config');
    assert.match(saved.ini, /\[acquisition\][\s\S]*gps_prn_mask=4(?:\r?\n|$)/);
    assert.match(saved.ini, /spectrum_enabled=false(?:\r?\n|$)/, 'Profile save preserves RF computation off');

    await until(() => page.locator('#start').isEnabled(), 'receiver ready to start');
    await page.locator('#start').click();
    await until(async () => (await get('status')).state === 'running', 'paced GPS replay running');
    await until(() => dopplerStep.isDisabled(), 'acquisition configuration locks during reception', 800);
    assert(await field('acquisition.enabled').isDisabled(), 'GPS module enable locks with the full receiver profile');
    assert(await page.locator('#apply').isDisabled(), 'Saving the receiver profile locks during reception');
    await select('acquisition');
    const acquired = await until(async () => {
      const status = await get('status');
      assert.notEqual(status.state, 'error', JSON.stringify(status));
      const satellite = status.acquisition.satellites.find(result => result.prn === 3 && result.phase === 'acquired');
      return satellite && {status, satellite};
    }, 'actual GPS PRN 3 confirmed by the C acquisition module');
    const {status, satellite} = acquired;
    assert.equal(status.acquisition.acquired_prn_mask, 4);
    assert.equal(status.acquisition.fft_size, 1024);
    assert.equal(status.acquisition.coherent_ms, 1);
    assert.equal(status.acquisition.channel_count, 1);
    assert(Math.abs(satellite.doppler_hz - fixture.doppler) <= 100, `Doppler: ${satellite.doppler_hz}`);
    assert.equal(satellite.carrier_hz, fixture.digitalIf + satellite.doppler_hz,
      'Physical Doppler remains distinct from the frontend digital IF');
    const expectedDelay = (fixture.delay - Number(satellite.reference_sample) % 2046 + 2046) % 2046;
    assert(circularDistance(satellite.code_delay_samples, expectedDelay, 2046) <= 3,
      `Delay ${satellite.code_delay_samples}, expected ${expectedDelay} at sample ${satellite.reference_sample}`);
    assert(satellite.confirmation_hits >= 3 && satellite.confirmation_trials <= 4);
    assert(satellite.metric > satellite.threshold && satellite.threshold > 0);
    assert(BigInt(satellite.processed_samples) > 2046n, 'Acquisition consumed multiple native millisecond frames');
    await until(async () => BigInt((await get('status')).processed_samples) > 2046n,
      'kernel publishes its periodically sampled processing counters');
    assert.equal(BigInt(status.analysis.sequence_id), 0n, 'PSD FFTs remain stopped while GPS acquisition executes');
    assert.equal(BigInt(status.analysis.histogram_sequence), 0n, 'Disabled histograms remain stopped');
    assert.equal((await get('analysis/config')).analysis.spectrum_enabled, false,
      'GPS acquisition does not enable spectrum computation');
    await until(() => page.locator('#acquisitionResults').textContent().then(text => /GPS 3/.test(text) && /acquired/.test(text)),
      'real acquired result reaches the UI table');
    assert.match(await page.locator('#acquisitionView').textContent(), /ready for tracking/i);
    assert.equal((await page.locator('#acquisitionResults tr').count()), 1, 'No synthetic satellite rows appear');
    assert.equal(await page.locator('[data-workspace-field="acqConfirmed"]').textContent(), '1');

    if (process.env.NGENE_ACQUISITION_SCREENSHOTS) {
      fs.mkdirSync(process.env.NGENE_ACQUISITION_SCREENSHOTS, {recursive:true});
      await page.screenshot({path:path.join(process.env.NGENE_ACQUISITION_SCREENSHOTS, 'gps-acquisition-prn3-real-replay.png'), fullPage:true});
    }
    await post('receiver/stop', {revision:(await get('config')).revision});
    await until(async () => (await get('status')).state === 'stopped', 'receiver stopped cleanly');
    await until(() => page.locator('#runState').textContent().then(text => /^stopped$/i.test(text)),
      'stopped receiver state reaches the browser');
    assert.equal(fs.statSync(input).size, fixture.data.length, 'The private input fixture is unchanged');
    await select('setup');
    await page.locator('[data-settings="acquisition"]').click();
    await until(() => field('acquisition.confirmation').isEnabled(), 'confirmation editable after stop');
    await field('acquisition.confirmation').selectOption('none');
    await page.locator('#apply').click();
    await until(async () => /\nconfirmation=none\r?\n/.test((await get('config')).ini), 'optional confirmation saved');
    if (process.env.NGENE_ACQUISITION_SCREENSHOTS) {
      await page.screenshot({path:path.join(process.env.NGENE_ACQUISITION_SCREENSHOTS, 'acquisition-settings-descriptor.png'), fullPage:true});
    }
    await until(() => page.locator('#start').isEnabled(), 'ready for replay without confirmation');
    await page.locator('#start').click();
    await select('acquisition');
    const withoutConfirmation = await until(async () => {
      const live = await get('status');
      return live.acquisition.satellites.find(s => s.phase === 'acquired' && s.confirmation_method === 'none');
    }, 'coarse/refined acquisition without M-of-N');
    assert.equal(withoutConfirmation.prn, 3);
    assert.equal(withoutConfirmation.confirmation_trials, 0);
    assert.equal(withoutConfirmation.confirmation_hits, 0);
    assert(withoutConfirmation.metric > withoutConfirmation.threshold && withoutConfirmation.threshold > 0);
    assert(BigInt(withoutConfirmation.processed_samples) < BigInt(satellite.processed_samples));
    await until(() => page.locator('#acquisitionResults').textContent().then(t => /Not requested/.test(t)), 'UI reports no confirmation');
    await post('receiver/stop', {revision:(await get('config')).revision});
    await until(async () => (await get('status')).state === 'stopped', 'second replay stopped');
    await page.setViewportSize({width:390, height:844});
    const geometry = await page.evaluate(() => ({width:innerWidth, scrollWidth:document.documentElement.scrollWidth}));
    assert(geometry.scrollWidth <= geometry.width + 1,
      `The acquired-results table must not overflow the mobile document: ${JSON.stringify(geometry)}`);
    if (process.env.NGENE_ACQUISITION_SCREENSHOTS)
      await page.screenshot({path:path.join(process.env.NGENE_ACQUISITION_SCREENSHOTS, 'gps-acquisition-prn3-mobile.png'), fullPage:true});
    await select('tracking');
    assert.match(await page.locator('#trackingView').textContent(), /Unavailable|not integrated/i,
      'Acquisition does not manufacture a tracking lock');
    assert.deepEqual(errors, [], 'No browser or CSP errors');
    assert.deepEqual(failedResponses, [], 'No failed application requests or missing embedded assets');
    await post('shutdown', {});
    assert.equal(await exit, 0, output);
    console.log(JSON.stringify({prn:satellite.prn, phase:satellite.phase, dopplerHz:satellite.doppler_hz,
      carrierHz:satellite.carrier_hz, delaySamples:satellite.code_delay_samples,
      expectedDelaySamples:expectedDelay, epoch:satellite.epoch, referenceSample:satellite.reference_sample,
      confirmation:`${satellite.confirmation_hits}/${satellite.confirmation_trials}`,
      rfSequence:status.analysis.sequence_id, histogramSequence:status.analysis.histogram_sequence}));
    console.log('PASS: embedded GPS acquisition UI; independently generated PRN 3 CI8/AWGN replay; capability-driven sidebar; invalid M>N, zero Doppler step and excessive code grid blocked locally without profile requests; actual configuration save and running locks; digital IF/Doppler/delay/epoch and M-of-N results; RF spectrum and histogram computation stay disabled; future tracking/Galileo unavailable; clean receiver shutdown and production CSP.');
  } catch (error) {
    if (output) console.error(output);
    throw error;
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill('SIGTERM'); await exit; }
    const relative = path.relative(fs.realpathSync.native(os.tmpdir()), temporaryRoot);
    assert(relative && !relative.startsWith('..') && !path.isAbsolute(relative), 'Cleanup stays inside this test temporary directory');
    fs.rmSync(temporaryRoot, {recursive:true, force:true});
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
