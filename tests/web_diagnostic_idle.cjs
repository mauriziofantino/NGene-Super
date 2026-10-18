/* Browser diagnostic CPU-work regression. Requires Node >=18 and Playwright. */
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const root=path.resolve(__dirname,'..');
const policy="default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self' ws://127.0.0.1:* ws://localhost:*; frame-ancestors 'none'; base-uri 'none'";
const assets=new Map([['/',['index.html','text/html']],['/app.js',['app.js','text/javascript']],['/workspace.js',['workspace.js','text/javascript']],['/receiver-config.js',['receiver-config.js','text/javascript']],['/style.css',['style.css','text/css']],['/ngene-logo.svg',['ngene-logo.svg','image/svg+xml']]]);
const profileIni=fs.readFileSync(path.join(root,'config','replay.ini'),'utf8');
const fixtureAnalysis={spectrum_enabled:true,histogram_enabled:true,units:'dbfs_bin',window:'hann',averages:1,spectrum_source:'post',histogram_source:'post'};
let fixtureRevision=1,configurationDelay=0,pendingConfigurations=0;
const configurationRequests=[];
const profile=()=>`${profileIni}\n[analysis]\n${Object.entries(fixtureAnalysis).map(([key,value])=>`${key}=${value}`).join('\n')}\n`;
const server=http.createServer((request,response)=>{
  response.setHeader('Content-Security-Policy',policy);
  const asset=assets.get(request.url);
  if(asset){response.setHeader('Content-Type',asset[1]);response.end(fs.readFileSync(path.join(root,'web',asset[0])));return;}
  response.setHeader('Content-Type','application/json');
  if(request.url==='/api/v1/bootstrap')response.end(JSON.stringify({token:'idle-fixture',os:'fixture',architecture:'browser',version:'test'}));
  else if(request.url==='/api/v1/config')response.end(JSON.stringify({revision:String(fixtureRevision),ini:profile()}));
  else if(request.url==='/api/v1/analysis/config'&&request.method==='GET')response.end(JSON.stringify({revision:String(fixtureRevision),analysis:fixtureAnalysis}));
  else if(request.url==='/api/v1/analysis/config'&&request.method==='POST'){
    let body='';request.on('data',chunk=>body+=chunk);request.on('end',()=>{
      const input=JSON.parse(body);configurationRequests.push(input);pendingConfigurations++;
      setTimeout(()=>{
        Object.assign(fixtureAnalysis,Object.fromEntries(Object.entries(input).filter(([key])=>key!=='revision')));fixtureRevision++;
        pendingConfigurations--;response.end(JSON.stringify({revision:String(fixtureRevision),analysis:fixtureAnalysis}));
      },configurationDelay);
    });
  }
  else {response.statusCode=404;response.end('{}');}
});
function canvasCalls(work,...ids){return ids.reduce((sum,id)=>sum+Object.values(work.canvases[id]||{}).reduce((total,n)=>total+n,0),0);}
function noPsdWork(work,message){
  assert.equal(work.psdReads,0,`${message}: read PSD values`);
  assert.equal(work.projectionCalls,0,`${message}: projected a 3D waterfall`);
  assert.equal(canvasCalls(work,'spectrum','waterfall','offscreen'),0,`${message}: touched spectrum/waterfall canvas storage or drawing APIs`);
}
function noDiagnosticWork(work,message){
  noPsdWork(work,message);
  assert.equal(work.histogramReads,0,`${message}: read histogram values`);
  assert.equal(canvasCalls(work,'histogramI','histogramQ'),0,`${message}: touched histogram canvases`);
}
(async()=>{
  let browser;
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  try{
    browser=await chromium.launch({headless:true,...(process.env.BROWSER_EXECUTABLE?{executablePath:process.env.BROWSER_EXECUTABLE}:{}),args:['--no-sandbox']});
    const page=await browser.newPage({viewport:{width:1512,height:1100}}),errors=[];
    page.on('pageerror',error=>errors.push(error.message));
    await page.addInitScript(()=>{
      // All receiver traffic is excluded: this test measures browser presentation work.
      window.__diagnosticRequests=[];
      window.WebSocket=class {
        static OPEN=1;readyState=0;
        constructor(){window.__diagnosticSocket=this;}
        close(){}
        send(message){window.__diagnosticRequests.push({message,time:performance.now()});}
      };
      window.__resetDiagnosticWork=()=>{window.__diagnosticWork={canvases:{},psdReads:0,histogramReads:0,projectionCalls:0};};
      window.__resetDiagnosticWork();
      const count=(canvas,operation)=>{
        const id=canvas?.id||'offscreen',calls=window.__diagnosticWork.canvases;
        (calls[id]??={})[operation]=(calls[id][operation]||0)+1;
      };
      const canvasPrototype=HTMLCanvasElement.prototype;
      const getContext=canvasPrototype.getContext;
      canvasPrototype.getContext=function(...args){count(this,'getContext');return getContext.apply(this,args);};
      for(const name of ['width','height']){
        const descriptor=Object.getOwnPropertyDescriptor(canvasPrototype,name);
        Object.defineProperty(canvasPrototype,name,{...descriptor,set(value){count(this,name);descriptor.set.call(this,value);}});
      }
      for(const name of Object.getOwnPropertyNames(CanvasRenderingContext2D.prototype)){
        const descriptor=Object.getOwnPropertyDescriptor(CanvasRenderingContext2D.prototype,name);
        if(name==='constructor'||typeof descriptor.value!=='function')continue;
        Object.defineProperty(CanvasRenderingContext2D.prototype,name,{...descriptor,value:function(...args){count(this.canvas,name);return descriptor.value.apply(this,args);}});
      }
      // The 3D renderer computes camera cosines before projecting its first vertex.
      const cosine=Math.cos;
      Math.cos=function(value){window.__diagnosticWork.projectionCalls++;return cosine(value);};
      const watched=(values,counter)=>new Proxy(values,{get(target,key,receiver){
        if(typeof key==='string'&&/^\d+$/.test(key))window.__diagnosticWork[counter]++;
        return Reflect.get(target,key,receiver);
      }});
      window.__makeDiagnosticFrame=(psdSequence,histogramSequence=psdSequence,options={})=>{
        const dbfs=Array.from({length:256},(_,bin)=>-85+12*Math.sin(bin/14+psdSequence/9)+(bin===135?45:0));
        let checksum=2166136261;
        for(const value of dbfs){let word=Math.floor(value*1000+0.5)>>>0;for(let byte=0;byte<4;byte++){checksum=Math.imul(checksum^(word&255),16777619)>>>0;word>>>=8;}}
        const histogram=Array.from({length:16},(_,bin)=>100+((bin+histogramSequence)%16)*20);
        const stats={mean:0,std:10,rms:10,min:-32,max:32,clipping_count:0,zero_count:0};
        return {
          sequence:String(Math.max(psdSequence,histogramSequence)),sequence_id:String(psdSequence),histogram_sequence:String(histogramSequence),
          revision:options.revision||'1',produced_seconds:options.produced_seconds??psdSequence*0.3,first_sample:String(psdSequence*100000),epoch:'1',
          sample_rate_hz:100000,fft_size:256,bin_hz:390.625,enbw_hz:585.9375,
          dbfs:watched(dbfs,'psdReads'),integrity_ok:true,nan_count:0,inf_count:0,checksum,telemetry_checksum:checksum,
          min:Math.min(...dbfs),max:Math.max(...dbfs),mean:dbfs.reduce((sum,x)=>sum+x,0)/dbfs.length,
          config:{spectrum_enabled:true,histogram_enabled:true,units:'dbfs_bin',window:'hann',averages:1,spectrum_source:'post',histogram_source:'post',...options.config},
          peak_hz:2734.375,peak_dbfs:dbfs[135],histogram_count:histogram.reduce((sum,n)=>sum+n,0),
          histogram_i:watched(histogram,'histogramReads'),histogram_q:watched([...histogram].reverse(),'histogramReads'),stats_i:stats,stats_q:stats,
          dc_magnitude:0,histogram_first_sample:String(histogramSequence*16384),histogram_epoch:'1',histogram_age_seconds:0,
          histogram_min:-128,histogram_max:127,clipped_i:'0',clipped_q:'0',compute_ms:0.2,
          skipped_bursts:0,contention_blocks:0,sample_backend:'fixture',backend:'fixture'
        };
      };
    });
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.waitForFunction(()=>document.getElementById('configBadge').textContent==='r1');
    const settle=()=>page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))));
    const reset=()=>page.evaluate(()=>window.__resetDiagnosticWork());
    const work=()=>page.evaluate(()=>window.__diagnosticWork);
    const snapshot=()=>page.evaluate(async()=>(await import('/app.js')).diagnosticSnapshot());
    const frame=(psd,hist=psd,options={})=>page.evaluate(async({psd,hist,options})=>{
      const {renderAnalysis}=await import('/app.js');renderAnalysis(window.__makeDiagnosticFrame(psd,hist,options));
    },{psd,hist,options:{revision:String(fixtureRevision),...options,config:{...fixtureAnalysis,...options.config}}});
    const measure=async action=>{await reset();await action();await settle();return work();};
    const waitConfigurations=async()=>{
      await page.waitForFunction(()=>!document.getElementById('showSpectrum').disabled&&!document.getElementById('showWaterfall').disabled);
      while(pendingConfigurations)await new Promise(resolve=>setTimeout(resolve,5));
      await settle();
    };
    const setPlot=async(id,checked)=>{
      const desired=await page.evaluate(({id,checked})=>['showSpectrum','showWaterfall'].some(name=>name===id?checked:document.getElementById(name).checked),{id,checked});
      const requests=configurationRequests.length,changed=fixtureAnalysis.spectrum_enabled!==desired;
      const response=changed?page.waitForResponse(response=>response.url().endsWith('/api/v1/analysis/config')&&response.request().method()==='POST'):null;
      await page.locator(`#${id}`).setChecked(checked);
      if(changed){
        await response;
        assert.equal(configurationRequests.length,requests+1,'A shared runtime enable transition must send one configuration request');
        for(const [key,value] of Object.entries(fixtureAnalysis))if(key!=='spectrum_enabled')assert.equal(configurationRequests.at(-1)[key],value,'A header enable overwrote unrelated analysis parameters');
        assert.equal(configurationRequests.at(-1).spectrum_enabled,desired,'A header enable sent the wrong runtime FFT state');
      }
      await waitConfigurations();
      if(!changed)assert.equal(configurationRequests.length,requests,'Toggling one plot while another is enabled needlessly reconfigured the FFT');
    };
    const histograms=()=>page.evaluate(()=>['histogramInfo','statsI','statsQ','dcMagnitude','histogramRails'].map(id=>document.getElementById(id).textContent));
    await page.locator('#waterfallMode').selectOption('3d');await settle();
    let measured=await measure(async()=>{for(let sequence=1;sequence<=8;sequence++)await frame(sequence);});
    assert(measured.psdReads>0&&measured.histogramReads>0,'Fixture did not exercise PSD and histogram processing');
    assert(canvasCalls(measured,'spectrum')>0&&canvasCalls(measured,'histogramI','histogramQ')>0,'Fixture did not exercise plot drawing');
    assert(measured.projectionCalls>0&&canvasCalls(measured,'waterfall')>0,'Fixture did not exercise the 3D renderer');
    const original=await snapshot();assert.equal(original.history.length,8);

    measured=await measure(()=>frame(8));
    noDiagnosticWork(measured,'An identical status snapshot');
    assert.deepEqual((await snapshot()).history,original.history);
    measured=await measure(()=>frame(8,9));
    noPsdWork(measured,'A histogram-only update');
    assert(measured.histogramReads>0&&canvasCalls(measured,'histogramI','histogramQ')>0,'A new histogram was not rendered');
    assert.deepEqual((await snapshot()).history,original.history,'Histogram-only updates appended PSD history');
    measured=await measure(()=>frame(8,9));
    noDiagnosticWork(measured,'An identical histogram-only snapshot');

    await page.locator('#showHistogram').uncheck();await settle();
    const inactiveHistogram=await histograms();
    measured=await measure(()=>frame(9,10));
    assert(measured.psdReads>0&&measured.projectionCalls>0,'Disabling histograms disabled the spectrum');
    assert.equal(measured.histogramReads,0,'A disabled histogram read its values');
    assert.equal(canvasCalls(measured,'histogramI','histogramQ'),0,'A disabled histogram touched its canvases');
    assert.deepEqual(await histograms(),inactiveHistogram,'A disabled histogram rebuilt its statistics');
    const beforeRateLimited=await snapshot();
    measured=await measure(()=>frame(10,10,{produced_seconds:2.71}));
    assert(measured.psdReads>0&&canvasCalls(measured,'spectrum')>0,'A rate-limited waterfall prevented the new spectrum from rendering');
    assert.equal(measured.projectionCalls,0,'A PSD inside the waterfall rate limit projected an unchanged mesh');
    assert.equal(canvasCalls(measured,'waterfall','offscreen'),0,'A PSD inside the waterfall rate limit redrew unchanged history');
    assert.deepEqual((await snapshot()).history,beforeRateLimited.history);

    await setPlot('showSpectrum',false);
    assert(await page.locator('#spectrumBody').isHidden(),'A disabled spectrum still occupies its plot area');
    assert(await page.locator('#spectrum').isHidden(),'Disabling the spectrum leaves its old image visible');
    assert(await page.locator('#showSpectrum').isVisible(),'The disabled spectrum lost its enable control');
    assert(await page.locator('#waterfallBody').isVisible(),'Disabling the spectrum also hid the enabled waterfall');
    const waterfallOnly=await snapshot();
    measured=await measure(()=>frame(11));
    assert(measured.psdReads>0&&measured.projectionCalls>0,'Waterfall-only mode did not process and project new PSD data');
    assert.equal(canvasCalls(measured,'spectrum'),0,'Waterfall-only mode touched the disabled spectrum canvas');
    assert.equal((await snapshot()).history.length,waterfallOnly.history.length+1,'Waterfall-only mode did not append history');

    await setPlot('showSpectrum',true);await setPlot('showWaterfall',false);
    assert(await page.locator('#waterfallBody').isHidden(),'A disabled waterfall still occupies its plot area');
    assert(await page.locator('#waterfall').isHidden(),'Disabling the waterfall leaves its old image visible');
    assert(await page.locator('#showWaterfall').isVisible(),'The disabled waterfall lost its enable control');
    assert(await page.locator('#spectrumBody').isVisible(),'Disabling the waterfall also hid the enabled spectrum');
    const spectrumOnly=await snapshot();
    measured=await measure(()=>frame(12));
    assert(measured.psdReads>0&&canvasCalls(measured,'spectrum')>0,'Spectrum-only mode did not render new PSD data');
    assert.equal(measured.projectionCalls,0,'Spectrum-only mode projected a hidden waterfall');
    assert.equal(canvasCalls(measured,'waterfall','offscreen'),0,'Spectrum-only mode touched hidden waterfall storage or drawing APIs');
    assert.deepEqual((await snapshot()).history,spectrumOnly.history,'Spectrum-only mode accumulated waterfall history');

    configurationDelay=200;
    measured=await measure(async()=>{
      const pending=setPlot('showSpectrum',false);
      await page.waitForFunction(()=>document.getElementById('showSpectrum').disabled);
      await frame(13);await frame(14); // Old enabled telemetry can arrive before the POST reply.
      await pending;
    });
    noDiagnosticWork(measured,'Last plot disabled while its runtime configuration request is pending');
    configurationDelay=0;
    assert(await page.locator('#showSpectrum').isVisible()&&await page.locator('#showWaterfall').isVisible(),'Disabled plots lost their enable controls');
    assert(await page.locator('#spectrumBody').isHidden()&&await page.locator('#waterfallBody').isHidden(),'Both disabled plots must collapse');
    const disabled=await snapshot();
    measured=await measure(async()=>{
      for(let sequence=15;sequence<=16;sequence++)await frame(sequence);
      await page.evaluate(()=>{
        for(const mode of ['2d','3d']){const input=document.getElementById('waterfallMode');input.value=mode;input.dispatchEvent(new Event('change'));}
        document.getElementById('colourMax').dispatchEvent(new Event('change'));
        document.getElementById('waterfallRight').click();
      });
      await page.setViewportSize({width:1480,height:1000});
    });
    noDiagnosticWork(measured,'Both displays disabled, including mode/scale/camera/resize events');
    assert.equal((await snapshot()).sequence,disabled.sequence);
    assert.deepEqual((await snapshot()).history,disabled.history,'Disabled displays accumulated PSD history');

    await page.locator('#showHistogram').check();await settle();
    measured=await measure(()=>frame(17));
    noPsdWork(measured,'Spectrum/waterfall disabled while histograms remain enabled');
    assert(measured.histogramReads>0&&canvasCalls(measured,'histogramI','histogramQ')>0,'Histogram-only mode did not render');
    assert.deepEqual((await snapshot()).history,disabled.history);
    await page.evaluate(()=>document.getElementById('pausePlot').click());await settle();
    measured=await measure(()=>frame(18));
    noDiagnosticWork(measured,'A new snapshot while plots are paused');

    measured=await measure(()=>setPlot('showSpectrum',true));
    assert(await page.locator('#spectrumBody').isVisible(),'A paused first enable did not reactivate runtime FFT availability');
    assert.equal(canvasCalls(measured,'waterfall','offscreen'),0,'Reenabling only the spectrum redrew the disabled waterfall');
    assert.equal(measured.projectionCalls,0,'Reenabling only the spectrum projected the disabled waterfall');
    measured=await measure(()=>setPlot('showWaterfall',true));
    assert.equal(measured.canvases.waterfall?.getContext,1,'Reenabling a paused waterfall must redraw cached history once');
    assert(measured.projectionCalls>0,'Reenabling the paused 3D waterfall did not project cached history');
    assert.equal((await snapshot()).history.length,0,'A runtime FFT configuration change retained old PSD history');
    await page.locator('#pausePlot').click();await frame(18);await settle();await page.locator('#pausePlot').click();
    const pausedCache=await snapshot();assert.equal(pausedCache.history.length,1,'Paused interaction fixture has no fresh PSD history');
    const beforeRotation=await snapshot();
    measured=await measure(()=>page.locator('#waterfallRight').click());
    assert(measured.projectionCalls>0&&canvasCalls(measured,'waterfall')>0,'A paused 3D camera could not rotate');
    assert.equal(canvasCalls(measured,'spectrum','histogramI','histogramQ'),0,'A paused camera redrew unrelated plots');
    assert.notEqual((await snapshot()).waterfall.camera.yaw,beforeRotation.waterfall.camera.yaw);
    assert.deepEqual((await snapshot()).history,beforeRotation.history,'Paused camera rotation mutated PSD history');

    // A draw already queued for the next animation frame must recheck each
    // enable: hiding a plot can itself deliver a late ResizeObserver callback.
    measured=await measure(()=>page.evaluate(()=>{
      document.getElementById('waterfallRight').click();
      const input=document.getElementById('showWaterfall');input.checked=false;input.dispatchEvent(new Event('change'));
    }));
    assert.equal(measured.projectionCalls,0,'A queued camera draw projected a waterfall after it was disabled');
    assert.equal(canvasCalls(measured,'waterfall','offscreen'),0,'A queued draw touched the disabled waterfall');
    await setPlot('showWaterfall',true);
    measured=await measure(()=>page.evaluate(()=>{
      const height=document.getElementById('spectrumHeight');height.value=300;height.dispatchEvent(new Event('input'));
      const input=document.getElementById('showSpectrum');input.checked=false;input.dispatchEvent(new Event('change'));
    }));
    assert.equal(canvasCalls(measured,'spectrum'),0,'A queued resize draw touched the spectrum after it was disabled');
    await setPlot('showSpectrum',true);
    assert.deepEqual((await snapshot()).history,beforeRotation.history,'Queued redraw or enable controls mutated paused history');

    fixtureAnalysis.spectrum_enabled=false;fixtureRevision++;
    measured=await measure(()=>frame(18));
    noPsdWork(measured,'Worker FFTs disabled while plots are paused');
    assert(await page.locator('#spectrumBody').isHidden()&&await page.locator('#waterfallBody').isHidden(),'Disabling worker FFTs while paused left the old plots visible');
    assert(!(await page.locator('#showSpectrum').isChecked())&&!(await page.locator('#showWaterfall').isChecked()),'Disabled runtime computation left enabled header switches');
    fixtureAnalysis.spectrum_enabled=true;fixtureRevision++;
    await frame(18);await settle();
    assert(await page.locator('#spectrumBody').isVisible()&&await page.locator('#waterfallBody').isVisible(),'Reenabling worker FFTs while paused did not restore plot availability');
    assert.deepEqual((await snapshot()).history,beforeRotation.history,'A paused configuration update changed PSD history');

    await page.locator('#pausePlot').click();await frame(19);await settle();
    const beforeHidden=await snapshot();
    measured=await measure(async()=>{
      await page.locator('#showConfiguration').click();
      await frame(20);
      await page.evaluate(()=>document.getElementById('waterfallRight').click());
      await page.setViewportSize({width:1460,height:1020});
    });
    noDiagnosticWork(measured,'The configuration view');
    assert.deepEqual((await snapshot()).history,beforeHidden.history,'Configuration view accumulated hidden history');
    await page.locator('#showObservation').click();await settle();
    measured=await measure(async()=>{
      await page.evaluate(()=>{window.__fixtureHidden=true;Object.defineProperty(document,'hidden',{configurable:true,get:()=>window.__fixtureHidden});document.dispatchEvent(new Event('visibilitychange'));});
      await frame(21);
      await page.evaluate(()=>{document.getElementById('waterfallRight').click();document.getElementById('colourMax').dispatchEvent(new Event('change'));});
      await page.setViewportSize({width:1450,height:1010});
    });
    noDiagnosticWork(measured,'A hidden browser document');
    assert.deepEqual((await snapshot()).history,beforeHidden.history,'Hidden document accumulated PSD history');
    await page.evaluate(()=>{window.__fixtureHidden=false;document.dispatchEvent(new Event('visibilitychange'));});await settle();

    // A status can still contain old PSD bins immediately after disabling worker FFTs.
    // The applied configuration must prevent processing even when the display checkbox is on.
    fixtureAnalysis.spectrum_enabled=false;fixtureRevision++;
    measured=await measure(()=>frame(22));
    assert(!(await page.locator('#showSpectrum').isChecked()));
    assert(!(await page.locator('#showWaterfall').isChecked()));
    assert(await page.locator('#spectrumBody').isHidden()&&await page.locator('#waterfallBody').isHidden(),'Disabling worker FFTs left a frozen spectrum/waterfall visible');
    assert(await page.locator('#showSpectrum').isVisible()&&await page.locator('#showWaterfall').isVisible(),'Worker-disabled plots lost their local enable controls');
    noPsdWork(measured,'Applied spectrum_enabled=false with a previously enabled display');
    assert(measured.histogramReads>0,'Disabling FFTs disabled histogram processing');
    const appliedDisabled=await snapshot();
    measured=await measure(async()=>{
      await frame(23);
      await page.evaluate(()=>document.getElementById('waterfallRight').click());
      await page.setViewportSize({width:1430,height:1010});
    });
    noPsdWork(measured,'Disabled worker FFTs receiving new snapshots and redraw requests');
    assert.deepEqual((await snapshot()).history,appliedDisabled.history,'Disabled worker FFTs accumulated stale PSD history');
    fixtureAnalysis.spectrum_enabled=true;fixtureRevision++;
    measured=await measure(()=>frame(24));
    assert(measured.psdReads>0&&measured.projectionCalls>0,'Reenabling worker FFTs did not resume rendering');
    assert(await page.locator('#spectrumBody').isVisible()&&await page.locator('#waterfallBody').isVisible(),'Reenabling worker FFTs did not reveal the enabled plots');
    assert.equal((await snapshot()).history.length,1,'Reenabling a new analysis revision retained old history');

    // An explicit unsubscribe immediately after a reply must survive the 34 ms
    // request guard instead of waiting for the one-second display refresh timer.
    // One plot alone still requires the shared FFT; only disabling both removes
    // spectrum interest while retaining the independent histogram subscription.
    const pacing=await page.evaluate(()=>{
      document.getElementById('refresh').value='1000';
      const socket=window.__diagnosticSocket;socket.readyState=WebSocket.OPEN;socket.onopen();
      const initial=window.__diagnosticRequests.at(-1);
      socket.onmessage({data:'null'}); // Set response pacing without receiver telemetry.
      const requestedAt=performance.now(),count=window.__diagnosticRequests.length;
      const spectrum=document.getElementById('showSpectrum');spectrum.checked=false;spectrum.dispatchEvent(new Event('change'));
      const waterfall=document.getElementById('showWaterfall');waterfall.checked=false;waterfall.dispatchEvent(new Event('change'));
      return {initial,requestedAt,count,afterChange:window.__diagnosticRequests.length};
    });
    assert.equal(pacing.initial.message,'status:3','Pacing fixture did not subscribe to both diagnostics');
    assert.equal(pacing.afterChange,pacing.count,'An immediate unsubscribe bypassed the response pacing guard');
    await page.waitForFunction(count=>window.__diagnosticRequests.length>count,pacing.count,{polling:5,timeout:1000});
    const unsubscribe=await page.evaluate(count=>window.__diagnosticRequests[count],pacing.count);
    assert.equal(unsubscribe.message,'status:2','Disabling both plots did not unsubscribe their shared receiver FFT work');
    assert(unsubscribe.time-pacing.requestedAt<200,'The unsubscribe waited for the selected 1000 ms refresh interval');
    for(const enabled of ['showSpectrum','showWaterfall']){
      const start=await page.evaluate(id=>{
        window.__diagnosticSocket.onmessage({data:'null'});
        const count=window.__diagnosticRequests.length,input=document.getElementById(id);
        input.checked=true;input.dispatchEvent(new Event('change'));return count;
      },enabled);
      await page.waitForFunction(count=>window.__diagnosticRequests.length>count,start,{polling:5,timeout:1000});
      assert.equal(await page.evaluate(count=>window.__diagnosticRequests[count].message,start),'status:3',`${enabled} alone did not subscribe to the shared receiver FFT`);
      const end=await page.evaluate(id=>{
        window.__diagnosticSocket.onmessage({data:'null'});
        const count=window.__diagnosticRequests.length,input=document.getElementById(id);
        input.checked=false;input.dispatchEvent(new Event('change'));return count;
      },enabled);
      await page.waitForFunction(count=>window.__diagnosticRequests.length>count,end,{polling:5,timeout:1000});
      assert.equal(await page.evaluate(count=>window.__diagnosticRequests[count].message,end),'status:2',`${enabled} disable did not restore histogram-only interest`);
    }
    assert.deepEqual(errors,[]);
    console.log('PASS: independent plot enables collapse disabled images, skip their canvas/history/projection work and unsubscribe shared FFTs when both are off; disabled/hidden diagnostics avoid PSD processing; histogram-only updates, paused cached redraw/camera controls and paced subscriptions work under production CSP.');
  }finally{if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
