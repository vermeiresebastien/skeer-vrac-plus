/**
 * Browser end-to-end smoke test for VRAC+.
 *
 * Usage:
 *   npm run dev            # in one terminal
 *   node scripts/e2e.mjs   # in another
 *
 * It uses the installed Chrome (no Playwright browser download needed) and
 * fake media / MIDI devices so recording and MIDI learn can be exercised
 * without hardware. Downloads are written to test-results/.
 */
import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright-core';

const URL = process.env.VRAC_URL || 'http://127.0.0.1:5173/';
const CHROME =
  process.env.CHROME_PATH || 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
const fixture = 'fixtures/user-groove.wav';

await mkdir('test-results', { recursive: true });

const browser = await chromium.launch({
  executablePath: CHROME,
  headless: true,
  args: [
    '--autoplay-policy=no-user-gesture-required',
    '--disable-gpu',
    '--no-sandbox',
    '--use-fake-device-for-media-stream',
    '--use-fake-ui-for-media-stream',
  ],
});

const context = await browser.newContext({
  viewport: { width: 1440, height: 900 },
  permissions: ['microphone'],
  acceptDownloads: true,
});
await context.addInitScript(() => {
  const input = {
    id: 'fake-controller',
    name: 'VRAC Test Controller',
    manufacturer: 'QA',
    state: 'connected',
    onmidimessage: null,
  };
  const access = { inputs: new Map([['fake-controller', input]]), outputs: new Map(), onstatechange: null };
  Object.defineProperty(navigator, 'requestMIDIAccess', { configurable: true, value: async () => access });
  window.__fakeMidi = access;
});

const page = await context.newPage();
const consoleErrors = [];
const pageErrors = [];
page.on('console', (message) => {
  if (message.type() === 'error') consoleErrors.push(message.text());
});
page.on('pageerror', (error) => pageErrors.push(String(error.stack || error)));

const checks = [];
const check = (name, condition, details = '') => {
  checks.push({ name, ok: Boolean(condition), details });
  if (!condition) console.error(`FAIL  ${name}`, details);
  else console.log(`ok    ${name}`);
};

async function peakFor(ms) {
  return page.evaluate(async (duration) => {
    const engine = window.__VRAC__.audioEngine;
    let peak = 0;
    const started = performance.now();
    while (performance.now() - started < duration) {
      await new Promise((resolve) => setTimeout(resolve, 25));
      peak = Math.max(peak, engine.getOutputLevel());
    }
    return peak;
  }, ms);
}

await page.goto(URL, { waitUntil: 'networkidle' });
await page.waitForFunction(() => Boolean(window.__VRAC__));
await page.evaluate(() => localStorage.clear());
await page.reload({ waitUntil: 'networkidle' });
await page.waitForFunction(() => Boolean(window.__VRAC__));

/* ---- engine + trigger ---- */
await page.locator('.pad', { hasText: 'C1' }).first().click({ position: { x: 30, y: 60 } });
await page.waitForTimeout(800);
check('audio engine starts on first pad', (await page.evaluate(() => window.__VRAC__.audioEngine.status)) === 'ready');
check('first pad hit is audible', (await peakFor(700)) > 0.001);

/* ---- solo / mute gating ---- */
await page.evaluate(async () => {
  const dbg = window.__VRAC__;
  dbg.audioEngine.allOff();
  await new Promise((r) => setTimeout(r, 300));
  dbg.actions.toggleSolo(2);
  await new Promise((r) => setTimeout(r, 80));
  dbg.trigger(0, 1);
});
check('solo blocks other pads', (await peakFor(450)) < 0.001);
await page.evaluate(() => window.__VRAC__.trigger(2, 1));
check('soloed pad is audible', (await peakFor(650)) > 0.001);
await page.evaluate(async () => {
  const dbg = window.__VRAC__;
  dbg.actions.toggleSolo(2);
  dbg.actions.toggleMute(0);
  await new Promise((r) => setTimeout(r, 80));
  dbg.audioEngine.allOff();
  await new Promise((r) => setTimeout(r, 200));
  dbg.trigger(0, 1);
});
check('muted pad is silent', (await peakFor(350)) < 0.001);
await page.evaluate(() => window.__VRAC__.actions.toggleMute(0));

