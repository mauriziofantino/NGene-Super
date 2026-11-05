/* Browser-only display regression. Requires Node >=18 and Playwright on the test host. */
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const root=path.resolve(__dirname,'..');
const policy="default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self' ws://127.0.0.1:* ws://localhost:*; frame-ancestors 'none'; base-uri 'none'";
const assets=new Map([['/',['index.html','text/html']],['/app.js',['app.js','text/javascript']],['/workspace.js',['workspace.js','text/javascript']],['/receiver-config.js',['receiver-config.js','text/javascript']],['/style.css',['style.css','text/css']],['/ngene-logo.svg',['ngene-logo.svg','image/svg+xml']]]);
let analysisRevision=1;
const analysis={spectrum_enabled:true,histogram_enabled:true,units:'dbfs_bin',window:'hann',averages:1,spectrum_source:'post',histogram_source:'post'};
const server=http.createServer((request,response)=>{
  response.setHeader('Content-Security-Policy',policy);
  for(const name of ['module-parameters.js','frequency-units.js','waterfall-relief.js'])assets.set('/'+name,[name,'text/javascript']);
  const asset=assets.get(request.url);
  if(asset){response.setHeader('Content-Type',asset[1]);response.end(fs.readFileSync(path.join(root,'web',asset[0])));return;}
  response.setHeader('Content-Type','application/json');
  if(request.url==='/api/v1/module-schema')response.end(fs.readFileSync(path.join(root,'web','module-parameters.json'),'utf8'));
  else if(request.url==='/api/v1/bootstrap')response.end(JSON.stringify({token:'plot-fixture',os:'fixture',architecture:'browser',version:'test'}));
  else if(request.url==='/api/v1/config')response.end(JSON.stringify({revision:'1',ini:fs.readFileSync(path.join(root,'config','replay.ini'),'utf8')}));
  else if(request.url==='/api/v1/analysis/config'&&request.method==='GET')response.end(JSON.stringify({revision:String(analysisRevision),analysis}));
  else if(request.url==='/api/v1/analysis/config'&&request.method==='POST'){
    let body='';request.on('data',chunk=>body+=chunk);request.on('end',()=>{
      Object.assign(analysis,Object.fromEntries(Object.entries(JSON.parse(body)).filter(([key])=>key!=='revision')));analysisRevision++;
      response.end(JSON.stringify({revision:String(analysisRevision),analysis}));
    });
  }
  else {response.statusCode=404;response.end('{}');}
});
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function until(predicate,message){for(let n=0;n<100;n++){if(await predicate())return;await delay(30);}throw new Error(message);}
(async()=>{
  let browser;
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  try{
    browser=await chromium.launch({headless:true,...(process.env.BROWSER_EXECUTABLE?{executablePath:process.env.BROWSER_EXECUTABLE}:{}),args:['--no-sandbox']});
    const context=await browser.newContext({viewport:{width:1512,height:1100},hasTouch:true});
    const page=await context.newPage(),errors=[];
    page.on('pageerror',error=>errors.push(error.message));
    // Receiver commands and transport are exercised separately by web_ui.cjs.
    await page.addInitScript(()=>{window.WebSocket=class {static OPEN=1;readyState=0;close(){}send(){}};});
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    const snapshot=()=>page.evaluate(async()=>(await import('/app.js')).diagnosticSnapshot());
    await until(()=>page.locator('#configBadge').textContent().then(text=>text==='r1'),'Fixture profile did not load');
    const logo=page.locator('.brandmark');
    assert(await logo.isVisible(),'The NGene logo is missing from the console header');
    await until(()=>logo.evaluate(image=>image.complete&&image.naturalWidth>0),'The NGene logo asset did not load under production CSP');
    await page.evaluate(async()=>{
      const {renderAnalysis}=await import('/app.js');
      const stats={mean:0,std:10,rms:10,min:-32,max:32,clipping_count:0,zero_count:0};
      for(let sequence=1;sequence<=128;sequence++){
        const drift=16*Math.sin(sequence/30),peak=(bin,centre)=>48*Math.exp(-(((bin-centre)/2.5)**2));
        const dbfs=Array.from({length:256},(_,bin)=>-65+3*Math.sin(bin*7+sequence*11)+peak(bin,80+drift)+peak(bin,128)+peak(bin,176-drift)+(sequence===64&&bin===210?45:0));
        let checksum=2166136261;
        for(const value of dbfs){let word=Math.floor(value*1000+0.5)>>>0;for(let byte=0;byte<4;byte++){checksum=Math.imul(checksum^(word&255),16777619)>>>0;word>>>=8;}}
        const frame={sequence:String(sequence),sequence_id:String(sequence),revision:'1',produced_seconds:sequence*0.3,
          first_sample:String(sequence*100000),epoch:'1',sample_rate_hz:100000,fft_size:256,bin_hz:390.625,enbw_hz:585.9375,
          dbfs,integrity_ok:true,nan_count:0,inf_count:0,checksum,telemetry_checksum:checksum,
          min:Math.min(...dbfs),max:Math.max(...dbfs),mean:dbfs.reduce((sum,x)=>sum+x,0)/dbfs.length,
          config:{units:'dbfs_bin',window:'hann',averages:1,spectrum_source:'post',histogram_source:'post'},
          peak_hz:2734.375,peak_dbfs:dbfs[135],histogram_count:0,histogram_i:[],histogram_q:[],stats_i:stats,stats_q:stats,
          dc_magnitude:0,histogram_first_sample:'0',histogram_age_seconds:0,clipped_i:'0',clipped_q:'0',compute_ms:0.2,
          skipped_bursts:0,contention_blocks:0,sample_backend:'fixture',backend:'fixture'};
        renderAnalysis(frame);window.reliefFixture=frame;
      }
    });
    await page.locator('#pausePlot').click();
    const original=await snapshot();assert.equal(original.history.length,128);
    await page.evaluate(async()=>{const {renderAnalysis}=await import('/app.js');renderAnalysis({...window.reliefFixture,sequence:'129',sequence_id:'129',produced_seconds:39});});
    assert.deepEqual((await snapshot()).history,original.history,'Paused input changed shared history');
    const registered=async()=>{
      const state=await snapshot();
      for(const key of ['rows','columns','newest','oldest'])assert.equal(state.waterfall[key],state.waterfall3D[key],'Synchronized '+key);
      assert.equal(state.waterfall3D.style,'surface');
      assert(state.waterfall3D.heightFraction>0,'3D power must have geometric height');
    };
    await registered();
    await page.locator('#showWaterfall3D').uncheck();
    await page.locator('#showWaterfall3D').check();
    await until(async()=>(await snapshot()).work.waterfall3D>original.work.waterfall3D,'Reenabled paused relief did not redraw');
    await registered();
    const spectrum=page.locator('#spectrum'),waterfall=page.locator('#waterfall');
    const image=()=>waterfall.evaluate(canvas=>canvas.toDataURL());
    const flat=await image(),heightBefore=(await waterfall.boundingBox()).height;
    await page.locator('#waterfallHeight').fill('500');
    await until(async()=>(await waterfall.boundingBox()).height>heightBefore+150,'Waterfall slider did not enlarge the canvas');
    assert.equal(await page.locator('#waterfallHeightValue').textContent(),'500 px');
    await page.locator('#waterfallResize').focus();await page.keyboard.press('ArrowDown');
    assert.equal(await page.locator('#waterfallResize').getAttribute('aria-valuenow'),'510');
    await page.locator('#spectrumResize').evaluate(handle=>handle.scrollIntoView({block:'center'}));
    const separator=await page.locator('#spectrumResize').boundingBox();
    await page.mouse.move(separator.x+separator.width/2,separator.y+separator.height/2);await page.mouse.down();
    await page.mouse.move(separator.x+separator.width/2,separator.y+separator.height/2+90,{steps:5});await page.mouse.up();
    assert.equal(await page.locator('#spectrumHeight').inputValue(),'350');
    assert((await spectrum.boundingBox()).height>320,'Spectrum resize did not update canvas layout');
    const panelWidth=(await page.locator('#spectrumPanel').boundingBox()).width;
    await page.locator('#expandPlots').click();
    assert.equal(await page.locator('#expandPlots').getAttribute('aria-expanded'),'true');
    assert((await page.locator('#spectrumPanel').boundingBox()).width>panelWidth,'Expanded plots did not use the viewport width');
    assert.equal(await page.locator('#spectrumPanel').getAttribute('aria-modal'),'true');
    const focusBounds=await page.locator('#spectrumPanel').evaluate(panel=>{
      const controls=[...panel.querySelectorAll('button,input,select,[tabindex="0"]')].filter(el=>!el.disabled&&el.getClientRects().length);
      return {first:controls[0].id,last:controls.at(-1).id};
    });
    await page.locator(`#${focusBounds.first}`).focus();await page.keyboard.press('Shift+Tab');
    assert.equal(await page.evaluate(()=>document.activeElement.id),focusBounds.last,'Focus escaped expanded plot dialog');
    await page.keyboard.press('Tab');assert.equal(await page.evaluate(()=>document.activeElement.id),focusBounds.first);
    await page.keyboard.press('Escape');
    assert.equal(await page.locator('#expandPlots').getAttribute('aria-expanded'),'false');
    assert.equal(await page.locator('#spectrumPanel').getAttribute('aria-modal'),null);
    assert.equal(await page.locator('#waterfallHeight').inputValue(),'510');
    assert.equal(await page.locator('#spectrumHeight').inputValue(),'350');
    const fixed=await snapshot();
    assert(fixed.waterfall3D.rows>1&&fixed.waterfall3D.rows<=2048);
    assert(fixed.waterfall3D.columns>1&&fixed.waterfall3D.columns<=1024);
    assert.equal(await page.locator('#waterfallMode,#waterfallOrbit').count(),0);
    await page.locator('#waterfall3DHeight').fill('400');
    assert.equal(await page.locator('#waterfall3DResize').getAttribute('aria-valuenow'),'400');
    await registered();
    if(process.env.NGENE_SCREENSHOTS){
      await page.locator('#spectrumPanel').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'relief-desktop.png'),style:'.workspace-topbar{visibility:hidden!important}'});
      await page.locator('#waterfall3D').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'relief-detail.png')});
    }
    await page.setViewportSize({width:390,height:844});
    await page.locator('#waterfall3D').scrollIntoViewIfNeeded();
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Mobile plot controls overflowed');
    assert.deepEqual((await snapshot()).history,original.history,'Resizing mutated PSD history');
    if(process.env.NGENE_SCREENSHOTS)await page.locator('#spectrumPanel').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'relief-mobile.png'),style:'.workspace-topbar{visibility:hidden!important}'});
    await page.locator('#clearWaterfall3D').click();
    assert.deepEqual((await snapshot()).history,[],'Relief clear did not clear shared history');
    await registered();
    await page.locator('#showWaterfall3D').uncheck();
    const enabledHeight=(await page.locator('#spectrumPanel').boundingBox()).height;
    await page.locator('#showSpectrum').uncheck();
    assert(await page.locator('#spectrumBody').isHidden()&&await page.locator('#waterfallBody').isVisible(),'Mobile spectrum enable also hid the waterfall');
    await page.locator('#showWaterfall').uncheck();
    await until(()=>page.locator('#showWaterfall').isEnabled(),'FFT disable command did not settle');
    assert(await page.locator('#spectrumBody').isHidden()&&await page.locator('#waterfallBody').isHidden(),'Disabled mobile plots left stale images visible');
    assert(await page.locator('#showSpectrum').isVisible()&&await page.locator('#showWaterfall').isVisible(),'Mobile plot enables disappeared with their plots');
    assert((await page.locator('#spectrumPanel').boundingBox()).height<enabledHeight/2,'Disabled mobile plots retained their canvas layout space');
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Disabled mobile plot controls overflowed');
    if(process.env.NGENE_SCREENSHOTS)await page.locator('#spectrumPanel').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'plot-interactions-mobile-disabled.png')});
    await page.locator('#showSpectrum').check();
    await until(()=>page.locator('#showWaterfall').isEnabled(),'FFT enable command did not settle');
    await page.locator('#showWaterfall').check();
    assert.deepEqual((await snapshot()).history,[],'A changed FFT configuration must clear history before fresh receiver data');
    assert.deepEqual(errors,[]);
    console.log('PASS: live plot sizing, mouse/keyboard resize, viewport expand/restore and focus, synchronized relief, shared clear, paused reenable, independent height and mobile layout under production CSP.');
  }finally{if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
