// The real sender page, served by a running LAN model drop container, in headless Chromium:
// opened from an invite link, it sends the chosen models, and every request it makes is recorded
// (the exact request header lines, the answer and its body) for container_capture.py, which then
// sends the same models with curl and checks that curl's requests carry the same protocol headers.
//
// Runs only when container_capture.py starts it with these settings (skipped otherwise, also in
// the repository gate set, which has no container):
//   LAN_DROP_PAGE_BASE    the container's address, for example http://127.0.0.1:18851
//   LAN_DROP_PAGE_CODE    the drop code the invite link carries
//   LAN_DROP_PAGE_FILES   a folder holding exactly the models to choose
//   LAN_DROP_PAGE_SENDER  what is typed into "Your name (optional)"
//   LAN_DROP_PAGE_OUT     where the record is written (JSON)
//   CHROMIUM_PATH         the Chromium executable
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

const env = process.env;
const settings = ['LAN_DROP_PAGE_BASE', 'LAN_DROP_PAGE_CODE', 'LAN_DROP_PAGE_FILES', 'LAN_DROP_PAGE_SENDER', 'LAN_DROP_PAGE_OUT', 'CHROMIUM_PATH'];
const missing = settings.filter((name) => env[name] === undefined);
const skip = missing.length ? `run by tests/lan_model_drop/container_capture.py against a running container (${missing.join(', ')} not set)` : false;

async function waitFor(check, what, timeoutMs = 30000) {
  const started = Date.now();
  for (;;) {
    const value = await check();
    if (value) return value;
    if (Date.now() - started > timeoutMs) throw new Error(`timed out waiting for ${what}`);
    await new Promise((resolve) => setTimeout(resolve, 50));
  }
}

