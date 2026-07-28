import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';

const PAGE_URL = process.env.BAMBU_PAGES_TEST_URL;
const PHYSICAL_WIDTHS = [320, 360, 420, 421, 560, 561, 640, 641, 860, 861, 1120, 1121, 1280];
const ZOOM_FACTORS = [1, 1.25, 1.5, 2];
const LANGUAGE_MODES = ['en', 'yue_HK', 'bilingual_en_yue_HK'];

const delay = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));

function chromePath() {
  const candidates = [
    process.env.CHROME_PATH,
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    '/usr/bin/google-chrome',
    '/usr/bin/chromium',
    '/usr/bin/chromium-browser',
  ].filter(Boolean);
  return candidates.find(existsSync);
}

async function waitForDevToolsPort(profileDirectory) {
  const portFile = join(profileDirectory, 'DevToolsActivePort');
  const deadline = Date.now() + 15_000;
  while (Date.now() < deadline) {
    try {
      const [port] = (await readFile(portFile, 'utf8')).trim().split(/\r?\n/);
      if (/^\d+$/.test(port))
        return Number(port);
    } catch {
      // Chrome creates the file after its browser process starts listening.
    }
    await delay(50);
  }
  throw new Error('Chrome did not publish a DevTools port within 15 seconds');
}

class DevToolsSession {
  constructor(url) {
    this.socket = new WebSocket(url);
    this.nextId = 1;
    this.pending = new Map();
    this.eventWaiters = new Map();
  }

