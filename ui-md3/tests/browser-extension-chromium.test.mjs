import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { chmodSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Loads browser-extension/ into a real Chromium and drives real downloads
// through it. A small stand-in native messaging host, registered for this
// profile only, records what the extension sends and answers as the test
// says, so the extension's own behaviour (holding the download, pausing,
// handing over, cancelling or resuming) is observed in the browser, not
// simulated. The Bambu Studio MD3 side of the handoff is not part of this test.
//
// Chromium is found through CHROMIUM_PATH or a Playwright browser folder
// (PLAYWRIGHT_BROWSERS_PATH); without one the test is skipped. Branded Google
// Chrome builds ignore --load-extension, so they are not used.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const extensionDir = path.join(repoDir, 'browser-extension');
const EXTENSION_ID = 'beapempohkjjpcjdfojdlngcbamofiok';
const { HOST_NAME, LOG_KEY, SETTINGS_KEY } = await import('../../browser-extension/capture-rules.js');

function findChromium() {
  const candidates = [];
  if (process.env.CHROMIUM_PATH) candidates.push(process.env.CHROMIUM_PATH);
  const browsers = process.env.PLAYWRIGHT_BROWSERS_PATH;
  if (browsers && existsSync(browsers)) {
    for (const entry of readdirSync(browsers).filter((name) => /^chromium-\d+$/.test(name)).sort().reverse()) {
      candidates.push(path.join(browsers, entry, 'chrome-linux', 'chrome'));
      candidates.push(path.join(browsers, entry, 'chrome-win', 'chrome.exe'));
    }
  }
  return candidates.find((candidate) => existsSync(candidate)) ?? null;
}

const chromiumPath = findChromium();
const skip = chromiumPath ? false : 'no Chromium build found (set CHROMIUM_PATH)';
const MODEL_BYTES = Buffer.alloc(64 * 1024, 7);

// ----------------------------------------------------------- DevTools client

class DevTools {
  constructor(url) {
    this.socket = new WebSocket(url);
    this.nextId = 0;
    this.pending = new Map();
    this.listeners = new Set();
    this.socket.addEventListener('message', (event) => {
      const message = JSON.parse(event.data);
      if (message.id !== undefined && this.pending.has(message.id)) {
        const { resolve, reject } = this.pending.get(message.id);
        this.pending.delete(message.id);
        if (message.error) reject(new Error(`${message.error.message} ${message.error.data ?? ''}`));
        else resolve(message.result);
      } else {
        for (const listener of this.listeners) listener(message);
      }
    });
    this.opened = new Promise((resolve, reject) => {
      this.socket.addEventListener('open', resolve, { once: true });
      this.socket.addEventListener('error', reject, { once: true });
    });
  }

  send(method, params = {}, sessionId) {
    const id = ++this.nextId;
    this.socket.send(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }));
    return new Promise((resolve, reject) => this.pending.set(id, { resolve, reject }));
  }

  close() {
    this.socket.close();
  }
}

