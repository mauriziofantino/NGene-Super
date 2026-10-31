/* Run the production 3D renderer against a recording Canvas context. */
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('web/app.js','utf8');
const draw=source.slice(source.indexOf('function drawWaterfall3D(){'),source.indexOf('function drawHistogram('));
const fills=[];let path=[];
const ctx={beginPath(){path=[];},moveTo(...xy){path.push(xy);},lineTo(...xy){path.push(xy);},closePath(){},fill(){fills.push({colour:this.fillStyle,path:[...path]});},stroke(){},fillText(){}};
const sandbox={diagnosticMask:()=>1,plotEnabled:()=>true,surface:()=>({ctx,left:52,right:820,top:16,bottom:368}),range:()=>[-100,0],frequencyRange:()=>[-500,500],displayFrequency:x=>x,analysisSnapshot:{sample_rate_hz:1000},historyRows:()=>128,waterfall3DHistory:[],plotWork:{waterfall3D:0},waterfallCssPalette:Array.from({length:256},(_,i)=>i),frequencyAxes(){},$(){return {};}};
vm.createContext(sandbox);vm.runInContext(draw,sandbox);
const render=(rows,age,bin)=>{
 sandbox.waterfall3DHistory=Array.from({length:rows},(_,j)=>({values:Array.from({length:1024},(_,k)=>j===age&&k===bin?0:-100)}));
 fills.length=0;sandbox.drawWaterfall3D();return fills.filter(f=>f.colour===255);
};
// A one-snapshot pulse must survive at every age, including those omitted by old point decimation.
for(let age=0;age<128;age++)assert(render(128,age,333).length,'Lost transient at history row '+age);
// Frequency pooling must preserve narrow events at both band edges and all mesh boundaries.
for(let bin=0;bin<1024;bin++)assert(render(8,4,bin).length,'Lost narrowband event at bin '+bin);
const a=render(16,4,333).map(f=>f.path),b=render(32,4,333).map(f=>f.path);
assert.deepEqual(a,b,'Existing events must not jump as the history fills');
const older=render(128,6,333),newer=render(128,4,333);
assert(Math.min(...older.flatMap(f=>f.path.map(xy=>xy[1])))<Math.min(...newer.flatMap(f=>f.path.map(xy=>xy[1]))),'Events must scroll toward the rear');
render(128,-1,0);
const rear=fills.slice(0,255);assert(rear.every(f=>f.path.every(xy=>Math.abs(xy[1]-(368-352*0.58))<1e-8)),'Rear skirt must terminate at its own depth');
assert(sandbox.waterfall3DRender.rows<=64&&sandbox.waterfall3DRender.columns<=256);
console.log('PASS: all temporal snapshots and frequency bins represented; stable depth during fill; rear skirt; bounded mesh.');
