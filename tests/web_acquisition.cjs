/* End-to-end optional GPS search: real controller, ring, worker, API and browser form. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const testTracking = process.env.NGENE_TEST_TRACKING === '1';
const testGalileo = process.env.NGENE_TEST_GALILEO === '1';
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(fn, label) {
  for (let k = 0; k < 300; k++) { const value = await fn(); if (value) return value; await delay(25); }
  throw Error('Timeout: ' + label);
}
(async () => {
  const root = path.resolve('build/web-acquisition-' + Date.now());
  fs.mkdirSync(root, {recursive: true}); fs.mkdirSync(path.join(root, 'profile'));
  const chips = [], iq = Buffer.alloc(2 * 2400000 * (testGalileo ? 4 : 1));
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
  if (testTracking) fs.appendFileSync(configPath, '[tracking]\nenabled=true\ntap_count=9\nspacing_chips=0.25\nsquare_carrier=false\nxor_correlation=true\n');
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
    const route = signal => `acquisition/search-space?prn=7&signal=${signal}`;
    for (const signal of ['gps', 'galileo']) {
      assert.deepEqual(await get(route(signal)), {valid: false});
    }
    const idle = await get('status');
    assert.equal(idle.state, 'stopped');
    assert.equal((await get('files')).revision, idle.revision);
    let config = await get('config'); assert.match(config.ini, /xor_correlation=true/);
    browser = await chromium.launch({headless: true,
      ...(process.env.BROWSER_EXECUTABLE ? {executablePath: process.env.BROWSER_EXECUTABLE} : {})});
    const page = await browser.newPage({viewport: {width: 1440, height: 1000}}), errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(base); await until(async ()=>(await page.locator('#connection').textContent())==='Connected','GUI');
    await page.locator('.workspace-nav-item[data-view="tracking"]').click();
    await page.locator('#trackingView [data-open-settings="receiver"]').click();
    assert.equal(await page.locator('[name="acquisition.enabled"],[name="galileo_acquisition.enabled"],[name="tracking.enabled"]').count(),0);
    await page.locator('[name="receiver.channels"]').fill('4');
    await page.locator('[name="receiver.gps_reserved"]').fill('1');
    await page.locator('[name="receiver.galileo_reserved"]').fill('1');
    await page.locator('#apply').click();
    await until(async ()=>(await get('config')).revision!==config.revision,'shared channel profile');
    config=await get('config');assert.match(config.ini,/channels=4/);
    assert(!/\[(?:acquisition|galileo_acquisition|tracking)\]\s+enabled=/.test(config.ini));
    await post('receiver/start',{revision:config.revision});
    const found=await until(async ()=>(await get('status')).acquisition?.results.find(r=>r.confirmed&&r.prn===7),'GPS confirmation');
    const expected=(231.3+Number(found.first_sample)/2400000*1023000*(1+1234/1575420000))%1023;
    const phaseError=Math.abs(found.code_phase_chips-expected);
    assert(Math.min(phaseError,1023-phaseError)<0.6);
    let status=await get('status');assert(status.acquisition.enabled&&status.galileo_acquisition.enabled);
    assert.equal(status.tracking.length,4);
    assert.deepEqual(status.tracking.map(t=>t.channel_id),[1,2,3,4]);
    assert.deepEqual(status.tracking.map(t=>t.reservation),[1,2,0,0]);
    const tracked=await until(async ()=>(await get('status')).tracking.find(t=>t.constellation===1&&t.prn===7&&t.active&&Number(t.integrations)>2),'GPS tracking slot');
    await page.locator('.workspace-nav-item[data-view="tracking"]').click();
    await until(async ()=>await page.locator('[data-channel-id]').count()===4,'unified channel overview');
    assert(await page.locator('[data-channel-overview]').isVisible());
    assert(!(await page.locator('[data-satellite-view]').isVisible()));
    await page.locator(`[data-channel-id="${tracked.channel_id}"]`).click();
    await until(async ()=>await page.locator('[data-satellite-view]').isVisible(),'satellite detail page');
    assert.equal(new URL(page.url()).hash,`#tracking/${tracked.channel_id}`);
    assert.match(await page.locator('[data-satellite-title]').textContent(),/GPS 7/);
    const taps=testTracking?9:3;
    await until(async ()=>await page.locator('[data-tracking-shape] circle').count()===taps,'measured tap points');
    assert.equal(await page.locator('[data-tracking-shape] polyline').count(),1,'Measured tap curve');
    assert.match(await page.locator('[data-satellite-acquisition]').textContent(),/confirmed/);
    assert.match(await page.locator('[data-tracking-detail]').textContent(),/C\/N0/);
    const range = page.locator('[data-tracking-range]');
    assert(Number(await range.inputValue()) > 0);
    await range.evaluate(input => {
      // Commit in one browser task so a status refresh cannot reset the draft.
      input.value = '10';
      input.dispatchEvent(new Event('change', {bubbles: true}));
    });
    await delay(120);
    assert.equal(await range.inputValue(), '10', 'Manual axis range is held');
    await until(async ()=>await page.locator('[data-channel-search-space] canvas').evaluateAll(nodes=>nodes.every(n=>n.dataset.prn==='7')),'selected satellite acquisition map');
    await page.screenshot({path:path.join(root,'channel-gps.png'),fullPage:true});
    await page.setViewportSize({width:390,height:844});
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+2),'mobile page fits viewport');
    await page.setViewportSize({width:1440,height:1000});
    await page.locator('[data-channel-back]').click();
    await until(async ()=>await page.locator('[data-channel-overview]').isVisible(),'return to channel overview');
    if(testGalileo) {
      const gal=await until(async ()=>(await get('status')).tracking.find(t=>t.constellation===2&&t.prn===7&&t.active&&t.main_peak_verified),'Galileo central peak tracking');
      await page.locator(`[data-channel-id="${gal.channel_id}"]`).click();
      await until(async ()=>await page.locator('[data-satellite-title]').textContent().then(v=>v.includes('Galileo 7')),'Galileo satellite page');
      await until(async ()=>await page.locator('[data-tracking-shape] circle').count()===taps,'Galileo correlators');
      await until(async ()=>(await get('status')).tracking.some(t=>t.constellation===2&&t.prn===7&&t.cn0_valid),'valid Galileo moment C/N0');
      assert.match(await page.locator('[data-tracking-detail]').textContent(),/BOC main peak verified/);
      await until(async ()=>await page.locator('[data-channel-search-space] canvas').evaluateAll(nodes=>nodes.every(n=>n.dataset.signal==='galileo')),'Galileo satellite acquisition map');
      await page.screenshot({path:path.join(root,'channel-galileo.png'),fullPage:true});
    }
    const [space, galileoSpace] = await Promise.all([get(route('gps')), get(route('galileo'))]);
    if (testGalileo) {
      assert.equal(galileoSpace.valid, true);
      assert.equal(galileoSpace.signal, 'galileo_e1b');
      assert.equal(galileoSpace.power.length, 116);
      assert(galileoSpace.power.every(row => row.length === 1024));
    }
    assert.equal(space.valid,true);assert.equal(space.confirmed,true);
    assert.equal(space.power.length,16);assert.equal(space.power[0].length,1024);
    assert.equal((await fetch(base+'/api/v1/acquisition/search-space?prn=33')).status,400);
    await post('receiver/stop',{revision:config.revision});
    await until(async ()=>(await get('status')).state==='stopped','stop');
    assert.equal((await get('acquisition/search-space?prn=7')).valid,true,'completed map survives stop');
    if(testGalileo) {
      await page.locator('.workspace-nav-item[data-view="tracking"]').click();
      await page.locator('#trackingView [data-open-settings="receiver"]').click();
      await until(async ()=>await page.locator('[name="receiver.channels"]').isEnabled(),'allocation editable');
      await page.locator('[name="receiver.channels"]').fill('2');
      await page.locator('[name="receiver.gps_reserved"]').fill('0');
      await page.locator('[name="receiver.galileo_reserved"]').fill('2');
      await page.locator('#apply').click();
      await until(async ()=>(await get('config')).revision!==config.revision,'Galileo reservations applied');
      config=await get('config');const oldRun=(await get('status')).run_id;
      await post('receiver/start',{revision:config.revision});
      await until(async ()=>{const s=await get('status');return s.run_id!==oldRun&&s.state==='running';},'reserved run started');
      await until(async ()=>(await get('status')).tracking.some(t=>t.constellation===2&&t.state===2),'reserved Galileo tracking');
      const reserved=await get('status');assert.equal(reserved.tracking.length,2);
      assert(reserved.tracking.every(t=>t.reservation===2));
      assert(reserved.acquisition.enabled&&reserved.galileo_acquisition.enabled,'Intrinsic stages remain enabled');
      assert.deepEqual(reserved.acquisition.results,[],'No eligible GPS slot admits no GPS search');
      await post('receiver/stop',{revision:config.revision});
      await until(async ()=>(await get('status')).state==='stopped','reserved run stop');
    }
    assert.deepEqual(errors, []);
    await post('shutdown', {}); assert.equal(await exited, 0);
    console.log('PASS: unified channel allocation, intrinsic stages, satellite pages, raw correlators, acquisition maps and reservations. '+root);
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill('SIGTERM'); await exited; }
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
