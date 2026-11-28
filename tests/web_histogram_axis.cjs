/* Inclusive ADC levels: grouped bins must not misplace zero or the sample mean. */
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const text=[],lines=[],bars=[];
const ctx={fillText(value,x,y){text.push({value,x,y,align:this.textAlign});},fillRect(x,y,w,h){bars.push({y,h});},beginPath(){},moveTo(x,y){lines.push({x,y});},lineTo(){},stroke(){},save(){},restore(){},setLineDash(){}};
const source=fs.readFileSync('web/app.js','utf8');
const draw=source.slice(source.indexOf('function drawHistogram('),source.indexOf('function drawAnalysis('));
const sandbox={diagnosticMask:()=>2,surface:()=>({ctx,w:760,h:180,left:52,right:742,top:16,bottom:148}),$:id=>({value:id==='histScale'?'count':'0'}),analysisSnapshot:{histogram_min:-128,histogram_max:127,histogram_count:65536,stats_i:{mean:.0752},stats_q:{mean:.1533}}};
vm.createContext(sandbox);vm.runInContext(draw,sandbox);
sandbox.drawHistogram('histogramI',Array(64).fill(1),'#39d5b0');
const zero=text.find(t=>t.value==='0'&&t.y===170);assert(zero);assert.equal(zero.align,'center');
assert(Math.abs(zero.x-(52+128.5/256*690))<1e-9,'Zero must label the actual zero coordinate');
const meanLine=lines.at(-1);assert(Math.abs(meanLine.x-(52+128.5752/256*690))<1e-9,'Mean marker must use individual samples, not bin centre');
assert(text.some(t=>String(t.value).includes('Mean 0.0752')&&String(t.value).includes('4 counts/bin')));
sandbox.analysisSnapshot.histogram_min=-132;
sandbox.analysisSnapshot.histogram_max=131;
sandbox.drawHistogram('histogramI',Array(64).fill(1),'#39d5b0');
assert(text.some(t=>String(t.value).includes('4.13 counts/bin')));
sandbox.analysisSnapshot.histogram_i=[7177];
sandbox.analysisSnapshot.histogram_q=[7104];
for(const [id,data] of [['histogramI',[7177]],['histogramQ',[7104]]]){
  sandbox.drawHistogram(id,data,'#39d5b0');
  assert.equal(bars.at(-1).h,132*data[0]/8254);
  assert(bars.at(-1).y>16,'Automatic scaling leaves room above both peaks');
}
sandbox.$=id=>({value:id==='histScale'?'count':'6500'});
sandbox.drawHistogram('histogramI',[7177],'#39d5b0');
assert.equal(bars.at(-1).y,16);
assert(text.some(t=>t.value==='Above Y maximum'));
console.log('PASS: inclusive ADC bin edges, centred zero label, exact fractional-mean marker and bin-width annotation.');
