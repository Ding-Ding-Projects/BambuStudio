// The drop site and the station agree. Whatever the drop service accepts and reports, the station
// takes exactly as it was reported: a model the sender was told arrived is never turned away by the
// station afterwards, an invite address the owner set is never quietly replaced, and the limits of
// the two halves are the same. Each case here is a disagreement found by running the container
// against the application's pure model (see container_fixtures.test.mjs and
// docs/features/application-integration/lan-model-drop-site.md). The station's side is the real
// pure model, built from src/slic3r/GUI/LanModelDrop with g++; skipped where g++ is missing.
import { test, before } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ASCII_STL, ConfigError, drop, readConfig, startDrop, station, zipLike } from './harness.mjs';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const hasGxx = spawnSync('g++', ['--version'], { encoding: 'utf8' }).status === 0;
const skip = hasGxx ? false : 'g++ is not installed';
const { normalizePublicUrl, cleanStationName } = await import(path.join(repo, 'lan-model-drop', 'server', 'config.mjs'));

let work = null;
let exe = null;
let serial = 0;

before(() => {
  if (skip) return;
  work = mkdtempSync(path.join(tmpdir(), 'lan-model-drop-agreement-'));
  process.on('exit', () => rmSync(work, { recursive: true, force: true }));
  exe = path.join(work, process.platform === 'win32' ? 'probe.exe' : 'probe');
  const build = spawnSync('g++', [
    '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Isrc',
    'tests/lan_model_drop/lan_model_drop_station_probe.cpp',
    'src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp',
    'src/slic3r/GUI/LanModelDrop/LanModelDropFiles.cpp',
    '-o', exe,
  ], { cwd: repo, encoding: 'utf8' });
  assert.equal(build.status, 0, build.stderr);
});

function file(content) {
  const target = path.join(work, `part-${++serial}`);
  writeFileSync(target, content);
  return target;
}

function probe(...args) {
  const run = spawnSync(exe, args.map(String), { encoding: 'utf8' });
  assert.equal(run.status, 0, run.stderr);
  return JSON.parse(run.stdout);
}

const modelHeader = readFileSync(path.join(repo, 'src/slic3r/GUI/LanModelDrop/LanModelDropModel.hpp'), 'utf8');
const stationSource = readFileSync(path.join(repo, 'src/slic3r/GUI/LanModelDrop/LanModelDropStation.cpp'), 'utf8');

function stationConstant(name) {
  const match = new RegExp(`constexpr\\s+std::(?:uint64_t|size_t)\\s+${name}\\s*=\\s*(\\d+)(?:ULL)?;`).exec(modelHeader);
  assert.ok(match, `${name} is declared in LanModelDropModel.hpp`);
  return Number(match[1]);
}

test('a name the station would refuse is refused by the drop box, where the sender sees why', { skip }, async (t) => {
  const d = await startDrop(t);
  // Made only of characters that reorder or hide text, or of zero-width space: the station shows
  // names without them, so nothing would be left of these.
  const refused = ['\u202e\u202e.stl', '\u200b.stl', '\u2066\u2069 .stl', '\u200e\u200f.3mf', '\ufeff\u061c.obj', ' \u200b .stl',
    '\u200b\u202e.step'];
  for (const fileName of refused) {
    const body = fileName.endsWith('.3mf') ? zipLike(64) : fileName.endsWith('.step') ? Buffer.from('ISO-10303-21;\n') : fileName.endsWith('.obj') ? Buffer.from('v 0 0 0\n') : ASCII_STL;
    const res = await drop(d.port, { code: d.code, fileName, body });
    assert.equal(res.status, 400, `${JSON.stringify(fileName)}: ${res.body}`);
    assert.equal(res.json.error, 'bad_request');
  }

  // The rest of such a name is kept, and the station lists it exactly as the drop box stored it.
  const kept = [
    ['a\u202eb.stl', '', 'ab.stl', ''],
    ['\ufeffpart.stl', '\u202eAda\u202c', 'part.stl', 'Ada'],
    ['\u200bwide\u200b name\u200b.stl', ' \u200b Mo \u2067Li\u2069 \u200b', 'wide\u200b name\u200b.stl', 'Mo Li'],
    ['\u6a21\u578b\u200f \u96f6\u4ef6.stl', '\u9673\u5927\u6587\u200e', '\u6a21\u578b \u96f6\u4ef6.stl', '\u9673\u5927\u6587'],
  ];
  for (const [fileName, sender] of kept) {
    const res = await drop(d.port, { code: d.code, fileName, body: ASCII_STL, sender });
    assert.equal(res.status, 201, `${JSON.stringify(fileName)}: ${res.body}`);
  }
  const inbox = await station(d.port, d.key, 'GET', '/api/station/inbox');
  assert.deepEqual(inbox.json.items.map(({ fileName, sender }) => [fileName, sender]), kept.map(([, , name, sender]) => [name, sender]));
  const listed = probe('inbox', file(inbox.body));
  assert.deepEqual(listed.invalid, [], 'the station refuses nothing the drop box accepted');
  assert.equal(listed.unusable, 0);
  assert.deepEqual(listed.items.map(({ fileName, sender }) => [fileName, sender]), kept.map(([, , name, sender]) => [name, sender]));
});

