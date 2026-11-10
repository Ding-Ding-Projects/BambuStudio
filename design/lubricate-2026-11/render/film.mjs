import { chromium } from 'playwright';
import fs from 'fs';
const dir = process.argv[2], out = process.argv[3], canvas = JSON.parse(fs.readFileSync(process.argv[4], 'utf8'));
const names = process.argv.slice(5);
const browser = await chromium.launch({ args: ['--allow-file-access-from-files'] });
const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
const res = {};
for (const n of names) {
  const board = canvas.boards[n + '.dc.html'];
  await page.setViewportSize({ width: 1440, height: 900 });
  await page.goto('file://' + dir + '/' + n + '.html');
  await page.evaluate(() => document.fonts.ready);
  await page.waitForTimeout(150);
  const pg = await page.evaluate(() => { const e = document.querySelector('.pg'); return e ? Math.ceil(e.getBoundingClientRect().height) : 0; });
  const wide = await page.evaluate(() => document.documentElement.scrollWidth);
  if (pg > 0) {
    await page.setViewportSize({ width: 1440, height: pg });
    await page.screenshot({ path: out + '/' + n + '.png', fullPage: false });
  } else {
    await page.setViewportSize({ width: 1440, height: board.h });
    await page.screenshot({ path: out + '/' + n + '.png' });
  }
  res[n] = { pg, board: board.h, wide };
  console.log(n, JSON.stringify(res[n]));
}
fs.writeFileSync(out + '/heights.json', JSON.stringify(res, null, 1));
await browser.close();
