// Usage: node scripts/render_acquisition_pdf.cjs [path-to-chromium-or-edge]
// Native browser printing preserves equations, Unicode, tables and vector diagrams.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const {spawnSync} = require('node:child_process');
const {pathToFileURL} = require('node:url');
const root = path.resolve(__dirname, '..');
const source = path.join(root, 'docs/references/acquisition.html');
const destination = path.join(root, 'docs/references/acquisition.pdf');
const browser = [process.argv[2], process.env.CHROMIUM_PATH,
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Google/Chrome/Application/chrome.exe'].find(p => p && fs.existsSync(p));
if (!browser) throw new Error('Set CHROMIUM_PATH or supply a Chromium/Edge executable.');
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'ngene-acq-pdf-'));
try {
  const result = spawnSync(browser, ['--headless', '--disable-gpu', '--no-first-run',
    '--no-default-browser-check', '--no-pdf-header-footer', '--disable-extensions',
    `--user-data-dir=${profile}`, `--print-to-pdf=${destination}`, pathToFileURL(source).href],
    {windowsHide:true, encoding:'utf8', timeout:60000});
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(result.stderr || `Browser exit ${result.status}`);
  if (!fs.existsSync(destination) || fs.statSync(destination).size < 20000)
    throw new Error('PDF missing or unexpectedly small.');
  console.log(`${destination} (${fs.statSync(destination).size} bytes)`);
} finally {
  // Remove only the unique temporary browser profile created by this process.
  try { fs.rmSync(profile, {recursive:true, force:true}); } catch {}
}
