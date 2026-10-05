/* Pinned composed-site verification through the installed cheap hidden route. */
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { readFile, writeFile, mkdir, readdir, realpath, rm } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const args = Object.fromEntries(process.argv.slice(2).reduce((all, value, index, list) => index % 2 ? all : [...all, [value.replace(/^--/, ''), list[index + 1]]], []));
for (const field of ['cheap-cli', 'edge', 'run-root', 'verifier', 'pythonw']) assert.ok(args[field], `Missing ${field}`);
const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const runRoot = path.resolve(args['run-root']);
await mkdir(runRoot, { recursive: false });
const output = path.join(runRoot, 'built');
const profile = path.join(runRoot, 'profile');
const captureRoot = path.join(runRoot, 'captures');
await mkdir(captureRoot);
const hash = data => createHash('sha256').update(data).digest('hex');
const write = (name, object) => writeFile(path.join(runRoot, name), JSON.stringify(object, null, 2));
const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
const revision = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: repo, encoding: 'utf8', windowsHide: true }).trim();
assert.equal(execFileSync('git', ['diff', '--name-only', 'HEAD', '--', 'ui-md3/site', 'ui-md3/landing.html'], { cwd: repo, encoding: 'utf8', windowsHide: true }).trim(), '', 'Site source must be committed before capture');
execFileSync(process.execPath, [path.join(repo, 'ui-md3/scripts/compose-site.mjs'), output], { cwd: repo, windowsHide: true, timeout: 120000 });
const files = [];
async function walk(directory) {
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const current = path.join(directory, entry.name);
    if (entry.isDirectory()) await walk(current);
    else files.push({ path: path.relative(output, current).replaceAll('\\', '/'), sha256: hash(await readFile(current)) });
  }
}
await walk(output);
const manifest = JSON.stringify({ files: files.sort((a, b) => a.path.localeCompare(b.path)) }, null, 2);
await writeFile(path.join(runRoot, 'output-manifest.json'), manifest);
await write('build-receipt.json', { sourceRevision: revision, outputManifestSha256: hash(manifest), command: 'node ui-md3/scripts/compose-site.mjs <owned-output>' });
const serverPort = Number(args['server-port'] || 45681);
const cdpPort = Number(args['cdp-port'] || 45682);
const url = `http://127.0.0.1:${serverPort}/index.html`;
const desktop = 'BambuSiteWording' + Date.now();
const quote = value => { assert.ok(!/["\r\n]/.test(value)); return '"' + value + '"'; };
const cheap = (tool, values = {}) => {
  const executable = args['cheap-python'] || args['cheap-cli'];
  const prefix = args['cheap-python'] ? ['-m', 'lowlevel_computer_use_mcp.server', 'cheap'] : [];
  const raw = execFileSync(executable, [...prefix, tool, '--json', JSON.stringify(values)], { encoding: 'utf8', windowsHide: true, timeout: 30000 });
  const result = JSON.parse(raw);
  assert.equal(result.ok, true, 'Cheap hidden route failed for ' + tool);
  return result;
};
let server, edge, socket;
const events = [];
const results = [];
const edgeHash = hash(await readFile(args.edge));
const version = execFileSync('powershell.exe', ['-NoProfile', '-Command', `(Get-Item -LiteralPath '${args.edge.replaceAll("'", "''")}').VersionInfo.FileVersion`], { encoding: 'utf8', windowsHide: true }).trim();
const ledger = { desktop, serverPort, cdpPort, profile, runRoot, sourceRevision: revision };
function identity(pid) {
  const raw = execFileSync('powershell.exe', ['-NoProfile', '-Command', `Get-CimInstance Win32_Process -Filter 'ProcessId=${Number(pid)}' | Select-Object ProcessId,ExecutablePath,CreationDate | ConvertTo-Json -Compress`], { encoding: 'utf8', windowsHide: true });
  return raw.trim() ? JSON.parse(raw) : null;
}
async function portUnused(port) {
  try { await fetch(`http://127.0.0.1:${port}/`, { signal: AbortSignal.timeout(1000) }); } catch { return; }
  throw new Error('Task port already serves a response');
}
async function ready(endpoint) {
  for (let attempt = 0; attempt < 40; attempt++) {
    try { const response = await fetch(endpoint, { signal: AbortSignal.timeout(1000) }); if (response.ok) return; } catch { /* bounded startup poll */ }
    await pause(250);
  }
  throw new Error('Startup deadline exceeded');
}
function proof(phase, name) {
  const target = path.join(runRoot, `${name}-${phase}.json`);
  execFileSync(process.execPath, [args.verifier, '--endpoint', `http://127.0.0.1:${cdpPort}/json/list`, '--expected-url', url, '--run-root', runRoot, '--edge-executable', args.edge, '--edge-sha256', edgeHash, '--edge-version', version, '--launch-pid', String(edge.pid), '--phase', phase, '--output', target], { windowsHide: true, timeout: 15000 });
  return target;
}
let nextId = 1;
const pending = new Map();
function send(method, params = {}) {
  return new Promise((resolve, reject) => {
    const id = nextId++;
    const timer = setTimeout(() => { pending.delete(id); reject(new Error('CDP deadline: ' + method)); }, 10000);
    pending.set(id, { resolve, reject, timer });
    socket.send(JSON.stringify({ id, method, params }));
  });
}
async function evaluate(expression) {
  const result = await send('Runtime.evaluate', { expression, returnByValue: true });
  if (result.exceptionDetails) throw new Error('Runtime expression failed');
  return result.result.value;
}
try {
  await portUnused(serverPort); await portUnused(cdpPort);
  const flags = [`--app=${url}`, `--user-data-dir=${profile}`, `--remote-debugging-port=${cdpPort}`, '--guest', '--disable-sync', '--disable-extensions', '--disable-component-extensions-with-background-pages', '--no-first-run', '--no-default-browser-check', '--edge-skip-compat-layer-relaunch', '--disable-features=msEdgeFirstRunExperience,msEdgeSignin,msEdgeSync'];
  edge = cheap('launch_on_headless_desktop', { name: desktop, command: [args.edge, ...flags].map(quote).join(' ') });
  ledger.edgePid = edge.pid;
  ledger.edgeIdentity = identity(edge.pid);
  // A live GUI process keeps the desktop attached while a console-free server
  // is launched on the same lane. The first navigation is reloaded after ready.
  server = cheap('launch_on_headless_desktop', { name: desktop, command: [args.pythonw, path.join(repo, 'ui-md3/scripts/site-verification-server.py'), output, String(serverPort)].map(quote).join(' ') });
  ledger.serverPid = server.pid;
  ledger.serverIdentity = identity(server.pid);
  await write('ownership.json', ledger);
  await ready(url);
  await ready(`http://127.0.0.1:${cdpPort}/json/list`);
  const preflight = proof('preflight', 'initial');
  const targetReceipt = JSON.parse(await readFile(preflight, 'utf8'));
  const targets = await (await fetch(`http://127.0.0.1:${cdpPort}/json/list`)).json();
  assert.equal(targets.length, 1); assert.equal(targets[0].url, url); assert.equal(targets[0].type, 'page');
  socket = new WebSocket(targets[0].webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.addEventListener('open', resolve, { once: true }); socket.addEventListener('error', reject, { once: true }); });
  socket.addEventListener('message', event => {
    const item = JSON.parse(event.data);
    if (item.id) {
      const request = pending.get(item.id); if (!request) return;
      pending.delete(item.id); clearTimeout(request.timer);
      if (item.error) request.reject(new Error(item.error.message)); else request.resolve(item.result);
    } else events.push(item);
  });
  for (const domain of ['Runtime', 'Log', 'Page', 'Accessibility', 'Network']) await send(domain + '.enable');
  await send('Page.reload');
  for (let attempt = 0; attempt < 30; attempt++) { if (await evaluate('document.readyState === "complete" && !!window.BambuSiteTabs')) break; await pause(200); }
  proof('interaction', 'open-settings');
  assert.equal(await evaluate('document.querySelectorAll(".tab[data-tab=settings]").length'), 1);
  await evaluate('document.querySelector(".tab[data-tab=settings]").click()');
  assert.equal(await evaluate('!!document.querySelector("#panel-settings input[type=file]")'), true);
  const first = await evaluate('({loaded:BambuWording.status().loaded, label:document.querySelector("#panel-settings input[type=file]").getAttribute("aria-label")})');
  assert.equal(first.loaded, false); assert.ok(first.label.length);
  // Neutral test data is created only in the private run root. It contains no
  // actual personal mappings and is never copied to the capture inventory.
  const fixture = path.join(runRoot, 'neutral.json');
  await writeFile(fixture, '{"schemaVersion":1,"entries":{"Personal wording":"Visitor terminology"}}');
  proof('interaction', 'upload');
  const dom = await send('DOM.getDocument');
  const field = await send('DOM.querySelector', { nodeId: dom.root.nodeId, selector: '#panel-settings input[type=file]' });
  await send('DOM.setFileInputFiles', { nodeId: field.nodeId, files: [fixture] });
  for (let attempt = 0; attempt < 30; attempt++) { if (await evaluate('BambuWording.status().loaded')) break; await pause(100); }
  assert.equal(await evaluate('BambuWording.status().loaded'), true);
  await send('Page.reload');
  for (let attempt = 0; attempt < 30; attempt++) { if (await evaluate('document.readyState === "complete" && !!window.BambuSiteTabs')) break; await pause(200); }
  proof('interaction', 'clear');
  await evaluate('document.querySelector(".tab[data-tab=settings]").click()');
  assert.equal(await evaluate('BambuWording.status().loaded'), true);
  await evaluate('document.querySelector("#panel-settings [data-copy="+JSON.stringify("wording.clear")+"]").click()');
  assert.equal(await evaluate('BambuWording.status().loaded'), false);
  for (const width of [1440, 390]) for (const mode of ['en', 'yue_HK', 'bilingual_en_yue_HK']) for (const theme of ['light', 'dark']) {
    const name = `${width}-${mode}-${theme}`;
    proof('interaction', name);
    await send('Emulation.setDeviceMetricsOverride', { width, height: 1000, deviceScaleFactor: 1, mobile: width === 390 });
    await send('Emulation.setTouchEmulationEnabled', { enabled: width === 390 });
    await evaluate(`BambuSite.setLanguageMode(${JSON.stringify(mode)}); BambuSite.set('theme',${JSON.stringify(theme)}); BambuSite.applyAppearance();`);
    await pause(150);
    const box = await evaluate('({clientWidth:document.documentElement.clientWidth,scrollWidth:document.documentElement.scrollWidth})');
    const ax = await send('Accessibility.getFullAXTree');
    const interactive = ax.nodes.filter(node => !node.ignored && ['button', 'link', 'textbox', 'checkbox', 'combobox', 'switch', 'tab'].includes(node.role?.value));
    const unnamed = interactive.filter(node => !node.name?.value).length;
    proof('capture', name);
    const capturedAt = new Date().toISOString();
    const image = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false });
    const bytes = Buffer.from(image.data, 'base64');
    await writeFile(path.join(captureRoot, name + '.png'), bytes);
    proof('final', name);
    results.push({ name, width, height: 1000, scale: 1, mobile: width === 390, evidenceKind: width === 390 ? 'emulated-mobile' : 'desktop', mode, theme, state: 'settings with empty local wording control', capturedAt, sha256: hash(bytes), imageWidth: bytes.readUInt32BE(16), imageHeight: bytes.readUInt32BE(20), bodyOverflow: box.scrollWidth > box.clientWidth + 1, unnamedInteractiveCount: unnamed, interactiveCount: interactive.length, rootFound: ax.nodes.some(node => node.role?.value === 'RootWebArea') });
  }
  const diagnostics = { exceptionCount: events.filter(event => event.method === 'Runtime.exceptionThrown').length, consoleErrorCount: events.filter(event => event.method === 'Log.entryAdded' && event.params.entry.level === 'error').length, failedResourceCount: events.filter(event => event.method === 'Network.loadingFailed').length, thirdPartyCount: events.filter(event => event.method === 'Network.requestWillBeSent' && !event.params.request.url.startsWith(`http://127.0.0.1:${serverPort}/`) && !event.params.request.url.startsWith('data:')).length };
  await write('runtime-result.json', { revision, outputManifestSha256: hash(manifest), interaction: { noFile: true, localUpload: true, reloadPersistence: true, clear: true, originalRestored: true }, diagnostics, results, limits: ['renderer wiring only; physical keyboard and touch not verified', 'scale 1 only in this focused run', 'shared School integration and palette indexing pending'], targetId: targetReceipt.targetId });
  console.log(JSON.stringify({ revision, captures: results.length, overflowCases: results.filter(row => row.bodyOverflow).map(row => row.name), unnamedCases: results.filter(row => row.unnamedInteractiveCount).map(row => row.name), diagnostics, runRoot }));
} finally {
  if (socket) { try { await send('Browser.close'); } catch { /* normal close may drop the socket */ } socket.close(); }
  await pause(1000);
  for (const owned of [{ process: edge, saved: ledger.edgeIdentity }, { process: server, saved: ledger.serverIdentity }]) if (owned.process?.pid) {
    const current = identity(owned.process.pid);
    if (current && JSON.stringify(current) === JSON.stringify(owned.saved)) cheap('kill_process', { pid: owned.process.pid, force: true });
  }
  const processesGone = (!edge || !identity(edge.pid)) && (!server || !identity(server.pid));
  let portsGone = true;
  for (const port of [serverPort, cdpPort]) {
    try { await fetch(`http://127.0.0.1:${port}/`, { signal: AbortSignal.timeout(500) }); portsGone = false; } catch { /* expected absence */ }
  }
  let desktopClosed = false;
  if (processesGone && portsGone) { cheap('close_headless_desktop', { name: desktop }); desktopClosed = true; }
  let profileGone = false;
  if (processesGone && portsGone) {
    try {
      const realProfile = await realpath(profile);
      const realRun = await realpath(runRoot);
      const relation = path.relative(realRun, realProfile);
      assert.ok(relation && !relation.startsWith('..') && !path.isAbsolute(relation));
      await rm(realProfile, { recursive: true }); profileGone = true;
    } catch (error) { if (error.code === 'ENOENT') profileGone = true; }
  }
  await write('cleanup.json', { completed: processesGone && portsGone && desktopClosed && profileGone, ownedOnly: true, processesGone, portsGone, desktopClosed, profileGone, ledger });
}
