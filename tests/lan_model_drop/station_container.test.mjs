// The station against the real drop service. The application's pure model (parsing, checks and the
// invite link, through tests/lan_model_drop/lan_model_drop_station_probe.cpp) reads what
// lan-model-drop/server answers when it is driven the way the station drives it: the station key in
// a Bearer header, status, inbox, each file with its SHA-256, DELETE after the decision, a new drop
// code, and the invite link. Runs where both the drop service and g++ are present; skipped
// otherwise (the service lives in lan-model-drop/, delivered separately).
import {test, before} from 'node:test';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {existsSync, mkdtempSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const harnessPath = path.join(repo, 'tests', 'lan_model_drop', 'harness.mjs');
const servicePresent = existsSync(path.join(repo, 'lan-model-drop', 'server', 'app.mjs')) && existsSync(harnessPath);
const hasGxx = spawnSync('g++', ['--version'], {encoding: 'utf8'}).status === 0;
const skip = !servicePresent ? 'the drop service (lan-model-drop/server) is not in this tree' : !hasGxx ? 'g++ is not installed' : false;

let h = null;
let work = null;
let exe = null;
let serial = 0;

before(async () => {
  if (skip) return;
  h = await import(harnessPath);
  work = mkdtempSync(path.join(tmpdir(), 'lan-model-drop-station-'));
  process.on('exit', () => rmSync(work, {recursive: true, force: true}));
  exe = path.join(work, process.platform === 'win32' ? 'probe.exe' : 'probe');
  const build = spawnSync('g++', [
    '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Isrc',
    'tests/lan_model_drop/lan_model_drop_station_probe.cpp',
    'src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp',
    '-o', exe,
  ], {cwd: repo, encoding: 'utf8'});
  assert.equal(build.status, 0, build.stderr);
});

function file(content) {
  const target = path.join(work, `part-${++serial}`);
  writeFileSync(target, content);
  return target;
}

function probe(...args) {
  const run = spawnSync(exe, args.map(String), {encoding: 'utf8'});
  assert.equal(run.status, 0, run.stderr);
  return JSON.parse(run.stdout);
}

// Raw header lines, as libcurl hands them to the station.
function rawHeaders(res) {
  const lines = [`HTTP/1.1 ${res.status}`];
  for (let i = 0; i < res.raw.length; i += 2) lines.push(`${res.raw[i]}: ${res.raw[i + 1]}`);
  return `${lines.join('\r\n')}\r\n\r\n`;
}

// The station's requests: the key in a Bearer header, JSON accepted; POST carries "{}".
function stationRequest(port, key, method, target) {
  const headers = {Authorization: `Bearer ${key}`, Accept: 'application/json'};
  const options = {method, path: target, headers};
  if (method === 'POST') {
    headers['Content-Type'] = 'application/json';
    options.body = '{}';
  }
  return h.request(port, options);
}

// h.request keeps parsed headers only; the raw list is fetched with a second, identical request.
async function download(port, key, id) {
  const {request: rawRequest} = await import('node:http');
  return new Promise((resolve, reject) => {
    const req = rawRequest({host: '127.0.0.1', port, method: 'GET', path: `/api/station/files/${id}`, agent: false,
      headers: {Authorization: `Bearer ${key}`, Accept: 'application/json'}}, (res) => {
      const parts = [];
      res.on('data', (chunk) => parts.push(chunk));
      res.on('end', () => resolve({status: res.statusCode, raw: res.rawHeaders, body: Buffer.concat(parts)}));
      res.on('error', reject);
    });
    req.on('error', reject);
    req.end();
  });
}

const BOM = Buffer.from([0xef, 0xbb, 0xbf]);
// Example private addresses, written from their octets.
const ip = (...octets) => octets.join('.');
const LAN_A = ip(192, 168, 1, 20);
const LAN_B = ip(10, 0, 0, 5);

function samples() {
  return [
    {name: 'Bracket.3MF', body: h.zipLike(300), sender: 'Ada'},
    {name: 'ascii cube.stl', body: h.ASCII_STL, sender: 'Bob 🙂'},
    {name: 'binary.stl', body: h.binaryStl(3)},
    {name: 'part.step', body: h.STEP, sender: '陳大文'},
    {name: 'part with bom.stp', body: Buffer.concat([BOM, Buffer.from('\r\n'), h.STEP])},
    {name: 'mesh.obj', body: h.OBJ},
    {name: 'indented.obj', body: Buffer.from('o cube\r\n\tv\t0 0 0\r\n\tv\t1 0 0\r\n\tv\t0 1 0\r\nf 1 2 3\r\n')},
    {name: 'model.amf', body: h.AMF_XML},
    {name: 'zipped.amf', body: h.zipLike(120)},
    {name: 'late marker.amf', body: Buffer.from(`<?xml version="1.0"?>\n<!--${' '.repeat(8000)}-->\n<amf unit="mm"/>\n`)},
    {name: 'C:\\Users\\ada\\..\\CON.stl', body: h.ASCII_STL},
    {name: '  spaced name .stl. ', body: h.ASCII_STL, sender: '  Mo  '},
  ];
}

test('the station reads the real service: status, inbox, each file with its SHA-256, and DELETE', {skip}, async (t) => {
  const d = await h.startDrop(t);

  const health = await h.request(d.port, {path: '/healthz'});
  assert.equal(health.status, 200);
  assert.deepEqual(probe('health', file(health.body)), {ok: true});

  const sent = samples();
  for (const s of sent) {
    const res = await h.drop(d.port, {code: d.code, fileName: s.name, body: s.body, sender: s.sender});
    assert.equal(res.status, 201, `${s.name}: ${res.body}`);
  }

  const status = await stationRequest(d.port, d.key, 'GET', '/api/station/status');
  const seen = probe('status', status.status, file(status.body));
  assert.equal(seen.state, 'Connected');
  assert.equal(seen.dropCode, d.code);
  assert.equal(seen.queued, sent.length);
  assert.equal(seen.publicUrl, '');

  const inbox = await stationRequest(d.port, d.key, 'GET', '/api/station/inbox');
  assert.equal(inbox.status, 200);
  const listed = probe('inbox', file(inbox.body));
  assert.equal(listed.ok, true);
  assert.deepEqual(listed.invalid, []);
  assert.equal(listed.unusable, 0);
  assert.equal(listed.items.length, sent.length);
  // The station keeps every name and sender exactly as the service cleaned them.
  for (const [i, item] of listed.items.entries()) {
    const raw = inbox.json.items[i];
    assert.deepEqual(item, {id: raw.id, fileName: raw.fileName, sender: raw.sender ?? '', bytes: raw.bytes, sha256: raw.sha256,
      receivedAt: raw.receivedAt, type: raw.type});
  }
  const names = listed.items.map((item) => item.fileName);
  assert.ok(names.includes('_CON.stl'), names.join(', '));
  assert.ok(names.includes('spaced name .stl'), names.join(', '));

  for (const item of listed.items) {
    const res = await download(d.port, d.key, item.id);
    assert.equal(res.status, 200);
    assert.equal(res.body.length, item.bytes);
    assert.equal(createHash('sha256').update(res.body).digest('hex'), item.sha256);
    assert.equal(probe('header', file(rawHeaders(res)), 'X-Content-SHA256').value, item.sha256);
    assert.deepEqual(probe('content', item.type, file(res.body)), {matches: true}, item.fileName);

    const removed = await stationRequest(d.port, d.key, 'DELETE', `/api/station/files/${item.id}`);
    assert.equal(removed.status, 204);
    const again = await stationRequest(d.port, d.key, 'DELETE', `/api/station/files/${item.id}`);
    assert.equal(again.status, 204);
    const gone = await download(d.port, d.key, item.id);
    assert.equal(gone.status, 404);
  }

  const empty = await stationRequest(d.port, d.key, 'GET', '/api/station/inbox');
  assert.deepEqual(probe('inbox', file(empty.body)), {ok: true, items: [], invalid: [], unusable: 0});
});

test('every model the service accepts passes the station checks', {skip}, async (t) => {
  const d = await h.startDrop(t);
  const variants = [];
  const prefixes = [Buffer.alloc(0), BOM, Buffer.from(' \r\n\t'), Buffer.concat([BOM, Buffer.from('\n\n')])];
  for (const prefix of prefixes) {
    variants.push(['step', 'v.step', Buffer.concat([prefix, h.STEP])]);
    variants.push(['stl', 'v.stl', Buffer.concat([prefix, h.ASCII_STL])]);
    variants.push(['amf', 'v.amf', Buffer.concat([prefix, h.AMF_XML])]);
  }
  for (const text of ['v 0 0 0\n', '\tv\t0 0 0\n', '# r\rv 0 0 0\r', '# n\r\n  v 0 0 0\r\n', 'o x\nvt 0 0\nv 1 2 3\n'])
    variants.push(['obj', 'v.obj', Buffer.from(text)]);
  variants.push(['stl', 'v.stl', h.binaryStl(1, {headerText: 'solid but binary'})]);
  variants.push(['3mf', 'v.3mf', h.zipLike(64)]);

  let accepted = 0;
  for (const [type, name, body] of variants) {
    const res = await h.drop(d.port, {code: d.code, fileName: name, body});
    if (res.status !== 201) continue;
    ++accepted;
    assert.deepEqual(probe('content', type, file(body)), {matches: true}, `${type}: ${JSON.stringify(body.toString('latin1').slice(0, 40))}`);
  }
  assert.ok(accepted >= variants.length - 2, `the service accepted only ${accepted} of ${variants.length}`);
});

test('a wrong or missing station key reads as Wrong station key', {skip}, async (t) => {
  const d = await h.startDrop(t);
  const wrong = await stationRequest(d.port, 'not-the-station-key-0123456789', 'GET', '/api/station/status');
  assert.equal(wrong.status, 401);
  assert.equal(probe('status', wrong.status, file(wrong.body)).state, 'WrongStationKey');
  const missing = await h.request(d.port, {path: '/api/station/inbox'});
  assert.equal(missing.status, 401);
  assert.equal(probe('failure', missing.status).state, 'WrongStationKey');
  assert.equal(probe('failure', 0).state, 'NotReachable');
});

test('New code rotates the drop code; a code fixed by DROP_CODE answers 409', {skip}, async (t) => {
  const d = await h.startDrop(t);
  const before = d.code;
  const renewed = await stationRequest(d.port, d.key, 'POST', '/api/station/drop-code');
  assert.equal(renewed.status, 200);
  const {code} = probe('code', file(renewed.body));
  assert.ok(code && code !== before, `${before} -> ${code}`);
  const status = await stationRequest(d.port, d.key, 'GET', '/api/station/status');
  assert.equal(probe('status', status.status, file(status.body)).dropCode, code);
  const old = await h.drop(d.port, {code: before, fileName: 'late.stl', body: h.ASCII_STL});
  assert.equal(old.status, 401);

  const fixed = await h.startDrop(t, {env: {DROP_CODE: '4321'}});
  const refused = await stationRequest(fixed.port, fixed.key, 'POST', '/api/station/drop-code');
  // The station reads 409 as "the code is fixed" and disables New link with that explanation.
  assert.equal(refused.status, 409);
  assert.equal(refused.json.error, 'fixed_code');
  assert.deepEqual(probe('code', file(refused.body)), {code: null});
});

test('the invite link: DROP_PUBLIC_URL first, else this computer\'s LAN address, with the code in the fragment', {skip}, async (t) => {
  const d = await h.startDrop(t, {env: {DROP_PUBLIC_URL: 'https://print.example.test/drop/'}});
  const status = await stationRequest(d.port, d.key, 'GET', '/api/station/status');
  const seen = probe('status', status.status, file(status.body));
  assert.equal(seen.publicUrl, 'https://print.example.test/drop');
  const pub = probe('invite', seen.publicUrl, `http://localhost:${d.port}`, LAN_A, '', seen.dropCode);
  assert.equal(pub.source, 'PublicUrl');
  assert.equal(pub.link, `https://print.example.test/drop/#code=${d.code}`);

  const plain = await h.startDrop(t);
  const plainStatus = probe('status', 200, file((await stationRequest(plain.port, plain.key, 'GET', '/api/station/status')).body));
  assert.equal(plainStatus.publicUrl, '');
  const lan = probe('invite', plainStatus.publicUrl, `http://localhost:${plain.port}`, `${LAN_A},${LAN_B}`, LAN_B, plainStatus.dropCode);
  assert.equal(lan.source, 'LanAddress');
  assert.deepEqual(lan.choices, [LAN_A, LAN_B]);
  assert.equal(lan.link, `http://${LAN_B}:${plain.port}/#code=${plain.code}`);
  // The fragment never reaches the service: the page itself is served for the bare path.
  const page = await h.request(plain.port, {path: '/'});
  assert.equal(page.status, 200);
  assert.match(String(page.headers['content-type']), /text\/html/);
  assert.equal(probe('invite', '', `http://localhost:${plain.port}`, '', '', plainStatus.dropCode).source, 'None');
});
