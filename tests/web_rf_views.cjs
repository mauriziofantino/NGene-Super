/* Real receiver regression: three independent RF views and shared FFT demand. */
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {spawn}=require('node:child_process');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),net=require('node:net');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function until(fn,label){for(let i=0;i<160;i++){const r=await fn();if(r)return r;await delay(25);}throw Error('Timeout: '+label);}
(async()=>{
 const exe=path.resolve(process.env.NGENE_EXECUTABLE||'build/ngene'),port=Number(process.env.NGENE_TEST_PORT||8117),base='http://127.0.0.1:'+port;
 const probe=net.createServer();await new Promise((r,j)=>{probe.once('error',j);probe.listen(port,'127.0.0.1',r);});await new Promise(r=>probe.close(r));
 const root=fs.mkdtempSync(path.resolve('build/rf-views-validation-')),input=path.join(root,'tone.cs8');
 const iq=Buffer.alloc(400000);for(let n=0;n<iq.length/2;n++){iq.writeInt8(Math.round(32*Math.cos(2*Math.PI*n/10)),2*n);iq.writeInt8(Math.round(32*Math.sin(2*Math.PI*n/10)),2*n+1);}fs.writeFileSync(input,iq);
 const child=spawn(exe,['--web','--port',String(port),'--output-dir',path.join(root,'profile')],{stdio:['ignore','pipe','pipe']});let log='';child.stdout.on('data',b=>log+=b);child.stderr.on('data',b=>log+=b);const exit=new Promise(r=>child.once('exit',r));let browser;
 try{
  const bootstrap=await until(async()=>{if(child.exitCode!==null)throw Error(log);try{return await(await fetch(base+'/api/v1/bootstrap')).json();}catch{return false;}},'startup');
  const get=async route=>(await fetch(base+'/api/v1/'+route)).json();
  const post=async(route,body)=>{const response=await fetch(base+'/api/v1/'+route,{method:'POST',headers:{'Content-Type':'application/json','X-NGene-Token':bootstrap.token},body:JSON.stringify(body)});const r=await response.json();assert(response.ok,JSON.stringify(r));return r;};
  const initial=await get('config');const config=await post('config',{revision:initial.revision,ini:`[receiver]\nmode=capture\nrun_seconds=0\nidle_timeout_ms=3000\n[frontend]\ntype=file\nsample_rate_hz=100000\ncenter_frequency_hz=1575420000\nsignal_frequency_hz=1575420000\n[replay]\nformat=CI8\npath=${input}\nrealtime=true\nloop=true\n[conditioning]\nmodule=bypass\n[buffer]\nwindow_samples=1000\ningress_blocks=128\nring_blocks=16\noverflow_policy=stop\n[recording]\nenabled=false\n[analysis]\nspectrum_enabled=true\nhistogram_enabled=true\nfft_size=256\naverages=1\nupdate_hz=30\nhistogram_pairs=1024\nhistogram_bins=32\nhistogram_update_hz=30\n[acquisition]\nenabled=true\n`});
  assert(!/\[acquisition\]/.test(config.ini),'retired profile section must not be saved');assert.equal(bootstrap.gps_acquisition,false);assert.equal((await fetch(base+'/api/v1/acquisition/caf')).status,404);
  browser=await chromium.launch({headless:true,...(process.env.BROWSER_EXECUTABLE?{executablePath:process.env.BROWSER_EXECUTABLE}:{}),args:['--no-sandbox']});const page=await browser.newPage({viewport:{width:1440,height:1250}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.addInitScript(()=>{window.canvasCalls={};for(const name of ['clearRect','fillRect','drawImage','fill','stroke','fillText']){const original=CanvasRenderingContext2D.prototype[name];CanvasRenderingContext2D.prototype[name]=function(...args){const id=this.canvas.id;if(id)window.canvasCalls[id]=(window.canvasCalls[id]||0)+1;return original.apply(this,args);};}});
  await page.goto(base);await until(async()=>await page.locator('#connection').textContent()==='Connected','GUI connected');await page.locator('#refresh').selectOption('33');await post('receiver/start',{revision:config.revision});
  const snapshot=()=>page.evaluate(async()=>({...(await import('/app.js')).diagnosticSnapshot(),calls:{...window.canvasCalls}}));
  await until(async()=>(await snapshot()).waterfall3D.history>=24,'3D live history');
  assert.equal(await page.locator('#waterfallMode,#waterfallOrbit').count(),0,'rotating view removed');
  for(const id of ['spectrum','waterfall','waterfall3D'])assert(await page.locator('#'+id+'Body').isVisible());
  const rects=await page.locator('#spectrum,#waterfall,#waterfall3D').evaluateAll(nodes=>nodes.map(n=>n.getBoundingClientRect().top));assert(rects[0]<rects[1]&&rects[1]<rects[2],'spectrum, 2D, 3D order');
  await page.locator('#spectrumPanel').screenshot({path:path.join(root,'desktop.png')});
  const ids=['spectrum','waterfall','waterfall3D'],toggles=['showSpectrum','showWaterfall','showWaterfall3D'];
  // All eight combinations, including each view running alone.
  for(const bits of [6,5,3,4,2,1,0,7]){
    for(let i=0;i<3;i++)await page.locator('#'+toggles[i]).setChecked(Boolean(bits&(1<<i)));
    await until(async()=>(await get('analysis/config')).analysis.spectrum_enabled===Boolean(bits),'master FFT demand '+bits);await delay(180);
    for(let i=0;i<3;i++)assert.equal(await page.locator('#'+ids[i]+'Body').isVisible(),Boolean(bits&(1<<i)));
    const before=await snapshot(),statusBefore=await get('status');await delay(420);const after=await snapshot(),statusAfter=await get('status');
    for(let i=0;i<3;i++){
      const id=ids[i];if(bits&(1<<i))assert(after.work[id]>before.work[id],id+' must draw when enabled');else{assert.equal(after.work[id],before.work[id],id+' must skip rendering preparation');assert.equal(after.calls[id],before.calls[id],id+' must not touch its canvas');}
    }
    if(!(bits&2))assert.deepEqual(after.history,before.history,'disabled 2D history must stay frozen');
    if(!(bits&4))assert.equal(after.waterfall3D.history,before.waterfall3D.history,'disabled 3D history must stay frozen');
    if(!bits){assert.equal(statusAfter.analysis.sequence_id,statusBefore.analysis.sequence_id,'no PSD/FFT with all three views off');assert(BigInt(statusAfter.analysis.histogram_sequence)>BigInt(statusBefore.analysis.histogram_sequence),'histogram remains independent');}
    assert(BigInt(statusAfter.processed_samples)>BigInt(statusBefore.processed_samples),'receiver must keep processing');
  }
  await page.setViewportSize({width:390,height:844});await page.locator('#waterfall3D').scrollIntoViewIfNeeded();await page.screenshot({path:path.join(root,'mobile.png'),fullPage:true});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'mobile overflow');assert.deepEqual(errors,[]);
  await post('shutdown',{});assert.equal(await exit,0);console.log('PASS: acquisition removed; all eight plot combinations; disabled canvases/history/geometry idle; all-off FFT stopped while histogram and receiver advance; fixed perspective; desktop/mobile. Evidence: '+root);
 }finally{if(browser)await browser.close();if(child.exitCode===null){child.kill('SIGTERM');await exit;}}
})().catch(e=>{console.error(e);process.exitCode=1;});