class DevTools {
  constructor(url) {
    this.socket = new WebSocket(url);
    this.nextId = 0;
    this.pending = new Map();
    this.events = [];
    this.socket.addEventListener('message', (event) => {
      const message = JSON.parse(event.data);
      if (message.id !== undefined && this.pending.has(message.id)) {
        const { resolve, reject } = this.pending.get(message.id);
        this.pending.delete(message.id);
        if (message.error) reject(new Error(message.error.message));
        else resolve(message.result);
      } else {
        this.events.push(message);
        this.onEvent?.(message);
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
}

test('the sender page from the container sends every chosen model, and its requests are recorded', { skip, timeout: 180000 }, async (t) => {
  const base = env.LAN_DROP_PAGE_BASE.replace(/\/+$/u, '');
  const files = fs.readdirSync(env.LAN_DROP_PAGE_FILES).sort().map((name) => path.join(env.LAN_DROP_PAGE_FILES, name));
  // Chromium keeps a socket in TMPDIR, whose path must stay short.
  const root = fs.mkdtempSync(path.join(process.platform !== 'win32' && fs.existsSync('/tmp') ? '/tmp' : os.tmpdir(), 'lan-drop-page-'));
  const profile = path.join(root, 'profile');
  fs.mkdirSync(profile);
  const browser = spawn(env.CHROMIUM_PATH, [
    '--headless=new', '--no-sandbox', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
    '--disable-background-networking', '--disable-component-update', '--disable-sync', '--no-pings',
    '--disable-default-apps', '--disable-domain-reliability', '--disable-breakpad', '--disable-crash-reporter', '--lang=en-US',
    `--user-data-dir=${profile}`, '--remote-debugging-port=0', 'about:blank',
  ], { stdio: ['ignore', 'ignore', 'pipe'], env: { ...env, TMPDIR: root, LANG: 'C.UTF-8', LC_ALL: 'C.UTF-8' } });
  const exited = new Promise((resolve) => browser.once('exit', resolve));
  browser.stderr.resume();
  let devtools = null;
  t.after(async () => {
    devtools?.socket.close();
    browser.kill('SIGKILL');
    await exited;
    // Best effort: a helper process Chromium started may still be writing into its profile.
    try {
      fs.rmSync(root, { recursive: true, force: true, maxRetries: 10, retryDelay: 200 });
    } catch {
      // Left in the temporary folder.
    }
  });
  const active = path.join(profile, 'DevToolsActivePort');
  const [port, socketPath] = await waitFor(() => fs.existsSync(active) && fs.readFileSync(active, 'utf8').trim().split('\n'), 'DevTools');
  devtools = new DevTools(`ws://127.0.0.1:${port}${socketPath}`);
  await devtools.opened;
  const { targetId } = await devtools.send('Target.createTarget', { url: 'about:blank' });
  const { sessionId } = await devtools.send('Target.attachToTarget', { targetId, flatten: true });
  const page = (method, params) => devtools.send(method, params, sessionId);
  // Each drop's answer is read when it arrives (the inspector does not keep answers marked
  // no-store), then passed on to the page unchanged.
  const bodies = new Map();
  devtools.onEvent = (message) => {
    if (message.sessionId !== sessionId || message.method !== 'Fetch.requestPaused') return;
    const { requestId, networkId } = message.params;
    bodies.set(networkId, (async () => {
      const answer = await page('Fetch.getResponseBody', { requestId });
      await page('Fetch.continueResponse', { requestId });
      return answer.base64Encoded ? Buffer.from(answer.body, 'base64').toString('utf8') : answer.body;
    })());
  };
  const evaluate = async (expression) => {
    const result = await page('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
    if (result.exceptionDetails) throw new Error(`page threw: ${result.exceptionDetails.exception?.description ?? result.exceptionDetails.text}`);
    return result.result.value;
  };
  for (const domain of ['Network', 'Page', 'DOM', 'Runtime']) await page(`${domain}.enable`);
  await page('Fetch.enable', { patterns: [{ urlPattern: '*/api/drop', requestStage: 'Response' }] });

  // The invite link fills the code and leaves the address bar.
  await page('Page.navigate', { url: `${base}/#code=${env.LAN_DROP_PAGE_CODE}` });
  await waitFor(() => evaluate("document.readyState === 'complete' && document.getElementById('code').value !== ''"), 'the invite');
  const invite = await evaluate("({ code: document.getElementById('code').value, href: location.href, hash: location.hash })");
  assert.deepEqual(invite, { code: env.LAN_DROP_PAGE_CODE, href: `${base}/`, hash: '' });
  const history = await page('Page.getNavigationHistory');
  assert.ok(history.entries.every((entry) => !entry.url.includes('code=')));

  const { root: documentNode } = await page('DOM.getDocument', { depth: 1 });
  const { nodeId } = await page('DOM.querySelector', { nodeId: documentNode.nodeId, selector: '#files' });
  await page('DOM.setFileInputFiles', { nodeId, files });
  await waitFor(() => evaluate(`document.querySelectorAll('#file-list .file-row[data-status="ready"]').length === ${files.length}`),
    'every model to be listed as ready');
  await evaluate(`(() => {
    const sender = document.getElementById('sender');
    sender.value = ${JSON.stringify(env.LAN_DROP_PAGE_SENDER)};
    document.getElementById('send').click();
  })()`);
  await waitFor(() => evaluate("['success', 'error'].includes(document.getElementById('live').dataset.tone)"), 'the send to finish', 120000);
  const rows = await evaluate("[...document.querySelectorAll('#file-list .file-row')].map((row) => ({ name: row.querySelector('.file-name').textContent, status: row.dataset.status }))");
  const live = await evaluate("document.getElementById('live').textContent");

  // Every drop request with the header lines Chromium really sent, its answer and the body.
  const drops = new Map();
  const extra = new Map();
  for (const message of devtools.events) {
    if (message.sessionId !== sessionId) continue;
    const params = message.params ?? {};
    if (message.method === 'Network.requestWillBeSent' && params.request.method === 'POST') {
      drops.set(params.requestId, { url: params.request.url, method: params.request.method });
    } else if (message.method === 'Network.requestWillBeSentExtraInfo') {
      extra.set(params.requestId, params.headers);
    } else if (message.method === 'Network.responseReceived' && drops.has(params.requestId)) {
      Object.assign(drops.get(params.requestId), { status: params.response.status, responseHeaders: params.response.headers });
    }
  }
  const requests = [];
  for (const [requestId, record] of drops) {
    assert.ok(bodies.has(requestId), `the answer to ${record.url} arrived`);
    const body = await bodies.get(requestId);
    requests.push({ ...record, requestHeaders: extra.get(requestId) ?? null, body });
  }
  assert.equal(requests.length, files.length, 'one request per chosen model');
  for (const request of requests) assert.ok(request.requestHeaders, `the raw request headers of ${request.url}`);
  fs.writeFileSync(env.LAN_DROP_PAGE_OUT, `${JSON.stringify({ invite, rows, live, requests }, null, 2)}\n`);
  assert.ok(rows.every((row) => row.status === 'sent'), JSON.stringify(rows));
});
