/* Real receiver workspace regression. Node >=18 and Playwright are test-host dependencies. */
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
const views = {
  journey:'journeyView', setup:'configurationView', rf:'observationView',
  tracking:'trackingView', navigation:'navigationView',
  pvt:'pvtView', record:'recordingView', health:'healthView'
};

async function until(predicate, message, timeout = 6000) {
  const deadline = Date.now() + timeout;
  while (Date.now() < deadline) {
    const value = await predicate();
    if (value) return value;
    await delay(25);
  }
  throw new Error(`Timed out: ${message}`);
}

(async () => {
  const executable = path.resolve(process.env.NGENE_EXECUTABLE || 'build/ngene');
  assert(fs.existsSync(executable), `Receiver executable not found: ${executable}`);
  // Reserve a free local port, without ever attaching to somebody else's receiver.
  const probe = net.createServer();
  await new Promise((resolve, reject) => {
    probe.once('error', reject);
    probe.listen(Number(process.env.NGENE_TEST_PORT || 0), '127.0.0.1', resolve);
  });
  const port = String(probe.address().port);
  await new Promise(resolve => probe.close(resolve));
  const root = fs.realpathSync.native(fs.mkdtempSync(path.join(os.tmpdir(), 'ngene-workspace-')));
  const profile = path.join(root, 'profile');
  const captures = path.join(root, 'captures');
  const input = path.join(root, 'workspace-tone.cs8');
  const iq = Buffer.alloc(9600000);
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
      assert.equal(child.exitCode, null, 'Receiver exited before startup');
      try { return await (await fetch(`${base}/api/v1/bootstrap`)).json(); }
      catch { return false; }
    }, 'private receiver service');
    const get = async route => {
      const response = await fetch(`${base}/api/v1/${route}`);
      assert.equal(response.status, 200, `GET ${route}`);
      return response.json();
    };
    const post = async (route, body) => {
      const response = await fetch(`${base}/api/v1/${route}`, {
        method:'POST', headers:{'Content-Type':'application/json', 'X-NGene-Token':bootstrap.token},
        body:JSON.stringify(body)
      });
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
sample_rate_hz=2400000
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
directory=${captures}
prefix=workspace_test
queue_blocks=128
[analysis]
spectrum_enabled=true
histogram_enabled=true
fft_size=256
averages=1
update_hz=20
histogram_pairs=1024
histogram_bins=32
histogram_update_hz=20
`});

    browser = await chromium.launch({headless:true,
      ...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE} : {}), args:['--no-sandbox']});
    const page = await browser.newPage({viewport:{width:1512, height:1100}});
    const errors = [], failedAssets = [], frames = [], writes = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
    page.on('response', response => {
      if (response.url().startsWith(base) && response.status() >= 400) failedAssets.push(`${response.status()} ${response.url()}`);
    });
    page.on('request', request => {
      if (request.method() === 'POST' && request.url().startsWith(`${base}/api/v1/`)) {
        writes.push({route:request.url().slice(`${base}/api/v1/`.length), body:request.postDataJSON()});
      }
    });
    page.on('websocket', socket => {
      let mask;
      socket.on('framesent', event => {
        const match = String(event.payload).match(/^status:([0-3])$/);
        if (match) mask = Number(match[1]);
      });
      socket.on('framereceived', event => frames.push({mask, status:JSON.parse(String(event.payload))}));
    });
    const document = await page.goto(base);
    assert(document.headers()['content-security-policy'], 'Keep the actual production CSP enabled');
    assert(!document.headers()['content-security-policy'].includes("'unsafe-eval'"), 'Workspace routing must not relax the production CSP');
    const workspaceModule = await fetch(`${base}/workspace.js`);
    assert.equal(workspaceModule.status, 200, 'Workspace module is embedded in the receiver executable');
    assert.match(workspaceModule.headers.get('content-type'), /^(?:application|text)\/javascript(?:;|$)/i, 'Workspace module has a JavaScript MIME type');
    assert.match(await workspaceModule.text(), /export\s+function\s+createReceiverWorkspace\b/, 'Workspace module route must return JavaScript, not the HTML fallback');
    const configurationModule = await fetch(`${base}/receiver-config.js`);
    assert.equal(configurationModule.status, 200, 'Receiver configuration module is embedded in the executable');
    assert.match(configurationModule.headers.get('content-type'), /^(?:application|text)\/javascript(?:;|$)/i);
    assert.match(await configurationModule.text(), /export\s+function\s+createReceiverConfiguration\b/);
    const connected = () => until(() => page.locator('#connection').textContent().then(value => value === 'Connected'), 'GUI connected');
    await connected();
    const snapshot = () => page.evaluate(async () => (await import('/app.js')).diagnosticSnapshot());
    const shown = async key => {
      await until(() => page.locator(`#${views[key]}`).isVisible(), `${key} workspace visible`);
      const hash = new URL(page.url()).hash;
      assert(hash === `#${key}` || (key === 'rf' && hash === ''), `${key} deep link (${hash})`);
      for (const [other, id] of Object.entries(views)) {
        assert.equal(await page.locator(`#${id}`).isVisible(), other === key, `Only ${key} workspace should be displayed (${other})`);
        assert.equal(await page.locator(`#${id}`).getAttribute('data-workspace-view'), other, `${other} workspace identity`);
      }
      const nav = page.locator(`[data-view="${key}"]`).first();
      assert(await nav.isVisible(), `${key} navigation remains available`);
      assert.equal(await nav.getAttribute('aria-current'), 'page', `${key} navigation has accessible current state`);
    };
    const select = async key => { await page.locator(`[data-view="${key}"]`).first().click(); await shown(key); };
    const screenshot = async name => {
      if (!process.env.NGENE_WORKSPACE_SCREENSHOTS) return;
      fs.mkdirSync(process.env.NGENE_WORKSPACE_SCREENSHOTS, {recursive:true});
      await page.screenshot({path:path.join(process.env.NGENE_WORKSPACE_SCREENSHOTS, name), fullPage:true});
    };
    const configurationTopics = ['source', 'receiver', 'acquisition', 'tracking', 'dsp', 'navigation', 'recording', 'analysis', 'display', 'profile'];
    const referenceTopics = ['receiver', 'acquisition', 'tracking', 'navigation'];
    const referenceOnlyTopics = ['navigation'];
    const referencesDisabled = async () => {
      assert(await page.locator('[data-reference-param]').count() > 30, 'Original receiver configuration exposes meaningful GNSS parameters');
      assert.deepEqual(await page.locator('[data-reference-param]').evaluateAll(controls => controls
        .filter(control => !control.matches(':disabled') || control.name || control.closest('form'))
        .map(control => control.dataset.referenceParam)), [], 'Unintegrated GNSS parameters must remain disabled, unnamed and outside operational forms');
    };
    const configurationShown = async topic => {
      await shown('setup');
      assert.equal(await page.locator('#workspaceCrumb').textContent(), 'Receiver configuration');
      assert.equal(await page.locator(`[data-settings="${topic}"]`).getAttribute('aria-pressed'), 'true');
      for (const reference of referenceTopics) {
        assert.equal(await page.locator(`[data-receiver-settings="${reference}"]`).isVisible(), reference === topic,
          `Only the selected GNSS configuration section is displayed (${topic}/${reference})`);
      }
      assert.equal(await page.locator('#configForm').isVisible(), ['source', 'receiver', 'acquisition', 'tracking', 'dsp', 'recording'].includes(topic),
        'Only topics with real receiver parameters expose the operational form');
      await referencesDisabled();
    };

    await shown('rf');
    assert.equal(await page.locator('[data-workspace-view]').count(), 8, 'All receiver workspaces are present');
    assert.deepEqual(await page.evaluate(() => {
      const seen = new Set();
      return [...document.querySelectorAll('[id]')].map(node => node.id).filter(id => seen.has(id) || !seen.add(id));
    }), [], 'Moving existing controls must not duplicate DOM IDs');
    const stoppedConfig = await get('config');
    const beforeBrowsing = writes.length;
    for (const key of Object.keys(views)) await select(key);
    await select('tracking');
    assert.deepEqual(await page.locator('.channel-list thead th').allTextContents(),
      ['Channel','Signal','PRN','Acquisition','Tracking','Loop','C/N0 (dB-Hz)','Doppler (Hz)','Allocation','Decoding','TOW (s)','PVT']);
    assert((await page.locator('.channel-list tbody tr').count())>0);
    assert.equal(await page.locator('.channel-status-grid').count(),0);
    await page.screenshot({path:path.resolve('build/channel-list.png'),fullPage:true});
    assert.equal(writes.length, beforeBrowsing, 'Viewing a workspace must not issue mutation requests');
    assert.deepEqual(await get('config'), stoppedConfig, 'Workspace navigation must preserve the applied receiver profile');
    assert.equal((await get('status')).state, 'stopped', 'Selecting module workspaces must not start the receiver');

    // Receiver configuration groups the full chain while keeping unavailable modules read-only.
    await select('setup');
    assert.equal(await page.locator('#showConfiguration .workspace-nav-name').textContent(), 'Receiver configuration');
    assert.deepEqual(await page.locator('[data-settings]').evaluateAll(buttons => buttons.map(button => button.dataset.settings)), configurationTopics);
    const beforeConfiguration = writes.length;
    for (const topic of configurationTopics) {
      await page.locator(`[data-settings="${topic}"]`).click();
      await configurationShown(topic);
      assert.equal(await page.locator('#apply').isDisabled(), referenceOnlyTopics.includes(topic),
        `${topic}: Apply must reflect whether this topic has implemented settings`);
      await screenshot(`receiver-configuration-${topic}.png`);
    }
    for (const [name, expected] of Object.entries({
      'Number of supported Galileo channels':'8', 'Number of supported EGNOS channels':'1',
      'Galileo Doppler Frequency step (Hz)':'87',
      'GPS DLLbandwidth':'2', 'GPS PLLbandwidth':'15', 'Galileo DLLbandwidth':'2', 'Galileo PLLbandwidth':'15',
      'GPS spacing':'0.8', 'Galileo spacing':'0.4', 'PVT rate':'1'
    })) assert.equal(await page.locator(`[data-reference-param="${name}"]`).inputValue(), expected,
      `${name} must use the original NGene shipped example`);
    assert.equal(writes.length, beforeConfiguration, 'Browsing GNSS reference settings must not issue mutations');
    assert.deepEqual(await get('config'), stoppedConfig, 'Reference values are not silently applied to the receiver');
    for (const [view, topic] of [['tracking', 'receiver'], ['navigation', 'navigation'], ['pvt', 'navigation']]) {
      await select(view);
      await page.locator(`#${views[view]} [data-open-settings="${topic}"]`).first().click();
      await configurationShown(topic);
    }
    assert.equal(writes.length, beforeConfiguration, 'Module configuration shortcuts only navigate');

    // Readiness must come from actual module availability, never illustrative PRNs/fixes.
    for (const key of ['tracking', 'navigation', 'pvt']) {
      await select(key);
      const text = await page.locator(`#${views[key]}`).innerText();
      if (key === 'tracking') assert.match(text, /channels|channel/i);
      else if(key==='pvt')assert.match(text,/No solution available/i);
      else assert.match(text,/No navigation data available/i);
      assert.doesNotMatch(text, /(?:GPS|GAL|SBAS)\s*(?:PRN\s*)?\d{2}\b|\b(?:latitude|longitude)\s*[:=]\s*-?\d/i,
        `${key} must not present invented tracked satellites or a synthetic fix`);
      const disabled = page.locator(`#${views[key]} button:disabled`);
      const count = await disabled.count();
      const before = writes.length;
      for (let n = 0; n < count; n++) await disabled.nth(n).evaluate(button => button.click());
      assert.equal(writes.length, before, `${key} unavailable commands must not call invented APIs`);
      if (key === 'tracking' || key === 'pvt') await screenshot(`workspace-${key}.png`);
    }

    await select('journey');
    const beforeTour = writes.length;
    await page.locator('[data-tour="start"]').click();
    await shown('setup');
    assert.match(await page.locator('#workspaceTour').innerText(), /Receiver configuration/, 'Guided stage uses the complete receiver configuration name');
    assert(await page.locator('#workspaceTour').isVisible(), 'The guided workflow explains its current stage');
    assert(await page.locator('#workspaceTour [data-tour="previous"]').isDisabled(), 'The first guided stage has no predecessor');
    for (const key of ['rf', 'tracking', 'navigation', 'pvt', 'record']) {
      await page.locator('#workspaceTour [data-tour="next"]').click();
      await shown(key);
    }
    await page.locator('#workspaceTour [data-tour="next"]').click();
    assert.equal(await page.locator('#workspaceTour').isVisible(), false, 'Finishing the guide closes it');
    assert.equal(writes.length, beforeTour, 'The guided tour must not start signal processing or save settings');
    assert.equal((await get('status')).state, 'stopped', 'The guided tour leaves receiver operation under user control');
    await select('journey');
    await select('tracking');
    await page.goBack(); await shown('journey');
    await page.goForward(); await shown('tracking');
    await page.goto(`${base}/#navigation`); await connected(); await shown('navigation');
    await page.reload(); await connected(); await shown('navigation');
    // Keyboard activation exercises the actual sidebar, without relaxing the page CSP.
    const journeyButton = page.locator('[data-view="journey"]').first();
    await journeyButton.focus(); await page.keyboard.press('Enter'); await shown('journey');

    // The original configuration validation remains usable in its new workspace.
    await select('setup');
    await page.locator('[data-settings="receiver"]').click();
    const beidou = page.locator('[name="beidou_acquisition.enabled"]');
    assert(await beidou.isEnabled(), 'BeiDou B1C data can be selected in the receiver profile');
    assert.equal(await beidou.inputValue(), 'false');
    await beidou.selectOption('true');
    assert.equal(await beidou.inputValue(), 'true');
    const ring = page.locator('[name="buffer.ring_blocks"]');
    assert(await ring.isEnabled(), 'Implemented receiver buffers remain editable alongside unavailable GNSS controls');
    await ring.fill('32'); await ring.blur();
    await page.locator('[data-settings="tracking"]').click();
    const ionosphere = page.locator('[name="tracking.broadcast_ionosphere"]');
    assert.equal(await ionosphere.inputValue(), 'true');
    await ionosphere.selectOption('false');
    const egnos = page.locator('[name="tracking.egnos"]');
    assert.equal(await egnos.inputValue(), 'false');
    await egnos.selectOption('true');
    for (const topic of referenceOnlyTopics) {
      const beforeTelemetry = frames.length;
      await page.locator(`[data-settings="${topic}"]`).click();
      await configurationShown(topic);
      await until(() => frames.length > beforeTelemetry, 'fresh stopped telemetry with GNSS configuration open');
      await referencesDisabled();
      assert(await page.locator('#apply').isDisabled(), 'Telemetry must not enable Apply for a reference-only topic, even with another unsaved draft');
    }
    assert.equal(await ring.inputValue(), '32', 'Reference navigation preserves the real receiver draft');
    await page.locator('[data-settings="source"]').click();
    const rate = page.locator('[name="frontend.sample_rate_hz"]');
    await rate.fill('0'); await rate.blur();
    await until(() => rate.getAttribute('aria-invalid').then(value => value === 'true'), 'visible invalid sample-rate feedback');
    const beforeInvalid = writes.length;
    await page.locator('#apply').click();
    assert.equal(writes.length, beforeInvalid, 'Invalid receiver settings must be blocked before any network mutation');
    assert.deepEqual(await get('config'), stoppedConfig, 'Invalid setup input must preserve saved configuration');
    await rate.fill('2.4'); await rate.blur();
    await page.locator('#apply').click();
    await until(() => page.locator('#configBadge').textContent().then(value => /^r\d+$/.test(value)), 'valid profile saved');
    const applied = await get('config');
    assert.match(applied.ini, /^\[beidou_acquisition\]\r?\nenabled=true(?:\r?\n|$)/m,
      'The BeiDou choice is saved in the operational profile');
    assert.match(applied.ini, /egnos=true(?:\r?\n|$)/);
    assert.match(applied.ini, /broadcast_ionosphere=false(?:\r?\n|$)/);
    assert.match(applied.ini, /ring_blocks=32(?:\r?\n|$)/, 'Implemented receiver buffer change reaches the service');
    const referenceNames = await page.locator('[data-reference-param]').evaluateAll(controls => controls.map(control => control.dataset.referenceParam));
    for (const name of referenceNames) assert(!applied.ini.includes(`${name}=`), `Reference parameter ${name} must never enter the INI`);
    assert.match(applied.ini, /^\[acquisition\]/m, 'Integrated acquisition parameters remain in the saved profile');
    assert.doesNotMatch(applied.ini, /^\[(?:navigation|pvt)\]/m, 'Unimplemented modules do not enter the saved operational profile');
    assert.equal(await page.locator('#configForm [aria-invalid="true"]').count(), 0, 'Disabled GNSS references must not introduce hidden form validation errors');

    // Real file replay drives RF, while no GNSS processing result is manufactured.
    await select('rf');
    await page.locator('#refresh').selectOption('33');
    await page.locator('#start').click();
    await until(async () => (await get('status')).state === 'running', 'sample pipeline running');
    await until(async () => (await snapshot()).history.length >= 3, 'real waterfall history');
    const first = await get('status');
    assert.equal(first.beidou_acquisition.enabled, true, 'The receiver enables B1C on start');
    assert(sequence(first) > 0n && histogramSequence(first) > 0n, 'Both real diagnostic producers must advance');
    assert(Math.abs(first.analysis.peak_hz - 240000) <= first.analysis.bin_hz / 2 + 1, 'RF spectrum must reflect the deterministic CI8 input');
    await screenshot('workspace-rf-real-replay.png');

    await select('setup');
    for (const topic of referenceTopics) {
      const beforeTelemetry = frames.length;
      await page.locator(`[data-settings="${topic}"]`).click();
      await configurationShown(topic);
      await until(() => frames.length > beforeTelemetry, 'fresh running telemetry with GNSS configuration open');
      await referencesDisabled();
      assert(await page.locator('#apply').isDisabled(), 'All full-profile saves stay locked during reception');
    }
    assert(await ring.isDisabled(), 'Implemented receiver parameters retain their running-state lock');
    await select('rf');

    const beforeNonRf = frames.length;
    await select('tracking');
    const withdrawn = await until(() => frames.slice(beforeNonRf).find(frame => frame.mask === 0), 'non-RF status:0 subscription');
    assert.equal(withdrawn.status.analysis.dbfs.length, 0, 'Hidden RF must not stream spectrum arrays');
    assert.equal(withdrawn.status.analysis.histogram_i.length, 0, 'Hidden RF must not stream histogram arrays');
    // Settle a bounded in-flight stage; steady hidden views must stop both workers.
    await delay(150);
    const dormant = await get('status');
    for (let n = 0; n < 6; n++) {
      await delay(50);
      const next = await get('status');
      assert.equal(sequence(next), sequence(dormant), 'Hidden RF must not compute new PSDs');
      assert.equal(histogramSequence(next), histogramSequence(dormant), 'Hidden RF must not compute new histograms');
      assert.equal(next.run_id, first.run_id, 'Workspace navigation must not restart the shared stream');
      assert.equal(next.state, 'running');
      if (n === 5) assert(BigInt(next.processed_samples) > BigInt(dormant.processed_samples), 'Sample processing must continue with RF diagnostics asleep');
    }
    const beforeReturn = frames.length;
    await select('rf');
    await until(() => frames.slice(beforeReturn).find(frame => frame.mask === 3 && sequence(frame.status) > sequence(dormant)), 'RF subscription and fresh FFT restored');
    await until(async () => BigInt((await snapshot()).sequence) > sequence(dormant), 'fresh visible waterfall sample');
    assert.equal((await get('status')).run_id, first.run_id);

    // Capture and transfer retain their real receiver-side lifecycle in the new workspace.
    await select('record');
    assert(await page.locator('#files').isVisible(), 'Capture files are reachable in Recording & replay');
    await page.locator('#recordStart').click();
    await until(async () => (await get('status')).recording_state === 'recording', 'recording started');
    await until(async () => BigInt((await get('status')).session_written_samples) >= 5000n, 'real capture samples written');
    await page.locator('#recordStop').click();
    await until(async () => (await get('status')).recording_state === 'off', 'recording queue drained');
    await page.locator('#reloadFiles').click();
    await until(() => page.locator('#files a[download]').count().then(count => count > 0), 'closed capture transfer links');
    const files = await get('files');
    assert(files.files.some(file => file.closed), 'A real closed capture must be enumerated');
    assert.equal((await get('status')).state, 'running', 'Stopping recording must preserve reception');
    await screenshot('workspace-recording-real-replay.png');
    await select('health');
    await screenshot('workspace-system-health.png');

    await page.setViewportSize({width:390, height:844});
    for (const key of ['journey', 'setup', 'rf', 'tracking', 'pvt', 'record', 'health']) {
      await select(key);
      const geometry = await page.evaluate(() => ({width:innerWidth, scrollWidth:document.documentElement.scrollWidth,
        overflow:[...document.querySelectorAll('body *')].map(node => ({node, bounds:node.getBoundingClientRect()}))
          .filter(({bounds}) => bounds.width && bounds.right > innerWidth + 1)
          .map(({node, bounds}) => ({element:node.id || `${node.tagName}.${node.className}`, right:Math.round(bounds.right), width:Math.round(bounds.width)})).slice(0, 20)}));
      if (geometry.scrollWidth > geometry.width + 1) await screenshot(`workspace-${key}-mobile-overflow-real-replay.png`);
      assert(geometry.scrollWidth <= geometry.width + 1, `${key} must fit a 390px viewport: ${JSON.stringify(geometry)}`);
    }
    await select('rf');
    await until(async () => (await snapshot()).waterfall3D.mode === '3d', 'real 3D waterfall projection');
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), '3D RF view must fit the mobile viewport');
    await screenshot('workspace-rf-mobile-real-replay.png');
    await select('setup');
    for (const topic of ['source', ...referenceTopics]) {
      await page.locator(`[data-settings="${topic}"]`).click();
      await configurationShown(topic);
      assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1),
        `${topic} receiver configuration must fit a 390px viewport`);
      if (topic === 'receiver' || topic === 'tracking') await screenshot(`receiver-configuration-${topic}-mobile.png`);
    }
    await page.setViewportSize({width:1512, height:1100});
    await select('journey');
    await screenshot('workspace-journey-real-session.png');

    const allowedWrites = new Set(['config', 'analysis/config', 'receiver/start', 'receiver/stop', 'recording/start', 'recording/stop', 'channels/observe']);
    assert.deepEqual(writes.filter(request => !allowedWrites.has(request.route)), [], 'No invented acquisition/tracking/navigation API endpoints');
    assert.equal((await snapshot()).rejected, 0, 'Real PSD integrity must remain valid');
    assert.deepEqual(errors, [], 'No browser errors or CSP violations');
    assert.deepEqual(failedAssets, [], 'No missing assets or failed application requests');
    await post('receiver/stop', {revision:(await get('config')).revision});
    await until(async () => (await get('status')).state === 'stopped', 'receiver stopped');
    await post('shutdown', {});
    assert.equal(await exit, 0);
    console.log('PASS: eight real receiver workspaces; nine receiver configuration topics and module shortcuts; original NGene reference values isolated from operational profiles and locked across telemetry; preserved front-end validation and receiver buffer save; hash/history/keyboard navigation; honest unavailable GNSS modules; loop replay PSD and 2D/3D waterfall; hidden-view diagnostic worker withdrawal and fresh resumption without stopping reception; real capture/transfer access; mobile layout; production CSP and embedded asset integrity.');
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill('SIGTERM'); await exit; }
    const temporary = fs.realpathSync.native(os.tmpdir());
    const relative = path.relative(temporary, root);
    assert(relative && !relative.startsWith('..') && !path.isAbsolute(relative), 'Cleanup must stay inside the scoped temporary directory');
    fs.rmSync(root, {recursive:true, force:true});
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
