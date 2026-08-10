#!/usr/bin/env node
/*
 * Captures real screenshots of the published Pages site.
 *
 *   node ui-md3/scripts/capture-site.mjs <url> <outputDir>
 *
 * Every image is a genuine capture of the running site through headless Chrome
 * and the DevTools protocol — the same browser and the same page the layout
 * gate measures. Nothing here is a mockup, and no shot is reused for a surface
 * it does not show.
 */
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { suppressAutomaticDimSum } from './browser-test-mode.mjs';

const PAGE_URL = process.argv[2] || 'http://127.0.0.1:4173/index.html';
const OUT_DIR = path.resolve(process.argv[3] || 'docs/screenshots/pages');
const ONLY_CAPTURE = process.env.BAMBU_CAPTURE_ONLY?.trim() || '';
const knownCaptures = new Set();
const completedCaptures = new Set();
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function chromePath() {
  return [
    process.env.CHROME_PATH,
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
    '/usr/bin/google-chrome',
    '/usr/bin/chromium',
  ].filter(Boolean).find(existsSync);
}

class Session {
  constructor(url) {
    this.socket = new WebSocket(url);
    this.nextId = 1;
    this.pending = new Map();
    this.waiters = new Map();
  }
  async connect() {
    await new Promise((resolve, reject) => {
      this.socket.addEventListener('open', resolve, { once: true });
      this.socket.addEventListener('error', reject, { once: true });
    });
    this.socket.addEventListener('message', (event) => {
      const message = JSON.parse(event.data);
      if (message.id) {
        const pending = this.pending.get(message.id);
        if (!pending) return;
        this.pending.delete(message.id);
        if (message.error) pending.reject(new Error(message.error.message));
        else pending.resolve(message.result);
        return;
      }
      const waiters = this.waiters.get(message.method);
      if (!waiters?.length) return;
      this.waiters.delete(message.method);
      for (const resolve of waiters) resolve(message.params);
    });
  }
  send(method, params = {}) {
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.socket.send(JSON.stringify({ id, method, params }));
    });
  }
  waitFor(method) {
    return new Promise((resolve) => {
      const waiters = this.waiters.get(method) ?? [];
      waiters.push(resolve);
      this.waiters.set(method, waiters);
    });
  }
  close() { this.socket.close(); }
}

const executable = chromePath();
assert.ok(executable, 'Chrome or Edge is required to capture the site');
const profile = await mkdtemp(path.join(tmpdir(), 'bambu-pages-capture-'));
const chrome = spawn(executable, [
  '--headless=new', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
  '--disable-extensions', '--hide-scrollbars', '--force-color-profile=srgb',
  '--remote-debugging-port=0', `--user-data-dir=${profile}`, 'about:blank',
], { stdio: ['ignore', 'ignore', 'ignore'], windowsHide: true });

async function devToolsPort() {
  const file = path.join(profile, 'DevToolsActivePort');
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    try {
      const [port] = (await readFile(file, 'utf8')).trim().split(/\r?\n/);
      if (/^\d+$/.test(port)) return Number(port);
    } catch { /* Chrome writes the file once it is listening. */ }
    await delay(50);
  }
  throw new Error('Chrome did not publish a DevTools port');
}

const port = await devToolsPort();
const created = await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, { method: 'PUT' });
const page = await created.json();
const session = new Session(page.webSocketDebuggerUrl);
await session.connect();
await session.send('Page.enable');
await session.send('Runtime.enable');
await suppressAutomaticDimSum(session);
await mkdir(OUT_DIR, { recursive: true });

async function viewport(width, height = 900, scale = 1) {
  await session.send('Emulation.setDeviceMetricsOverride', {
    width, height, deviceScaleFactor: scale, mobile: false,
  });
}

async function open(query) {
  const url = new URL(PAGE_URL);
  for (const [key, value] of Object.entries(query || {})) url.searchParams.set(key, value);
  const loaded = session.waitFor('Page.loadEventFired');
  await session.send('Page.navigate', { url: url.href });
  await loaded;
  await session.send('Runtime.evaluate', {
    expression: 'Promise.race([document.fonts.ready, new Promise(r => setTimeout(r, 4000))])',
    awaitPromise: true,
  });
  await delay(250);
}

async function evaluate(expression) {
  const result = await session.send('Runtime.evaluate', { expression, returnByValue: true });
  return result.result.value;
}