async function waitFor(check, what, timeoutMs = 15000) {
  const started = Date.now();
  for (;;) {
    const value = await check();
    if (value) return value;
    if (Date.now() - started > timeoutMs) throw new Error(`timed out waiting for ${what}`);
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

// ---------------------------------------------------------------- fixtures

function startServer() {
  const server = createServer((request, response) => {
    const url = new URL(request.url, 'http://127.0.0.1');
    if (url.pathname === '/page.html') {
      response.writeHead(200, { 'content-type': 'text/html; charset=utf-8' });
      response.end('<!doctype html><title>Models</title><a id="model" href="/files/benchy.3mf?sig=abc">Download model</a>'
        + '<a id="notes" href="/files/notes.txt">Download notes</a>');
    } else if (url.pathname === '/files/benchy.3mf') {
      response.writeHead(200, {
        'content-type': 'application/octet-stream',
        'content-disposition': 'attachment; filename="benchy.3mf"',
        'content-length': MODEL_BYTES.length,
      });
      response.end(MODEL_BYTES);
    } else if (url.pathname === '/files/notes.txt') {
      response.writeHead(200, { 'content-type': 'text/plain', 'content-disposition': 'attachment; filename="notes.txt"' });
      response.end('plain notes\n');
    } else {
      response.writeHead(404);
      response.end();
    }
  });
  return new Promise((resolve) => server.listen(0, '127.0.0.1', () => resolve(server)));
}

// The stand-in host: reads one length-prefixed message, records it, and
// answers according to mode.json, which the test rewrites between steps.
function writeHost(root) {
  const script = path.join(root, 'host.mjs');
  const record = path.join(root, 'received.jsonl');
  const mode = path.join(root, 'mode.json');
  writeFileSync(script, `#!${process.execPath}
import { appendFileSync, readFileSync } from 'node:fs';
let buffer = Buffer.alloc(0);
process.stdin.on('data', (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  if (buffer.length < 4) return;
  const size = buffer.readUInt32LE(0);
  if (buffer.length < 4 + size) return;
  const message = JSON.parse(buffer.subarray(4, 4 + size).toString('utf8'));
  appendFileSync(${JSON.stringify(record)}, JSON.stringify({ origin: process.argv[2], message }) + '\\n');
  const mode = JSON.parse(readFileSync(${JSON.stringify(mode)}, 'utf8'));
  let reply;
  if (message.type === 'hello') reply = { type: 'hello', protocol: 1, app: 'Bambu Studio MD3', version: 'test' };
  else if (mode.answer === 'queued') reply = { type: 'capture-result', protocol: 1, captureId: message.captureId, status: 'queued', queueItemId: 'queue-1' };
  else reply = { type: 'capture-result', protocol: 1, captureId: message.captureId, status: 'declined', reason: mode.answer };
  const body = Buffer.from(JSON.stringify(reply), 'utf8');
  const header = Buffer.alloc(4);
  header.writeUInt32LE(body.length, 0);
  process.stdout.write(Buffer.concat([header, body]));
});
`);
  chmodSync(script, 0o755);
  return { script, record, mode };
}

async function launch() {
  const root = mkdtempSync(path.join(os.tmpdir(), 'bambu-extension-'));
  const profile = path.join(root, 'profile');
  const downloads = path.join(root, 'downloads');
  mkdirSync(path.join(profile, 'Default'), { recursive: true });
  mkdirSync(downloads);
  // The page starts several downloads in a row; allow that without the
  // browser's "download multiple files" prompt, which a headless run cannot answer.
  writeFileSync(path.join(profile, 'Default', 'Preferences'), JSON.stringify({
    download: { default_directory: downloads, prompt_for_download: false, directory_upgrade: true },
    profile: { default_content_setting_values: { automatic_downloads: 1 } },
  }));
  const host = writeHost(root);
  writeFileSync(host.mode, JSON.stringify({ answer: 'queued' }));
  const hostsDir = path.join(profile, 'NativeMessagingHosts');
  mkdirSync(hostsDir);
  const register = () => writeFileSync(path.join(hostsDir, `${HOST_NAME}.json`), JSON.stringify({
    name: HOST_NAME,
    description: 'Test stand-in for the Bambu Studio MD3 browser connection',
    path: host.script,
    type: 'stdio',
    allowed_origins: [`chrome-extension://${EXTENSION_ID}/`],
  }));
  register();

  const browser = spawn(chromiumPath, [
    '--headless=new', '--no-sandbox', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
    `--user-data-dir=${profile}`, '--remote-debugging-port=0',
    `--disable-extensions-except=${extensionDir}`, `--load-extension=${extensionDir}`, 'about:blank',
  ], { stdio: ['ignore', 'ignore', 'pipe'] });
  const exited = new Promise((resolve) => browser.once('exit', resolve));
  let stderr = '';
  browser.stderr.on('data', (chunk) => { stderr += chunk; });
  const active = path.join(profile, 'DevToolsActivePort');
  let devtools = null;
  let worker = null;
  try {
    const [port, socketPath] = await waitFor(() => existsSync(active) && readFileSync(active, 'utf8').trim().split('\n'),
      'DevTools to start');
    devtools = new DevTools(`ws://127.0.0.1:${port}${socketPath}`);
    await devtools.opened;
    const workerTarget = await waitFor(async () => (await devtools.send('Target.getTargets')).targetInfos
      .find((target) => target.type === 'service_worker' && target.url === `chrome-extension://${EXTENSION_ID}/service-worker.js`),
    'the extension service worker');
    ({ sessionId: worker } = await devtools.send('Target.attachToTarget', { targetId: workerTarget.targetId, flatten: true }));
  } catch (error) {
    // A browser that never came up must not keep the test process alive.
    browser.kill('SIGKILL');
    await exited;
    browser.stderr.destroy();
    devtools?.close();
    rmSync(root, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
    throw new Error(`${error.message}\n${stderr.slice(-2000)}`);
  }

  async function inWorker(expression) {
    const result = await devtools.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }, worker);
    if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
    return result.result.value;
  }

  async function openPage(url) {
    const { targetId } = await devtools.send('Target.createTarget', { url: 'about:blank' });
    const { sessionId } = await devtools.send('Target.attachToTarget', { targetId, flatten: true });
    await devtools.send('Page.enable', {}, sessionId);
    const loaded = new Promise((resolve) => {
      const listener = (message) => {
        if (message.sessionId === sessionId && message.method === 'Page.loadEventFired') {
          devtools.listeners.delete(listener);
          resolve();
        }
      };
      devtools.listeners.add(listener);
    });
    await devtools.send('Page.navigate', { url }, sessionId);
    await loaded;
    const evaluate = async (expression) => {
      const result = await devtools.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true, userGesture: true }, sessionId);
      if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
      return result.result.value;
    };
    return { targetId, sessionId, evaluate, close: () => devtools.send('Target.closeTarget', { targetId }) };
  }

  const received = () => (existsSync(host.record)
    ? readFileSync(host.record, 'utf8').trim().split('\n').filter(Boolean).map((line) => JSON.parse(line))
    : []);

  return {
    root, downloads, host, devtools, inWorker, openPage, received,
    answer: (answer) => writeFileSync(host.mode, JSON.stringify({ answer })),
    unregister: () => rmSync(path.join(hostsDir, `${HOST_NAME}.json`)),
    register,
    async close() {
      // Close the browser the way a person would, so its helper processes
      // finish writing to the profile before the folder is removed.
      const exitWithin = (ms) => new Promise((resolve) => {
        const timer = setTimeout(() => resolve(false), ms);
        exited.then(() => {
          clearTimeout(timer);
          resolve(true);
        });
      });
      devtools.send('Browser.close').catch(() => undefined);
      if (!(await exitWithin(10000))) {
        browser.kill('SIGKILL');
        await exited;
      }
      devtools.close();
      browser.stderr.destroy();
      rmSync(root, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
    },
  };
}

