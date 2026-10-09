// Drives the sender page in a real headless Chromium against the drop
// service started in-process: the invite link fills the code and leaves the
// address bar and the history, a model is sent and lands in the inbox, a
// wrong code is explained, the icon font draws every icon, and the page
// loads nothing from anywhere else and breaks no Content-Security-Policy
// rule. Skipped when no Chromium build is found (set CHROMIUM_PATH, or
// PLAYWRIGHT_BROWSERS_PATH to a folder with chromium-<n> builds).
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { ASCII_STL, dropRoot, repoRoot, startDrop, station, tempDir } from './harness.mjs';

const { MESSAGES } = await import(path.join(dropRoot, 'site', 'i18n.js'));
const english = MESSAGES.en;

function findChromium() {
  const candidates = [];
  if (process.env.CHROMIUM_PATH) candidates.push(process.env.CHROMIUM_PATH);
  const browsers = process.env.PLAYWRIGHT_BROWSERS_PATH;
  if (browsers && fs.existsSync(browsers)) {
    for (const entry of fs.readdirSync(browsers).filter((name) => /^chromium-\d+$/.test(name)).sort().reverse()) {
      candidates.push(path.join(browsers, entry, 'chrome-linux', 'chrome'));
      candidates.push(path.join(browsers, entry, 'chrome-win', 'chrome.exe'));
    }
  }
  return candidates.find((candidate) => fs.existsSync(candidate)) ?? null;
}

const chromiumPath = findChromium();
const skip = chromiumPath ? false : 'no Chromium build found (set CHROMIUM_PATH)';

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
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
}

async function launch(t) {
  // Chromium on Linux keeps its single-instance socket in TMPDIR, and a long
  // socket path stops it from starting, so a long TMPDIR is replaced by /tmp.
  const shortBase = process.platform !== 'win32' && os.tmpdir().length > 48 && fs.existsSync('/tmp') ? '/tmp' : os.tmpdir();
  const root = fs.mkdtempSync(path.join(shortBase, 'lan-drop-browser-'));
  const profile = path.join(root, 'profile');
  fs.mkdirSync(profile);
  const browser = spawn(chromiumPath, [
    '--headless=new', '--no-sandbox', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
    '--disable-background-networking', '--disable-component-update', '--disable-sync', '--no-pings',
    '--disable-default-apps', '--disable-domain-reliability', '--lang=en-US',
    `--user-data-dir=${profile}`, '--remote-debugging-port=0', 'about:blank',
  ], { stdio: ['ignore', 'ignore', 'pipe'], env: { ...process.env, TMPDIR: root } });
  const exited = new Promise((resolve) => browser.once('exit', resolve));
  browser.stderr.resume();
  let devtools = null;
  t.after(async () => {
    devtools?.close();
    browser.kill('SIGKILL');
    await exited;
    browser.stderr.destroy();
    fs.rmSync(root, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
  });
  const active = path.join(profile, 'DevToolsActivePort');
  const [port, socketPath] = await waitFor(() => fs.existsSync(active) && fs.readFileSync(active, 'utf8').trim().split('\n'),
    'DevTools to start');
  devtools = new DevTools(`ws://127.0.0.1:${port}${socketPath}`);
  await devtools.opened;
  const { targetId } = await devtools.send('Target.createTarget', { url: 'about:blank' });
  const { sessionId } = await devtools.send('Target.attachToTarget', { targetId, flatten: true });
  const page = {
    send: (method, params) => devtools.send(method, params, sessionId),
    async evaluate(expression) {
      const result = await devtools.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }, sessionId);
      if (result.exceptionDetails) throw new Error(`page threw: ${result.exceptionDetails.exception?.description ?? result.exceptionDetails.text}`);
      return result.result.value;
    },
  };
  const requests = [];
  const logs = [];
  devtools.listeners.add((message) => {
    if (message.sessionId !== sessionId) return;
    if (message.method === 'Network.requestWillBeSent') requests.push(message.params.request.url);
    if (message.method === 'Log.entryAdded') logs.push(`${message.params.entry.level} ${message.params.entry.text}`);
    if (message.method === 'Runtime.exceptionThrown') logs.push(`exception ${message.params.exceptionDetails.text}`);
  });
  await page.send('Network.enable');
  await page.send('Log.enable');
  await page.send('Runtime.enable');
  await page.send('Page.enable');
  await page.send('DOM.enable');
  return { page, requests, logs };
}

