// Render the acquisition reference without external packages or a browser.
// The HTML source remains the editable, styled version of the document.
const fs = require('node:fs');
const path = require('node:path');

const root = path.resolve(__dirname, '..');
const source = path.join(root, 'docs', 'references', 'acquisition.html');
const destination = path.join(root, 'docs', 'references', 'acquisition.pdf');
const html = fs.readFileSync(source, 'utf8');
const body = html.split('<body>')[1].split('</body>')[0];
const decoded = body
  .replace(/<h1[^>]*>/g, '\n# ').replace(/<h2[^>]*>/g, '\n## ')
  .replace(/<h3[^>]*>/g, '\n### ').replace(/<tr[^>]*>/g, '\n')
  .replace(/<t[dh][^>]*>/g, ' | ').replace(/<\/(?:p|pre|div|table|h1|h2|h3|tr)>/g, '\n')
  .replace(/<li[^>]*>/g, '\n* ').replace(/<[^>]+>/g, '')
  .replace(/&gt;/g, '>').replace(/&lt;/g, '<').replace(/&amp;/g, '&')
  .replace(/&nbsp;/g, ' ').replace(/−/g, '-').replace(/±/g, '+/-')
  .replace(/π/g, 'pi').replace(/τ/g, 'tau').replace(/Σ/g, 'Sum')
  .replace(/²/g, '^2').replace(/⁻/g, '-').replace(/⁹/g, '9')
  .replace(/→/g, ' -> ').replace(/×/g, ' x ').replace(/≈/g, '~')
  .replace(/[–—]/g, '-').replace(/⁻/g, '-').replace(/₀/g, '0')
  .replace(/·/g, ' - ').replace(/‘|’/g, "'").replace(/“|”/g, '"')
  .replace(/<\/t[dh]>/g, '').replace(/\r/g, '');

function wrap(text, limit) {
  const words = text.trim().split(/\s+/).filter(Boolean);
  const lines = [];
  let line = '';
  for (const word of words) {
    if (line && line.length + word.length + 1 > limit) {
      lines.push(line);
      line = '';
    }
    line += (line ? ' ' : '') + word;
  }
  if (line) lines.push(line);
  return lines;
}

const logical = [];
for (const raw of decoded.split('\n')) {
  const line = raw.trim().replace(/\s+/g, ' ');
  if (!line) continue;
  const heading = line.startsWith('# ');
  const subheading = line.startsWith('## ');
  const text = line.replace(/^#{1,3} /, '').replace(/^\| /, '');
  if (heading || subheading) logical.push({text:'', size:6});
  for (const part of wrap(text, heading ? 62 : subheading ? 78 : 100))
    logical.push({text:part, size:heading ? 17 : subheading ? 12 : 9.5,
      bold:heading || subheading});
  if (heading || subheading) logical.push({text:'', size:4});
}

const pages = [];
let commands = [];
let y = 792;
function page() {
  if (commands.length) pages.push(commands.join('\n'));
  commands = [];
  y = 792;
}
function escapePdf(s) {
  return s.replace(/\\/g, '\\\\').replace(/\(/g, '\\(').replace(/\)/g, '\\)')
    .replace(/[^\x20-\x7e]/g, '?');
}
for (const item of logical) {
  const height = item.size + (item.size >= 12 ? 6 : 4);
  if (y - height < 46) page();
  if (item.text) commands.push(`BT /${item.bold ? 'F2' : 'F1'} ${item.size} Tf 48 ${y.toFixed(1)} Td (${escapePdf(item.text)}) Tj ET`);
  y -= height;
}
page();

const objects = [null];
function add(value) { objects.push(value); return objects.length - 1; }
const catalog = add('');
const pagesId = add('');
const regular = add('<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>');
const bold = add('<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>');
const pageIds = [];
for (const content of pages) {
  const data = `${content}\n`;
  const streamId = add(`<< /Length ${Buffer.byteLength(data, 'ascii')} >>\nstream\n${data}endstream`);
  pageIds.push(add(`<< /Type /Page /Parent ${pagesId} 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 ${regular} 0 R /F2 ${bold} 0 R >> >> /Contents ${streamId} 0 R >>`));
}
objects[catalog] = `<< /Type /Catalog /Pages ${pagesId} 0 R >>`;
objects[pagesId] = `<< /Type /Pages /Kids [${pageIds.map(id => `${id} 0 R`).join(' ')}] /Count ${pageIds.length} >>`;
let pdf = '%PDF-1.4\n';
const offsets = [0];
for (let id = 1; id < objects.length; id++) {
  offsets[id] = Buffer.byteLength(pdf, 'ascii');
  pdf += `${id} 0 obj\n${objects[id]}\nendobj\n`;
}
const startxref = Buffer.byteLength(pdf, 'ascii');
pdf += `xref\n0 ${objects.length}\n0000000000 65535 f \n`;
for (let id = 1; id < objects.length; id++) pdf += `${String(offsets[id]).padStart(10, '0')} 00000 n \n`;
pdf += `trailer\n<< /Size ${objects.length} /Root ${catalog} 0 R >>\nstartxref\n${startxref}\n%%EOF\n`;
fs.writeFileSync(destination, pdf, 'ascii');
console.log(`${destination} (${pageIds.length} pages)`);
