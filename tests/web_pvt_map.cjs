/* Real embedded map with mocked tiles: no automated requests reach the OSM tile service. */
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {spawn}=require('node:child_process');
const path=require('node:path'),fs=require('node:fs'),assert=require('node:assert/strict');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function until(page,predicate){for(let k=0;k<100;k++){if(await page.evaluate(predicate))return;await delay(30);}throw Error('Map update timed out');}
(async()=>{
 const executable=path.resolve(process.env.NGENE_EXECUTABLE||'build/ngene');
 const root=path.resolve('build/pvt-map-test-'+Date.now());fs.mkdirSync(root,{recursive:true});
 const port=18296,base=`http://127.0.0.1:${port}`;
 const child=spawn(executable,['--web','--port',String(port),'--output-dir',root],{stdio:'ignore'});
 let browser;
 try {
  let bootstrap;for(let k=0;k<100;k++){try{bootstrap=await(await fetch(base+'/api/v1/bootstrap')).json();break;}catch{await delay(100);}}
  assert(bootstrap,'service starts');
  assert.equal(bootstrap.beidou_b1c_operational,true);
  const realStatus=await (await fetch(base+'/api/v1/status')).json();
  assert.equal(realStatus.pvt.beidou,0,'Live API advertises zero BeiDou measurements while the receiver is stopped');
  assert.equal(realStatus.pvt.clock_beidou_m,undefined,'No BeiDou bias is invented');
  browser=await chromium.launch({headless:true,executablePath:process.env.BROWSER_EXECUTABLE});
  const page=await browser.newPage({viewport:{width:1100,height:850}});let tiles=0;
  await page.route('**/app.js',route=>route.fulfill({contentType:'application/javascript',body:'/* Isolated workspace fixture; production modules and CSP are served unchanged. */'}));
  await page.route('https://tile.openstreetmap.org/**',route=>{tiles++;return route.fulfill({contentType:'image/png',body:Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+a3ioAAAAASUVORK5CYII=','base64')});});
  await page.goto(base);
  await page.evaluate(async()=>{
   const {createReceiverWorkspace}=await import('/workspace.js');
   window.channelInterest=[];
   window.geometryWorkspace=createReceiverWorkspace({onChannelInterest:id=>window.channelInterest.push(id)});
   window.geometrySnapshot={connected:true,profile:{'receiver.channels':'2'},status:{state:'running',run_id:'geometry',tracking:[
    {channel_id:1,state:0,constellation:0,prn:0,active:false,cn0_valid:false,taps:[]},
    {channel_id:2,state:2,constellation:2,prn:7,active:true,pll:true,cn0_valid:true,cn0_db_hz:42.5,doppler_hz:1250,integrations:'1',epoch:'1',taps:[]}
   ],pvt:{available:true,latitude_deg:45,longitude_deg:8,height_m:100,residual_rms_m:2,tow_seconds:100,epoch:'1',sample:'10',satellites:4,gps:2,galileo:2,dop_available:true,gdop:2.4,hdop:1.2,vdop:1.8,sky:[
    {constellation:1,prn:1,azimuth_deg:0,elevation_deg:30},
    {constellation:2,prn:7,azimuth_deg:90,elevation_deg:60}
   ]}}};
   window.geometryWorkspace.setView('pvt');window.geometryWorkspace.update(window.geometrySnapshot);
  });
  assert.equal(await page.locator('[data-pvt-sky] circle').count(),5,'Three elevation rings and two real satellites');
  assert(Math.abs(Number(await page.locator('[data-pvt-sky] circle').nth(3).getAttribute('cy'))-80)<1e-10,'North is up');
  assert(Math.abs(Number(await page.locator('[data-pvt-sky] circle').nth(4).getAttribute('cx'))-230)<1e-10,'East is right');
  assert.match(await page.locator('[data-pvt-values]').textContent(),/GDOP2\.40HDOP1\.20VDOP1\.80/);
  const layout=await page.evaluate(()=>{
   const overview=document.querySelector('.pvt-overview'),summary=overview.children[0].getBoundingClientRect(),sky=overview.children[1].getBoundingClientRect();
   return {summaryTop:summary.top,skyTop:sky.top,overviewBottom:overview.getBoundingClientRect().bottom,mapTop:document.querySelector('[data-pvt-map]').parentElement.getBoundingClientRect().top,skyplotWidth:document.querySelector('.pvt-skyplot').getBoundingClientRect().width};
  });
  assert(Math.abs(layout.summaryTop-layout.skyTop)<1,'Position summary and skyplot share a row');
  assert(layout.mapTop>=layout.overviewBottom,'Map is below the summary and skyplot');
  assert(layout.skyplotWidth<=240,'Skyplot remains compact');
  await page.setViewportSize({width:700,height:850});
  const narrowLayout=await page.evaluate(()=>{
   const overview=document.querySelector('.pvt-overview'),summary=overview.children[0].getBoundingClientRect(),sky=overview.children[1].getBoundingClientRect();
   return {summaryBottom:summary.bottom,skyTop:sky.top,skyBottom:sky.bottom,mapTop:document.querySelector('[data-pvt-map]').parentElement.getBoundingClientRect().top};
  });
  assert(narrowLayout.skyTop>=narrowLayout.summaryBottom&&narrowLayout.mapTop>=narrowLayout.skyBottom,'Narrow screens stack the summary, skyplot and map');
  await page.setViewportSize({width:1100,height:850});
  await page.screenshot({path:path.resolve('build/pvt-geometry-preview.png'),fullPage:true});
  await page.evaluate(()=>{window.geometryWorkspace.setView('tracking');window.geometryWorkspace.update(window.geometrySnapshot);});
  const channelRows=page.locator('.channel-list tbody tr');
  const stages=async row=>{
   const cells=await row.locator('td').allTextContents();
   return [cells[3],cells[4],cells[9],cells[11]];
  };
  assert(await page.locator('#trackingView [data-beidou-availability]').isVisible());
  assert.deepEqual(await stages(channelRows.nth(1)),['Confirmed','Tracking','—','Used in PVT'],
    'Historical telemetry can still match constellation, PRN and continuity epoch');
  await page.evaluate(()=>{
   const t=window.geometrySnapshot.status.tracking[1];
   t.tracking_start_sample='10';
   t.valid_subframes='1';
   t.tow_valid=false;
   window.geometryWorkspace.update(window.geometrySnapshot);
  });
  assert.deepEqual(await stages(channelRows.nth(1)),['Confirmed','Tracking','Decoded','—'],
    'An old fix cannot be attributed to a replacement assignment in the same epoch');
  await page.evaluate(()=>{
   const t=window.geometrySnapshot.status.tracking[1];
   t.tracking_start_sample='9';
   t.epoch='2';
   window.geometryWorkspace.update(window.geometrySnapshot);
  });
  assert.deepEqual(await stages(channelRows.nth(1)),['Confirmed','Tracking','Decoded','—'],
    'A fix from before a gap cannot supply PVT membership');
  assert.equal(await page.locator('.channel-cn0 meter').count(),2);
  assert.equal(await page.locator('.channel-cn0 meter').nth(1).getAttribute('value'),'42.5');
  assert.equal(await page.locator('.workspace-nav-dot').count(),0,'No ambiguous module status lights');
  const positions=await page.locator('.channel-list tbody tr').first().locator('td').evaluateAll(cells=>cells.map(c=>c.getBoundingClientRect().x));
  await page.evaluate(()=>{window.geometrySnapshot.status.tracking[0].constellation=1;window.geometryWorkspace.update(window.geometrySnapshot);});
  assert.deepEqual(await page.locator('.channel-list tbody tr').first().locator('td').evaluateAll(cells=>cells.map(c=>c.getBoundingClientRect().x)),positions,'Unassigned does not shift columns');
  await page.evaluate(()=>{window.geometryWorkspace.setView('pvt');window.geometrySnapshot.status.pvt={available:false};window.geometryWorkspace.update(window.geometrySnapshot);});
  assert.equal(await page.locator('[data-pvt-sky] circle').count(),0,'No fabricated sky positions without a fix');
  await page.evaluate(()=>window.geometryWorkspace.setView('tracking'));
  assert.equal(await page.getByRole('button',{name:'Preview BeiDou B1C'}).count(),0);
  await page.setViewportSize({width:1100,height:850});
  await page.evaluate(()=>{
   const snapshot=window.geometrySnapshot;
   const t=snapshot.status.tracking[1];
   const side={offset_chips:0.25,i:0.01,q:0.02};
   const first={...t,sample_rate_hz:1000,first_sample:'29996',end_sample:'30000',code_rate_hz:1023000,
    taps:[{...side,offset_chips:-0.25},{offset_chips:0,i:0.002,q:0.06},side]};
   snapshot.status.tracking[1]=first;
   window.geometryWorkspace.update(snapshot);
   snapshot.status.tracking[1]={...first,first_sample:'30000',end_sample:'30004',taps:[first.taps[0],{offset_chips:0,i:0.003,q:-0.06},side]};
   window.geometryWorkspace.update(snapshot);
  });
  await page.evaluate(()=>window.geometryWorkspace.setView('tracking'));
  await channelRows.nth(1).getByRole('link').click();
  await page.getByRole('heading',{name:'Signed prompt correlator versus time'}).waitFor();
  const signedPrompt=page.locator('[data-tracking-prompt]');
  assert.match(await signedPrompt.textContent(),/Prompt Q \(ADC units\/sample\)/);
  const y=await signedPrompt.locator('circle').evaluateAll(dots=>dots.map(dot=>Number(dot.getAttribute('cy'))));
  assert.equal(y.length,2);
  assert(y[0]<100&&y[1]>150,'Both lobes of a vertical scatter appear with their Q sign');
  await page.evaluate(()=>window.geometryWorkspace.setView('tracking'));await delay(100);tiles=0;
  await page.evaluate(async()=>{
   const {createPvtMap,mercator}=await import('/pvt-map.js');
   if(mercator(0,0,2).some(v=>Math.abs(v-512)>1e-10))throw Error('Mercator origin');
   document.querySelector('main').replaceChildren();
   const root=document.createElement('section');document.querySelector('main').append(root);
   window.testMap=createPvtMap(root);
   window.testFix={available:true,latitude_deg:45.07,longitude_deg:7.69,epoch:'1',sample:'100'};
   window.testMap.update(window.testFix,'1',true);
  });
  await delay(150);assert.equal(tiles,0,'hidden map does not request tiles');
  await page.evaluate(()=>window.testMap.setVisible(true));
  await until(page,()=>document.querySelector('canvas').dataset.latitude==='45.07');
  assert(tiles>0&&tiles<30,'only viewport tiles load');
  await page.evaluate(()=>{window.testMap.update({...window.testFix,sample:'200',longitude_deg:7.6901},'1',true);});
  await until(page,()=>document.querySelector('canvas').dataset.points==='2');
  await page.evaluate(()=>window.testMap.update({available:false},'1',true));
  await until(page,()=>document.querySelector('[data-map-status]').textContent.startsWith('Last position'));
  assert.equal(await page.locator('canvas').getAttribute('data-points'),'2');
  await page.locator('[data-map-in]').click();await page.locator('canvas').focus();await page.keyboard.press('ArrowRight');
  assert.equal(await page.locator('[data-map-follow]').getAttribute('aria-pressed'),'false');
  await page.locator('[data-map-follow]').click();
  assert.equal(await page.locator('[data-map-follow]').getAttribute('aria-pressed'),'true');
  await page.screenshot({path:path.resolve('build/pvt-map-preview.png'),fullPage:true});
  await page.evaluate(()=>window.testMap.update(null,'2',false));
  await delay(100);assert.equal(await page.locator('canvas').getAttribute('data-latitude'),null,'new run clears stale position');
  await fetch(base+'/api/v1/shutdown',{method:'POST',headers:{'Content-Type':'application/json','X-NGene-Token':bootstrap.token},body:'{}'});
  console.log('PASS: embedded PVT map, projection, hidden-view requests, trace, loss, run reset and keyboard/pointer controls (mocked tiles).');
 } finally {if(browser)await browser.close();child.kill();}
})().catch(e=>{console.error(e);process.exitCode=1;});