test('the drop box takes no model larger, and holds no more models, than the station reads', () => {
  const maxItemBytes = stationConstant('kMaxItemBytes');
  const maxInboxItems = stationConstant('kMaxInboxItems');
  const roomy = String(maxItemBytes * 2);

  assert.equal(readConfig({ DROP_MAX_BYTES: String(maxItemBytes) }).maxBytes, maxItemBytes);
  assert.throws(() => readConfig({ DROP_MAX_BYTES: String(maxItemBytes + 1), DROP_QUEUE_MAX_BYTES: roomy }),
    (error) => error instanceof ConfigError && /DROP_MAX_BYTES/.test(error.message) && error.message.includes(String(maxItemBytes)));
  assert.equal(readConfig({ DROP_QUEUE_MAX_FILES: String(maxInboxItems) }).queueMaxFiles, maxInboxItems);
  assert.throws(() => readConfig({ DROP_QUEUE_MAX_FILES: String(maxInboxItems + 1) }),
    (error) => error instanceof ConfigError && /DROP_QUEUE_MAX_FILES/.test(error.message) && error.message.includes(String(maxInboxItems)));

  // A full queue of the longest names and senders still fits the inbox answer the station reads.
  const limit = /exchange\(cfg, Method::Get, inbox_url\(\*cfg\.address\), (\d+) \* 1024 \* 1024,/.exec(stationSource);
  assert.ok(limit, 'the station reads the inbox with a size limit in MiB');
  const widest = {
    id: 'f'.repeat(32),
    fileName: `${'\u{1f9f1}'.repeat(196)}.stl`,
    sender: '\u{1f9f1}'.repeat(40),
    bytes: maxItemBytes,
    sha256: 'f'.repeat(64),
    receivedAt: '2026-10-09T08:15:42Z',
    type: 'step',
  };
  const answer = JSON.stringify({ items: Array.from({ length: maxInboxItems }, () => widest) });
  assert.ok(Buffer.byteLength(answer) < Number(limit[1]) * 1024 * 1024, `${Buffer.byteLength(answer)} bytes`);
});

test('every DROP_PUBLIC_URL the drop box accepts reaches the invite link unchanged', { skip }, () => {
  // An example private address, written from its octets.
  const LAN = [192, 168, 1, 20].join('.');
  const values = [
    `http://${LAN}:8833`, 'https://drop.example.org/', 'https://Drop.Example.ORG:443/models/', 'http://[FD00::20]:8833/',
    'http://bambu-pc:8833', 'https://example.org/a%20b', 'https://b\u00fccher.example/drop', 'http://example.org/a/../b',
    'http://a-.example', 'http://example.org.', 'http://1234', 'http://0x7f.1', 'http://127.1:8833', 'http://example.org/~user/drop',
    'http://example.org/d%C3%BCse', 'http://example.org/{x}', 'http://example.org/a"b', 'http://example.org/a`b', 'HTTP://EXAMPLE.ORG',
    'http://example.org:65535', 'http://example.org:08833', `http://[::ffff:${LAN}]`, 'https://print.example.test/drop/',
    `http://${'a'.repeat(63)}.example`, `https://example.org/${'p'.repeat(900)}`,
    // Each of these was accepted and reported by the drop box, and then ignored by the station, so the
    // invite went to another address without a word.
    'http://drop_box.lan:8833', 'http://example.org:0', 'http://example.org/a|b', 'http://example.org/a^b', 'http://example.org/[x]',
    'http://example.org//drop', 'http://a$b.example', 'http://a!b.example', 'http://a+b.example', "http://a'b.example",
    'http://a(b).example', 'http://a,b.example', 'http://a;b=c.example', 'http://a&b.example', 'http://-a.example',
    'http://a..b.example', `http://${'a'.repeat(64)}.example`, `https://example.org/${'p'.repeat(1100)}`, 'http://example.org/%',
    'http://example.org/%zz', 'http://example.org/a%2',
  ];
  for (const [index, value] of values.entries()) {
    let reported;
    try {
      reported = normalizePublicUrl(value);
    } catch (error) {
      assert.ok(error instanceof ConfigError, `${value}: ${error}`);
      assert.match(error.message, /DROP_PUBLIC_URL/);
      assert.ok(index >= 25, `${value}: ordinary addresses stay accepted`);
      continue;
    }
    // Newer Node URL parsers percent-encode a caret in the path. That
    // normalized address is valid when the station accepts it unchanged.
    if (index >= 25) assert.equal(value, 'http://example.org/a^b');
    assert.equal(probe('public-url', reported).url, reported, `${JSON.stringify(value)} is reported as ${JSON.stringify(reported)}`);
  }
});

test('the station shows the station name the drop box reports, all of its characters', { skip }, async (t) => {
  // The drop box keeps at most 60 characters of DROP_STATION_NAME (config.mjs, cleanStationName).
  assert.equal([...cleanStationName('x'.repeat(100))].length, 60);
  const sixty = '\u4e01\u4e01\u6253\u5370\u5ba4 Bambu Studio MD3 on the workshop PC by the window \u{1f5a8}\u{fe0f} 1';
  const seventy = `${'Workshop computer number one '.repeat(3)}`.slice(0, 70);
  for (const name of [sixty, seventy]) {
    const d = await startDrop(t, { env: { DROP_STATION_NAME: name } });
    const status = await station(d.port, d.key, 'GET', '/api/station/status');
    const reported = status.json.stationName;
    assert.equal(reported, cleanStationName(name));
    assert.ok([...reported].length > 40 && [...reported].length <= 60, `${[...reported].length} characters`);
    assert.equal(probe('status', status.status, file(status.body)).stationName, reported, name);
  }
});
