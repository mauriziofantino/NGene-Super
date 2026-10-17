/* Real-receiver plot visibility/subscription regression. Requires Node >=18 and Playwright. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const net = require('node:net');
const os = require('node:os');
const path = require('node:path');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const sequence = status => BigInt(status.analysis.sequence_id);
const histogramSequence = status => BigInt(status.analysis.histogram_sequence);

async function until(predicate, message, timeout = 5000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    const result = await predicate();
    if (result) return result;
    await delay(25);
  }
  throw new Error(`Timed out: ${message}`);
}

(async () => {
  const executable = path.resolve(process.env.NGENE_EXECUTABLE || 'build/ngene');
  assert(fs.existsSync(executable), `Receiver executable not found: ${executable}`);
  const port = process.env.NGENE_TEST_PORT || '8109';
  // Never attach to or change a receiver that another session already has running.
  const probe = net.createServer();
  await new Promise((resolve, reject) => {
    probe.once('error', reject);
    probe.listen(Number(port), '127.0.0.1', resolve);
  });
  await new Promise(resolve => probe.close(resolve));

  // Canonicalize Windows TEMP: CivetWeb rejects short-name directory aliases.
  const root = fs.realpathSync.native(fs.mkdtempSync(path.join(os.tmpdir(), 'ngene-plot-enable-')));
  const profile = path.join(root, 'profile');
  const input = path.join(root, 'tone.cs8');
  const iq = Buffer.alloc(400000);
  for (let n = 0; n < iq.length / 2; n++) {
    iq.writeInt8(Math.round(32 * Math.cos(2 * Math.PI * n / 10)), 2 * n);
    iq.writeInt8(Math.round(32 * Math.sin(2 * Math.PI * n / 10)), 2 * n + 1);
  }
  fs.writeFileSync(input, iq, {flag:'wx'});
  const base = `http://127.0.0.1:${port}`;
  const child = spawn(executable, ['--web', '--port', port, '--output-dir', profile], {stdio:['ignore', 'pipe', 'inherit']});
  const exit = new Promise(resolve => child.once('exit', resolve));
  child.stdout.resume();
  let browser;
  try {
    const bootstrap = await until(async () => {
      assert.equal(child.exitCode, null, 'Receiver exited before service startup');
      try { return await (await fetch(`${base}/api/v1/bootstrap`)).json(); }
      catch { return false; }
    }, 'receiver service startup');
    const get = async route => {
      const response = await fetch(`${base}/api/v1/${route}`);
      assert.equal(response.status, 200, `GET ${route}`);
      return response.json();
    };
    const post = async (route, body) => {
      const response = await fetch(`${base}/api/v1/${route}`, {
        method:'POST', headers:{'Content-Type':'application/json', 'X-NGene-Token':bootstrap.token}, body:JSON.stringify(body)
      });
      const result = await response.json();
      assert(response.ok, `POST ${route}: ${response.status} ${JSON.stringify(result)}`);
      return result;
    };
    const initial = await get('config');
    const config = await post('config', {revision:initial.revision, ini:`
[receiver]
mode=capture
run_seconds=0
idle_timeout_ms=3000
[frontend]
type=file
sample_rate_hz=100000
center_frequency_hz=1575420000
signal_frequency_hz=1575420000
[replay]
format=CI8
path=${input}
realtime=true
loop=true
[conditioning]
module=bypass
[buffer]
window_samples=1000
ingress_blocks=128
ring_blocks=16
overflow_policy=stop
[recording]
enabled=false
[analysis]
spectrum_enabled=false
histogram_enabled=true
fft_size=256
averages=1
update_hz=30
histogram_pairs=1024
histogram_bins=32
histogram_update_hz=30
`});
    browser = await chromium.launch({headless:true, ...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE} : {}), args:['--no-sandbox']});
    const page = await browser.newPage({viewport:{width:1512, height:1100}});
    const errors = [], frames = [], analysisRequests = [];
    let expectedRequestFailure = false;
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => {
      if (message.type() === 'error' && !(expectedRequestFailure && message.text().includes('status of 409'))) errors.push(message.text());
    });
    page.on('request', request => {
      if (request.method() === 'POST' && request.url() === `${base}/api/v1/analysis/config`) analysisRequests.push(request.postDataJSON());
    });
    page.on('websocket', socket => {
      let mask;
      socket.on('framesent', event => {
        const match = String(event.payload).match(/^status:([0-3])$/);
        if (match) mask = Number(match[1]);
      });
      socket.on('framereceived', event => {
        frames.push({mask, time:Date.now(), status:JSON.parse(String(event.payload))});
      });
    });
    const document = await page.goto(base);
    assert(document.headers()['content-security-policy'], 'Production CSP must remain enabled');
    await until(() => page.locator('#connection').textContent().then(text => text === 'Connected'), 'GUI connection');
    await page.locator('#refresh').selectOption('33');
    const nextFrame = async (mask, after = frames.length, predicate = () => true, timeout = 5000) => until(
      () => frames.slice(after).find(frame => frame.mask === mask && predicate(frame.status)),
      `GUI status:${mask} telemetry`, timeout
    );
    const visibility = async (spectrum, waterfall) => {
      for (const [id, enabled] of [['spectrum', spectrum], ['waterfall', waterfall]]) {
        assert.equal(await page.locator(`#${id}Body`).isVisible(), enabled, `${id} body visibility`);
        assert.equal(await page.locator(`#${id}`).isVisible(), enabled, `${id} stale canvas visibility`);
        assert.equal(await page.locator(`#${id}Disabled`).isVisible(), !enabled, `${id} disabled message visibility`);
        assert(await page.locator(`#${id}Title`).isVisible(), `${id} heading must remain available`);
        assert(await page.locator(`#show${id[0].toUpperCase()}${id.slice(1)}`).isVisible(), `${id} enable switch must remain available`);
      }
    };
    const snapshot = () => page.evaluate(async () => (await import('/app.js')).diagnosticSnapshot());
    const advancing = async (before, mask = 3) => (await nextFrame(mask, frames.length, status => sequence(status) > (status.analysis.revision===before.analysis.revision?sequence(before):0n) && histogramSequence(status)>0n)).status;
    const receiving = (before, after, message) => {
      assert.equal(after.state, 'running', message);
      assert.equal(after.run_id, before.run_id, `${message}: receiver restarted`);
      assert(BigInt(after.processed_samples) > BigInt(before.processed_samples), `${message}: receiver stopped advancing`);
      assert(histogramSequence(after) > (after.analysis.revision===before.analysis.revision?histogramSequence(before):0n), `${message}: independent histogram stopped advancing`);
    };
    const dormantPsd = async message => {
      // A running FFT may finish. Settle for 100 ms, then observe several new
      // receiver windows, all well inside the two-second lost-client lease.
      await delay(100);
      const first = await get('status');
      for (let n = 0; n < 6; n++) {
        await delay(45);
        const after = await get('status');
        assert.equal(sequence(after), sequence(first), `${message}: PSD sequence advanced`);
        if (n === 5) receiving(first, after, message);
      }
      return first;
    };
    const screenshot = async name => {
      if (process.env.NGENE_SCREENSHOTS) {
        fs.mkdirSync(process.env.NGENE_SCREENSHOTS, {recursive:true});
        await page.screenshot({path:path.join(process.env.NGENE_SCREENSHOTS, name), fullPage:true});
      }
    };
    const masterEnabled = async enabled => {
      const result = await until(async () => {
        const value = await get('analysis/config');
        return value.analysis.spectrum_enabled === enabled && value;
      }, `runtime spectrum_enabled=${enabled}`);
      await until(async () => await page.locator('#showSpectrum').isEnabled() && await page.locator('#showWaterfall').isEnabled(), 'plot enable request completed');
      return result;
    };
    const toggle = async (id, enabled) => {
      const before = await get('analysis/config'), requestCount = analysisRequests.length;
      await page.locator(`#${id}`).setChecked(enabled);
      await until(async () => (await page.locator(`#${id}`).isChecked()) === enabled && await page.locator(`#${id}`).isEnabled(), `${id} settled ${enabled}`);
      const any = await page.locator('#showSpectrum').isChecked() || await page.locator('#showWaterfall').isChecked();
      const result = await masterEnabled(any);
      unchangedAnalysis(before, result, `${id} must preserve unrelated backend analysis settings`);
      for (const request of analysisRequests.slice(requestCount)) {
        unchangedAnalysis(before,{analysis:Object.fromEntries(Object.entries(request).filter(([key])=>key!=='revision'))},'Header controls must retain the applied server analysis settings');
        assert.equal(request.spectrum_enabled, any, 'Header runtime master follows either enabled plot');
      }
      return result;
    };
    const unchangedAnalysis = (before, after, message) => {
      const withoutEnabled = analysis => Object.fromEntries(Object.entries(analysis).filter(([key]) => key !== 'spectrum_enabled'));
      assert.deepEqual(withoutEnabled(after.analysis), withoutEnabled(before.analysis), message);
    };

    const logo = await fetch(`${base}/ngene-logo.svg`);
    assert.equal(logo.status, 200, 'Embedded logo route');
    assert.match(logo.headers.get('content-type'), /^image\/svg\+xml(?:;|$)/i, 'Logo MIME type');
    assert.match(await logo.text(), /<svg\b/, 'Logo route must return the SVG asset');
    assert(await page.locator('.brandmark').isVisible(), 'NGene logo must be visible in the top-left brand');
    await page.locator('.brandmark').evaluate(image => image.decode());
    const brand = await page.locator('.brandmark').evaluate(image => ({src:image.src, width:image.naturalWidth}));
    assert(brand.width > 0, 'NGene logo must decode under production CSP');
    assert.equal(await page.locator('link[rel="icon"]').evaluate(link => link.href), brand.src, 'Favicon must use the brand SVG');

    // Start from the user's actual failure case: computation disabled in the
    // saved profile. Header Enable must turn it on, without a Diagnostics visit.
    await visibility(false, false);
    assert.equal(await page.locator('#showSpectrum').isChecked(), false, 'Initially disabled computation must show an off spectrum switch');
    assert.equal(await page.locator('#showWaterfall').isChecked(), false, 'Initially disabled computation must show an off waterfall switch');
    await post('receiver/start', {revision:config.revision});
    await nextFrame(2, 0, status => status.state === 'running' && histogramSequence(status) > 0n);
    await dormantPsd('Initially disabled runtime spectrum');
    const originalAnalysis = await get('analysis/config');
    const spectrumStart = await toggle('showSpectrum', true);
    unchangedAnalysis(originalAnalysis, spectrumStart, 'First spectrum Enable must preserve all unrelated runtime parameters');
    await visibility(true, false);
    await nextFrame(3, 0, status => status.state === 'running' && sequence(status) > 0n);
    await toggle('showSpectrum', false);
    await visibility(false, false);
    const initiallyDormant = await dormantPsd('Last spectrum switch off changes runtime master');
    await toggle('showWaterfall', true);
    await visibility(false, true);
    await advancing(initiallyDormant);
    await toggle('showSpectrum', true);
    const initialFrame = await nextFrame(3, 0, status => status.state === 'running' && sequence(status) > 0n && histogramSequence(status) > 0n);
    await until(async () => (await snapshot()).history.length >= 2, 'live waterfall history');
    await visibility(true, true);
    for (const id of ['showSpectrum', 'showWaterfall']) {
      assert(await page.locator(`#${id}`).evaluate(input => Boolean(input.closest('.plot-heading'))), `${id} belongs in its plot heading`);
    }
    await screenshot('plot-enable-desktop.png');

    // Waterfall alone still needs the same PSD; hiding the old spectrum must
    // collapse its entire body immediately without obscuring its enable switch.
    const beforeWaterfallOnly = await get('status');
    const requestsBeforeWaterfallOnly = analysisRequests.length;
    await toggle('showSpectrum', false);
    assert.equal(analysisRequests.length, requestsBeforeWaterfallOnly, 'Hiding one plot while another is enabled must not change the runtime master');
    await visibility(false, true);
    const waterfallOnly = await advancing(beforeWaterfallOnly);
    assert(waterfallOnly.analysis.dbfs.length > 0);
    await page.setViewportSize({width:390, height:844});
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), 'Mobile plot controls overflow the viewport');
    await visibility(false, true);
    await screenshot('plot-enable-waterfall-only-mobile.png');
    await page.setViewportSize({width:1512, height:1100});

    await toggle('showSpectrum', true);
    await toggle('showWaterfall', false);
    await visibility(true, false);
    const spectrumOnly = await advancing(waterfallOnly);
    const historyBeforeDisabled = (await snapshot()).history;
    await advancing(spectrumOnly);
    assert.deepEqual((await snapshot()).history, historyBeforeDisabled, 'Disabled waterfall must not accumulate hidden history');

    // Even at one-second GUI refresh, explicit disable must immediately publish
    // histogram-only interest rather than waiting for the PSD lease to expire.
    await page.locator('#refresh').selectOption('1000');
    const beforeDisable = frames.length, disabledAt = Date.now();
    await toggle('showSpectrum', false);
    await visibility(false, false);
    const disabled = await nextFrame(2, beforeDisable, () => true, 1500);
    assert(disabled.time - disabledAt < 1500, 'PSD unsubscribe was delayed toward the fallback lease');
    assert.equal(disabled.status.analysis.dbfs.length, 0, 'Disabled plots must not request a PSD payload');
    await masterEnabled(false);
    await nextFrame(2,frames.length,status=>status.analysis.config.spectrum_enabled===false);
    assert(disabled.status.analysis.histogram_i.length > 0, 'Histogram interest must remain independent');
    await dormantPsd('Both GUI plots disabled');
    await screenshot('plot-enable-both-disabled.png');

    // Either plot must independently restart production after both were off.
    await page.locator('#refresh').selectOption('33');
    const dormant = await get('status');
    await toggle('showSpectrum', true);
    await visibility(true, false);
    const spectrumResumed = await advancing(dormant);
    receiving(initialFrame.status, spectrumResumed, 'Spectrum enabled again');
    await toggle('showSpectrum', false);
    await nextFrame(2);
    const dormantAgain = await dormantPsd('Spectrum disabled again');
    await toggle('showWaterfall', true);
    await visibility(false, true);
    await advancing(dormantAgain);

    // Runtime settings also arrive via telemetry. Switches must visibly go off
    // when computation is externally disabled, then restore active preferences.
    await toggle('showSpectrum', true);
    const analysis = await get('analysis/config');
    await post('analysis/config', {revision:analysis.revision, ...analysis.analysis, spectrum_enabled:false});
    await nextFrame(2, frames.length, status => status.analysis.config.spectrum_enabled === false);
    await until(() => page.locator('#spectrumBody').isVisible().then(visible => !visible), 'runtime spectrum disable collapses plots');
    await visibility(false, false);
    assert.equal(await page.locator('#showSpectrum').isChecked(), false, 'Runtime disable must show the spectrum switch off');
    assert.equal(await page.locator('#showWaterfall').isChecked(), false, 'Runtime disable must show the waterfall switch off');
    await dormantPsd('Runtime spectrum computation disabled');
    const disabledAnalysis = await get('analysis/config');
    await post('analysis/config', {revision:disabledAnalysis.revision, ...disabledAnalysis.analysis, spectrum_enabled:true});
    await until(() => page.locator('#spectrumBody').isVisible(), 'runtime spectrum enable restores checked plots');
    await visibility(true, true);
    await advancing(dormantAgain);

    // Apply the same runtime setting using the real Diagnostics form and keep
    // one preference unchecked while backend computation is restored.
    // Refresh the form revision after the external API changes above.
    await page.reload();
    await until(() => page.locator('#connection').textContent().then(text => text === 'Connected'), 'GUI reconnect after external settings');
    await toggle('showWaterfall', false);
    await page.locator('#showConfiguration').click();
    await page.locator('[data-settings="analysis"]').click();
    await page.locator('#analysis_spectrum_enabled').selectOption('false');
    await page.locator('#applyAnalysis').click();
    await until(() => page.locator('#analysisNotice').textContent().then(text => text.startsWith('Applied')), 'Diagnostics form applied disable');
    await page.locator('#showObservation').click();
    await visibility(false, false);
    await page.locator('#showConfiguration').click();
    await page.locator('#analysis_spectrum_enabled').selectOption('true');
    await page.locator('#applyAnalysis').click();
    await until(() => page.locator('#analysisNotice').textContent().then(text => text.startsWith('Applied')), 'Diagnostics form applied enable');
    await page.locator('#showObservation').click();
    await visibility(true, false);

    // Pausing presentation must not turn header Enable back into a cosmetic
    // preference: first-on/last-off still update the runtime configuration.
    await page.locator('#pausePlot').click();
    await nextFrame(0);
    await toggle('showSpectrum', false);
    await visibility(false, false);
    await toggle('showWaterfall', true);
    await visibility(false, true);
    await nextFrame(0, frames.length, status => status.analysis.config.spectrum_enabled === true);
    assert.equal(await page.locator('#pausePlot').textContent(), 'Resume plots', 'Runtime header changes must preserve paused presentation');
    await delay(100);
    const pausedBefore = await get('status');
    await delay(240);
    const pausedAfter = await get('status');
    assert.equal(sequence(pausedAfter), sequence(pausedBefore), 'Paused plots must not create PSD interest merely because runtime computation was enabled');
    assert(BigInt(pausedAfter.processed_samples) > BigInt(pausedBefore.processed_samples), 'Paused plot control changes must preserve reception');
    await page.locator('#pausePlot').click();
    await advancing(pausedAfter);

    // The server can reject a runtime update. Pending switches must not accept
    // competing changes, and failed requests must restore the effective state.
    const rejectedToggle = async (id, enabled, spectrum, waterfall) => {
      const prior = await get('analysis/config');
      const message = `Injected rejection of ${id}=${enabled}`;
      let release, intercepted;
      const gate = new Promise(resolve => { release = resolve; });
      const arrived = new Promise(resolve => { intercepted = resolve; });
      const handler = async route => {
        if (route.request().method() !== 'POST') return route.continue();
        intercepted();
        await gate;
        await route.fulfill({status:409, contentType:'application/json', body:JSON.stringify({error:message})});
      };
      expectedRequestFailure = true;
      await page.route('**/api/v1/analysis/config', handler);
      try {
        const changing = page.locator(`#${id}`).setChecked(enabled);
        await arrived;
        try {
          assert(await page.locator('#showSpectrum').isDisabled(), 'Pending runtime update must disable the spectrum switch');
          assert(await page.locator('#showWaterfall').isDisabled(), 'Pending runtime update must disable the waterfall switch');
        } finally { release(); }
        await changing;
        await until(() => page.locator('#plotNotice').textContent().then(text => text.includes(message)), 'visible runtime toggle error');
        assert(await page.locator('#plotNotice').isVisible(), 'Runtime errors must remain visible in Observation');
        assert.equal(await page.locator('#plotNotice').getAttribute('role'), 'alert', 'Runtime toggle errors must be accessible alerts');
        await until(async () => await page.locator('#showSpectrum').isEnabled() && await page.locator('#showWaterfall').isEnabled(), 'failed runtime update settled');
        assert.equal(await page.locator('#showSpectrum').isChecked(), spectrum, 'Rejected update must restore the spectrum switch');
        assert.equal(await page.locator('#showWaterfall').isChecked(), waterfall, 'Rejected update must restore the waterfall switch');
        await visibility(spectrum, waterfall);
        assert.deepEqual(await get('analysis/config'), prior, 'Rejected header request must not change runtime configuration');
      } finally {
        release();
        await page.unroute('**/api/v1/analysis/config', handler);
        expectedRequestFailure = false;
      }
    };
    await rejectedToggle('showWaterfall', false, false, true);
    await toggle('showWaterfall', false);
    await rejectedToggle('showSpectrum', true, false, false);
    await toggle('showSpectrum', true);
    await until(() => page.locator('#plotNotice').isVisible().then(visible => !visible), 'successful runtime update clears its prior error');

    // Header updates fetch current applied settings and must not overwrite
    // unrelated unsaved receiver fields or unsubmitted Diagnostics edits.
    await post('receiver/stop', {revision:(await get('analysis/config')).revision});
    await until(() => page.locator('#runState').textContent().then(text => text === 'stopped'), 'receiver stopped for unsaved profile edits');
    await page.locator('#showConfiguration').click();
    await page.locator('[data-settings="recording"]').click();
    await page.locator('[name="recording.prefix"]').fill('unsaved_plot_draft');
    await page.locator('[data-settings="analysis"]').click();
    await page.locator('#analysis_fft_size').selectOption('1024');
    await page.locator('#analysis_window').selectOption('blackman');
    const appliedBeforeDraftToggles = await get('analysis/config');
    const profileBeforeDraftToggles = await get('config');
    await page.locator('#showObservation').click();
    await toggle('showSpectrum', false);
    await toggle('showWaterfall', true);
    const appliedAfterDraftToggles = await get('analysis/config');
    unchangedAnalysis(appliedBeforeDraftToggles, appliedAfterDraftToggles, 'Header updates must not apply dirty Diagnostics parameters');
    assert.equal(appliedAfterDraftToggles.analysis.fft_size, 256, 'Unsubmitted FFT-size edit must remain a draft');
    assert.equal((await get('config')).ini.includes('prefix=unsaved_plot_draft'), false, 'Header updates must not save an unsaved recording prefix');
    assert.equal(profileBeforeDraftToggles.ini.includes('prefix=unsaved_plot_draft'), false);
    await page.locator('#showConfiguration').click();
    assert.equal(await page.locator('#analysis_fft_size').inputValue(), '1024', 'Header updates must preserve the dirty FFT-size input');
    assert.equal(await page.locator('#analysis_window').inputValue(), 'blackman', 'Header updates must preserve the dirty window input');
    assert.equal(await page.locator('#analysis_spectrum_enabled').inputValue(), 'true', 'Diagnostics master input must reflect the header update');
    await page.locator('[data-settings="recording"]').click();
    assert.equal(await page.locator('[name="recording.prefix"]').inputValue(), 'unsaved_plot_draft', 'Header updates must preserve the unsaved receiver profile');
    assert(await page.locator('#apply').isEnabled(), 'The preserved receiver profile must remain editable');
    await page.locator('#showObservation').click();
    assert.equal((await snapshot()).rejected, 0, 'Live replay PSD integrity must remain valid');
    assert.deepEqual(errors, [], 'No browser errors or CSP violations');

    await post('shutdown', {});
    assert.equal(await exit, 0);
    console.log('PASS: real CI8 replay; header first-on/last-off change runtime spectrum_enabled and preserve unrelated settings; disabled startup resumes from either switch; independent plot collapse/PSD interest; API and Diagnostics synchronization; paused runtime toggles; pending controls and rejected-request rollback with accessible errors; dirty profile/Diagnostics preservation; embedded SVG/CSP and desktop/mobile layout.');
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill('SIGTERM'); await exit; }
    const temporary = fs.realpathSync.native(os.tmpdir());
    const relative = path.relative(temporary, root);
    assert(relative && !relative.startsWith('..') && !path.isAbsolute(relative), 'Cleanup must stay inside the scoped temporary directory');
    fs.rmSync(root, {recursive:true, force:true});
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
