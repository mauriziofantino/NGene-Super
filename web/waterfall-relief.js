/* Orthographic frequency/time/power surface. Each PSD bin is a vertex;
 * power displaces its screen position, rather than merely changing its colour.
 * The time grid uses the configured history capacity, so filling history does
 * not move existing vertices relative to their age. */
const stops = [[5,9,35],[12,23,77],[23,48,127],[22,118,193],[63,218,234],[197,255,252]];
export const reliefPalette = Array.from({length:256}, (_, level) => {
  const x=level/255*(stops.length-1),i=Math.min(stops.length-2,Math.floor(x)),f=x-i;
  return stops[i].map((value,k)=>Math.round(value+(stops[i+1][k]-value)*f));
});

export function projectWaterfallPoint(age, level, rows, imageRows) {
  const rise=(imageRows-1)*0.38;
  return rise+age/Math.max(1,rows-1)*(imageRows-1-rise)-level*rise;
}

/** Rasterize connected height-field strips, front (oldest) to rear (newest).
 * A strip has one pixel column per telemetry bin, without frequency pooling.
 * Foreground strips occlude background surfaces, as in a solid 3D plot.
 * The input history remains the shared, immutable 2D/3D observations. */
export function paintWaterfallRelief(pixels, history, columns, rows, lo, hi, imageRows=rows) {
  if(!Number.isInteger(columns)||columns<1||columns>1024||!Number.isInteger(rows)||rows<1||rows>2048||
     !Number.isInteger(imageRows)||imageRows<2||imageRows>2048||
     !Number.isFinite(lo)||!Number.isFinite(hi)||lo>=hi||pixels.length!==columns*imageRows*4)
    throw new Error('Invalid waterfall relief geometry or power range.');
  const active=Math.min(rows,history.length),previousY=new Float32Array(columns),previousLevel=new Float32Array(columns);
  const horizon=new Int32Array(columns);horizon.fill(imageRows);
  const levelOf=value=>Number.isFinite(value)?Math.max(0,Math.min(1,(value-lo)/(hi-lo))):0;
  pixels.fill(0);
  const rise=(imageRows-1)*0.38,step=(imageRows-1-rise)/Math.max(1,rows-1);
  for(let j=active-1;j>=0;j--)for(let k=0;k<columns;k++){
    const values=history[j].values,level=levelOf(values[k]);
    const hasFront=j<active-1,y=rise+j*step-level*rise,back=hasFront?previousY[k]:y;
    const frontLevel=previousLevel[k];previousY[k]=y;previousLevel[k]=level;
    const first=Math.max(0,Math.floor(Math.min(y,back))),last=Math.min(horizon[k]-1,imageRows-1,Math.ceil(Math.max(y,back)));
    // A per-frequency horizon removes hidden strips before shading. No row or
    // bin is pooled; every vertex contributes to visibility. Each output pixel
    // is written at most once, even at the maximum history depth.
    if(first>last)continue;
    horizon[k]=first;
    const dx=3.5*(levelOf(values[Math.min(columns-1,k+1)])-levelOf(values[Math.max(0,k-1)]));
    const dt=hasFront?2*(level-frontLevel):0;
    const light=Math.max(0,(0.82+0.46*dx+0.34*dt)/Math.sqrt(1+dx*dx+dt*dt));
    for(let screenY=first;screenY<=last;screenY++){
      const f=Math.abs(y-back)<0.001?1:Math.max(0,Math.min(1,(screenY-back)/(y-back)));
      const power=hasFront?frontLevel+f*(level-frontLevel):level;
      const colour=reliefPalette[Math.round(power*255)],shade=0.5+0.3*light+0.3*power,offset=4*(screenY*columns+k);
      for(let c=0;c<3;c++)pixels[offset+c]=Math.min(255,Math.round(colour[c]*shade));
      pixels[offset+3]=255;
    }
  }
  return {rows:active,columns,imageRows,heightFraction:0.38};
}
