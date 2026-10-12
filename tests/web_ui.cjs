/* Optional end-to-end test. Requires Node >=18 and Playwright only on the test host. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const {spawn} = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const assert = require('node:assert/strict');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
(async () => {
  const executable = path.resolve(process.env.NGENE_EXECUTABLE || 'build/ngene');
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'ngene-web-'));
  const output = path.join(root, 'selected folder');
  const profile = path.join(root, 'profile');
  const input = path.join(root, 'recorded-input.cs8');
  const iq = Buffer.alloc(400000);
  for(let n=0;n<iq.length/2;n++){iq.writeInt8(Math.round(32*Math.cos(2*Math.PI*n/10)),2*n);iq.writeInt8(Math.round(32*Math.sin(2*Math.PI*n/10)),2*n+1);}
  fs.writeFileSync(input,iq,{flag:'wx'});
  const port = process.env.NGENE_TEST_PORT || '8093';
  const base = `http://127.0.0.1:${port}`;
  const child = spawn(executable, ['--web', '--port', port, '--output-dir', profile], {stdio:['ignore','pipe','inherit']});
  let browser;
  const exit = new Promise(resolve => child.once('exit', resolve));
  const errors = [];
  try {
    let bootstrap;
    for (let i = 0; i < 100; i++) {
      try { bootstrap = await (await fetch(`${base}/api/v1/bootstrap`)).json(); break; } catch { await delay(50); }
    }
    assert(bootstrap, 'Server did not start');
    const post = (route, body, token = bootstrap.token) => fetch(`${base}/api/v1/${route}`, {method:'POST',headers:{'Content-Type':'application/json','X-NGene-Token':token},body:JSON.stringify(body)});
    browser = await chromium.launch({headless:true, ...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE} : {}), args:['--no-sandbox']});
    const page = await browser.newPage({viewport:{width:1512,height:1100},acceptDownloads:true});
    // Poll via CDP evaluation: Playwright 1.51's waitForFunction uses eval under page CSP.
    // Keep NGene's actual Content-Security-Policy enabled throughout the GUI test.
    page.waitForFunction = async (predicate, argument) => {
      const deadline = Date.now() + 30000;
      while (Date.now() < deadline) { if (await page.evaluate(predicate, argument)) return; await delay(25); }
      throw new Error(`Timed out waiting for GUI condition: ${predicate}`);
    };
    page.on('request',request=>{if(process.env.NGENE_DEBUG_TEST && request.method()==='POST')console.log('POST',request.url(),request.postData()?.slice(0,120));});
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => { if (message.type() === 'error' && !message.text().includes('status of 400')) errors.push(message.text()); });
    let websocketFrames = 0, websocketCloses = 0;
    page.on('websocket', ws => { ws.on('framereceived', () => websocketFrames++); ws.on('close', () => websocketCloses++); });
    const state = text => page.waitForFunction(value => document.getElementById('runState').textContent === value, text);
    const recording = text => page.waitForFunction(value => document.getElementById('recordBadge').textContent === value, text);
    await page.goto(base);
    await page.waitForFunction(() => document.getElementById('connection').textContent === 'Connected');
    await state('stopped');
    await page.locator('#showConfiguration').click();
    await page.locator('[data-settings="source"]').click();
    await page.locator('#filePreset').click();
    await page.locator('[name="frontend.sample_rate_hz"]').fill('100000');
    await page.locator('#browseReplay').click();
    await page.locator('#pickerParent').click();
    await page.locator('#pickerEntries tr').filter({hasText:'recorded-input.cs8'}).getByRole('button', {name:'Select',exact:true}).click();
    await page.locator('#pickerUse').click();
    assert.equal(await page.locator('[name="replay.path"]').inputValue(),input);
    await page.locator('[name="frontend.sample_rate_hz"]').fill('100000');
    await page.locator('[name="replay.loop"]').selectOption('true');
    await page.locator('[data-settings="receiver"]').click();
    await page.locator('[name="buffer.window_samples"]').fill('1000');
    await page.locator('[data-settings="recording"]').click();
    await page.locator('[name="recording.directory"]').fill(output);
    await page.locator('[name="recording.prefix"]').fill('field_test');
    await page.locator('[name="recording.point"]').selectOption('pre');
    await page.locator('[data-settings="source"]').click();
    await page.locator('[name="frontend.signal_frequency_hz"]').fill('1575440000');
    assert.equal(await page.locator('[name="hackrf.lna_gain_db"] option').count(),6);
    assert.equal(await page.locator('[name="hackrf.vga_gain_db"] option').count(),32);
    assert(await page.locator('#start').isDisabled());
    await page.locator('#apply').click();
    await page.waitForFunction(() => document.getElementById('configBadge').textContent === 'r2');
    assert.equal(fs.existsSync(output), true);
    const persisted = fs.readFileSync(path.join(profile,'receiver.ini'),'utf8');
    assert(persisted.includes('prefix=field_test'));
    assert(persisted.includes(`directory=${output}`));
    let cfg = await (await fetch(`${base}/api/v1/config`)).json();
    assert.equal((await post('config/validate',{revision:cfg.revision,ini:'[buffer]\nwindow_samples=0\n'})).status,400);
    for (const route of ['config','config/validate']) {
      assert.equal((await post(route,{revision:cfg.revision,ini:'[buffer]\nwindow_samples=invalid\n'})).status,400);
      assert.deepEqual(await (await fetch(`${base}/api/v1/config`)).json(),cfg,
        'Rejected configuration must not change the saved profile or revision');
      assert.equal(fs.readFileSync(path.join(profile,'receiver.ini'),'utf8'),persisted);
    }
    const pickerMany = path.join(root,'many captures'); fs.mkdirSync(pickerMany);
    for(let n=0;n<510;n++)fs.writeFileSync(path.join(pickerMany,`capture-${String(n).padStart(3,'0')}.cs8`),Buffer.from([128,127]));
    const listing = await (await post('replay-files/browse',{path:pickerMany})).json();
    assert.equal(listing.entries.length,500); assert.equal(listing.truncated,true);
    const filtered = await (await post('replay-files/browse',{path:pickerMany,filter:'CAPTURE-509'})).json();
    assert.deepEqual(filtered.entries.map(file=>file.name),['capture-509.cs8'],'Filter must precede the response limit');
    const badInput = path.join(root,'invalid-sidecar.cs8'); fs.writeFileSync(badInput,Buffer.from([128,127]));
    fs.writeFileSync(`${badInput}.meta.ini`,'[capture]\nversion=2\nformat=CF32\n');
    assert.equal((await post('replay-files/inspect',{path:badInput})).status,400);
    assert.deepEqual(await (await fetch(`${base}/api/v1/config`)).json(),cfg,'Rejected sidecar changed the receiver profile');
    assert.equal((await post('receiver/start',{revision:cfg.revision},'invalid')).status,403);
    assert.equal((await fetch(`${base}/api/v1/status`,{headers:{Origin:'http://other.example'}})).status,403);
    await page.locator('#start').click(); await state('running');
    assert.equal((await post('config',{revision:cfg.revision,ini:cfg.ini})).status,409);
    assert(await page.locator('[name="recording.directory"]').isDisabled());
    await page.locator('#showObservation').click();
    await page.locator('#recordStart').click(); await recording('recording');
    await page.waitForFunction(() => Number(document.getElementById('sessionSamples').textContent.replaceAll(',','')) >= 5000);
    const filesWriting = await (await fetch(`${base}/api/v1/files`)).json();
    assert(filesWriting.files.some(f => !f.closed));
    assert.equal((await fetch(`${base}/api/v1/files/${filesWriting.files[0].name}`)).status,409);
    assert.equal((await post('files/delete',{name:filesWriting.files[0].name,revision:filesWriting.revision})).status,409);
    assert(fs.existsSync(path.join(output,filesWriting.files[0].name)),'Active capture was removed');
    assert.equal(await page.locator('#receiverSession').count(), 0, 'Observation must not select another session');
    assert.equal((await fetch(`${base}/api/v1/replay/config`)).status,410);
    assert.equal((await post('replay/receiver/start',{revision:cfg.revision})).status,410);
    assert.equal((await (await fetch(`${base}/api/v1/status`)).json()).state,'running');
    await page.reload();
    await page.waitForFunction(() => document.getElementById('connection').textContent === 'Connected');
    await state('running'); await recording('recording');
    await page.locator('#pausePlot').click();
    assert.equal(await page.locator('#pausePlot').textContent(),'Resume plots');
    await page.locator('#pausePlot').click();
    await page.waitForFunction(() => document.getElementById('spectrumInfo').textContent.includes('2048-point'));
    let initialAnalysis;
    for(let i=0;i<100;i++){initialAnalysis=await (await fetch(`${base}/api/v1/status`)).json();if(initialAnalysis.analysis.histogram_i.reduce((a,b)=>a+b,0)===65536)break;await delay(50);}

    assert(Math.abs(initialAnalysis.analysis.peak_hz-10000)<100);
    assert.equal(initialAnalysis.digital_if_hz,20000);
    assert.equal(initialAnalysis.hardware_frequency_hz,1575420000);
    assert(!('preview' in initialAnalysis),'Raw IQ must not be streamed');
    assert(initialAnalysis.analysis.integrity_ok);
    assert.equal(initialAnalysis.analysis.checksum,initialAnalysis.analysis.telemetry_checksum);
    if(process.env.NGENE_DEBUG_TEST)console.log('INITIAL',initialAnalysis.analysis.histogram_sequence,initialAnalysis.analysis.revision,initialAnalysis.revision,initialAnalysis.analysis.histogram_count,initialAnalysis.analysis.histogram_i.length,initialAnalysis.message);
    assert.equal(initialAnalysis.analysis.histogram_i.reduce((a,b)=>a+b,0),65536);
    await page.locator('#showConfiguration').click();
    assert(await page.locator('[name="frontend.sample_rate_hz"]').isDisabled());
    await page.locator('[data-settings="analysis"]').click();
    assert(await page.locator('#analysis_fft_size').isEnabled());
    await page.locator('#analysis_fft_size').selectOption('4096');
    await page.locator('#analysis_window').selectOption('blackman');
    await page.locator('#analysis_histogram_bins').selectOption('128');
    await page.locator('#analysis_histogram_pairs').fill('8192');
    await page.locator('#analysis_overlap_percent').fill('37');
    await page.locator('#analysis_update_hz').fill('12.5');
    await page.locator('#analysis_temporal').selectOption('ema');
    await page.locator('#analysis_smoothing_alpha').fill('0.35');
    await page.locator('#analysis_histogram_update_hz').fill('3');
    await page.locator('#applyAnalysis').click();
    await page.locator('[data-settings="display"]').click();
    await page.locator('#frequencyReference').selectOption('signal');
    await page.locator('[data-settings="analysis"]').click();
    await page.locator('#applyAnalysis').click();
    await page.waitForFunction(() => document.getElementById('analysisNotice').textContent.startsWith('Applied'));
    if(process.env.NGENE_SCREENSHOTS) await page.screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'webgui-configuration.png'),fullPage:true});
    await page.locator('#showObservation').click();
    await page.waitForFunction(() => document.getElementById('spectrumInfo').textContent.includes('4096-point'));
    const runtime=await (await fetch(`${base}/api/v1/status`)).json();
    assert.equal(runtime.state,'running');assert.equal(runtime.run_id,initialAnalysis.run_id);
    assert.equal(runtime.analysis.config.overlap_percent,37);
    assert.equal(runtime.analysis.config.update_hz,12.5);
    assert.equal(runtime.analysis.config.temporal,'ema');
    assert.equal(runtime.analysis.histogram_i.length,128);
    assert.equal(runtime.analysis.histogram_q.reduce((a,b)=>a+b,0),8192);
    await page.waitForFunction(() => Number(document.getElementById('waterfallInfo').textContent.match(/(\d+) snapshots/)?.[1])>=16);
    const diagnostics=await page.evaluate(async()=>{
      const m=await import('/app.js'),a=(await (await fetch('/api/v1/status')).json()).analysis;
      const before=m.diagnosticSnapshot();
      if(!m.psdIntegrity(a))throw new Error('Valid producer frame rejected');
      const corrupted=structuredClone(a);corrupted.dbfs[0]+=3;
      if(m.psdIntegrity(corrupted))throw new Error('Corrupted PSD accepted');
      m.renderAnalysis(corrupted);
      const after=m.diagnosticSnapshot();
      if(after.history.length!==before.history.length||after.rejected!==before.rejected+1)throw new Error('Corrupt frame changed history');
      if(Math.abs(m.displayFrequency(10000)+10)>1e-9)throw new Error('Signal-centred axis shifted samples or sign');
      return after;
    });
    assert(diagnostics.skipped>0,'Test must exercise sequence skips');
    assert(diagnostics.history.every(row=>Array.isArray(row.values)&&row.values.every(Number.isFinite)));
    await page.locator('#refresh').selectOption('1000');
    await delay(2300);
    const slow=await page.evaluate(async()=> (await import('/app.js')).diagnosticSnapshot());
    assert(slow.skipped>diagnostics.skipped,'Slow GUI must discard obsolete frames');
    assert(slow.history.every(row=>row!==null),'Skipped sequences must not create coloured separator rows');
    await page.locator('#refresh').selectOption('33');
    const closesBeforeDelay = websocketCloses;
    await page.evaluate(() => {
      const original = WebSocket.prototype.send;
      let delayed = false;
      WebSocket.prototype.send = function(data) {
        if (!delayed && typeof data==='string' && data.startsWith('status')) {
          delayed = true; setTimeout(() => original.call(this,data),80);
        } else original.call(this,data);
      };
      setTimeout(() => { WebSocket.prototype.send = original; },1000);
    });
    await delay(1200);
    assert.equal(websocketCloses,closesBeforeDelay,'One delayed request must not trigger a burst or disconnect');
    assert.equal(await page.locator('#connection').textContent(),'Connected','30 Hz selection must not cause unanswered requests');
    await page.locator('#showSpectrum').uncheck();await page.locator('#showHistogram').uncheck();
    await delay(2400);
    const dormant=await (await fetch(`${base}/api/v1/status`)).json();
    await delay(350);
    const dormant2=await (await fetch(`${base}/api/v1/status`)).json();
    assert.equal(dormant2.analysis.sequence,dormant.analysis.sequence,'Hidden diagnostic producers must stop');
    assert(BigInt(dormant2.processed_samples)>BigInt(dormant.processed_samples),'Receiver stopped with hidden diagnostics');
    await page.locator('#showSpectrum').check();await page.locator('#showHistogram').check();
    await page.locator('#refresh').selectOption('250');

    if (process.env.NGENE_SCREENSHOTS) {
      fs.mkdirSync(process.env.NGENE_SCREENSHOTS,{recursive:true});
      await page.screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'webgui-desktop.png'),fullPage:true});
    }
    await page.locator('#recordStop').click(); await recording('off'); await state('running');
    const first = await (await fetch(`${base}/api/v1/status`)).json();
    await page.waitForFunction(() => Number(document.getElementById('processed').textContent.replaceAll(',','')) > 20000);
    await page.locator('#reloadFiles').click();
    const captures = await (await fetch(`${base}/api/v1/files`)).json();
    const capture = captures.files.find(file=>file.name.endsWith('.cs8'));
    const sidecar = JSON.parse(JSON.stringify(fs.readFileSync(path.join(output,`${capture.name}.meta.ini`),'utf8')));
    assert(sidecar.includes('version=2') && sidecar.includes(`data_file=${capture.name}`));
    await page.locator('#stop').click(); await state('stopped');
    await page.locator('#files tr').filter({hasText:capture.name}).filter({has:page.getByRole('button',{name:'Replay',exact:true})}).getByRole('button',{name:'Replay',exact:true}).click();
    await page.locator('#pickerUse').waitFor({state:'visible'});
    await page.waitForFunction(()=>!document.getElementById('pickerUse').disabled);
    assert((await page.locator('#pickerMetadata').textContent()).includes('100000 complex pairs/s'));
    const pickerButton = await page.locator('#pickerUse').boundingBox();
    assert(pickerButton && pickerButton.y>=0 && pickerButton.y+pickerButton.height<=1100,'Picker confirmation must stay visible');
    if(process.env.NGENE_SCREENSHOTS)await page.screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'replay-file-picker.png'),fullPage:true});
    await page.locator('#pickerUse').click();
    assert.equal(await page.locator('#receiverSession').count(),0);
    assert.equal(await page.locator('[name="frontend.type"]').inputValue(),'file');
    assert.equal(await page.locator('[name="frontend.sample_rate_hz"]').inputValue(),'100000');
    assert.equal(await page.locator('[name="frontend.signal_frequency_hz"]').inputValue(),'1575440000');
    await page.locator('[data-settings="receiver"]').click();
    assert.equal(await page.locator('[name="conditioning.module"]').inputValue(),'bypass');
    await page.locator('#apply').click();
    await page.waitForFunction(()=>!document.getElementById('configBadge').classList.contains('dirty'));
    await page.locator('#start').click(); await state('running');
    assert.equal((await (await fetch(`${base}/api/v1/status`)).json()).state,'running','Selected file did not start in the one receiver');
    assert.equal((await post('files/delete',{name:capture.name,revision:(await (await fetch(`${base}/api/v1/config`)).json()).revision})).status,409,'Running replay input must be protected');

    await page.locator('#showObservation').click();
    await page.locator('#recordStart').click(); await recording('recording');
    await page.locator('#stop').click(); await state('stopped'); await recording('off');
    const status = await (await fetch(`${base}/api/v1/status`)).json();
    assert(BigInt(status.processed_samples) > 0n && BigInt(status.run_id) > BigInt(first.run_id), "Counters reset for the newly selected source");
    assert.equal(status.output_directory, output);
    assert(status.recording_name.startsWith('field_test_'));
    const files = await (await fetch(`${base}/api/v1/files`)).json();
    assert.equal(files.files.length,8);
    for (const f of files.files) {
      assert(f.closed);
      const bytes = Buffer.from(await (await fetch(`${base}/api/v1/files/${f.name}`)).arrayBuffer());
      assert.deepEqual(bytes,fs.readFileSync(path.join(output,f.name)),'Transfer bytes differ');
      assert.equal(bytes.length,Number(f.bytes));
      if(f.name.endsWith('.meta.ini'))assert(bytes.toString().includes('recording_point=pre_conditioning'));
    }
    await page.locator('#reloadFiles').click();
    await page.locator('#files a').first().waitFor();
    const downloadPromise = page.waitForEvent('download');
    await page.locator('#files a').first().click();
    const download = await downloadPromise;
    await download.saveAs(path.join(root, 'download-copy'));
    assert.deepEqual(fs.readFileSync(path.join(root,'download-copy')),fs.readFileSync(path.join(output,download.suggestedFilename())));
    // Deletion is token/revision guarded, confined to captures, and individually confirmed.
    const victim = files.files.find(f=>f.name.endsWith('.cs8'));
    assert.equal((await post('files/delete',{name:victim.name,revision:files.revision},'invalid')).status,403);
    assert.equal((await post('files/delete',{name:'../receiver.ini',revision:files.revision})).status,400);
    assert.equal((await post('files/delete',{name:'receiver.ini',revision:files.revision})).status,400);
    assert.equal((await post('files/delete',{name:victim.name,revision:'999999'})).status,409);
    await page.locator('.delete-file').evaluateAll((nodes,name)=>nodes.find(n=>n.dataset.filename===name).click(),victim.name);
    await page.locator('#deleteFileDialog').waitFor({state:'visible'});
    assert.equal(await page.locator('#deleteFileName').textContent(),victim.name);
    await page.locator('#deleteFileDialog button[value="cancel"]').click();
    assert(fs.existsSync(path.join(output,victim.name)),'Cancel removed data');
    await page.locator('.delete-file').evaluateAll((nodes,name)=>nodes.find(n=>n.dataset.filename===name).click(),victim.name);
    await page.locator('#confirmDeleteFile').click();
    await page.waitForFunction(name=>!Array.from(document.querySelectorAll('.delete-file')).some(b=>b.dataset.filename===name),victim.name);
    assert(!fs.existsSync(path.join(output,victim.name)),'Confirmed delete did not remove data');
    assert.equal((await post('files/delete',{name:victim.name,revision:files.revision})).status,404);
    assert.equal(fs.readdirSync(output).length,7,'Related capture files should remain');
    if(process.platform!=='win32') {
      const external = path.join(root,'external.cs8');fs.writeFileSync(external,'KEEP');
      fs.symlinkSync(external,path.join(output,'linked.cs8'));
      assert.equal((await post('files/delete',{name:'linked.cs8',revision:files.revision})).status,400);
      assert.equal(fs.readFileSync(external,'utf8'),'KEEP');
    }
    await page.locator('#showConfiguration').click();
    await page.locator('[data-settings="source"]').click();
    const sourceHeight = await page.locator('.configuration-shell').evaluate(el=>el.getBoundingClientRect().height);
    assert(sourceHeight<1100,'Source settings still form an excessively long page');
    if(process.env.NGENE_SCREENSHOTS) await page.screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'webgui-configuration.png'),fullPage:true});
    // Native gain widgets use returned device capabilities. This HTTP response is a
    // deterministic GUI fixture, not a physical hardware validation.
    const supports = await (await fetch(`${base}/api/v1/bootstrap`)).json();
    if (supports.rtlsdr && supports.bladerf) {
      await page.route('**/api/v1/frontend/capabilities', async route => {
        const draft = JSON.parse(route.request().postData()).ini;
        const rtl = draft.includes('type=rtlsdr');
        await route.fulfill({contentType:'application/json',body:JSON.stringify({
          format:rtl?'CU8':'SC16_Q11',sample_rate_min_hz:rtl?225001:80000,
          sample_rate_max_hz:rtl?3200000:61440000,frequency_min_hz:47000000,
          frequency_max_hz:rtl?1766000000:6000000000,
          tuner_gains_tenths_db:rtl?[0,77,144]:[],gain_min_db:-12,gain_max_db:60,gain_step_db:1
        })});
      });
      await page.locator('#rtlPreset').click();
      await page.locator('#deviceLimits').click();
      await page.waitForFunction(()=>document.getElementById('notice').textContent.includes('limits loaded'));
      assert.deepEqual(await page.locator('[name="rtlsdr.gain_tenths_db"] option').evaluateAll(opts=>opts.map(o=>o.value)),['0','77','144']);
      await page.locator('[name="rtlsdr.auto_gain"]').selectOption('false');
      assert(await page.locator('[name="rtlsdr.gain_tenths_db"]').isEnabled());
      assert.equal(await page.locator('[name="frontend.sample_rate_hz"]').getAttribute('max'),'3200000');
      await page.locator('#bladePreset').click();
      await page.locator('#deviceLimits').click();
      await page.waitForFunction(()=>document.getElementById('notice').textContent.includes('SC16_Q11'));
      const gain = page.locator('[name="bladerf.gain_db"]');
      assert.equal(await gain.getAttribute('min'),'-12');assert.equal(await gain.getAttribute('max'),'60');
      assert.equal(await gain.getAttribute('step'),'1');assert(await gain.isEnabled());
      await page.unroute('**/api/v1/frontend/capabilities');
      await page.locator('#filePreset').click();
    }
    await page.setViewportSize({width:390,height:844});
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1),'Mobile layout overflows');
    if (process.env.NGENE_SCREENSHOTS) await page.screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'webgui-mobile.png'),fullPage:true});
    await page.locator('#browseReplay').click();
    await page.locator('#pickerEntries').getByRole('button',{name:'Select',exact:true}).first().click();
    await page.waitForFunction(()=>!document.getElementById('pickerUse').disabled);
    const mobilePicker = await page.locator('#pickerUse').boundingBox();
    assert(mobilePicker && mobilePicker.y>=0 && mobilePicker.y+mobilePicker.height<=844,'Mobile picker confirmation must stay visible');
    assert(await page.locator('#replayFileDialog').evaluate(el=>el.scrollWidth<=el.clientWidth+1),'Mobile picker overflows');
    if(process.env.NGENE_SCREENSHOTS)await page.screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'replay-file-picker-mobile.png'),fullPage:true});
    await page.locator('#pickerClose').click();
    await page.locator('[data-settings="recording"]').click();
    assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1),'Mobile recording layout overflows');
    assert(websocketFrames > 3,'Missing live WebSocket telemetry');
    assert.deepEqual(errors,[]);
    await post('shutdown',{});
    assert.equal(await exit,0);
    console.log('PASS: receiver-side file picker (paths with spaces), per-file metadata import, single active source/no independent replay, recording deletion/active protection/token/revision/path/symlink checks, topic settings and mobile; PSD integrity/corruption, sequence skips without separator rows, slow/latest-frame GUI, 30 Hz polling, dormant producers, independent histogram, tuning axes, gain steps; recording, transfer, persistence, mobile and shutdown.');
  } finally {
    if (browser) await browser.close();
    if (child.exitCode === null) { child.kill('SIGTERM'); await exit; }
    fs.rmSync(root,{recursive:true,force:true});
  }
})().catch(error => {console.error(error);process.exitCode=1;});
