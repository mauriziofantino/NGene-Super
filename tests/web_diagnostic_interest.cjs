/* Real-receiver diagnostic subscription regression. Requires Node >=18 and Playwright. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const counter = (status, key) => BigInt(status.analysis[key]);

(async () => {
  const executable = path.resolve(process.env.NGENE_EXECUTABLE || 'build/ngene');
  // Canonicalize TEMP on Windows: CivetWeb rejects short-name directory aliases.
  const root = fs.realpathSync.native(fs.mkdtempSync(path.join(os.tmpdir(), 'ngene-interest-')));
  const profile = path.join(root, 'profile');
  const input = path.join(root, 'tone.cs8');
  const iq = Buffer.alloc(9600000);
  for (let n = 0; n < iq.length / 2; n++) {
    iq.writeInt8(Math.round(32 * Math.cos(2 * Math.PI * n / 10)), 2 * n);
    iq.writeInt8(Math.round(32 * Math.sin(2 * Math.PI * n / 10)), 2 * n + 1);
  }
  fs.writeFileSync(input, iq, {flag:'wx'});
  const port = process.env.NGENE_TEST_PORT || '8107';
  const base = `http://127.0.0.1:${port}`;
  const child = spawn(executable, ['--web', '--port', port, '--output-dir', profile], {stdio:['ignore', 'pipe', 'inherit']});
  const exit = new Promise(resolve => child.once('exit', resolve));
  child.stdout.resume();
  let browser;
  try {
    let bootstrap;
    for (let attempt = 0; attempt < 100; attempt++) {
      try { bootstrap = await (await fetch(`${base}/api/v1/bootstrap`)).json(); break; }
      catch { if (child.exitCode !== null) break; await delay(50); }
    }
    assert(bootstrap, 'Receiver service did not start');
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
[analysis]
spectrum_enabled=true
histogram_enabled=true
fft_size=256
averages=1
update_hz=30
histogram_pairs=1024
histogram_bins=32
histogram_update_hz=30
`});
    browser = await chromium.launch({headless:true, ...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE} : {}), args:['--no-sandbox']});
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    // Load the real origin and CSP; this test controls its own real WebSocket clients.
    await page.route('**/app.js', route => route.fulfill({contentType:'text/javascript', body:''}));
    const document = await page.goto(base);
    assert(document.headers()['content-security-policy'], 'Production CSP must remain enabled');
    await page.evaluate(() => { window.__diagnosticClients = new Map(); });
    const connect = id => page.evaluate(async ({id, token}) => {
      const socket = new WebSocket(`${location.origin.replace(/^http/, 'ws')}/api/v1/events?token=${encodeURIComponent(token)}`);
      const client = {socket, lastSent:0, pending:null};
      window.__diagnosticClients.set(id, client);
      socket.addEventListener('message', event => {
        const pending = client.pending;
        client.pending = null;
        if (pending) pending.resolve(JSON.parse(event.data));
      });
      socket.addEventListener('close', () => {
        if (client.pending) client.pending.reject(new Error(`WebSocket ${id} closed before replying`));
      });
      await new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(new Error(`WebSocket ${id} did not open`)), 5000);
        socket.addEventListener('open', () => { clearTimeout(timeout); resolve(); }, {once:true});
        socket.addEventListener('error', () => { clearTimeout(timeout); reject(new Error(`WebSocket ${id} failed`)); }, {once:true});
      });
    }, {id, token:bootstrap.token});
    const request = (id, mask) => page.evaluate(async ({id, mask}) => {
      const client = window.__diagnosticClients.get(id);
      // Pace every connection independently, including consecutive helper calls.
      await new Promise(resolve => setTimeout(resolve, Math.max(0, 40 - (performance.now() - client.lastSent))));
      if (client.socket.readyState !== WebSocket.OPEN) throw new Error(`WebSocket ${id} is not open`);
      if (client.pending) throw new Error(`Overlapping requests on WebSocket ${id}`);
      return new Promise((resolve, reject) => {
        const timeout = setTimeout(() => { client.pending = null; reject(new Error(`WebSocket ${id} did not reply`)); }, 5000);
        client.pending = {
          resolve:value => { clearTimeout(timeout); resolve(value); },
          reject:error => { clearTimeout(timeout); reject(error); }
        };
        client.lastSent = performance.now();
        client.socket.send(`status:${mask}`);
      });
    }, {id, mask});
    const close = id => page.evaluate(async id => {
      const client = window.__diagnosticClients.get(id);
      if (client.socket.readyState === WebSocket.CLOSED) return;
      await new Promise(resolve => {
        client.socket.addEventListener('close', resolve, {once:true});
        client.socket.close();
      });
      window.__diagnosticClients.delete(id);
    }, id);
    const until = async (id, mask, predicate, message) => {
      const deadline = Date.now() + 5000;
      while (Date.now() < deadline) {
        const status = await request(id, mask);
        if (predicate(status)) return status;
      }
      throw new Error(`Timed out: ${message}`);
    };
    const observe = async (id, mask, duration = 240) => {
      const result = [];
      const deadline = Date.now() + duration;
      do { result.push(await request(id, mask)); } while (Date.now() < deadline);
      return result;
    };
    const arrays = (status, mask, spectrumEnabled = true) => {
      assert.equal(status.analysis.dbfs.length > 0, Boolean(mask & 1) && spectrumEnabled, `status:${mask} PSD payload`);
      assert.equal(status.analysis.histogram_i.length > 0, Boolean(mask & 2), `status:${mask} I histogram payload`);
      assert.equal(status.analysis.histogram_q.length > 0, Boolean(mask & 2), `status:${mask} Q histogram payload`);
      assert.equal(status.analysis.config.spectrum_enabled, spectrumEnabled, 'Subscription must preserve analysis settings');
      assert.equal(status.analysis.config.histogram_enabled, true);
      for (const key of ['sequence', 'sequence_id', 'histogram_sequence', 'revision']) {
        assert.match(status.analysis[key], /^\d+$/, `${key} metadata must remain present`);
      }
    };
    const stillReceiving = (before, after, message) => {
      assert.equal(after.state, 'running', message);
      assert(BigInt(after.processed_samples) > BigInt(before.processed_samples), `${message}: reception stopped`);
      assert.equal(after.run_id, before.run_id, `${message}: receiver restarted`);
    };
    const stopped = (samples, key, message) => {
      assert(samples.length >= 3, 'Check multiple processed windows after interest changes');
      for (const sample of samples) assert.equal(counter(sample, key), counter(samples[0], key), message);
      stillReceiving(samples[0], samples.at(-1), message);
    };

    await connect('first');
    await post('receiver/start', {revision:config.revision});
    await until('first', 0, status => status.state === 'running' && BigInt(status.processed_samples) > 0n, 'file receiver running');
    const idle = await observe('first', 0);
    idle.forEach(status => arrays(status, 0));
    stopped(idle, 'sequence_id', 'No initial spectrum subscriber must mean no PSD production');
    stopped(idle, 'histogram_sequence', 'No initial histogram subscriber must mean no histogram production');

    await until('first', 3, status => counter(status, 'sequence_id') > 0n && counter(status, 'histogram_sequence') > 0n, 'both diagnostic producers active');
    await request('first', 0);
    // An FFT already executing may finish; sample only after it can settle. This
    // remains far shorter than the two-second fallback lease for lost clients.
    await delay(100);
    const disabled = await observe('first', 0);
    disabled.forEach(status => arrays(status, 0));
    stopped(disabled, 'sequence_id', 'Last spectrum subscriber must stop PSD immediately');
    stopped(disabled, 'histogram_sequence', 'Last histogram subscriber must stop histogram immediately');

    const resumed = await until('first', 3, status => counter(status, 'sequence_id') > counter(disabled.at(-1), 'sequence_id') && counter(status, 'histogram_sequence') > counter(disabled.at(-1), 'histogram_sequence'), 'diagnostics resumed');
    arrays(resumed, 3);
    await request('first', 2);
    await delay(100);
    const histogramOnly = await observe('first', 2);
    histogramOnly.forEach(status => arrays(status, 2));
    stopped(histogramOnly, 'sequence_id', 'Histogram-only subscription must stop PSD without waiting for its lease');
    assert(counter(histogramOnly.at(-1), 'histogram_sequence') > counter(histogramOnly[0], 'histogram_sequence'), 'Histogram must continue while PSD is disabled');
    assert.equal(histogramOnly.at(-1).analysis.histogram_i.reduce((sum, count) => sum + count, 0), 1024);
    await request('first', 0);

    // A second client's independent PSD/both subscription must survive another
    // client's explicit unsubscribe and close. Exercise both supported PSD masks.
    for (const mask of [1, 3]) {
      const passive = mask === 1 ? 'first' : 'passive';
      if (mask === 3) await connect(passive);
      await connect('producer');
      const previous = await request(passive, 0);
      const active = await until('producer', mask, status => counter(status, 'sequence_id') > counter(previous, 'sequence_id') && (mask !== 3 || counter(status, 'histogram_sequence') > counter(previous, 'histogram_sequence')), `second status:${mask} client active`);
      arrays(active, mask);
      for (let i = 0; i < 5; i++) {
        arrays(await request(passive, 0), 0);
        arrays(await request('producer', mask), mask);
      }
      const afterUnrelatedDisable = await request('producer', mask);
      assert(counter(afterUnrelatedDisable, 'sequence_id') > counter(active, 'sequence_id'), `Unrelated status:0 suppressed status:${mask} client's PSD`);
      await close(passive);
      const afterUnrelatedClose = await observe('producer', mask);
      assert(counter(afterUnrelatedClose.at(-1), 'sequence_id') > counter(afterUnrelatedClose[0], 'sequence_id'), `Unrelated close suppressed status:${mask} client's PSD`);
      if (mask === 3) assert(counter(afterUnrelatedClose.at(-1), 'histogram_sequence') > counter(afterUnrelatedClose[0], 'histogram_sequence'), 'Independent histogram subscriber must survive another client closing');
      await connect('observer');
      await request('observer', 0);
      if (mask === 1) await request('producer', 0);
      else await close('producer');
      await delay(100);
      const noProducer = await observe('observer', 0);
      stopped(noProducer, 'sequence_id', `Last status:${mask} client ${mask === 1 ? 'unsubscribe' : 'close'} must stop PSD immediately`);
      stopped(noProducer, 'histogram_sequence', 'No histogram consumer must leave the histogram producer dormant');
      if (mask === 1) await close('producer');
      await close('observer');
    }

    await connect('configured');
    await until('configured', 3, status => status.analysis.dbfs.length > 0, 'PSD subscription restored');
    const analysisConfig = await get('analysis/config');
    const configured = await post('analysis/config', {revision:analysisConfig.revision, ...analysisConfig.analysis, spectrum_enabled:false});
    await until('configured', 3, status => status.revision === configured.revision && !status.analysis.config.spectrum_enabled && counter(status, 'histogram_sequence') > 0n, 'runtime disabled spectrum with active histogram');
    await delay(100);
    const spectrumOff = await observe('configured', 3);
    spectrumOff.forEach(status => arrays(status, 3, false));
    stopped(spectrumOff, 'sequence_id', 'spectrum_enabled=false must suppress PSD even with an interested client');
    assert(counter(spectrumOff.at(-1), 'histogram_sequence') > counter(spectrumOff[0], 'histogram_sequence'), 'Configured spectrum disable must preserve independent histogram production');
    assert.deepEqual(errors, []);
    await close('configured');
    await post('shutdown', {});
    assert.equal(await exit, 0);
    console.log('PASS: real CI8 replay keeps receiving with dormant diagnostics; immediate last-client unsubscribe/close; independent clients and histogram; masked WebSocket arrays retain configuration and sequence metadata; configured spectrum disable suppresses PSD.');
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill('SIGTERM'); await exit; }
    const temporary = fs.realpathSync.native(os.tmpdir());
    const relative = path.relative(temporary, root);
    assert(relative && !relative.startsWith('..') && !path.isAbsolute(relative), 'Cleanup must stay inside the scoped temporary directory');
    fs.rmSync(root, {recursive:true, force:true});
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
