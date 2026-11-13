/* Absolute-value plot regression: all three taps can move in both axes. */
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
(async()=>{
 const {trackingPlotPoints}=await import(pathToFileURL(path.resolve('web/workspace.js')));
 const a={code_delay_chips:0,taps:[{offset_chips:-0.4,i:20,q:0},{offset_chips:0,i:40,q:0},{offset_chips:0.4,i:25,q:0}]};
 const b={code_delay_chips:0.2,taps:[{offset_chips:-0.4,i:30,q:0},{offset_chips:0,i:55,q:0},{offset_chips:0.4,i:40,q:0}]};
 const p=trackingPlotPoints(a,'magnitude',100),q=trackingPlotPoints(b,'magnitude',100);
 for(let n=0;n<3;++n){assert(Math.abs(q[n].x-p[n].x-0.2)<1e-12);assert(q[n].y<p[n].y);assert.equal(q[n].value,b.taps[n].i);}
 assert.notEqual(p[1].y,q[1].y);assert.notEqual(p[1].y,25);assert.notEqual(q[1].y,25);
 assert.equal(trackingPlotPoints({...a,taps:[{offset_chips:0,i:3,q:-4}]},'q',10)[0].value,-4);
 console.log('PASS: absolute correlator values and free Early/Prompt/Late movement');
})().catch(e=>{console.error(e);process.exitCode=1;});
