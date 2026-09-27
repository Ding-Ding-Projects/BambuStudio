/*
 * The headless-Chrome plumbing shared by the runtime test suites.
 *
 * Two suites need a real browser: runtime-layout-clipping.mjs measures the
 * published site at every width, zoom and language, and offline-render.test.mjs
 * proves the UI kit still renders with the network cut. Both need the same
 * hundred and eighty lines of DevTools plumbing, so it lives here once.
 *
 * Chrome is launched with --remote-debugging-port=0 and found again through its
 * DevToolsActivePort file, so concurrent runs never collide on a fixed port.
 */
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdtemp, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { suppressAutomaticDimSum } from '../scripts/browser-test-mode.mjs';
export const delay = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));

export function chromePath() {
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

export class DevToolsSession {
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

/**
 * Launches headless Chrome and returns a connected DevTools session.
 *
 * @param extraArguments appended to the command line. The offline suite passes
 *   a host-resolver rule here to blackhole every name but loopback.
 */
export async function startChrome(extraArguments = []) {
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
    ...extraArguments,
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
    await suppressAutomaticDimSum(session);
    return { processHandle, profileDirectory, session };
  } catch (error) {
    processHandle.kill();
    await rm(profileDirectory, { recursive: true, force: true });
    throw error;
  }
}

export async function stopChrome(chrome) {
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

/**
 * Navigates and waits for load.
 *
 * @param requireFonts by default a page whose webfonts never settle is a
 *   failure, because the layout suite measures text. The offline suite turns it
 *   off: it is asking whether the React tree mounts, and a page deliberately
 *   cut off from the network is entitled to fall back to system fonts.
 */
export async function navigate(session, url, { requireFonts = true } = {}) {
  const loaded = session.waitFor('Page.loadEventFired');
  await session.send('Page.navigate', { url });
  await loaded;
  if (!requireFonts) return;
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
