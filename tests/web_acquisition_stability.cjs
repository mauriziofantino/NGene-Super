/* Browser regression with deterministic telemetry; no receiver or RF hardware required. */
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const http = require('node:http'), fs = require('node:fs'), path = require('node:path');
const assert = require('node:assert/strict');
(async () => {
  const root = path.resolve(__dirname, '..');
  const server = http.createServer((request,response) => {
    const file = path.resolve(root, '.' + request.url);
    if (!file.startsWith(root + path.sep)) { response.writeHead(403).end(); return; }
    fs.readFile(file,(error,bytes) => {
      if (error) { response.writeHead(404).end(); return; }
      response.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : file.endsWith('.css') ? 'text/css' : 'text/html');
      response.end(bytes);
    });
  });
  let browser;
  try {
    await new Promise((resolve,reject) => { server.once('error',reject); server.listen(0,'127.0.0.1',resolve); });
    browser = await chromium.launch({headless:true,
      ...(process.env.BROWSER_EXECUTABLE ? {executablePath:process.env.BROWSER_EXECUTABLE} : {})});
    for (const width of [1440,390]) {
      const page = await browser.newPage({viewport:{width,height:1000}}), errors = [];
      page.on('pageerror',error => errors.push(error.message));
      await page.goto(`http://127.0.0.1:${server.address().port}/tests/web_acquisition_stability.html`);
      await page.waitForFunction(() => /^(PASS|FAIL):/.test(document.querySelector('#outcome').textContent));
      const outcome = await page.locator('#outcome').textContent();
      assert(outcome.startsWith('PASS:'),outcome); assert.deepEqual(errors,[]);
      console.log(`${width}px: ${outcome}`); await page.close();
    }
  } finally { await browser?.close(); await new Promise(resolve => server.close(resolve)); }
})().catch(error => { console.error(error); process.exitCode = 1; });