async function shoot(name, options) {
  knownCaptures.add(name);
  if (ONLY_CAPTURE && name !== ONLY_CAPTURE) return;
  const settings = options || {};
  let clip;
  if (settings.selector) {
    const box = await evaluate(`JSON.stringify((() => {
      const element = document.querySelector(${JSON.stringify(settings.selector)});
      if (!element) return null;
      const rect = element.getBoundingClientRect();
      const pad = ${settings.pad || 0};
      return { x: Math.max(0, rect.x - pad), y: Math.max(0, rect.y - pad),
        width: rect.width + pad * 2, height: rect.height + pad * 2 };
    })())`);
    if (!box) throw new Error(`Nothing matched ${settings.selector} for ${name}`);
    clip = { ...JSON.parse(box), scale: 1 };
  }
  const shot = await session.send('Page.captureScreenshot', {
    format: 'png',
    captureBeyondViewport: Boolean(clip),
    ...(clip ? { clip } : {}),
  });
  const file = path.join(OUT_DIR, `${name}.png`);
  await writeFile(file, Buffer.from(shot.data, 'base64'));
  completedCaptures.add(name);
  console.log(`captured ${path.basename(file)}`);
}

try {
  // Full tabs at desktop width, dark theme (the site's default).
  await viewport(1280, 980);
  for (const tab of ['overview', 'screens', 'materialyou', 'download', 'changelog', 'regex', 'settings', 'build']) {
    await open({ tab });
    await shoot(`tab-${tab}`);
  }

  // The tab strip alone: full labels, then the overflow menu it falls back to.
  await open({ tab: 'overview' });
  await shoot('tabstrip-wide', { selector: 'header', pad: 0 });

  await viewport(420, 900);
  await open({ tab: 'overview' });
  await evaluate("document.querySelector('.tabstrip-overflow').click()");
  await delay(200);
  await shoot('tabstrip-overflow-menu', { selector: 'header' });

  // Settings: the two funny sliders and the cross-tab search result.
  await viewport(1280, 980);
  await open({ tab: 'settings' });
  await shoot('settings-language-and-funny', { selector: '.settings-group[data-group="settings.group.language"]', pad: 8 });
  await evaluate(`(() => {
    const input = document.querySelector('#panel-settings .sf-input');
    input.value = 'accent';
    input.dispatchEvent(new Event('input', { bubbles: true }));
  })()`);
  await delay(200);
  await shoot('settings-search-cross-tab', { selector: '#panel-settings', pad: 0 });

  // The regex lab with a live match set.
  await open({ tab: 'regex' });
  await delay(400);
  await shoot('regex-lab-matches', { selector: '.regex-lab', pad: 8 });

  // The changelog's calendar range picker, opened on the From field.
  await open({ tab: 'changelog' });
  await evaluate("document.querySelector('.calendar-open[data-for=\"from\"]').click()");
  await delay(250);
  await shoot('changelog-calendar', { selector: '.changelog-controls', pad: 8 });

  // Bilingual mode at a narrow width — the widest labels the site has to hold.
  await viewport(420, 900);
  await open({ tab: 'overview', lang: 'bilingual_en_yue_HK' });
  await shoot('bilingual-narrow');
  await open({ tab: 'settings', lang: 'yue_HK' });
  await shoot('settings-cantonese-narrow');

  // The dim sum card. The 10% draw is not forced in normal use; this capture
  // constructs the same public-photo surface directly so it can be documented.
  await viewport(1280, 980);
  await open({ tab: 'overview' });
  if (!ONLY_CAPTURE || ONLY_CAPTURE === 'dim-sum-card') {
    const dimSumRender = await session.send('Runtime.evaluate', { expression: `(() => {
      const dish = window.BAMBU_DIM_SUM.dishes[0];
      return window.BambuSite.renderDimSumSurprise(dish);
    })()`, awaitPromise: true, returnByValue: true });
    assert.equal(dimSumRender.exceptionDetails, undefined,
      dimSumRender.exceptionDetails?.exception?.description || 'Dim sum renderer threw');
    assert.equal(dimSumRender.result.value, true, 'Public dim sum photo did not load and decode');
  }
  await shoot('dim-sum-card', { selector: '.dimsum', pad: 0 });

  // Light theme, so the palette is documented in both schemes.
  await open({ tab: 'materialyou' });
  await evaluate("window.BambuSite.set('theme','light'); window.BambuSite.applyAppearance();");
  await delay(250);
  await shoot('material-you-light');

  if (ONLY_CAPTURE) {
    assert.ok(knownCaptures.has(ONLY_CAPTURE),
      `BAMBU_CAPTURE_ONLY names an unknown capture: ${ONLY_CAPTURE}`);
    assert.ok(completedCaptures.has(ONLY_CAPTURE),
      `BAMBU_CAPTURE_ONLY did not complete the requested capture: ${ONLY_CAPTURE}`);
  }
} finally {
  try { await session.send('Browser.close'); } catch { chrome.kill(); }
  session.close();
  await delay(500);
  await rm(profile, { recursive: true, force: true, maxRetries: 20, retryDelay: 100 });
}