  async connect() {
    await new Promise((resolve, reject) => {
      this.socket.addEventListener('open', resolve, { once: true });
      this.socket.addEventListener('error', reject, { once: true });
    });
    this.socket.addEventListener('message', event => {
      const message = JSON.parse(event.data);
      if (message.id) {
        const pending = this.pending.get(message.id);
        if (!pending)
          return;
        this.pending.delete(message.id);
        if (message.error)
          pending.reject(new Error(message.error.message));
        else
          pending.resolve(message.result);
        return;
      }
      const waiters = this.eventWaiters.get(message.method);
      if (!waiters?.length)
        return;
      this.eventWaiters.delete(message.method);
      for (const resolve of waiters)
        resolve(message.params);
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
    return new Promise(resolve => {
      const waiters = this.eventWaiters.get(method) ?? [];
      waiters.push(resolve);
      this.eventWaiters.set(method, waiters);
    });
  }

  close() {
    this.socket.close();
  }
}

async function startChrome() {
  const executable = chromePath();
  assert.ok(executable, 'Chrome or Edge is required for runtime clipping checks');
  const profileDirectory = await mkdtemp(join(tmpdir(), 'bambu-pages-layout-'));
  const processHandle = spawn(executable, [
    '--headless=new',
    '--disable-background-networking',
    '--disable-component-update',
    '--disable-default-apps',
    '--disable-extensions',
    '--disable-features=Translate',
    '--disable-gpu',
    '--no-first-run',
    '--no-default-browser-check',
    '--remote-debugging-port=0',
    `--user-data-dir=${profileDirectory}`,
    'about:blank',
  ], {
    stdio: ['ignore', 'ignore', 'pipe'],
    windowsHide: true,
  });

  let stderr = '';
  processHandle.stderr.on('data', chunk => {
    if (stderr.length < 16_384)
      stderr += chunk.toString();
  });

  try {
    const port = await waitForDevToolsPort(profileDirectory);
    const created = await fetch(`http://127.0.0.1:${port}/json/new?about:blank`, {
      method: 'PUT',
    });
    assert.equal(created.ok, true, `Could not create a headless browser page: ${stderr}`);
    const page = await created.json();
    const session = new DevToolsSession(page.webSocketDebuggerUrl);
    await session.connect();
    await session.send('Page.enable');
    await session.send('Runtime.enable');
    return { processHandle, profileDirectory, session };
  } catch (error) {
    processHandle.kill();
    await rm(profileDirectory, { recursive: true, force: true });
    throw error;
  }
}

async function stopChrome(chrome) {
  let exited = chrome.processHandle.exitCode !== null;
  const exitPromise = exited
    ? Promise.resolve()
    : new Promise(resolve => chrome.processHandle.once('exit', () => {
        exited = true;
        resolve();
      }));
  try {
    await chrome.session.send('Browser.close');
  } catch {
    chrome.processHandle.kill();
  }
  await Promise.race([exitPromise, delay(5_000)]);
  chrome.session.close();
  if (!exited) {
    chrome.processHandle.kill('SIGKILL');
    await Promise.race([exitPromise, delay(2_000)]);
  }
  await rm(chrome.profileDirectory, {
    recursive: true,
    force: true,
    maxRetries: 20,
    retryDelay: 100,
  });
}

async function navigate(session, url) {
  const loaded = session.waitFor('Page.loadEventFired');
  await session.send('Page.navigate', { url });
  await loaded;
  const fonts = await session.send('Runtime.evaluate', {
    expression: `Promise.race([
      document.fonts.ready.then(() => true),
      new Promise(resolve => setTimeout(() => resolve(false), 5000))
    ])`,
    awaitPromise: true,
    returnByValue: true,
  });
  assert.equal(
    fonts.result.value,
    true,
    'Page fonts did not become ready within 5 seconds'
  );
}

async function measure(session) {
  const expression = `JSON.stringify((() => {
    const visible = element => {
      const style = getComputedStyle(element);
      const rect = element.getBoundingClientRect();
      return style.display !== 'none' && style.visibility !== 'hidden' &&
        rect.width > 0 && rect.height > 0;
    };
    const rectOf = element => {
      const rect = element.getBoundingClientRect();
      return {
        name: element.getAttribute('aria-label') ||
          element.textContent.trim().replace(/\\s+/g, ' ').slice(0, 80) ||
          element.id || element.tagName,
        left: rect.left,
        right: rect.right,
        top: rect.top,
        bottom: rect.bottom,
        width: rect.width,
        height: rect.height,
      };
    };
    const headerElements = [...document.querySelectorAll(
      'header .brand, header a, header button, header select'
    )].filter(visible).map(rectOf);
    const headerOverlaps = [];
    for (let left = 0; left < headerElements.length; left++) {
      for (let right = left + 1; right < headerElements.length; right++) {
        const a = headerElements[left];
        const b = headerElements[right];
        const overlapWidth = Math.min(a.right, b.right) - Math.max(a.left, b.left);
        const overlapHeight = Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top);
        if (overlapWidth > 1 && overlapHeight > 1)
          headerOverlaps.push([a.name, b.name]);
      }
    }
    const controls = [...document.querySelectorAll(
      'header button, header select, header a'
    )].filter(visible).map(rectOf);
    const cards = [...document.querySelectorAll('.card')].map(card => {
      const outer = card.getBoundingClientRect();
      const content = card.querySelector('.card-copy') || card;
      const inner = content.getBoundingClientRect();
      return {
        outerBottom: outer.bottom,
        innerBottom: inner.bottom,
        outerRight: outer.right,
        innerRight: inner.right,
      };
    });
    const overflowOffenders = [...document.querySelectorAll('body *')]
      .filter(visible)
      .map(element => {
        const rect = element.getBoundingClientRect();
        return {
          selector: [
            element.tagName.toLowerCase(),
            element.id ? '#' + element.id : '',
            [...element.classList].map(name => '.' + name).join(''),
          ].join(''),
          left: Math.round(rect.left * 100) / 100,
          right: Math.round(rect.right * 100) / 100,
          width: Math.round(rect.width * 100) / 100,
          scrollWidth: element.scrollWidth,
          intentionallyHidden:
            element.matches('.brand-name') && rect.width <= 1.5 && rect.height <= 1.5,
        };
      })
      .filter(element =>
        !element.intentionallyHidden && (
          element.left < -1 ||
          element.right > document.documentElement.clientWidth + 1 ||
          element.scrollWidth > Math.ceil(element.width) + 1
        )
      )
      .sort((a, b) =>
        Math.max(b.right - document.documentElement.clientWidth, b.scrollWidth - b.width) -
        Math.max(a.right - document.documentElement.clientWidth, a.scrollWidth - a.width)
      )
      .slice(0, 8);
    return {
      innerWidth,
      clientWidth: document.documentElement.clientWidth,
      scrollWidth: document.documentElement.scrollWidth,
      selectedLanguage: document.getElementById('languageMode').value,
      headerElements,
      headerOverlaps,
      controls,
      overflowOffenders,
      cardOverflow: cards.filter(card =>
        card.innerBottom > card.outerBottom + 1 ||
        card.innerRight > card.outerRight + 1
      ).length,
    };
  })())`;
  const evaluated = await session.send('Runtime.evaluate', {
    expression,
    returnByValue: true,
  });
  return JSON.parse(evaluated.result.value);
}

/*
 * Every tab panel, measured in one page load per viewport.
 *
 * Panels render lazily, so a tab that is never activated is never measured;
 * activating them in-page is both faster and stricter than one navigation per
 * tab. Target-size checks cover controls (buttons, selects, inputs and the
 * anchors styled as buttons) rather than inline links inside prose, which are
 * text, not targets.
 */
async function measureEveryTab(session) {
  const expression = `JSON.stringify((() => {
    const visible = element => {
      const style = getComputedStyle(element);
      const rect = element.getBoundingClientRect();
      return style.display !== 'none' && style.visibility !== 'hidden' &&
        rect.width > 0 && rect.height > 0;
    };
    const describe = element => [
      element.tagName.toLowerCase(),
      element.id ? '#' + element.id : '',
      [...element.classList].map(name => '.' + name).join(''),
    ].join('');
    const results = [];
    const ids = window.BambuSiteTabs.ids();
    for (const id of ids) {
      window.BambuSiteTabs.activate(id);
      const panel = document.getElementById('panel-' + id);
      const clientWidth = document.documentElement.clientWidth;
      const offenders = [...panel.querySelectorAll('*')]
        .filter(visible)
        .filter(element => {
          const rect = element.getBoundingClientRect();
          return rect.left < -1 || rect.right > clientWidth + 1 ||
            element.scrollWidth > Math.ceil(rect.width) + 1;
        })
        .map(describe)
        .slice(0, 6);
      const undersized = [...panel.querySelectorAll(
        'button, select, textarea, input, a.btn, a.asset'
      )]
        .filter(visible)
        .filter(element => {
          const rect = element.getBoundingClientRect();
          if (rect.height >= 43.5 && rect.width >= 43.5) return false;
          // A checkbox inside a 44px label is reachable through the label.
          const label = element.closest('label');
          if (label) {
            const labelRect = label.getBoundingClientRect();
            if (labelRect.height >= 43.5 && labelRect.width >= 43.5) return false;
          }
          return true;
        })
        .map(element => describe(element) + ' ' +
          Math.round(element.getBoundingClientRect().width) + 'x' +
          Math.round(element.getBoundingClientRect().height))
        .slice(0, 6);
      const tabs = [...document.querySelectorAll('#tabstrip .tab')].filter(visible);
      const stripRows = [...new Set(tabs.map(tab => tab.offsetTop))].length;
      results.push({
        id,
        empty: panel.textContent.trim().length < 40,
        offenders,
        undersized,
        stripRows,
        documentOverflow: document.documentElement.scrollWidth > clientWidth,
      });
    }
    return results;
  })())`;
  const evaluated = await session.send('Runtime.evaluate', { expression, returnByValue: true });
  return JSON.parse(evaluated.result.value);
}

test('every tab renders inside the viewport at each supported width, zoom and language', {
  timeout: 240_000,
  skip: !PAGE_URL && 'Set BAMBU_PAGES_TEST_URL to the locally served landing page',
}, async () => {
  const chrome = await startChrome();
  const failures = [];
  let cases = 0;
  try {
    for (const physicalWidth of [320, 360, 640, 1280]) {
      for (const zoom of [1, 1.5, 2]) {
        const cssWidth = Math.max(120, Math.floor(physicalWidth / zoom));
        await chrome.session.send('Emulation.setDeviceMetricsOverride', {
          width: cssWidth,
          height: 900,
          deviceScaleFactor: zoom,
          mobile: false,
          screenWidth: physicalWidth,
          screenHeight: Math.round(900 * zoom),
        });
        for (const language of LANGUAGE_MODES) {
          const url = new URL(PAGE_URL);
          url.searchParams.set('lang', language);
          await navigate(chrome.session, url.href);
          const context = `${physicalWidth}px @ ${zoom * 100}% (${cssWidth} CSS px), ${language}`;
          for (const result of await measureEveryTab(chrome.session)) {
            cases++;
            if (result.empty)
              failures.push(`${context}: tab ${result.id} rendered no content`);
            if (result.documentOverflow)
              failures.push(`${context}: tab ${result.id} overflows the document horizontally`);
            if (result.offenders.length)
              failures.push(`${context}: tab ${result.id} clips ${JSON.stringify(result.offenders)}`);
            if (result.undersized.length)
              failures.push(`${context}: tab ${result.id} undersized ${JSON.stringify(result.undersized)}`);
            if (result.stripRows > 1)
              failures.push(`${context}: tab strip wrapped onto ${result.stripRows} rows instead of overflowing`);
          }
        }
      }
    }
  } finally {
    await stopChrome(chrome);
  }

  assert.equal(cases, 4 * 3 * 3 * 8);
  assert.deepEqual(failures, []);
});

test('landing page stays inside every supported width, zoom and language viewport', {
  timeout: 300_000,
  skip: !PAGE_URL && 'Set BAMBU_PAGES_TEST_URL to the locally served landing page',
}, async () => {
  const chrome = await startChrome();
  const failures = [];
  let cases = 0;
  try {
    for (const physicalWidth of PHYSICAL_WIDTHS) {
      for (const zoom of ZOOM_FACTORS) {
        const cssWidth = Math.max(120, Math.floor(physicalWidth / zoom));
        await chrome.session.send('Emulation.setDeviceMetricsOverride', {
          width: cssWidth,
          height: 900,
          deviceScaleFactor: zoom,
          mobile: false,
          screenWidth: physicalWidth,
          screenHeight: Math.round(900 * zoom),
        });
        for (const language of LANGUAGE_MODES) {
          const url = new URL(PAGE_URL);
          url.searchParams.set('lang', language);
          await navigate(chrome.session, url.href);
          const result = await measure(chrome.session);
          cases++;

          const context = `${physicalWidth}px @ ${zoom * 100}% (${cssWidth} CSS px), ${language}`;
          if (result.scrollWidth > result.clientWidth) {
            failures.push(
              `${context}: horizontal overflow ${result.scrollWidth}/${result.clientWidth}; ` +
              `offenders ${JSON.stringify(result.overflowOffenders)}`
            );
          }
          for (const element of result.headerElements) {
            if (element.left < -1 || element.right > result.clientWidth + 1)
              failures.push(`${context}: header element outside viewport: ${element.name}`);
          }
          if (result.headerOverlaps.length)
            failures.push(`${context}: header overlaps ${JSON.stringify(result.headerOverlaps)}`);
          for (const control of result.controls) {
            if (control.height < 43.5 || control.width < 43.5)
              failures.push(`${context}: undersized target ${control.name} ${control.width}x${control.height}`);
          }
          if (result.cardOverflow)
            failures.push(`${context}: ${result.cardOverflow} feature card(s) clip content`);
          if (result.selectedLanguage !== language)
            failures.push(`${context}: selected language is ${result.selectedLanguage}`);
        }
      }
    }
  } finally {
    await stopChrome(chrome);
  }

  assert.equal(cases, 156);
  assert.deepEqual(failures, []);
});
