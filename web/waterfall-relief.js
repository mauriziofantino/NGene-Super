/* Fixed 3D frequency/power/time mesh: real geometry, depth testing,
 * directional lighting and time traces. The global power scale is shared. */
const surfaceTilt=35,surfaceHeight=0.5;
const stops=[[0,12,85],[0,34,145],[0,76,215],[0,141,245],[38,207,255],[197,247,255]];
export const reliefPalette=Array.from({length:256},(_,n)=>{const x=n/255*5,i=Math.min(4,Math.floor(x)),f=x-i;return stops[i].map((v,k)=>Math.round(v+(stops[i+1][k]-v)*f));});
const levelOf=(v,lo,hi)=>Number.isFinite(v)?Math.max(0,Math.min(1,(v-lo)/(hi-lo))):0;
const edgeHeight=age=>age<=0||age>=1?0:Math.sin(Math.PI*age);
/** Retain unfinished movement when another observation arrives. */
export function createWaterfallScroll(duration=180){
 let pending=0,started=0,pausedAt=null;
 const offset=now=>pending*(1-Math.max(0,Math.min(1,((pausedAt??now)-started)/duration)));
 return {
  offset,
  push(now){pending=offset(now)+1;started=pausedAt??now;},
  pause(now){if(pausedAt===null)pausedAt=now;},
  resume(now){if(pausedAt!==null){started+=now-pausedAt;pausedAt=null;}},
  reset(){pending=0;started=0;pausedAt=null;}
 };
}
export function buildWaterfallMesh(history,columns,capacity,lo,hi){
 const rows=Math.min(capacity,history.length),gridRows=rows?capacity:0;
 if(columns<2||columns>1024||!Number.isInteger(capacity)||capacity<2||capacity>2048||!Number.isFinite(lo)||!Number.isFinite(hi)||lo>=hi)throw Error('Invalid 3D waterfall geometry.');
 const positions=new Float32Array((gridRows?gridRows+1:0)*columns*3),normals=new Float32Array(positions.length),levels=new Float32Array(positions.length/3);
 const tilt=surfaceTilt*Math.PI/180,c=Math.cos(tilt),s=Math.sin(tilt),step=1/(capacity-1);
 const sample=(j,k)=>levelOf(history[Math.min(rows-1,j)].values[k],lo,hi);
 for(let j=0;j<gridRows;j++)for(let k=0;k<columns;k++){
  const i=j*columns+k,level=sample(j,k),age=j*step,h=(level-0.5)*surfaceHeight;
  positions.set([2*k/(columns-1)-1,h,age],3*i);levels[i]=level;
  const l=sample(j,Math.max(0,k-1)),r=sample(j,Math.min(columns-1,k+1));
  const near=Math.max(0,j-1),far=Math.min(gridRows-1,j+1);
  const hn=(sample(near,k)-0.5)*surfaceHeight*edgeHeight(near*step);
  const hf=(sample(far,k)-0.5)*surfaceHeight*edgeHeight(far*step);
  normals.set([-(r-l)*surfaceHeight*edgeHeight(age)*(columns-1)/4,1,(hf-hn)*(capacity-1)/4],3*i);
 }
 // Hold the oldest known edge across unfilled history, without adding observations.
 // The extra rear boundary stays fixed while actual rows scroll out of view.
 if(gridRows){
  for(let k=0;k<columns;k++){
   const i=gridRows*columns+k;positions.set([2*k/(columns-1)-1,0,2],3*i);
   normals.set([0,1,0],3*i);levels[i]=sample(gridRows-1,k);
  }
 }
 // Tilt the frequency/power plane around the horizontal time axis.
 // Leave a small margin for relief at the upper and lower frequency edges.
 const frequencyScale=0.98;
 const matrix=new Float32Array([0,frequencyScale*c,frequencyScale*s,0,0,s,-c,0,1,0,0,0,0,0,0,1]);
 return {rows,gridRows,columns,positions,normals,levels,matrix};
}
export function projectSurfacePoint(frequency,level,age,mesh){
 age=Math.max(0,Math.min(1,age));
 const h=(level-0.5)*surfaceHeight*edgeHeight(age),z=1-2*age,m=mesh.matrix;
 return {x:z,y:m[1]*frequency+m[5]*h,depth:m[2]*frequency+m[6]*h};
}
export function createWaterfallSurface(canvas){
 const gl=canvas.getContext('webgl',{alpha:true,antialias:true,preserveDrawingBuffer:true});
 if(!gl||!gl.getExtension('OES_element_index_uint'))throw Error('3D waterfall requires WebGL.');
 const compile=(type,source)=>{const s=gl.createShader(type);gl.shaderSource(s,source);gl.compileShader(s);if(!gl.getShaderParameter(s,gl.COMPILE_STATUS))throw Error(gl.getShaderInfoLog(s));return s;};
 const program=gl.createProgram();
 gl.attachShader(program,compile(gl.VERTEX_SHADER,'attribute vec3 position;attribute vec3 normal;attribute float level;uniform mat4 projection;uniform float rowOffset;uniform float ageStep;varying vec3 vNormal;varying float vLevel;void main(){float age=position.z>1.5?1.0:max(0.0,position.z-rowOffset*ageStep);float edge=age<=0.0||age>=1.0?0.0:sin(3.14159265359*age);gl_Position=projection*vec4(position.x,position.y*edge,1.0-2.0*age,1.0);vNormal=normal;vLevel=level;}'));
 gl.attachShader(program,compile(gl.FRAGMENT_SHADER,'precision mediump float;uniform sampler2D palette;uniform float trace;varying vec3 vNormal;varying float vLevel;void main(){vec3 n=normalize(vNormal);float diffuse=max(0.0,dot(n,normalize(vec3(-0.2,0.85,0.5))));vec3 colour=texture2D(palette,vec2(clamp(vLevel,0.0,1.0),0.5)).rgb;gl_FragColor=trace>0.5?vec4(mix(colour*0.22,vec3(0.01,0.04,0.16),0.5),0.62):vec4(colour*(0.36+0.7*diffuse),1.0);}'));
 gl.linkProgram(program);if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(program));gl.useProgram(program);
 const attributes={};for(const [name,size] of [['position',3],['normal',3],['level',1]])attributes[name]={location:gl.getAttribLocation(program,name),buffer:gl.createBuffer(),size};
 const triangles=gl.createBuffer(),traces=gl.createBuffer(),projection=gl.getUniformLocation(program,'projection'),offsetUniform=gl.getUniformLocation(program,'rowOffset'),ageStep=gl.getUniformLocation(program,'ageStep'),trace=gl.getUniformLocation(program,'trace');
 const texture=gl.createTexture();gl.bindTexture(gl.TEXTURE_2D,texture);
 for(const parameter of [gl.TEXTURE_MIN_FILTER,gl.TEXTURE_MAG_FILTER])gl.texParameteri(gl.TEXTURE_2D,parameter,gl.LINEAR);
 for(const parameter of [gl.TEXTURE_WRAP_S,gl.TEXTURE_WRAP_T])gl.texParameteri(gl.TEXTURE_2D,parameter,gl.CLAMP_TO_EDGE);
 gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA,256,1,0,gl.RGBA,gl.UNSIGNED_BYTE,new Uint8Array(reliefPalette.flatMap(c=>[...c,255])));
 let mesh=null,key='',triangleCount=0,traceCount=0;
 return {draw(history,columns,capacity,lo,hi,width,height,rowOffset=0){
  const nextKey=[history[0]?.sequence,history.at(-1)?.sequence,history.length,columns,capacity,lo,hi].join(':');
  if(!mesh||key!==nextKey){
   key=nextKey;mesh=buildWaterfallMesh(history,columns,capacity,lo,hi);
   for(const [name,data] of Object.entries({position:mesh.positions,normal:mesh.normals,level:mesh.levels})){const a=attributes[name];gl.bindBuffer(gl.ARRAY_BUFFER,a.buffer);gl.bufferData(gl.ARRAY_BUFFER,data,gl.DYNAMIC_DRAW);gl.enableVertexAttribArray(a.location);gl.vertexAttribPointer(a.location,a.size,gl.FLOAT,false,0,0);}
   const indices=[],lines=[];
   for(let j=0;j<mesh.gridRows;j++)for(let k=0;k<columns-1;k++){const a=j*columns+k;lines.push(a,a+1);indices.push(a,a+columns,a+1,a+1,a+columns,a+columns+1);}
   triangleCount=indices.length;traceCount=lines.length;
   gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,triangles);gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,new Uint32Array(indices),gl.STATIC_DRAW);
   gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,traces);gl.bufferData(gl.ELEMENT_ARRAY_BUFFER,new Uint32Array(lines),gl.STATIC_DRAW);
  }
  if(canvas.width!==width)canvas.width=width;if(canvas.height!==height)canvas.height=height;
  gl.viewport(0,0,width,height);gl.clearColor(0,0,0,0);gl.clear(gl.COLOR_BUFFER_BIT|gl.DEPTH_BUFFER_BIT);gl.enable(gl.DEPTH_TEST);gl.depthFunc(gl.LEQUAL);
  rowOffset=Math.max(0,Math.min(capacity-1,rowOffset));
  gl.uniformMatrix4fv(projection,false,mesh.matrix);gl.uniform1f(offsetUniform,rowOffset);gl.uniform1f(ageStep,1/(capacity-1));
  gl.uniform1f(trace,0);gl.enable(gl.POLYGON_OFFSET_FILL);gl.polygonOffset(1,1);gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,triangles);gl.drawElements(gl.TRIANGLES,triangleCount,gl.UNSIGNED_INT,0);
  gl.disable(gl.POLYGON_OFFSET_FILL);gl.uniform1f(trace,1);gl.enable(gl.BLEND);gl.blendFunc(gl.SRC_ALPHA,gl.ONE_MINUS_SRC_ALPHA);gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER,traces);gl.drawElements(gl.LINES,traceCount,gl.UNSIGNED_INT,0);gl.disable(gl.BLEND);
  return {rows:mesh.rows,gridRows:mesh.gridRows,columns,imageRows:height,style:'mesh',azimuth:0,elevation:surfaceTilt,rowOffset,scrollProgress:1-Math.min(1,rowOffset),projection:Array.from(mesh.matrix),vertices:mesh.gridRows*columns,triangles:triangleCount/3};
 }};
}
