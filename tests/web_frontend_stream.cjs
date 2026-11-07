/* Real service/browser: immutable SiGe controls and canonical CI16 replay/recording. */
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {spawn}=require('node:child_process');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn,label){for(let n=0;n<200;n++){const v=await fn();if(v)return v;await delay(30);}throw Error('Timeout: '+label);}
(async()=>{
 const exe=path.resolve(process.env.NGENE_EXECUTABLE||'build/msvc-x64-multisdr/Release/ngene.exe');
 const port=Number(process.env.NGENE_TEST_PORT||8128),base='http://127.0.0.1:'+port;
 const root=fs.mkdtempSync(path.resolve('build/frontend-stream-ui-')),input=path.join(root,'tone.sc16q11');
 const iq=Buffer.alloc(400000);for(let n=0;n<100000;n++){iq.writeInt16LE(Math.round(512*Math.cos(2*Math.PI*n/10)),4*n);iq.writeInt16LE(Math.round(512*Math.sin(2*Math.PI*n/10)),4*n+2);}fs.writeFileSync(input,iq);
 const child=spawn(exe,['--web','--port',String(port),'--output-dir',path.join(root,'profile')],{stdio:['ignore','pipe','pipe']});let log='';child.stdout.on('data',b=>log+=b);child.stderr.on('data',b=>log+=b);const exit=new Promise(r=>child.once('exit',r));let browser;
 try{
  const bootstrap=await until(async()=>{if(child.exitCode!==null)throw Error(log);try{return await(await fetch(base+'/api/v1/bootstrap')).json();}catch{return false;}},'startup');
  const get=async route=>(await fetch(base+'/api/v1/'+route)).json();
  const post=async(route,body,expected=200)=>{const response=await fetch(base+'/api/v1/'+route,{method:'POST',headers:{'Content-Type':'application/json','X-NGene-Token':bootstrap.token},body:JSON.stringify(body)});const result=await response.json();if(expected===200)assert(response.ok,JSON.stringify(result));else assert.equal(response.status,expected,JSON.stringify(result));return result;};
  assert.equal(bootstrap.frontends?.sige??bootstrap.sige,true,'SiGe must be compiled into this fixture build');
  browser=await chromium.launch({headless:true,...(process.env.BROWSER_EXECUTABLE?{executablePath:process.env.BROWSER_EXECUTABLE}:{}),args:['--no-sandbox']});
  const page=await browser.newPage({viewport:{width:1440,height:1100}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(base);await until(async()=>await page.locator('#connection').textContent()==='Connected','connected');
  await page.locator('#showConfiguration').click();await page.locator('[data-settings="source"]').click();
  await page.locator('#sigePreset').click();
  assert.equal(await page.locator('[name^="conditioning."]').count(),0,'Retired DC controls');
  assert.equal(await page.locator('[name="frontend.stream_format"]').count(),0,'Native precision is automatic');
  for(const key of ['frontend.sample_rate_hz','frontend.center_frequency_hz','frontend.signal_frequency_hz','hackrf.bandwidth_hz'])assert(await page.locator(`[name="${key}"]`).isDisabled(),key+' must be fixed for SiGe');
  assert.equal(await page.locator('[name="frontend.sample_rate_hz"]').inputValue(),'8.1838');
  assert.equal(await page.locator('[name="frontend.center_frequency_hz"]').inputValue(),'1575.3816');
  assert.equal(await page.locator('[name="frontend.signal_frequency_hz"]').inputValue(),'1.57542');
  assert(await page.locator('#calibrateGains').isDisabled());assert(await page.locator('#sigeInfo').isVisible());
  await page.screenshot({path:path.join(root,'sige-settings.png'),fullPage:true});
  await page.locator('#apply').click();await until(async()=>/type=sige/.test((await get('config')).ini),'saved SiGe');
  const saved=await get('config');assert(!/\[conditioning\]|dc_iir|dc_shift/.test(saved.ini));
  await post('config',{revision:saved.revision,ini:saved.ini.replace('sample_rate_hz=8183800','sample_rate_hz=8183801')},400);
  const current=await get('config');
  const profile=await post('config',{revision:current.revision,ini:`[frontend]\ntype=file\nsample_rate_hz=100000\ncenter_frequency_hz=1575420000\nsignal_frequency_hz=1575420000\nstream_format=auto\n[replay]\nformat=SC16_Q11\npath=${input}\nrealtime=true\nloop=true\n[receiver]\nmode=capture\nrun_seconds=0\n[buffer]\nwindow_samples=1000\ningress_blocks=128\nring_blocks=16\noverflow_policy=stop\n[recording]\nenabled=false\n[analysis]\nspectrum_enabled=true\nhistogram_enabled=true\nfft_size=256\naverages=1\nupdate_hz=20\nhistogram_pairs=1024\nhistogram_bins=32\n`});
  await page.reload();await until(async()=>await page.locator('#connection').textContent()==='Connected','reconnected');
  await page.locator('#showObservation').click();
  await post('receiver/start',{revision:profile.revision});
  const running=await until(async()=>{const s=await get('status');return s.state==='running'&&s.analysis?.sequence_id!=='0'?s:false;},'CI16 running');
  assert.equal(running.sample_format,'CI16');assert.equal(running.native_format,'SC16_Q11');assert.equal(running.adc_bits,12);
  const firstHistogram=await until(async()=>{const s=await get('status');return BigInt(s.analysis.histogram_sequence)>0n?s.analysis:false;},'initial histogram');
  assert.equal(firstHistogram.dc_removal,'simd_initial_mean');assert(firstHistogram.dc_estimate_pairs>0);
  const nextHistogram=await until(async()=>{const s=await get('status');return BigInt(s.analysis.histogram_sequence)>BigInt(firstHistogram.histogram_sequence)+2n?s.analysis:false;},'later histogram');
  for(const key of ['removed_dc_i','removed_dc_q','dc_estimate_pairs'])assert.equal(nextHistogram[key],firstHistogram[key],'Fixed DC metadata '+key);
  await until(async()=>/Fixed DC offset/.test(await page.locator('#dcMagnitude').textContent()),'fixed DC display');
  await page.locator('#showObservation').click();await until(async()=>Number((await page.locator('#waterfall3DInfo').textContent()).match(/(\d+) snapshots/)?.[1])>=8,'3D history');
  assert(!/NaN|Infinity/.test(await page.locator('#spectrumPeak').textContent()));
  await page.locator('#waterfall3D').screenshot({path:path.join(root,'waterfall3d.png')});
  assert.deepEqual(errors,[]);
  await post('receiver/stop',{revision:(await get('config')).revision});await until(async()=>(await get('status')).state==='stopped','stop');
  if(process.env.NGENE_SIGE_LIVE==='1') {
    const current=await get('config');const liveConfig=await post('config',{revision:current.revision,ini:saved.ini});
    await page.reload();await until(async()=>await page.locator('#connection').textContent()==='Connected','live connected');
    await page.locator('#showObservation').click();await page.locator('#refresh').selectOption('33');
    await post('receiver/start',{revision:liveConfig.revision});
    const initial=await until(async()=>{const s=await get('status');if(s.state==='error')throw Error(s.message);return s.state==='running'&&BigInt(s.processed_samples)>1000000n?s:false;},'SiGe live samples');
    await delay(2000);
    await post('recording/start',{revision:(await get('config')).revision});
    await until(async()=>(await get('status')).recording_state==='recording','live recording');
    await delay(1000);
    await post('recording/stop',{revision:(await get('config')).revision});
    await until(async()=>(await get('status')).recording_state==='off','live recording close');
    await delay(2000);
    const live=await get('status');assert.equal(live.state,'running');assert.equal(live.adc_bits,1);assert.equal(live.native_format,'CI8');
    assert(BigInt(live.processed_samples)>30000000n);assert(BigInt(live.written_samples)>0n);
    fs.writeFileSync(path.join(root,'sige-live-status.json'),JSON.stringify(live,null,2));
    // A leading Q component may be discarded once to establish I/Q pairing.
    // This is reported as an unknown startup gap; no later gap is acceptable.
    assert(BigInt(initial.gap_events)<=1n,'At most one initial I/Q alignment gap');
    assert.equal(live.gap_events,initial.gap_events,'No gap after initial I/Q alignment');
    for(const key of ['known_dropped_samples','ingress_overflow_events','transport_errors'])assert.equal(live[key],'0',key);
    const files=await get('files'),sidecar=files.files.find(f=>f.name.endsWith('.cs8.meta.ini'));
    assert(sidecar,'Recorded SiGe sidecar');
    const metadata=await(await fetch(base+'/api/v1/files/'+encodeURIComponent(sidecar.name))).text();
    assert(metadata.includes('source_adc_bits=1\n')&&metadata.includes('significant_bits=1\n'),'One-bit SiGe precision must survive byte storage');
    assert(!metadata.includes('dc_shift='),'New recordings must omit retired DC settings');
    await page.locator('#spectrumPanel').screenshot({path:path.join(root,'sige-live-rf.png')});
    const soakSeconds=Number(process.env.NGENE_SIGE_SOAK_SECONDS||0),runs=Number(process.env.NGENE_SIGE_RUNS||1);
    assert(Number.isFinite(soakSeconds)&&soakSeconds>=0&&soakSeconds<=3600);
    assert(Number.isInteger(runs)&&runs>=1&&runs<=100);
    for(let run=1;run<=runs;run++){
      if(run>1){
        await post('receiver/start',{revision:(await get('config')).revision});
        await until(async()=>{const s=await get('status');if(s.state==='error')throw Error(s.message);return s.state==='running'&&BigInt(s.processed_samples)>1000000n;},'SiGe restart '+run);
      }
      let previous=await get('status');const baseline=previous.gap_events;
      const timeline=[];
      for(let second=0;second<soakSeconds;second++){
        await delay(1000);const sample=await get('status');timeline.push(sample);
        fs.writeFileSync(path.join(root,'sige-soak-'+run+'.json'),JSON.stringify(timeline,null,2));
        assert.equal(sample.state,'running','SiGe run '+run+' second '+second+': '+sample.message);
        assert(BigInt(sample.processed_samples)>BigInt(previous.processed_samples),'SiGe stopped delivering samples');
        assert.equal(sample.gap_events,baseline,'SiGe continuity after startup');
        for(const key of ['known_dropped_samples','ingress_overflow_events','transport_errors'])assert.equal(sample[key],'0',key);
        previous=sample;
      }
      console.log('SiGe run '+run+': '+previous.processed_samples+' pairs, '+previous.wall_seconds+' s, startup gaps '+baseline);
      await post('receiver/stop',{revision:(await get('config')).revision});await until(async()=>(await get('status')).state==='stopped','SiGe soak stop');
    }
  }
  await post('shutdown',{});assert.equal(await exit,0);
  console.log('PASS: fixed SiGe source controls/API validation; no DC controls/profile; Q11 to canonical CI16 metadata and live PSD; 3D projection. Evidence: '+root);
 }finally{if(browser)await browser.close();if(child.exitCode===null){child.kill();await exit;}}
})().catch(e=>{console.error(e);process.exitCode=1;});