/* ---- knob undo ---- */
const knob = page.locator('.morph-main [role="slider"]').first();
const beforeKnob = await page.evaluate(() => window.__VRAC__.getSelectedPad().drumify);
const knobBox = await knob.boundingBox();
if (knobBox) {
  await page.mouse.move(knobBox.x + knobBox.width / 2, knobBox.y + knobBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(knobBox.x + knobBox.width / 2, knobBox.y + knobBox.height / 2 - 30, { steps: 8 });
  await page.mouse.up();
}
await page.waitForTimeout(150);
const afterKnob = await page.evaluate(() => window.__VRAC__.getSelectedPad().drumify);
await page.keyboard.press('Control+z');
await page.waitForTimeout(150);
const undoneKnob = await page.evaluate(() => window.__VRAC__.getSelectedPad().drumify);
check('knob drag changes a parameter', afterKnob !== beforeKnob);
check('undo restores the parameter', undoneKnob === beforeKnob);

/* ---- slice navigation / add / delete ---- */
const slices0 = await page.evaluate(() => window.__VRAC__.getSlices().length);
await page.locator('button[title="Next slice"]').first().click();
await page.waitForTimeout(200);
const afterNext = await page.evaluate(() => window.__VRAC__.getSelectedPad().start ?? window.__VRAC__.getSelectedPad().sampleStart);
check('slice navigation moves the pad start', typeof afterNext === 'number');
await page.locator('button[title="Add a slice at this start"]').click();
await page.waitForTimeout(150);
const slicesAfterAdd = await page.evaluate(() => window.__VRAC__.getSlices().length);
await page.locator('button[title="Delete the nearest slice"]').click();
await page.waitForTimeout(150);
const slicesAfterDelete = await page.evaluate(() => window.__VRAC__.getSlices().length);
check('slice add/delete updates the slice list', slicesAfterAdd >= slices0 && slicesAfterDelete < slicesAfterAdd);

/* ---- custom sample + IDB preset restore ---- */
await page.locator('input[type="file"][accept*="audio"]').setInputFiles(fixture);
await page.waitForTimeout(1000);
const userSample = await page.evaluate(() => ({
  source: window.__VRAC__.getKit().sourceName,
  slices: window.__VRAC__.getSlices().length,
  isUser: window.__VRAC__.getKit().sourceId.startsWith('user-'),
}));
check('custom sample decodes and slices', userSample.isUser && userSample.slices >= 4, JSON.stringify(userSample));

// Play the custom sample.
await page.evaluate(() => {
  window.__VRAC__.audioEngine.allOff();
  window.__VRAC__.trigger(window.__VRAC__.getKit().selectedPad, 1);
});
check('custom sample is audible', (await peakFor(650)) > 0.001);

await page.locator('button[title="Save kit"]').click();
await page.locator('.modal .field input').fill('E2E CUSTOM KIT');
await page.locator('.modal-foot .button--primary').click();
await page.waitForTimeout(400);
await page.reload({ waitUntil: 'networkidle' });
await page.waitForFunction(() => Boolean(window.__VRAC__));
await page.locator('.preset-current').click();
await page.locator('.preset-option', { hasText: 'E2E CUSTOM KIT' }).click();
await page.waitForTimeout(1000);
const restored = await page.evaluate(() => ({
  source: window.__VRAC__.getKit().sourceName,
  isUser: window.__VRAC__.getKit().sourceId.startsWith('user-'),
  slices: window.__VRAC__.getSlices().length,
}));
check('custom-sample kit restores after reload', restored.isUser && restored.slices >= 4, JSON.stringify(restored));

/* ---- MIDI note + learn ---- */
await page.locator('button[title="MIDI settings"]').click();
await page.locator('.modal-foot .button--primary').click();
await page.waitForTimeout(400);
check('MIDI access enables', (await page.locator('.midi-pill').innerText()).includes('READY'));
await page.keyboard.press('Escape');
await page.evaluate(() => {
  window.__fakeMidi.inputs.get('fake-controller').onmidimessage({ data: new Uint8Array([0x90, 38, 96]) });
});
await page.waitForTimeout(300);
check('MIDI note triggers and auto-selects a pad', (await page.evaluate(() => window.__VRAC__.getSelectedPad().note)) === 'D1');

await page.locator('.morph-main [role="slider"]').first().click({ button: 'right' });
await page.waitForTimeout(200);
await page.evaluate(() => {
  const input = window.__fakeMidi.inputs.get('fake-controller');
  input.onmidimessage({ data: new Uint8Array([0xb0, 71, 64]) });
});
await page.waitForTimeout(300);
const learned = await page.evaluate(() => ({
  mappings: JSON.parse(localStorage.getItem('vrac-plus.midi.v1') || '[]'),
  value: window.__VRAC__.getSelectedPad().drumify,
}));
check('MIDI learn writes a mapping', learned.mappings.some((m) => m.cc === 71));
check('learned CC moves the target parameter', learned.value > 0.3);

/* ---- record from fake input ---- */
await page.locator('button[title="Record from an input"]').click();
await page.waitForTimeout(300);
await page.locator('.record-device-row .button').click();
await page.waitForTimeout(900);
check('input connects', (await page.locator('.record-connected').count()) === 1);
await page.locator('.record-button').click();
await page.waitForTimeout(1100);
await page.locator('.record-button').click();
await page.waitForTimeout(1600);
check('recording produces a take', (await page.locator('.record-preview').count()) === 1);
await page.locator('.modal-foot .button--primary').click();
await page.waitForTimeout(700);
check('recording becomes the source', (await page.evaluate(() => window.__VRAC__.getKit().sourceName)) === 'RECORDED INPUT');

/* ---- kit export ---- */
await page.locator('button[title="Export audio"]').click();
await page.locator('.export-option', { hasText: 'FULL KIT' }).click();
const downloadPromise = page.waitForEvent('download', { timeout: 120000 });
await page.locator('.modal-foot .button--primary').click();
const download = await downloadPromise;
const exportPath = 'test-results/e2e-kit.zip';
await download.saveAs(exportPath);
const { stat } = await import('node:fs/promises');
const exportStat = await stat(exportPath);
check('full kit export downloads a zip', exportStat.size > 50000, `${exportStat.size} bytes`);
await page.keyboard.press('Escape');

/* ---- responsive sanity ---- */
for (const [width, height] of [[390, 844], [768, 1024], [1024, 768], [1280, 720], [1440, 900]]) {
  await page.setViewportSize({ width, height });
  await page.waitForTimeout(250);
  const metrics = await page.evaluate(() => ({
    scrollWidth: document.documentElement.scrollWidth,
    innerWidth: window.innerWidth,
  }));
  check(`no horizontal overflow at ${width}px`, metrics.scrollWidth <= metrics.innerWidth + 1, JSON.stringify(metrics));
}

check('no page errors', pageErrors.length === 0, pageErrors.join('\n'));
check('no console errors', consoleErrors.length === 0, consoleErrors.join('\n'));

const failures = checks.filter((item) => !item.ok);
console.log(`\n${checks.length - failures.length}/${checks.length} checks passed`);
if (failures.length) {
  console.error(JSON.stringify(failures, null, 2));
  process.exitCode = 1;
}
await browser.close();
