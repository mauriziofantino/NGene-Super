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
  for(const name of ['module-parameters.js','frequency-units.js','waterfall-relief.js','acquisition-search-space.js','acquisition-table.js','pvt-map.js'])assets.set('/'+name,[name,'text/javascript']);
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
  let browser,liveSocket,liveTimer;
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
    if(process.env.NGENE_MOTION_TEST==='1'){
      await page.evaluate(async()=>{
        const {renderAnalysis}=await import('/app.js');document.getElementById('waterfallRate').value='30';
        const stats={mean:0,std:10,rms:10,min:-32,max:32,clipping_count:0,zero_count:0};
        window.publishMotionFrame=sequence=>{
          const dbfs=Array(256).fill(-65);
          if(sequence===40)for(let k=117;k<=139;k++)dbfs[k]=0;
          if(sequence>80){dbfs[12]=sequence%2?-200:-65;dbfs[16]=sequence%2?20:-30;}
          let checksum=2166136261;
          for(const value of dbfs){let word=Math.floor(value*1000+0.5)>>>0;for(let b=0;b<4;b++){checksum=Math.imul(checksum^(word&255),16777619)>>>0;word>>>=8;}}
          renderAnalysis({sequence:String(sequence),sequence_id:String(sequence),revision:'1',epoch:'1',produced_seconds:sequence*.3,first_sample:String(sequence*100000),sample_rate_hz:100000,fft_size:256,bin_hz:390.625,enbw_hz:585.9375,dbfs,integrity_ok:true,nan_count:0,inf_count:0,checksum,telemetry_checksum:checksum,min:Math.min(...dbfs),max:Math.max(...dbfs),mean:dbfs.reduce((a,b)=>a+b,0)/256,config:{units:'dbfs_bin',window:'hann',averages:1,spectrum_source:'post',histogram_source:'post'},peak_hz:0,peak_dbfs:Math.max(...dbfs),histogram_count:0,histogram_i:[],histogram_q:[],stats_i:stats,stats_q:stats,dc_magnitude:0,histogram_first_sample:'0',histogram_age_seconds:0,clipped_i:'0',clipped_q:'0',compute_ms:0,skipped_bursts:0,contention_blocks:0,sample_backend:'motion',backend:'motion'});
        };
        for(let n=1;n<=80;n++)window.publishMotionFrame(n);
        window.sampleMotionMarker=()=>Object.fromEntries(['waterfall','waterfall3D'].map(id=>{
          const canvas=document.getElementById(id),w=canvas.width,h=canvas.height,d=Math.min(devicePixelRatio||1,2);
          const left=Math.ceil(52*d),right=Math.floor(w-18*d),top=Math.ceil(16*d),bottom=Math.floor(h-32*d);
          const middle=Math.round((top+bottom)/2),height=Math.round((bottom-top)*0.44);
          const width=right-left,pixels=canvas.getContext('2d').getImageData(left,middle-Math.floor(height/2),width,height).data;
          let sum=0,count=0;
          for(let y=0;y<height;y++)for(let x=0;x<width;x++){
            const p=4*(y*width+x),red=pixels[p],green=pixels[p+1];
            if(id==='waterfall'?red>200&&green<120:green>110){sum+=x;count++;}
          }
          return [id,count?sum/count/width:null];
        }));
      });
      await until(async()=>(await snapshot()).waterfall3D.scrollProgress===1,'Initial motion preload settled');
      await page.locator('#waterfall3D').scrollIntoViewIfNeeded();
      if(process.env.NGENE_SCREENSHOTS)await page.evaluate(()=>{
        const video=document.createElement('canvas');video.width=1280;video.height=500;
        const ctx=video.getContext('2d'),stream=video.captureStream(30),chunks=[];
        const recorder=new MediaRecorder(stream,{mimeType:'video/webm;codecs=vp8',videoBitsPerSecond:2000000});
        const timer=setInterval(()=>{ctx.drawImage(document.getElementById('waterfall'),0,0,640,500);ctx.drawImage(document.getElementById('waterfall3D'),640,0,640,500);},33);
        recorder.ondataavailable=event=>{if(event.data.size)chunks.push(event.data);};recorder.start();
        window.finishMotionRecording=()=>new Promise(resolve=>{
          recorder.onstop=()=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result.split(',')[1]);reader.readAsDataURL(new Blob(chunks,{type:'video/webm'}));};
          clearInterval(timer);recorder.stop();for(const track of stream.getTracks())track.stop();
        });
      });
      const samples=[];const sample=async()=>samples.push(await page.evaluate(()=>window.sampleMotionMarker()));
      await sample();
      for(let sequence=81;sequence<=145;sequence++){
        await page.evaluate(sequence=>window.publishMotionFrame(sequence),sequence);await sample();await delay(40);await sample();
      }
      await until(async()=>(await snapshot()).waterfall3D.scrollProgress===1,'Motion settled');await sample();
      for(const row of samples)assert(row.waterfall!==null&&row.waterfall3D!==null,'Marker visibility: '+JSON.stringify(row));
      for(let n=1;n<samples.length;n++){
        assert(samples[n].waterfall<=samples[n-1].waterfall+0.004,'2D marker reversed direction');
        assert(samples[n].waterfall3D<=samples[n-1].waterfall3D+0.004,'3D marker reversed direction');
      }
      assert(samples[0].waterfall-samples.at(-1).waterfall>0.25,'2D must actually scroll');
      assert(samples[0].waterfall3D-samples.at(-1).waterfall3D>0.25,'3D must actually scroll');
      const result=await snapshot();assert.equal(result.waterfall.gridRows,128);assert.equal(result.waterfall3D.gridRows,128);
      if(process.env.NGENE_SCREENSHOTS)fs.writeFileSync(path.join(process.env.NGENE_SCREENSHOTS,'motion-marker.json'),JSON.stringify(samples,null,2));
      if(process.env.NGENE_SCREENSHOTS)fs.writeFileSync(path.join(process.env.NGENE_SCREENSHOTS,'motion.webm'),Buffer.from(await page.evaluate(()=>window.finishMotionRecording()),'base64'));
      assert.deepEqual(errors,[]);console.log('PASS: one physical marker moves only forward in both canvases, through initial filling, overlapping arrivals, new extrema and full history.');return;
    }
    if(process.env.NGENE_LIVE_ENDPOINT){
      const endpoint=new URL(process.env.NGENE_LIVE_ENDPOINT);
      const bootstrap=await(await fetch(endpoint.origin+'/api/v1/bootstrap')).json();
      liveSocket=await new Promise((resolve,reject)=>{
        const request=http.request({hostname:endpoint.hostname,port:endpoint.port,path:'/api/v1/events?token='+encodeURIComponent(bootstrap.token),headers:{Connection:'Upgrade',Upgrade:'websocket',Origin:endpoint.origin,'Sec-WebSocket-Version':'13','Sec-WebSocket-Key':require('node:crypto').randomBytes(16).toString('base64')}});
        request.on('upgrade',(_,socket)=>{socket.on('data',()=>{});resolve(socket);});request.on('error',reject);request.on('response',r=>reject(Error('Live WebSocket: '+r.statusCode)));request.end();
      });
      liveTimer=setInterval(()=>{
        const message=Buffer.from('status:3'),mask=require('node:crypto').randomBytes(4),frame=Buffer.alloc(6+message.length);
        frame[0]=0x81;frame[1]=0x80|message.length;mask.copy(frame,2);
        for(let n=0;n<message.length;n++)frame[6+n]=message[n]^mask[n%4];liveSocket.write(frame);
      },300);
      const frames=[];let sequence='0';
      for(let n=0;n<100&&frames.length<40;n++){
        const live=await(await fetch(process.env.NGENE_LIVE_ENDPOINT+'/api/v1/status')).json();
        const frame=live.analysis;
        if(frame?.dbfs?.length&&frame.sequence_id!==sequence){
          sequence=frame.sequence_id;frames.push(frame);
          await page.evaluate(async frame=>{const {renderAnalysis}=await import('/app.js');renderAnalysis(frame);},frame);
        }
        await delay(250);
      }
      assert(frames.length>=8,'Live source did not publish enough PSD frames');
      await until(async()=>(await snapshot()).waterfall3D.scrollProgress===1,'Live scrolling settled');
      const state=await snapshot();assert.equal(state.waterfall3D.style,'mesh');assert(state.waterfall3D.triangles>0);
      assert.deepEqual(errors,[]);
      const output=process.env.NGENE_SCREENSHOTS||'build';
      fs.writeFileSync(path.join(output,'live-frames.json'),JSON.stringify(frames));
      await page.locator('#spectrumPanel').screenshot({path:path.join(output,'live-panels.png'),style:'.workspace-topbar{visibility:hidden!important}'});
      await page.locator('#waterfall3D').screenshot({path:path.join(output,'live-3d.png')});
      console.log('PASS: live SDR frames through updated UI, genuine 3D mesh, scrolling and colour bars. '+output);
      return;
    }
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
    const moving=await page.evaluate(async()=>{
      const {diagnosticSnapshot}=await import('/app.js');
      return {state:diagnosticSnapshot(),image:document.getElementById('waterfall3D').toDataURL()};
    });
    await until(async()=>(await snapshot()).waterfall3D.scrollProgress===1,'3D scroll did not finish');
    assert.deepEqual((await snapshot()).history,moving.state.history,'Animation must not add observations');
    if(moving.state.waterfall3D.scrollProgress<1)assert(await page.locator('#waterfall3D').evaluate(canvas=>canvas.toDataURL())!==moving.image,'3D history must visibly scroll');
    await page.locator('#pausePlot').click();
    const original=await snapshot();assert.equal(original.history.length,128);
    await page.evaluate(async()=>{const {renderAnalysis}=await import('/app.js');renderAnalysis({...window.reliefFixture,sequence:'129',sequence_id:'129',produced_seconds:39});});
    assert.deepEqual((await snapshot()).history,original.history,'Paused input changed shared history');
    const registered=async()=>{
      const state=await snapshot();
      for(const key of ['rows','columns','newest','oldest'])assert.equal(state.waterfall[key],state.waterfall3D[key],'Synchronized '+key);
      assert.equal(state.waterfall3D.style,'mesh');
      if(state.history.length>1)assert(state.waterfall3D.triangles>0,'3D must render actual triangles with depth');
    };
    await registered();
    await page.locator('#showWaterfall3D').uncheck();
    await page.locator('#showWaterfall3D').check();
    await until(async()=>(await snapshot()).work.waterfall3D>original.work.waterfall3D,'Reenabled paused relief did not redraw');
    await registered();
    const spectrum=page.locator('#spectrum'),waterfall=page.locator('#waterfall');
    assert((await spectrum.boundingBox()).height>=400,'Spectrum must already be large');
    assert((await waterfall.boundingBox()).height>=440,'2D waterfall must already be large');
    assert((await page.locator('#waterfall3D').boundingBox()).height>=480,'3D waterfall must already be large');
    assert.equal(await page.locator('#spectrumHeight,#waterfallHeight,#waterfall3DHeight,.plot-resize').count(),0,'Height sliders and resize handles must be removed');
    assert.equal(await page.locator('#expandPlots,#frequencyMin,#frequencyMax,#resetFrequency').count(),0,'Zoom controls must be removed');
    assert.equal(await page.locator('#spectrumPanel').getAttribute('aria-modal'),null);
    const fixed=await snapshot();
    assert(fixed.waterfall3D.rows>1&&fixed.waterfall3D.rows<=2048);
    assert(fixed.waterfall3D.columns>1&&fixed.waterfall3D.columns<=1024);
    assert.equal(await page.locator('#waterfallMode,#waterfallOrbit').count(),0);
    assert.equal(fixed.waterfall3D.elevation,60,'Fixed frequency-axis tilt');
    assert.equal(fixed.waterfall3D.azimuth,0,'No lateral rotation of the time axis');
    await registered();
    if(process.env.NGENE_SCREENSHOTS){
      await page.locator('#spectrumPanel').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'relief-desktop.png'),style:'.workspace-topbar{visibility:hidden!important}'});
      await page.locator('#waterfall3D').screenshot({path:path.join(process.env.NGENE_SCREENSHOTS,'relief-detail.png')});
    }
    await page.setViewportSize({width:390,height:844});
    await page.locator('#waterfall3D').scrollIntoViewIfNeeded();
    assert((await spectrum.boundingBox()).height>=300,'Mobile spectrum must remain observable');
    assert((await waterfall.boundingBox()).height>=360,'Mobile waterfall must remain observable');
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
    console.log('PASS: large default plots, no zoom/height controls, increased relief tilt, synchronized history, shared clear, paused reenable and mobile visibility under production CSP.');
  }finally{clearInterval(liveTimer);liveSocket?.destroy();if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
