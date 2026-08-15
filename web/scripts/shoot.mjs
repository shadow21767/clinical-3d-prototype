// Dev-only helper: screenshots every step of the walkthrough so the staging and
// camera framing can be checked without opening a browser by hand.
import puppeteer from 'puppeteer-core';

const URL = 'http://localhost:5173/';
const OUT = process.argv[2] || '/tmp';

// Attaches to a headless Chrome already listening on 9222; launching one from
// here fails because puppeteer's temporary profile directory is unwritable.
const browser = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222' });

const page = await browser.newPage();
await page.setViewport({ width: 1440, height: 900, deviceScaleFactor: 1 });

const problems = [];
page.on('console', (m) => {
  if (m.type() === 'error') problems.push(`console: ${m.text()}`);
});
page.on('pageerror', (e) => problems.push(`pageerror: ${e.message}`));

await page.goto(URL, { waitUntil: 'networkidle0', timeout: 30000 });
await page.waitForSelector('canvas', { timeout: 15000 });

// Confirm WebGL actually produced a context rather than silently failing.
const renderer = await page.evaluate(() => {
  const gl = document.querySelector('canvas')?.getContext('webgl2');
  if (!gl) return null;
  const info = gl.getExtension('WEBGL_debug_renderer_info');
  return info ? gl.getParameter(info.UNMASKED_RENDERER_WEBGL) : 'webgl2 ok';
});
console.log('renderer:', renderer);

for (let i = 0; i < 5; i++) {
  if (i > 0) await page.keyboard.press('ArrowRight');
  // Let the camera and pose easing settle before capturing.
  await new Promise((r) => setTimeout(r, 2600));
  await page.screenshot({ path: `${OUT}/step${i + 1}.png` });
  console.log(`captured step ${i + 1}`);
}

console.log(problems.length ? `PROBLEMS:\n${problems.join('\n')}` : 'no console errors');
await page.close();
await browser.disconnect();
