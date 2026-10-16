/* Optional browser regression test. Requires Node >=18 and Playwright on the test host. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const assert = require('node:assert/strict');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

(async () => {
  const executable = path.resolve(process.env.NGENE_EXECUTABLE || 'build/ngene');
  const temporary = fs.realpathSync.native(os.tmpdir());
  // CivetWeb requires canonical paths, including Windows TEMP short-name aliases.
  const root = fs.realpathSync.native(fs.mkdtempSync(path.join(temporary, 'ngene-validation-')));
  const profile = path.join(root, 'profile');
  const port = process.env.NGENE_TEST_PORT || '8098';
  const base = `http://127.0.0.1:${port}`;
  const child = spawn(executable, ['--web', '--port', port, '--output-dir', profile], {stdio:['ignore', 'pipe', 'inherit']});
  const exit = new Promise(resolve => child.once('exit', resolve));
  child.stdout.resume();
  let browser;
  const errors = [];
  try {
    let bootstrap;
    for (let attempt = 0; attempt < 100; attempt++) {
      try { bootstrap = await (await fetch(`${base}/api/v1/bootstrap`)).json(); break; }
      catch { if (child.exitCode !== null) break; await delay(50); }
    }
    assert(bootstrap, 'Receiver service did not start');
    browser = await chromium.launch({headless:true, ...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE} : {}), args:['--no-sandbox']});
    const page = await browser.newPage({viewport:{width:1512, height:1100}});
    page.on('pageerror', error => errors.push(error.message));
    let websocketFrames = 0, profilePosts = 0, analysisPosts = 0;
    page.on('websocket', socket => socket.on('framereceived', () => websocketFrames++));
    page.on('request', request => {
      if (request.method() !== 'POST') return;
      if (/\/api\/v1\/config(?:\/validate)?$/.test(request.url())) profilePosts++;
      if (request.url().endsWith('/api/v1/analysis/config')) analysisPosts++;
    });
    // A missing optional RTL driver must not skip client validation coverage.
    // Only the bootstrap capability flag is supplied; no RF hardware is simulated.
    if (!bootstrap.rtlsdr) await page.route('**/api/v1/bootstrap', async route => {
      const response = await route.fetch();
      const body = await response.json();
      await route.fulfill({response, json:{...body, rtlsdr:true}});
    });
    const waitUntil = async (predicate, message) => {
      const deadline = Date.now() + 10000;
      while (Date.now() < deadline) {
        if (await predicate()) return;
        await delay(25);
      }
      throw new Error(`Timed out: ${message}`);
    };
    const field = key => page.locator(`[name="${key}"]`);
    const inlineError = input => input.locator('..').locator('small.field-error');
    const invalidField = async input => {
      await waitUntil(async () => await input.getAttribute('aria-invalid') === 'true', 'persistent field error');
      assert(await inlineError(input).isVisible(), 'An invalid field must have a visible inline explanation');
      assert((await inlineError(input).textContent()).trim(), 'An inline error must explain the problem');
      const describedBy = (await input.getAttribute('aria-describedby') || '').split(/\s+/);
      assert(describedBy.includes(await inlineError(input).getAttribute('id')), 'Assistive technology must associate the inline error with its field');
    };
    const correctedField = async input => {
      await waitUntil(async () => await input.getAttribute('aria-invalid') !== 'true', 'corrected field error to clear');
      assert(!(await inlineError(input).isVisible()) || !(await inlineError(input).textContent()).trim(), 'A corrected field must not retain an obsolete error');
    };
    // Sample the visible state throughout several real telemetry replies, rather
    // than comparing only the two endpoints of a potentially flickering message.
    const stableError = async (summaryId, input = null) => {
      const summary = page.locator(`#${summaryId}`);
      const message = await summary.textContent();
      assert(message.trim(), 'The validation summary must contain an explanation');
      const explanation = input ? await inlineError(input).textContent() : null;
      const start = websocketFrames;
      const deadline = Date.now() + 10000;
      while (websocketFrames - start < 8) {
        assert(Date.now() < deadline, 'Validation persistence requires at least eight live telemetry replies');
        assert.equal(await summary.textContent(), message, 'Telemetry replaced the validation summary');
        assert(await summary.evaluate(node => node.classList.contains('error')), 'Telemetry cleared the validation error styling');
        if (input) {
          assert.equal(await input.getAttribute('aria-invalid'), 'true', 'Telemetry cleared field validity');
          assert.equal(await inlineError(input).textContent(), explanation, 'Telemetry replaced the inline error');
          assert(await inlineError(input).isVisible(), 'Telemetry hid the inline error');
        }
        await delay(25);
      }
    };
    await page.goto(base);
    await waitUntil(() => page.locator('#connection').textContent().then(value => value === 'Connected'), 'WebSocket connection');
    await waitUntil(() => page.locator('#runState').textContent().then(value => value === 'stopped'), 'stopped receiver');
    assert(await page.locator('#configForm').evaluate(form => form.noValidate), 'Profile validation must not use transient native popups');
    assert(await page.locator('#analysisForm').evaluate(form => form.noValidate), 'Analysis validation must not use transient native popups');
    await page.locator('#refresh').selectOption('33');
    await page.locator('#showConfiguration').click();
    await page.locator('#rtlPreset').click();
    const rate = field('frontend.sample_rate_hz');
    await rate.fill('16000000');
    const beforeInvalidRate = profilePosts;
    await page.locator('#apply').click();
    await invalidField(rate);
    await stableError('configMessage', rate);
    assert.equal(profilePosts, beforeInvalidRate, 'An unsupported RTL-SDR sample rate reached the backend');
    await rate.fill('2400000');
    await correctedField(rate);
    assert(!(await page.locator('#configMessage').evaluate(node => node.classList.contains('error'))));

    // The RTL2832U clock also excludes a range inside its minimum/maximum limits.
    for (const value of ['300001', '500000', '900000']) {
      await rate.fill(value);
      await page.locator('#apply').click();
      await invalidField(rate);
      assert.equal(profilePosts, beforeInvalidRate, `Unsupported RTL-SDR rate ${value} reached the backend`);
    }
    for (const value of ['225001', '300000', '900001', '3200000', '2400000']) {
      await rate.fill(value);
      await correctedField(rate);
      assert(await rate.evaluate(input => input.checkValidity()), `Supported RTL-SDR rate ${value} was rejected`);
    }
    const ppm = field('rtlsdr.ppm');
    await ppm.fill('-25');
    assert(await ppm.evaluate(input => input.checkValidity()), 'Negative frequency correction is valid');
    await correctedField(ppm);

    await page.locator('[data-settings="receiver"]').click();
    const ring = field('buffer.ring_blocks');
    for (const value of ['1.5', '0', '']) {
      await ring.fill(value);
      await page.locator('#apply').click();
      await invalidField(ring);
      assert.equal(profilePosts, beforeInvalidRate, `Invalid queue size ${JSON.stringify(value)} reached the backend`);
      await ring.fill('16');
      await correctedField(ring);
    }
    const windowSamples = field('buffer.window_samples');
    await windowSamples.fill('1.5');
    await page.locator('[data-settings="profile"]').click();
    await page.locator('#apply').click();
    await invalidField(windowSamples);
    assert.equal(await page.locator('[data-settings="receiver"]').getAttribute('aria-pressed'), 'true', 'Validation must reveal the topic containing the invalid field');
    await windowSamples.fill('2400');
    await correctedField(windowSamples);
    const timeout = field('receiver.idle_timeout_ms');
    await timeout.fill('99');
    await page.locator('#apply').click();
    await invalidField(timeout);
    await timeout.fill('2000');
    await correctedField(timeout);
    assert.equal(profilePosts, beforeInvalidRate, 'Local validation errors must never save the draft');

    let validations = 0;
    const backendError = 'Backend validation fixture: inconsistent stream configuration.';
    await page.route('**/api/v1/config/validate', async route => {
      validations++;
      await delay(200);
      await route.fulfill({status:validations === 1 || validations === 3 ? 400 : 200,
        contentType:'application/json', body:JSON.stringify(validations === 1 || validations === 3 ? {error:backendError} : {valid:true})});
    });
    await page.locator('[data-settings="profile"]').click();
    await page.locator('#validate').click();
    await waitUntil(() => page.locator('#configMessage').textContent().then(value => value.includes(backendError)), 'backend validation error');
    await waitUntil(() => page.locator('#validate').isEnabled(), 'validation controls after HTTP failure');
    await stableError('configMessage');
    // A retry can replace the old error only after its own successful response.
    await page.locator('#validate').click();
    await waitUntil(async () => validations === 2 && await page.locator('#validate').isEnabled(), 'successful validation retry');
    assert(!(await page.locator('#configMessage').evaluate(node => node.classList.contains('error'))), 'Successful retry retained the backend error');
    await page.locator('#validate').click();
    await waitUntil(() => page.locator('#configMessage').textContent().then(value => value.includes(backendError)), 'second backend validation error');
    await waitUntil(() => page.locator('#validate').isEnabled(), 'controls after second HTTP failure');
    await page.locator('[data-settings="source"]').click();
    await ppm.fill('-24');
    assert(!(await page.locator('#configMessage').evaluate(node => node.classList.contains('error'))), 'Editing the rejected draft retained an obsolete backend error');
    await page.locator('[data-settings="profile"]').click();
    await page.locator('#validate').click();
    await waitUntil(async () => validations === 4 && await page.locator('#validate').isEnabled(), 'validation after correcting the draft');
    assert(!(await page.locator('#configMessage').evaluate(node => node.classList.contains('error'))));
    await page.unroute('**/api/v1/config/validate');

    // Device-limit requests may complete after the user chooses another source.
    // The delayed fixture must not become the new source's cached capabilities.
    let capabilitiesRequested = false, capabilitiesReplied = false;
    await page.route('**/api/v1/frontend/capabilities', async route => {
      capabilitiesRequested = true;
      await delay(200);
      await route.fulfill({contentType:'application/json', body:JSON.stringify({
        format:'STALE_LIMITS_FIXTURE', sample_rate_min_hz:225001, sample_rate_max_hz:1000000,
        frequency_min_hz:47000000, frequency_max_hz:1766000000, tuner_gains_tenths_db:[0,77]
      })});
      capabilitiesReplied = true;
    });
    await page.locator('[data-settings="source"]').click();
    await page.locator('#deviceLimits').click();
    await waitUntil(() => capabilitiesRequested, 'device capabilities request');
    await page.locator('#filePreset').click();
    const replayRateMaximum = await rate.getAttribute('max');
    await waitUntil(() => capabilitiesReplied, 'stale device capabilities response');
    await delay(100);
    assert.equal(await field('frontend.type').inputValue(), 'file');
    assert.equal(await rate.getAttribute('max'), replayRateMaximum, 'A stale RTL-SDR response changed the file replay sample-rate limits');
    assert(!(await page.locator('#notice').textContent()).includes('STALE_LIMITS_FIXTURE'), 'The UI reported stale device capabilities as current');
    await page.unroute('**/api/v1/frontend/capabilities');

    await page.locator('[data-settings="analysis"]').click();
    const overlap = page.locator('#analysis_overlap_percent');
    const beforeInvalidAnalysis = analysisPosts;
    await overlap.fill('100');
    await page.locator('#applyAnalysis').click();
    await invalidField(overlap);
    await stableError('analysisNotice', overlap);
    assert.equal(analysisPosts, beforeInvalidAnalysis, 'Invalid analysis settings reached the backend');
    await overlap.fill('37');
    await correctedField(overlap);
    assert(!(await page.locator('#analysisNotice').evaluate(node => node.classList.contains('error'))));
    await page.locator('#applyAnalysis').click();
    await waitUntil(() => page.locator('#analysisNotice').textContent().then(value => value.startsWith('Applied')), 'valid analysis settings applied');
    assert.equal(analysisPosts, beforeInvalidAnalysis + 1);
    assert.deepEqual(errors, [], 'Browser JavaScript errors');
    await fetch(`${base}/api/v1/shutdown`, {method:'POST', headers:{'Content-Type':'application/json', 'X-NGene-Token':bootstrap.token}, body:'{}'});
    assert.equal(await exit, 0);
    console.log('PASS: persistent profile and analysis inline validation across live telemetry; RTL-SDR 16 MHz rejection, supported ranges and forbidden clock interval; integer/required queue and window bounds, hidden-topic focus, negative PPM, timeout bounds; delayed backend errors, HTTP cleanup, successful retry and draft correction; stale device capabilities after changing source. No RF hardware required.');
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill('SIGTERM'); await exit; }
    const resolved = fs.realpathSync.native(root);
    assert(resolved.startsWith(`${temporary}${path.sep}`) && path.basename(resolved).startsWith('ngene-validation-'), 'Refusing to remove a path outside the test temporary directory');
    fs.rmSync(resolved, {recursive:true, force:true});
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
