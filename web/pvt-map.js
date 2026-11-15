/* Receiver solutions on Web Mercator. Only visible viewport tiles are requested. */
export function mercator(latitude, longitude, zoom) {
  const scale=256*2**zoom,lat=Math.max(-85.05112878,Math.min(85.05112878,latitude))*Math.PI/180;
  return [(longitude+180)/360*scale,(1-Math.asinh(Math.tan(lat))/Math.PI)/2*scale];
}
export function createPvtMap(root) {
  root.innerHTML='<div class="pvt-map-controls"><button type="button" data-map-in aria-label="Zoom map in">+</button><button type="button" data-map-out aria-label="Zoom map out">−</button><button type="button" data-map-follow aria-pressed="true">Follow receiver</button><span data-map-status>No position solution available</span></div><div class="pvt-map-frame"><canvas tabindex="0" aria-label="Receiver position map. Arrow keys pan, plus and minus zoom."></canvas><a class="pvt-map-attribution" href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">© OpenStreetMap contributors</a></div><p class="muted">Online street map · drag to pan, scroll to zoom. The track contains receiver solutions. Map tiles require internet access.</p>';
  const canvas=root.querySelector('canvas'),ctx=canvas.getContext('2d'),label=root.querySelector('[data-map-status]');
  const followButton=root.querySelector('[data-map-follow]'),tiles=new Map();
  let visible=false,zoom=16,center=null,position=null,history=[],run=null,lastKey='',following=true,queued=false,drag=null,current=false,tileError=false;
  function schedule() {
    if(queued||!visible)return;
    queued=true;requestAnimationFrame(()=>{queued=false;if(visible)draw();});
  }
  function follow(value) {following=value;followButton.setAttribute('aria-pressed',String(value));}
  function changeZoom(delta) {zoom=Math.max(2,Math.min(19,zoom+delta));schedule();}
  function tile(z,x,y) {
    const count=2**z;if(y<0||y>=count)return null;
    x=((x%count)+count)%count;const key=`${z}/${x}/${y}`;
    if(!tiles.has(key)) {
      const img=new Image();img.referrerPolicy='origin';
      img.onload=()=>{img.dataset.ready='1';schedule();};
      img.onerror=()=>{img.dataset.failed='1';tileError=true;schedule();};
      img.src=`https://tile.openstreetmap.org/${key}.png`;tiles.set(key,img);
      if(tiles.size>96)tiles.delete(tiles.keys().next().value);
    }
    return tiles.get(key);
  }
  function draw() {
    const box=canvas.getBoundingClientRect(),w=box.width,h=box.height,dpr=devicePixelRatio||1;
    if(!w||!h)return;
    canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);ctx.scale(dpr,dpr);
    ctx.fillStyle='#102537';ctx.fillRect(0,0,w,h);
    if(!center||!position) {
      ctx.fillStyle='#a9c5d7';ctx.font='15px system-ui';ctx.textAlign='center';
      ctx.fillText('Waiting for a calculated PVT position',w/2,h/2);return;
    }
    const c=mercator(center[0],center[1],zoom),left=c[0]-w/2,top=c[1]-h/2,world=256*2**zoom;
    for(let y=Math.floor(top/256);y<=Math.floor((top+h)/256);y++)
      for(let x=Math.floor(left/256);x<=Math.floor((left+w)/256);x++) {
        const img=tile(zoom,x,y),px=x*256-left,py=y*256-top;
        if(img?.dataset.ready)ctx.drawImage(img,px,py,256,256);
        else {ctx.strokeStyle='#264257';ctx.strokeRect(px,py,256,256);}
      }
    const point=p=>{const v=mercator(p.latitude_deg,p.longitude_deg,zoom);let dx=v[0]-c[0];dx-=world*Math.round(dx/world);return[w/2+dx,v[1]-top];};
    ctx.beginPath();let previous;
    for(const p of history) {
      const v=point(p);
      if(!previous||p.segment!==previous.segment)ctx.moveTo(...v);else ctx.lineTo(...v);
      previous=p;
    }
    ctx.lineWidth=2;ctx.strokeStyle='#e96b38';ctx.stroke();
    const v=point(position);ctx.beginPath();ctx.arc(...v,8,0,Math.PI*2);
    ctx.fillStyle=current?'#0878da':'#8494a0';ctx.fill();ctx.strokeStyle='#fff';ctx.lineWidth=3;ctx.stroke();
    label.textContent=`${current?'Position':'Last position'}: ${position.latitude_deg.toFixed(7)}°, ${position.longitude_deg.toFixed(7)}° · zoom ${zoom}${tileError?' · Map background unavailable':''}`;
    canvas.dataset.latitude=String(position.latitude_deg);canvas.dataset.longitude=String(position.longitude_deg);
    canvas.dataset.points=String(history.length);
  }
  let segment=0,wasAvailable=false;
  root.querySelector('[data-map-in]').onclick=()=>changeZoom(1);
  root.querySelector('[data-map-out]').onclick=()=>changeZoom(-1);
  followButton.onclick=()=>{follow(true);if(position)center=[position.latitude_deg,position.longitude_deg];schedule();};
  canvas.addEventListener('wheel',event=>{event.preventDefault();changeZoom(event.deltaY<0?1:-1);},{passive:false});
  function pan(dx,dy) {
    if(!center)return;
    const c=mercator(center[0],center[1],zoom),scale=256*2**zoom;
    const y=Math.max(0,Math.min(scale,c[1]+dy));
    center=[Math.atan(Math.sinh(Math.PI*(1-2*y/scale)))*180/Math.PI,(((c[0]+dx)/scale*360)%360+540)%360-180];
    follow(false);schedule();
  }
  canvas.addEventListener('pointerdown',event=>{drag=[event.clientX,event.clientY];canvas.setPointerCapture(event.pointerId);});
  canvas.addEventListener('pointermove',event=>{if(!drag)return;pan(drag[0]-event.clientX,drag[1]-event.clientY);drag=[event.clientX,event.clientY];});
  canvas.addEventListener('pointerup',()=>{drag=null;});canvas.addEventListener('pointercancel',()=>{drag=null;});
  canvas.addEventListener('keydown',event=>{
    const steps={ArrowLeft:[-100,0],ArrowRight:[100,0],ArrowUp:[0,-100],ArrowDown:[0,100]};
    if(steps[event.key]){event.preventDefault();pan(...steps[event.key]);}
    else if(['+','=','-'].includes(event.key)){event.preventDefault();changeZoom(event.key==='-'?-1:1);}
  });
  new ResizeObserver(schedule).observe(canvas);
  return {
    setVisible(value){visible=value;schedule();},
    update(solution,runId,running) {
      if(run!==runId){run=runId;position=center=null;history=[];lastKey='';segment=0;wasAvailable=false;label.textContent='No position solution available';for(const key of ['latitude','longitude','points'])delete canvas.dataset[key];}
      const valid=!!solution?.available&&Number.isFinite(solution.latitude_deg)&&Math.abs(solution.latitude_deg)<=90&&Number.isFinite(solution.longitude_deg)&&Math.abs(solution.longitude_deg)<=180;
      current=valid&&running;
      if(valid) {
        const key=`${solution.epoch}:${solution.sample}`;
        if(key!==lastKey) {
          if(!wasAvailable)segment++;
          position={...solution,segment};history.push(position);if(history.length>600)history.shift();lastKey=key;
          if(following||!center)center=[position.latitude_deg,position.longitude_deg];
        }
      }
      wasAvailable=valid;schedule();
    }
  };
}
