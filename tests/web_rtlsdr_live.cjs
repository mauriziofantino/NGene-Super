/* Explicit opt-in hardware check. Opens the connected RTL and never saves its gains. */
const fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process');
const assert=require('node:assert/strict');
if(process.env.NGENE_RTL_LIVE!=='1')throw Error('Set NGENE_RTL_LIVE=1 to exercise the connected RTL-SDR.');
const wait=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const source=process.env.NGENE_LIVE_SOURCE||'rtlsdr';assert(['rtlsdr','hackrf'].includes(source));
 const dir=fs.mkdtempSync(path.resolve('build/'+source+'-live-')),port=Number(process.env.NGENE_TEST_PORT||8138),base='http://127.0.0.1:'+port;
 const child=spawn(path.resolve(process.env.NGENE_EXECUTABLE||'build/msvc-x64-multisdr/Release/ngene.exe'),['--web','--port',String(port),'--output-dir',path.join(dir,'profile')],{stdio:['ignore','pipe','pipe']});
 let log='',token;child.stdout.on('data',x=>log+=x);child.stderr.on('data',x=>log+=x);
 const get=async route=>{if(child.exitCode!==null)throw Error('Receiver exited: '+child.exitCode);return(await fetch(base+'/api/v1/'+route)).json();};
 const post=async(route,data)=>{const r=await fetch(base+'/api/v1/'+route,{method:'POST',headers:{'Content-Type':'application/json','X-NGene-Token':token},body:JSON.stringify(data)});const d=await r.json();assert(r.ok,JSON.stringify(d));return d;};
 try{
  for(let n=0;n<100;n++){try{token=(await get('bootstrap')).token;if(token)break;}catch{}await wait(100);}
  const ini=fs.readFileSync('config/'+source+'.ini','utf8'),caps=await post('frontend/capabilities',{ini}),config=await get('config');
  console.log(JSON.stringify({dir,device:caps.device_model,serial:caps.device_serial}));
  let job=await post('frontend/calibrate',{revision:config.revision,ini});
  while(job.state==='running'&&job.tested<30){await wait(100);job=await get('frontend/calibrate');}
  const at=Date.now();await post('frontend/calibrate/cancel',{job_id:job.job_id});
  while(job.state==='running'){await wait(50);job=await get('frontend/calibrate');assert(Date.now()-at<5000,'Cancellation exceeded five seconds');}
  assert.equal(job.state,'cancelled');const stopMs=Date.now()-at;
  console.log(JSON.stringify({stop_ms:stopMs,settings:job.tested/3}));
  job=await post('frontend/calibrate',{revision:config.revision,ini});const started=Date.now();let tick=0;
  while(job.state==='running'){
   await wait(250);job=await get('frontend/calibrate');await get('status');
   if(++tick%40===0)console.log(JSON.stringify({settings:job.tested/3,seconds:(Date.now()-started)/1000}));
   assert(Date.now()-started<300000,'Full calibration exceeded five minutes');
  }
  fs.writeFileSync(path.join(dir,'calibration.json'),JSON.stringify({...job,stop_ms:stopMs,wall_seconds:(Date.now()-started)/1000},null,2));
  const expected=source==='rtlsdr'?756:/2839/.test(caps.gain_policy)?252:/2831/.test(caps.gain_policy)?192:384;
  assert.equal(job.state,'completed',job.error);assert.equal(job.tested,expected*3);assert.equal((await get('config')).revision,config.revision);
  if(process.env.NGENE_EXPECT_HIGH_LNA==='1'&&source==='hackrf')assert(job.accepted&&job.lna_gain_db>=32,'An accepted high-LNA setting must outrank low-LNA/high-VGA loading');
  const opened=await post('frontend/capabilities',{ini});assert.equal(opened.device_serial,caps.device_serial,'SDR must be released after completion');
  console.log(JSON.stringify({state:job.state,accepted:job.accepted,settings:job.tested/3,seconds:(Date.now()-started)/1000,rms:job.rms_fraction,lna:job.lna_gain_db,vga:job.vga_gain_db,amp:job.rf_amp,rf:job.rtl_gain_tenths_db,if:job.rtl_if_gain_db,dir}));
  await post('shutdown',{});
 }finally{
  if(child.exitCode===null){child.kill();await new Promise(r=>child.once('exit',r));}
  fs.writeFileSync(path.join(dir,'service.log'),log);
 }
})().catch(e=>{console.error(e);process.exitCode=1;});
