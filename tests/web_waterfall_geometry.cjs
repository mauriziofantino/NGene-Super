/* Geometry contract: a real frontal mesh, with power height and depth. */
const assert=require('node:assert/strict'),fs=require('node:fs');
(async()=>{
 const source=fs.readFileSync('web/waterfall-relief.js','utf8');
 const {buildWaterfallMesh,projectSurfacePoint,createWaterfallScroll}=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
 const history=Array.from({length:128},(_,j)=>({sequence:String(128-j),values:Array.from({length:1024},(_,k)=>-70+4*Math.sin(k*.2+j*.3))}));
 const before=JSON.stringify(history),mesh=buildWaterfallMesh(history,1024,128,-100,-40);
 assert.equal(mesh.positions.length,129*1024*3);assert.equal(mesh.normals.length,mesh.positions.length);
 for(let j=0;j<128;j++)for(let k=0;k<1024;k++){
  const index=j*1024+k;assert.equal(mesh.positions[index*3],Math.fround(2*k/1023-1));
  assert.equal(mesh.levels[index],Math.fround((history[j].values[k]+100)/60));
  assert(Number.isFinite(mesh.normals[index*3])&&Number.isFinite(mesh.normals[index*3+2]));
 }
 const low=projectSurfacePoint(0,.2,.5,mesh),high=projectSurfacePoint(0,.8,.5,mesh);
 assert(high.y>low.y,'Power must change vertex height');assert.notEqual(high.depth,low.depth,'Power must participate in 3D depth');
 const near=projectSurfacePoint(-.6,.5,0,mesh),far=projectSurfacePoint(-.6,.5,1,mesh);
 assert.equal(near.x,far.x,'Frontal view must not introduce lateral rotation');assert(near.depth<far.depth,'New observations must occupy the front');assert(near.y<far.y,'Old observations must recede');
 let min=Infinity,max=-Infinity;
 for(let i=0;i<mesh.levels.length;i++){const p=projectSurfacePoint(mesh.positions[3*i],mesh.levels[i],mesh.positions[3*i+2],mesh);min=Math.min(min,p.y);max=Math.max(max,p.y);}
 assert(Math.abs(min+1)<1e-6&&Math.abs(max-1)<1e-6,'Time edges must fill the whole pane');
 assert.equal(JSON.stringify(history),before,'Mesh preparation must not modify observations');
 const short=buildWaterfallMesh(history.slice(0,16),1024,128,-100,-40);
 assert.equal(short.rows,16);assert.equal(short.gridRows,128);
 assert.deepEqual(short.matrix,mesh.matrix,'Filling history must not move the camera');
 for(let j=0;j<16;j++)assert.equal(short.positions[j*1024*3+2],mesh.positions[j*1024*3+2],'History spacing must remain fixed');
 const extreme=buildWaterfallMesh([{values:Array(1024).fill(0)}],1024,128,-250,0);
 assert.deepEqual(extreme.matrix,mesh.matrix,'Changing power extrema must not refit the camera');
 for(const level of [0,.25,.5,.75,1]){
  let previous=-Infinity;
  for(let j=0;j<=1000;j++){const p=projectSurfacePoint(0,level,j/1000,mesh);assert(p.y>=previous-1e-7,'A moving feature must never reverse vertically');previous=p.y;}
 }
 const timeline=createWaterfallScroll(180);let index=16,last2D=-Infinity,last3D=-Infinity;
 for(let now=0;now<=1000;now+=10){
  if(now%50===0){const before=index-timeline.offset(now);timeline.push(now);index++;assert(Math.abs(index-timeline.offset(now)-before)<1e-10,'An arrival must not restart or jump existing motion');}
  const age=(index-timeline.offset(now))/127,y2D=age*500,y3D=projectSurfacePoint(0,.8,age,mesh).y;
  assert(y2D>=last2D-1e-7&&y3D>=last3D-1e-7,'Both views must move monotonically during overlapping updates');last2D=y2D;last3D=y3D;
 }
 const frozen=timeline.offset(1010);timeline.pause(1010);assert.equal(timeline.offset(5010),frozen);timeline.resume(5010);assert.equal(timeline.offset(5010),frozen);
 assert.equal(timeline.offset(5210),0);timeline.reset();assert.equal(timeline.offset(0),0);
 console.log('PASS: fixed full-pane camera/spacing, genuine depth, monotonic 2D/3D motion, continuous overlapping arrivals, pause/resume and unchanged observations.');
})().catch(e=>{console.error(e);process.exitCode=1;});
