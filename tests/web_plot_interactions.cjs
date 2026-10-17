/* Browser-only display regression. Requires Node >=18 and Playwright on the test host. */
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const root=path.resolve(__dirname,'..');
const policy="default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self' ws://127.0.0.1:* ws://localhost:*; frame-ancestors 'none'; base-uri 'none'";
const assets=new Map([['/',['index.html','text/html']],['/app.js',['app.js','text/javascript']],['/style.css',['style.css','text/css']],['/ngene-logo.svg',['ngene-logo.svg','image/svg+xml']]]);
const server=http.createServer((request,response)=>{
  response.setHeader('Content-Security-Policy',policy);
  const asset=assets.get(request.url);
  if(asset){response.setHeader('Content-Type',asset[1]);response.end(fs.readFileSync(path.join(root,'web',asset[0])));return;}
  response.setHeader('Content-Type','application/json');
  if(request.url==='/api/v1/bootstrap')response.end(JSON.stringify({token:'plot-fixture',os:'fixture',architecture:'browser',version:'test'}));
  else if(request.url==='/api/v1/config')response.end(JSON.stringify({revision:'1',ini:fs.readFileSync(path.join(root,'config','replay.ini'),'utf8')}));
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
      for(let sequence=1;sequence<=64;sequence++){
        const dbfs=Array.from({length:256},(_,bin)=>-85+12*Math.sin(bin/14+sequence/9)+(bin===135?45:0));
        let checksum=2166136261;
        for(const value of dbfs){let word=Math.floor(value*1000+0.5)>>>0;for(let byte=0;byte<4;byte++){checksum=Math.imul(checksum^(word&255),16777619)>>>0;word>>>=8;}}
        renderAnalysis({sequence:String(sequence),sequence_id:String(sequence),revision:'1',produced_seconds:sequence*0.3,
          first_sample:String(sequence*100000),epoch:'1',sample_rate_hz:100000,fft_size:256,bin_hz:390.625,enbw_hz:585.9375,
          dbfs,integrity_ok:true,nan_count:0,inf_count:0,checksum,telemetry_checksum:checksum,
          min:Math.min(...dbfs),max:Math.max(...dbfs),mean:dbfs.reduce((sum,x)=>sum+x,0)/dbfs.length,
          config:{units:'dbfs_bin',window:'hann',averages:1,spectrum_source:'post',histogram_source:'post'},
          peak_hz:2734.375,peak_dbfs:dbfs[135],histogram_count:0,histogram_i:[],histogram_q:[],stats_i:stats,stats_q:stats,
          dc_magnitude:0,histogram_first_sample:'0',histogram_age_seconds:0,clipped_i:'0',clipped_q:'0',compute_ms:0.2,
          skipped_bursts:0,contention_blocks:0,sample_backend:'fixture',backend:'fixture'});
      }
    });
    await page.locator('#pausePlot').click();
    const original=await snapshot();assert.equal(original.history.length,64);
    const spectrum=page.locator('#spectrum'),waterfall=page.locator('#waterfall');
    const image=()=>waterfall.evaluate(canvas=>canvas.toDataURL());
    const flat=await image(),heightBefore=(await waterfall.boundingBox()).height;
    await page.locator('#waterfallHeight').fill('500');
    await until(async()=>(await waterfall.boundingBox()).height>heightBefore+150,'Waterfall slider did not enlarge the canvas');
    assert.equal(await page.locator('#waterfallHeightValue').textContent(),'500 px');
    await page.locator('#waterfallResize').focus();await page.keyboard.press('ArrowDown');
    assert.equal(await page.locator('#waterfallResize').getAttribute('aria-valuenow'),'510');
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
    await page.locator('#waterfallMode').selectOption('3d');
    const initial3D=await snapshot(),initialCamera=initial3D.waterfall.camera;
    assert(initial3D.waterfall.rows>1&&initial3D.waterfall.rows<=48);assert(initial3D.waterfall.columns>1&&initial3D.waterfall.columns<=192);
    assert.notEqual(await image(),flat);
    const defaultImage=await image();
    // Manually entered bounds can exceed HTML min/max; clipping must never invert
    // or collapse the power/colour scale used by the 3D projection.
    for(const [minimum,maximum] of [['dbMin','dbMax'],['colourMin','colourMax']]){
      for(const [lo,hi] of [[200,300],[-400,-300],[100,200],[-300,-250]]){
        await page.evaluate(({minimum,maximum,lo,hi})=>{
          document.getElementById(minimum).value=lo;document.getElementById(maximum).value=hi;
          document.getElementById(maximum).dispatchEvent(new Event('change'));
        },{minimum,maximum,lo,hi});
        assert((await image())===defaultImage,`${minimum}: invalid clipped scale ${lo}..${hi} must use the default range`);
      }
      await page.evaluate(({minimum,maximum})=>{
        document.getElementById(minimum).value=-100;document.getElementById(maximum).value=0;
        document.getElementById(maximum).dispatchEvent(new Event('change'));
      },{minimum,maximum});
    }
    await page.locator('#waterfallRight').click();await page.locator('#waterfallUp').click();
    await until(async()=>{const camera=(await snapshot()).waterfall.camera;return camera.yaw===initialCamera.yaw+10&&camera.pitch===initialCamera.pitch+5;},'Arrow controls did not rotate the waterfall');
    assert.notEqual(await image(),defaultImage,'Rotating camera did not change the rendered projection');
    await waterfall.scrollIntoViewIfNeeded();let box=await waterfall.boundingBox();
    await page.mouse.move(box.x+box.width/2,box.y+box.height/2);await page.mouse.down();
    await page.mouse.move(box.x+box.width/2+100,box.y+box.height/2+30,{steps:6});await page.mouse.up();
    await until(async()=>(await snapshot()).waterfall.camera.yaw>initialCamera.yaw+30,'Mouse drag did not change camera yaw');
    await waterfall.focus();await page.keyboard.press('ArrowLeft');await page.keyboard.press('ArrowDown');
    await page.keyboard.press('Home');
    await until(async()=>JSON.stringify((await snapshot()).waterfall.camera)===JSON.stringify(initialCamera),'Home did not reset camera');
    assert.equal(await image(),defaultImage,'Reset did not restore exact projection of paused history');
    for(let i=0;i<30;i++)await page.locator('#waterfallUp').click();
    await until(async()=>(await snapshot()).waterfall.camera.pitch===78,'Tilt did not stop at its upper bound');
    await page.locator('#waterfallReset').click();
    assert.deepEqual((await snapshot()).history,original.history,'Plot interactions mutated PSD history');
    if(process.env.NGENE_SCREENSHOTS){fs.mkdirSync(process.env.NGENE_SCREENSHOTS,{recursive:true});await page.locator('#spectrumPanel').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'plot-interactions-desktop.png')});}
    await page.setViewportSize({width:390,height:844});await page.locator('#waterfallMode').scrollIntoViewIfNeeded();
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Mobile plot controls overflowed');
    await page.locator('#waterfallHeight').fill('270');await waterfall.scrollIntoViewIfNeeded();box=await waterfall.boundingBox();
    const beforeTouch=(await snapshot()).waterfall.camera;
    const cdp=await context.newCDPSession(page);
    const x=box.x+box.width/2,y=box.y+box.height/2;
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x,y}]});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:x+60,y:y+20}]});
    await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
    await until(async()=>(await snapshot()).waterfall.camera.yaw!==beforeTouch.yaw,'Touch drag did not rotate camera');
    await page.locator('#expandPlots').click();
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Expanded mobile plots overflowed');
    await page.locator('#expandPlots').click();
    await page.locator('#waterfallMode').selectOption('2d');
    assert(await page.locator('#waterfallOrbit').isHidden());
    assert.equal(await waterfall.evaluate(canvas=>getComputedStyle(canvas).touchAction),'auto','2D waterfall prevents touch scrolling');
    assert.equal(await waterfall.getAttribute('tabindex'),'-1');
    assert.deepEqual((await snapshot()).history,original.history,'Mode changes or mobile resizing mutated PSD history');
    await page.locator('#waterfallMode').selectOption('3d');await page.locator('#waterfallReset').click();
    if(process.env.NGENE_SCREENSHOTS)await page.locator('#spectrumPanel').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'plot-interactions-mobile.png')});
    const enabledHeight=(await page.locator('#spectrumPanel').boundingBox()).height;
    await page.locator('#showSpectrum').uncheck();
    assert(await page.locator('#spectrumBody').isHidden()&&await page.locator('#waterfallBody').isVisible(),'Mobile spectrum enable also hid the waterfall');
    await page.locator('#showWaterfall').uncheck();
    assert(await page.locator('#spectrumBody').isHidden()&&await page.locator('#waterfallBody').isHidden(),'Disabled mobile plots left stale images visible');
    assert(await page.locator('#showSpectrum').isVisible()&&await page.locator('#showWaterfall').isVisible(),'Mobile plot enables disappeared with their plots');
    assert((await page.locator('#spectrumPanel').boundingBox()).height<enabledHeight/2,'Disabled mobile plots retained their canvas layout space');
    assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'Disabled mobile plot controls overflowed');
    if(process.env.NGENE_SCREENSHOTS)await page.locator('#spectrumPanel').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'plot-interactions-mobile-disabled.png')});
    await page.locator('#showSpectrum').check();await page.locator('#showWaterfall').check();
    assert.deepEqual((await snapshot()).history,original.history,'Mobile plot enables mutated paused PSD history');
    assert.deepEqual(errors,[]);
    console.log('PASS: live plot sizing, mouse/keyboard resize, viewport expand/restore and focus, 3D depth-sorted orbit via mouse/touch/arrows/keyboard/reset, bounded tilt/mesh, mobile layout/2D scroll and unchanged paused PSD history under production CSP.');
  }finally{if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