async function chooseFiles(page, files) {
  const { root } = await page.send('DOM.getDocument', { depth: 1 });
  const { nodeId } = await page.send('DOM.querySelector', { nodeId: root.nodeId, selector: '#files' });
  await page.send('DOM.setFileInputFiles', { nodeId, files });
}

test('the sender page works end to end in Chromium', { skip, timeout: 90000 }, async (t) => {
  const service = await startDrop(t, { env: { DROP_STATION_NAME: 'Workshop PC' } });
  const base = `http://127.0.0.1:${service.port}/`;
  const { page, requests, logs } = await launch(t);
  const files = tempDir(t, 'lan-drop-files-');
  const model = path.join(files, 'cube.stl');
  fs.writeFileSync(model, ASCII_STL);

  // 1. The invite link fills the code and the fragment disappears.
  await page.send('Page.navigate', { url: `${base}#code=${service.code}` });
  await waitFor(() => page.evaluate("document.readyState === 'complete' && document.getElementById('code').value !== ''"),
    'the page to read the invite');
  const opened = await page.evaluate(`({
    code: document.getElementById('code').value,
    href: location.href,
    hash: location.hash,
    help: document.getElementById('code-help').textContent,
    station: document.getElementById('station-line').textContent,
    title: document.title,
    lang: document.documentElement.lang,
    readOnly: document.getElementById('code').readOnly,
  })`);
  assert.deepEqual(opened, {
    code: service.code,
    href: base,
    hash: '',
    help: english.codeFromLink,
    station: 'Sending to Workshop PC',
    title: english.pageTitle,
    lang: 'en',
    readOnly: false,
  });
  const history = await page.send('Page.getNavigationHistory');
  assert.ok(history.entries.every((entry) => !entry.url.includes('code=')), JSON.stringify(history.entries.map((e) => e.url)));
  assert.equal(history.entries.at(-1).url, base);

  // 2. Every icon draws as one glyph from the bundled subset, unlike a word
  // the font has no ligature for.
  const script = fs.readFileSync(path.join(repoRoot, 'scripts', 'md3', 'subset_lan_drop_icons.py'), 'utf8');
  const icons = [...(/ICONS = \(([\s\S]*?)\)/.exec(script)?.[1] ?? '').matchAll(/"([a-z_]+)"/g)].map((m) => m[1]);
  const widths = await page.evaluate(`(async () => {
    await document.fonts.ready;
    const measure = (name) => {
      const span = document.createElement('span');
      span.setAttribute('data-icon', '');
      span.style.fontSize = '20px';
      span.textContent = name;
      document.body.append(span);
      const width = span.getBoundingClientRect().width;
      span.remove();
      return width;
    };
    await document.fonts.load('20px "Material Symbols Outlined"', 'send');
    return Object.fromEntries(${JSON.stringify([...icons, 'not_an_icon_name'])}.map((name) => [name, measure(name)]));
  })()`);
  for (const name of icons) assert.ok(widths[name] > 0 && widths[name] <= 26, `${name} drew ${widths[name]} px wide`);
  assert.ok(widths.not_an_icon_name > 60, `a plain word stays a word (${widths.not_an_icon_name} px)`);

  // 3. A model goes in, with the sender's name; a file whose content is not
  // a model is refused for itself and not offered again.
  const fake = path.join(files, 'fake.3mf');
  fs.writeFileSync(fake, 'plain text, not a 3MF archive');
  const notes = path.join(files, 'notes.txt');
  fs.writeFileSync(notes, 'notes');
  await chooseFiles(page, [model, fake, notes]);
  await waitFor(() => page.evaluate("document.querySelectorAll('#file-list .file-row').length === 3"), 'the files to be listed');
  assert.equal(await page.evaluate("document.getElementById('send-label').textContent"), 'Send 2');
  assert.equal(await page.evaluate("document.querySelectorAll('#file-list .file-row')[2].dataset.status"), 'rejected');
  await page.evaluate("document.getElementById('sender').value = 'Mei'; document.getElementById('send').click()");
  const done = await waitFor(() => page.evaluate("!document.getElementById('send').disabled || document.getElementById('live').dataset.tone === 'error' ? document.getElementById('live').textContent : ''"),
    'the send to finish');
  assert.equal(done, english.liveDone.replace('$1', '1').replace('$2', '2'));
  const inbox = (await station(service.port, service.key, 'GET', '/api/station/inbox')).json.items;
  assert.deepEqual(inbox.map(({ fileName, sender, type, bytes }) => ({ fileName, sender, type, bytes })),
    [{ fileName: 'cube.stl', sender: 'Mei', type: 'stl', bytes: ASCII_STL.length }]);
  const rows = await page.evaluate("[...document.querySelectorAll('#file-list .file-row')].map((row) => row.dataset.status + ' ' + row.querySelector('.file-state').textContent)");
  assert.deepEqual(rows, ['sent Sent', `rejected Not sent: ${english.reasonType}`, `rejected Not sent: ${english.reasonType}`]);
  assert.equal(await page.evaluate("document.getElementById('send').disabled"), true, 'nothing is left to send');

  // 4. A wrong code is explained, with the way to a new link, and the
  // keyboard goes back to the code.
  const wrong = service.code === '000000' ? '111111' : '000000';
  await page.evaluate(`(() => {
    const input = document.getElementById('code');
    input.value = '${wrong}';
    input.dispatchEvent(new Event('input', { bubbles: true }));
  })()`);
  assert.equal(await page.evaluate("document.getElementById('code-help').textContent"), english.codeHelp);
  fs.writeFileSync(path.join(files, 'second.stl'), ASCII_STL);
  await chooseFiles(page, [path.join(files, 'second.stl')]);
  await page.evaluate("document.getElementById('send').click()");
  await waitFor(() => page.evaluate("document.getElementById('live').dataset.tone === 'error'"), 'the wrong code answer');
  const refused = await page.evaluate(`({
    live: document.getElementById('live').textContent,
    error: document.getElementById('code-error').textContent,
    errorHidden: document.getElementById('code-error').hidden,
    invalid: document.getElementById('code').getAttribute('aria-invalid'),
    focus: document.activeElement.id,
    status: document.querySelectorAll('#file-list .file-row')[3].dataset.status,
  })`);
  assert.deepEqual(refused, {
    live: english.liveWrongCode,
    error: english.codeWrong,
    errorHidden: false,
    invalid: 'true',
    focus: 'code',
    status: 'failed',
  });
  assert.equal((await station(service.port, service.key, 'GET', '/api/station/inbox')).json.items.length, 1);

  // 5. A link opened while the page is showing is read the same way.
  await page.evaluate("location.hash = '#code=246810'");
  await waitFor(() => page.evaluate("document.getElementById('code').value === '246810' && location.hash === ''"), 'the second invite');
  assert.equal(await page.evaluate("document.getElementById('code').getAttribute('aria-invalid')"), null);

  // 6. Cantonese and both languages switch in place and are remembered.
  await page.evaluate("document.querySelector('button[data-mode=\"yue_HK\"]').click()");
  assert.equal(await page.evaluate("document.documentElement.lang + '|' + document.getElementById('form-heading').textContent"),
    `yue-HK|${MESSAGES.yue_HK.formHeading}`);
  await page.evaluate("document.querySelector('button[data-mode=\"bilingual\"]').click()");
  assert.equal(await page.evaluate("document.getElementById('form-heading').textContent"), `${english.formHeading}${MESSAGES.yue_HK.formHeading}`);
  assert.equal(await page.evaluate("document.querySelector('#form-heading .secondary').lang"), 'yue-HK');
  assert.equal(await page.evaluate("localStorage.getItem('lan-model-drop.language')"), 'bilingual');

  // 7. Nothing came from anywhere else, and no policy rule was broken.
  assert.ok(requests.length >= 8, requests.join('\n'));
  for (const url of requests) assert.ok(url.startsWith(base), `request to ${url}`);
  // The browser reports the refused uploads (401 wrong code, 415 not a
  // model) as failed loads; anything else is a fault.
  const unexpected = logs.filter((line) => !/the server responded with a status of (?:401|415)\b/.test(line));
  assert.deepEqual(unexpected, []);
});
