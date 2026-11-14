import { chromium } from 'playwright-core';
import { mkdir } from 'node:fs/promises';

const browser = await chromium.launch({
  executablePath: 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  headless: true,
  args: ['--autoplay-policy=no-user-gesture-required', '--disable-gpu', '--no-sandbox'],
});
await mkdir('docs/screenshots', { recursive: true });
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
await page.goto('http://127.0.0.1:5173/', { waitUntil: 'networkidle' });
await page.waitForFunction(() => Boolean(window.__SLICEFORGE__));

// Start the engine, then show a live hit on the selected pad.
await page.locator('.audio-status').click();
await page.waitForTimeout(900);
await page.evaluate(() => window.__SLICEFORGE__.trigger(3, 1));
await page.waitForTimeout(70);
await page.screenshot({ path: 'docs/screenshots/sliceforge-light.png' });

// Dark theme
await page.locator('button[title="Toggle light/dark theme"]').click();
await page.waitForTimeout(400);
await page.evaluate(() => window.__SLICEFORGE__.trigger(3, 1));
await page.waitForTimeout(70);
await page.screenshot({ path: 'docs/screenshots/sliceforge-dark.png' });

// Back to light, capture the library
await page.locator('button[title="Toggle light/dark theme"]').click();
await page.waitForTimeout(300);
await page.locator('button[title="Open the sample library"]').click();
await page.waitForTimeout(2300);
await page.screenshot({ path: 'docs/screenshots/sliceforge-library.png' });
await page.locator('.modal-close').click();

// Help
await page.locator('button[title="Help & shortcuts"]').click();
await page.waitForTimeout(300);
await page.screenshot({ path: 'docs/screenshots/sliceforge-help.png' });
await page.locator('.modal-close').click();
await page.evaluate(() => (document.activeElement instanceof HTMLElement ? document.activeElement.blur() : undefined));

// Mobile full page
await page.setViewportSize({ width: 390, height: 844 });
await page.waitForTimeout(300);
await page.screenshot({ path: 'docs/screenshots/sliceforge-mobile.png', fullPage: true });

await browser.close();