// -------------------------------------------------------------------- test

test('a real Chromium hands model downloads over, keeps everything else, and resumes when the handoff fails', { skip, timeout: 120000 }, async (t) => {
  const server = await startServer();
  const base = `http://127.0.0.1:${server.address().port}`;
  const browser = await launch();
  t.after(async () => {
    server.closeAllConnections();
    server.close();
    await browser.close();
  });

  const readLog = () => browser.inWorker(`chrome.storage.local.get(${JSON.stringify(LOG_KEY)}).then((v) => v[${JSON.stringify(LOG_KEY)}] ?? [])`);
  const searchDownloads = () => browser.inWorker('chrome.downloads.search({})');
  const page = await browser.openPage(`${base}/page.html`);

  // 1. The application queues the model: the browser keeps no copy.
  await page.evaluate("document.getElementById('model').click()");
  const [first] = await waitFor(async () => (await readLog()).length >= 1 && readLog(), 'the first handoff');
  assert.equal(first.outcome, 'queued');
  assert.equal(first.queueItemId, 'queue-1');
  const sent = browser.received().find((entry) => entry.message.type === 'capture').message;
  assert.equal(browser.received()[0].origin, `chrome-extension://${EXTENSION_ID}/`);
  assert.equal(sent.fileName, 'benchy.3mf');
  assert.equal(sent.url, `${base}/files/benchy.3mf?sig=abc`);
  assert.equal(sent.source, `${base}/page.html`);
  assert.equal(sent.modelType, '3mf');
  assert.equal(sent.totalBytes, MODEL_BYTES.length);
  await waitFor(async () => (await searchDownloads()).every((item) => item.state !== 'in_progress'), 'the browser to drop its copy');
  assert.deepEqual((await searchDownloads()).filter((item) => item.state === 'complete'), [], 'the browser finished nothing');
  assert.deepEqual(readdirSync(browser.downloads).filter((name) => !name.endsWith('.crdownload')), []);

  // 2. The application declines: the browser resumes and saves the whole file.
  browser.answer('busy');
  await page.evaluate("document.getElementById('model').click()");
  await waitFor(async () => (await readLog()).length >= 2, 'the declined handoff');
  const declined = (await readLog())[0];
  assert.equal(declined.outcome, 'kept');
  assert.equal(declined.code, 'declined-busy');
  const saved = await waitFor(async () => (await searchDownloads()).find((item) => item.state === 'complete'), 'the browser to finish');
  assert.equal(path.basename(saved.filename), 'benchy.3mf');
  assert.deepEqual(readFileSync(saved.filename), MODEL_BYTES);

  // 3. No browser connection registered: the browser keeps the download.
  browser.unregister();
  browser.answer('queued');
  await page.evaluate("document.getElementById('model').click()");
  await waitFor(async () => (await readLog()).length >= 3, 'the unregistered handoff');
  assert.equal((await readLog())[0].code, 'host-missing');
  await waitFor(async () => (await searchDownloads()).filter((item) => item.state === 'complete').length === 2, 'the second browser download');
  browser.register();

  // 4. A download that is not a model never reaches the host or the log.
  const before = browser.received().length;
  await page.evaluate("document.getElementById('notes').click()");
  await waitFor(async () => (await searchDownloads()).some((item) => item.state === 'complete' && item.filename.endsWith('notes.txt')), 'the notes download');
  assert.equal(browser.received().length, before);
  assert.equal((await readLog()).length, 3);

  // 5. Capture switched off in settings: the model stays in the browser untouched.
  await browser.inWorker(`chrome.storage.local.set({ ${JSON.stringify(SETTINGS_KEY)}: { enabled: false } })`);
  await page.evaluate("document.getElementById('model').click()");
  await waitFor(async () => (await searchDownloads()).filter((item) => item.state === 'complete' && /benchy.*\.3mf$/.test(item.filename)).length === 3, 'the uncaptured model');
  assert.equal(browser.received().length, before);
  await browser.inWorker(`chrome.storage.local.set({ ${JSON.stringify(SETTINGS_KEY)}: { enabled: true } })`);
  await page.close();

  // 6. The settings page checks the connection and switches language.
  const options = await browser.openPage(`chrome-extension://${EXTENSION_ID}/options.html`);
  await waitFor(async () => (await options.evaluate("document.querySelector('main').getAttribute('aria-busy')")) === null, 'the settings page');
  await options.evaluate("document.getElementById('check-connection').click()");
  await waitFor(async () => /Connected to Bambu Studio MD3 test/.test(await options.evaluate("document.getElementById('connection-state').textContent")), 'the connection check');
  assert.equal(await options.evaluate("document.getElementById('extension-id').textContent"), EXTENSION_ID);
  assert.equal(await options.evaluate('document.querySelectorAll("#recent tbody tr").length'), 3);
  await options.evaluate("(() => { const s = document.getElementById('language'); s.value = 'yue_HK'; s.dispatchEvent(new Event('change')); })()");
  await waitFor(async () => (await options.evaluate("document.getElementById('page-heading').textContent")) === '下載接手設定', 'Cantonese headings');
  assert.equal(await options.evaluate('document.documentElement.lang'), 'yue-HK');
  assert.equal(await options.evaluate("document.getElementById('status').textContent"), '儲存咗。');
  await options.close();
});
