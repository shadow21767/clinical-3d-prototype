// Dev-only helper: exercises the interactive controls (orbit drag, view reset,
// hotspot clicks) and reports what the UI did, so behavior can be checked
// without driving the browser by hand.
import puppeteer from 'puppeteer-core';

const URL = 'http://localhost:5173/';
const OUT = process.argv[2] || '/tmp/cw';

const browser = await puppeteer.connect({ browserURL: 'http://127.0.0.1:9222' });
const page = await browser.newPage();
await page.setViewport({ width: 1440, height: 900, deviceScaleFactor: 1 });

const problems = [];
page.on('console', (m) => {
  if (m.type() === 'error') problems.push(`console: ${m.text()}`);
});
page.on('pageerror', (e) => problems.push(`pageerror: ${e.message}`));

await page.goto(URL, { waitUntil: 'networkidle0', timeout: 30000 });
await page.waitForSelector('canvas');
const settle = (ms = 2200) => new Promise((r) => setTimeout(r, ms));

const readState = () =>
  page.evaluate(() => ({
    title: document.querySelector('.panel h1')?.textContent,
    mode: document.querySelector('.mode')?.textContent?.trim(),
    chips: [...document.querySelectorAll('.chip')].map((c) => c.textContent.trim()),
  }));

await settle(2600);
console.log('initial:', JSON.stringify(await readState()));
await page.screenshot({ path: `${OUT}/i1-guided.png` });

// Drag across the canvas: should hand control to the viewer.
// Start well clear of any hotspot, otherwise the press is treated as a select.
await page.mouse.move(320, 620);
await page.mouse.down();
for (let i = 0; i < 12; i++) await page.mouse.move(320 + i * 14, 620 - i * 5);
await page.mouse.up();
await settle(1200);
console.log('after drag:', JSON.stringify(await readState()));
await page.screenshot({ path: `${OUT}/i2-freelook.png` });

// Scroll to zoom.
await page.mouse.move(620, 460);
await page.mouse.wheel({ deltaY: -220 });
await settle(1000);
await page.screenshot({ path: `${OUT}/i3-zoomed.png` });

// Reset back to guided framing.
const resetBtn = await page.$$('.chip');
for (const b of resetBtn) {
  if ((await b.evaluate((e) => e.textContent)).includes('Reset')) await b.click();
}
await settle(2400);
console.log('after reset:', JSON.stringify(await readState()));
await page.screenshot({ path: `${OUT}/i4-reset.png` });

// Hotspot click, if a pixel coordinate was passed as argv[3],argv[4].
if (process.argv[3] && process.argv[4]) {
  const x = Number(process.argv[3]);
  const y = Number(process.argv[4]);
  await page.mouse.move(x, y);
  await settle(700);
  await page.screenshot({ path: `${OUT}/i5-hover.png` });
  await page.mouse.click(x, y);
  await settle(2400);
  console.log(`after clicking (${x},${y}):`, JSON.stringify(await readState()));
  await page.screenshot({ path: `${OUT}/i6-afterclick.png` });
}

console.log(problems.length ? `PROBLEMS:\n${problems.join('\n')}` : 'no console errors');
await page.close();
await browser.disconnect();
