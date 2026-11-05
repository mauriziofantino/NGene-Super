/* Height projection, narrow transient preservation and shared-history contracts. */
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
(async()=>{
 const moduleSource=fs.readFileSync('web/waterfall-relief.js','utf8');
 const {paintWaterfallRelief,projectWaterfallPoint}=await import('data:text/javascript;base64,'+Buffer.from(moduleSource).toString('base64'));
 const pixels=(columns,rows)=>new Uint8ClampedArray(columns*rows*4);
 const fixture=(rows,columns,age,bin)=>Array.from({length:rows},(_,j)=>({sequence:String(rows-j),values:Object.freeze(Array.from({length:columns},(_,k)=>j===age&&k===bin?0:-100))}));
 const bright=(image,columns,row,bin)=>image[4*(row*columns+bin)+1];
 for(let age=0;age<128;age++){
   const data=pixels(64,128);paintWaterfallRelief(data,fixture(128,64,age,31),64,128,-100,0);
   const apex=Math.floor(projectWaterfallPoint(age,1,128,128));
   assert(bright(data,64,apex,31)>200,'Lost projected transient at age '+age);
   assert(projectWaterfallPoint(age,1,128,128)<projectWaterfallPoint(age,0,128,128),'Power must change geometry');
 }
 for(let bin=0;bin<1024;bin++){
   const data=pixels(1024,8);paintWaterfallRelief(data,fixture(8,1024,4,bin),1024,8,-100,0);
   assert(bright(data,1024,Math.floor(projectWaterfallPoint(4,1,8,8)),bin)>200,'Lost narrow event at bin '+bin);
 }
 const history=fixture(16,64,4,31),before=JSON.stringify(history),short=pixels(64,128),long=pixels(64,128);
 paintWaterfallRelief(short,history,64,128,-100,0);
 paintWaterfallRelief(long,[...history,...fixture(16,64,-1,-1)],64,128,-100,0);
 const apex=Math.floor(projectWaterfallPoint(4,1,128,128));
 assert.deepEqual([...short.slice(4*(apex*64+31),4*(apex*64+32))],[...long.slice(4*(apex*64+31),4*(apex*64+32))],'History growth moved an existing event');
 assert.equal(JSON.stringify(history),before,'Projection mutated the shared PSD history');
 assert.equal(short[4*(127*64)+3],0,'Unpopulated time must remain blank');
 const flat=pixels(16,8);paintWaterfallRelief(flat,Array.from({length:8},()=>({values:Array(16).fill(-50)})),16,8,-100,0);
 const baseline=Math.floor(projectWaterfallPoint(0,0.5,8,8));
 assert.equal(flat[3],0,'A flat surface must be projected below the power ceiling');
 assert(flat[4*(baseline*16)+3]>0,'Projected flat surface missing');
 // Both views use the same frequency crop and observations. 3D has a power
 // projection and screen-height raster, deliberately different from the 2D grid.
 const source=fs.readFileSync('web/app.js','utf8'),draw=source.slice(source.indexOf('function drawWaterfall(){'),source.indexOf('function drawHistogram('));
 const calls=[],canvas=()=>({getContext:()=>({createImageData:(w,h)=>({data:pixels(w,h)}),putImageData(){}})});
 const ctx={fillText(){},beginPath(){},moveTo(){},lineTo(){},stroke(){},drawImage(bitmap,...args){calls.push(args);}};
 const sandbox={paintWaterfallRelief,diagnosticMask:()=>1,plotEnabled:()=>true,surface:()=>({ctx,left:52,right:820,top:16,bottom:368}),range:()=>[-100,0],frequencyRange:()=>[-200,300],displayFrequency:x=>x,analysisSnapshot:{sample_rate_hz:1000,dbfs:Array(64),config:{units:"dbfs_bin"}},historyRows:()=>128,waterfallHistory:history,plotWork:{waterfall:0,waterfall3D:0},waterfallPalette:Array.from({length:256},(_,i)=>[i,i,i]),waterfallBitmap:canvas(),waterfall3DBitmap:canvas(),frequencyAxes(){},$(){return {};}};
 vm.createContext(sandbox);vm.runInContext(draw,sandbox);sandbox.drawWaterfall();sandbox.drawWaterfall3D();
 assert.equal(calls[0][0],calls[1][0],'Frequency crop start differs');
 assert.equal(calls[0][2],calls[1][2],'Frequency crop width differs');
 assert.deepEqual(calls[0].slice(4),calls[1].slice(4),'Plot destination differs');
 assert.equal(sandbox.waterfall3DRender.style,'surface');
 assert.equal(sandbox.waterfall3DRender.imageRows,352);
 for(const key of ['rows','columns','newest','oldest'])assert.equal(sandbox.waterfallRender[key],sandbox.waterfall3DRender[key]);
 const count=calls.length;sandbox.plotEnabled=()=>false;sandbox.drawWaterfall3D();assert.equal(calls.length,count,'Disabled relief touched its canvas');
 console.log('PASS: power changes vertex height; narrow transients at every age/bin; stable projection as history fills; immutable history; synchronized crop/frame span; disabled canvas idle.');
})().catch(e=>{console.error(e);process.exitCode=1;});
