#!/usr/bin/env node
/*
 * Captures the interactive prototype's surfaces, and dumps the accessibility
 * evidence for the fixes that have no visible surface.
 *
 *   node ui-md3/scripts/capture-app.mjs <appUrl> <outputDir>
 *
 * Everything here is measured from a real headless Chrome against the real
 * files — accessible names come from Chrome's own AX tree, not from reading the
 * markup and assuming.
 */
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const APP_URL = process.argv[2] || 'http://127.0.0.1:4173/app/index.html';
const OUT_DIR = path.resolve(process.argv[3] || 'docs/screenshots/pages/app');
const delay = (ms) => new Promise((r) => setTimeout(r, ms));

function chromePath() {
  return [
    process.env.CHROME_PATH,
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
    '/usr/bin/google-chrome',
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
assert.ok(executable, 'Chrome or Edge is required');
const profile = await mkdtemp(path.join(tmpdir(), 'bambu-app-capture-'));
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
    } catch { /* written once listening */ }
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
await session.send('Accessibility.enable');
await mkdir(OUT_DIR, { recursive: true });

async function viewport(width, height = 900, scale = 1) {
  await session.send('Emulation.setDeviceMetricsOverride', {
    width, height, deviceScaleFactor: scale, mobile: false,
  });
}
async function open(query) {
  const url = new URL(APP_URL);
  for (const [k, v] of Object.entries(query || {})) url.searchParams.set(k, v);
  const loaded = session.waitFor('Page.loadEventFired');
  await session.send('Page.navigate', { url: url.href });
  await loaded;
  await session.send('Runtime.evaluate', {
    expression: 'Promise.race([document.fonts.ready, new Promise(r => setTimeout(r, 4000))])',
    awaitPromise: true,
  });
  await delay(400);
}
async function evaluate(expression) {
  const result = await session.send('Runtime.evaluate', { expression, returnByValue: true });
  return result.result.value;
}

/*
 * The prototype's runtime batches its re-render, so a probe that types into a
 * field and reads the DOM in the same turn measures the state before the
 * filter ran — and reports a working filter as broken.
 */
async function evaluateAfterRender(expression) {
  const result = await session.send('Runtime.evaluate', {
    expression,
    awaitPromise: true,
    returnByValue: true,
  });
  return result.result.value;
}
const settle = 'new Promise(r => setTimeout(r, 120))';
async function shoot(name, selector, pad = 0) {
  let clip;
  if (selector) {
    const box = await evaluate(`JSON.stringify((() => {
      const el = document.querySelector(${JSON.stringify(selector)});
      if (!el) return null;
      const r = el.getBoundingClientRect();
      return { x: Math.max(0, r.x - ${pad}), y: Math.max(0, r.y - ${pad}),
        width: r.width + ${pad * 2}, height: r.height + ${pad * 2} };
    })())`);
    if (!box) throw new Error(`no element for ${name}: ${selector}`);
    clip = { ...JSON.parse(box), scale: 1 };
  }
  const shot = await session.send('Page.captureScreenshot', {
    format: 'png', captureBeyondViewport: Boolean(clip), ...(clip ? { clip } : {}),
  });
  await writeFile(path.join(OUT_DIR, `${name}.png`), Buffer.from(shot.data, 'base64'));
  console.log(`captured ${name}.png`);
}

const evidence = {};

try {
  // #4 — the title bar at the width where the window controls used to be
  // 217px past the viewport edge with no scrollbar to reach them.
  await viewport(640, 700);
  await open({ view: 'prepare' });
  await shoot('titlebar-640', '.titlebar', 4);
  evidence.titleBarAt640 = JSON.parse(await evaluate(`JSON.stringify((() => {
    const bar = document.querySelector('.titlebar');
    const close = [...document.querySelectorAll('.tb-controls button')].pop();
    const r = close.getBoundingClientRect();
    return {
      viewportWidth: document.documentElement.clientWidth,
      titleBarScrollWidth: bar.scrollWidth,
      closeButtonRight: Math.round(r.right),
      closeButtonReachable: r.right <= document.documentElement.clientWidth + 1,
      documentScrollWidth: document.documentElement.scrollWidth,
    };
  })())`));

  // #5 — a search field that actually filters, and its empty state.
  await viewport(1280, 900);
  await open({ view: 'filament' });
  await shoot('ink-search-before', '.titlebar ~ *:last-child', 0).catch(() => shoot('ink-search-before'));
  evidence.search = JSON.parse(await evaluateAfterRender(`(async () => {
    const set = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
    const input = document.querySelector('input[placeholder="Search inks"]');
    // Count the preset rows themselves. Counting every "Bambu …" on the page
    // also counts the product name in the title bar and each row's vendor
    // cell, which is how a passing filter can look like a failing one.
    const PRESETS = /Bambu (PLA Basic|PLA Matte|PETG HF|ABS|Support|TPU 95A)/g;
    const rows = () => new Set(document.body.innerText.match(PRESETS) || []).size;
    const type = async (value) => {
      set.call(input, value);
      input.dispatchEvent(new Event('input', { bubbles: true }));
      await ${settle};
    };
    const before = rows();
    await type('petg');
    const petg = { count: rows(), keepsPetg: /Bambu PETG HF/.test(document.body.innerText), dropsAbs: !/Bambu ABS/.test(document.body.innerText) };
    await type('.');
    const dot = { count: rows(), empty: /No inks match/i.test(document.body.innerText) };
    await type('');
    return JSON.stringify({
      rowsUnfiltered: before,
      plainPetg: petg,
      literalDot: dot,
      restoredAfterClear: rows() === before
    });
  })()`));
  await evaluate(`(() => {
    const set = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
    const input = document.querySelector('input[placeholder="Search inks"]');
    set.call(input, 'petg'); input.dispatchEvent(new Event('input', { bubbles: true }));
  })()`);
  await delay(300);
  await shoot('ink-search-filtered');

  // #1 and #2 — accessible names and switch state, from Chrome's own AX tree.
  await open({ view: 'settings' });
  await shoot('settings-switches', 'body', 0);
  const tree = await session.send('Accessibility.getFullAXTree');
  const switches = tree.nodes.filter((n) => n.role?.value === 'switch');
  evidence.switches = switches.map((n) => ({
    name: n.name?.value,
    checked: n.properties?.find((p) => p.name === 'checked')?.value?.value,
  }));
  const unnamedButtons = tree.nodes.filter((n) =>
    n.role?.value === 'button' && (!n.name?.value || !n.name.value.trim()));
  evidence.unnamedButtons = unnamedButtons.length;

  await open({ view: 'prepare' });
  const prepareTree = await session.send('Accessibility.getFullAXTree');
  evidence.gizmoNames = prepareTree.nodes
    .filter((n) => n.role?.value === 'button' && /Move|Rotate|Scale|Place|Support|Seam|Cut/i.test(n.name?.value || ''))
    .map((n) => n.name.value)
    .slice(0, 8);
  evidence.ligatureNames = prepareTree.nodes
    .filter((n) => /^(open_with|rotate_right|vertical_align_bottom|blur_on|compress|content_cut)$/.test(n.name?.value || ''))
    .length;

  // #3 — dialog semantics, measured by driving the drawer.
  evidence.dialog = JSON.parse(await evaluate(`JSON.stringify((() => {
    const opener = [...document.querySelectorAll('button')].find(b => /account_tree/.test(b.textContent));
    opener.focus(); opener.click();
    return { opened: true };
  })())`));
  await delay(400);
  await shoot('version-history-dialog', '[role="dialog"]', 8);
  evidence.dialog = JSON.parse(await evaluate(`JSON.stringify((() => {
    const dialog = document.querySelector('[role="dialog"]');
    const behind = [...document.querySelectorAll('button')].find(b => !b.closest('[role=dialog]') && b.offsetParent !== null);
    behind && behind.focus();
    return {
      role: dialog.getAttribute('role'),
      ariaModal: dialog.getAttribute('aria-modal'),
      focusInsideDialog: !!(document.activeElement && document.activeElement.closest('[role=dialog]')),
      inertAncestorSiblings: document.querySelectorAll('#app [inert]').length,
      focusRefusedBehindScrim: document.activeElement !== behind,
    };
  })())`));
  await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))`);
  await delay(300);
  evidence.dialogAfterEscape = JSON.parse(await evaluate(`JSON.stringify({
    dialogGone: !document.querySelector('[role="dialog"]'),
    focusBackOnOpener: /account_tree/.test((document.activeElement && document.activeElement.textContent) || ''),
    inertCleared: document.querySelectorAll('#app [inert]').length === 0
  })`));

  await writeFile(path.join(OUT_DIR, 'evidence.json'), `${JSON.stringify(evidence, null, 2)}\n`);
  console.log(JSON.stringify(evidence, null, 2));
} finally {
  try { await session.send('Browser.close'); } catch { chrome.kill(); }
  session.close();
  await delay(500);
  await rm(profile, { recursive: true, force: true, maxRetries: 20, retryDelay: 100 });
}
