/* Absolute-value plot regression: all three taps can move in both axes. */
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
const path=require('node:path');
(async()=>{
 const {trackingPlotPoints,trackingPromptComponent}=await import(pathToFileURL(path.resolve('web/workspace.js')));
 const a={code_delay_chips:0,taps:[{offset_chips:-0.4,i:20,q:0},{offset_chips:0,i:40,q:0},{offset_chips:0.4,i:25,q:0}]};
 const b={code_delay_chips:0.2,taps:[{offset_chips:-0.4,i:30,q:0},{offset_chips:0,i:55,q:0},{offset_chips:0.4,i:40,q:0}]};
 const p=trackingPlotPoints(a,'magnitude',100),q=trackingPlotPoints(b,'magnitude',100);
 for(let n=0;n<3;++n){assert(Math.abs(q[n].x-p[n].x-0.2)<1e-12);assert(q[n].y<p[n].y);assert.equal(q[n].value,b.taps[n].i);}
 assert.notEqual(p[1].y,q[1].y);assert.notEqual(p[1].y,25);assert.notEqual(q[1].y,25);
 assert.equal(trackingPlotPoints({...a,taps:[{offset_chips:0,i:3,q:-4}]},'q',10)[0].value,-4);
 assert.equal(trackingPlotPoints({...b,constellation:2},'magnitude',100)[2].x,2*trackingPlotPoints(b,'magnitude',100)[2].x);
 const history=(...prompt)=>prompt.map(([i,q])=>({...a,taps:[a.taps[0],{i,q},a.taps[2]]}));
 assert.equal(trackingPromptComponent(history([0.002,0.06],[-0.003,-0.06],[0.004,0.002]),1),'q','Both Q polarities contribute even when the last sample favours I');
 assert.equal(trackingPromptComponent(history([0.06,0.002],[-0.06,-0.003]),1),'i','A horizontal scatter uses I');
 assert.equal(trackingPromptComponent(history([0.01,0.01]),1),'i','Equal energy defaults to I');
 assert.equal(trackingPromptComponent([],1),'i','Empty history defaults to I');
 console.log('PASS: absolute correlator values, free Early/Prompt/Late movement and dominant signed prompt component');
})().catch(e=>{console.error(e);process.exitCode=1;});
