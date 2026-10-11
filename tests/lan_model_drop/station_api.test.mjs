// The station side of the LAN model drop protocol (version 1): the station
// key, status, inbox, file download and removal, the drop code, expiry and
// the secret files in the data volume.
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {
  ASCII_STL, binaryStl, drop, dropRoot, manualClock, OBJ, request, startDrop, station, STEP, tempDir, zipLike,
} from './harness.mjs';

const isWindows = process.platform === 'win32';
const sha256 = (buffer) => createHash('sha256').update(buffer).digest('hex');

test('/healthz answers without a key', async (t) => {
  const service = await startDrop(t);
  const response = await request(service.port, { path: '/healthz' });
  assert.equal(response.status, 200);
  assert.deepEqual(response.json, { ok: true, service: 'lan-model-drop', protocol: 1 });
});

test('station endpoints need the station key in a Bearer header', async (t) => {
  const service = await startDrop(t);
  const targets = [
    ['GET', '/api/station/status'],
    ['GET', '/api/station/inbox'],
    ['GET', `/api/station/files/${'a'.repeat(32)}`],
    ['DELETE', `/api/station/files/${'a'.repeat(32)}`],
    ['POST', '/api/station/drop-code'],
    ['GET', '/api/station/unknown'],
  ];
  const wrongKeys = [
    null,
    '',
    'x',
    service.key.slice(0, -1),
    `${service.key}x`,
    service.key.replace(/^./, (c) => (c === 'A' ? 'B' : 'A')),
    'k'.repeat(500),
  ];
  for (const [method, target] of targets) {
    for (const key of wrongKeys) {
      const headers = key === null ? {} : { Authorization: `Bearer ${key}` };
      const response = await request(service.port, { method, path: target, headers });
      assert.equal(response.status, 401, `${method} ${target} with ${key === null ? 'no key' : 'a wrong key'}`);
      assert.equal(response.json.error, 'unauthorized');
      assert.match(response.headers['www-authenticate'], /^Bearer /);
    }
    // The right key in another scheme is still refused.
    const basic = await request(service.port, { method, path: target, headers: { Authorization: `Basic ${service.key}` } });
    assert.equal(basic.status, 401);
  }
  const right = await station(service.port, service.key, 'GET', '/api/station/status');
  assert.equal(right.status, 200);
  const lowerCase = await request(service.port, { path: '/api/station/status', headers: { Authorization: `bearer ${service.key}` } });
  assert.equal(lowerCase.status, 200, 'the scheme name is case-insensitive');
  // The drop code is not a station key.
  assert.equal((await station(service.port, service.code, 'GET', '/api/station/status')).status, 401);
});

test('the station key is compared in constant time', () => {
  const secrets = fs.readFileSync(path.join(dropRoot, 'server', 'secrets.mjs'), 'utf8');
  const app = fs.readFileSync(path.join(dropRoot, 'server', 'app.mjs'), 'utf8');
  const compare = /export function secretsEqual\([^)]*\) \{([\s\S]*?)\n\}/.exec(secrets)?.[1] ?? '';
  assert.match(compare, /createHash\('sha256'\)[\s\S]*createHash\('sha256'\)/, 'both sides are hashed to equal length');
  assert.match(compare, /timingSafeEqual\(/);
  // No early-exit comparison of the values themselves.
  assert.doesNotMatch(compare, /candidate\s*[!=]==?\s*secret|secret\s*[!=]==?\s*candidate|\.equals\(|\.length\s*[!=]==?|\ba\s*[!=]==?\s*b\b/);
  // Station requests and drop codes both go through it, and nothing else
  // compares a secret.
  assert.match(app, /secretsEqual\(match \? match\[1\] : '', stationKey\.key\)/);
  assert.match(app, /secretsEqual\(code\.trim\(\), dropCode\.code\)/);
  assert.doesNotMatch(app, /stationKey\.key\s*[!=]==?|[!=]==?\s*stationKey\.key/);
  assert.doesNotMatch(app, /dropCode\.code\s*[!=]==?|[!=]==?\s*dropCode\.code/);
});

test('status reports the protocol, station and drop box', async (t) => {
  const service = await startDrop(t, { env: { DROP_STATION_NAME: 'Workshop PC', DROP_MAX_BYTES: '5000', DROP_QUEUE_MAX_BYTES: '50000', DROP_TTL_HOURS: '6' } });
  assert.equal((await drop(service.port, { code: service.code, fileName: 'a.3mf', body: zipLike(300) })).status, 201);
  assert.equal((await drop(service.port, { code: service.code, fileName: 'b.obj', body: OBJ })).status, 201);
  const response = await station(service.port, service.key, 'GET', '/api/station/status');
  assert.equal(response.status, 200);
  assert.deepEqual(response.json, {
    protocol: 1,
    stationName: 'Workshop PC',
    dropCode: service.code,
    queued: 2,
    queuedBytes: 300 + OBJ.length,
    maxBytes: 5000,
    ttlHours: 6,
    publicUrl: null,
  });
  assert.match(response.json.dropCode, /^\d{6}$/);
});

test('status reports DROP_PUBLIC_URL as publicUrl, without a trailing slash', async (t) => {
  const cases = [
    ['https://drop.example.org', 'https://drop.example.org'],
    ['https://drop.example.org/', 'https://drop.example.org'],
    ['http://192.0.2.20:8833/', 'http://192.0.2.20:8833'],
    ['  HTTP://Workshop-PC.local:8833/drop/  ', 'http://workshop-pc.local:8833/drop'],
    ['https://drop.example.org:443/', 'https://drop.example.org'],
  ];
  for (const [configured, expected] of cases) {
    const service = await startDrop(t, { env: { DROP_PUBLIC_URL: configured } });
    const response = await station(service.port, service.key, 'GET', '/api/station/status');
    assert.equal(response.status, 200);
    assert.equal(response.json.publicUrl, expected, configured);
    // The invite link the station builds from it reaches the sender page.
    const link = new URL(`${response.json.publicUrl}/#code=${service.code}`);
    assert.equal(link.hash, `#code=${service.code}`);
    assert.ok(link.pathname.endsWith('/'), link.href);
    await service.close();
  }
  const unset = await startDrop(t, { env: { DROP_PUBLIC_URL: '' } });
  assert.equal((await station(unset.port, unset.key, 'GET', '/api/station/status')).json.publicUrl, null);
});

test('the inbox lists items oldest first with exactly the protocol fields', async (t) => {
  const clock = manualClock();
  const service = await startDrop(t, { clock });
  const sent = [
    ['first.stl', ASCII_STL, 'Ada', 'stl'],
    ['second.step', STEP, '', 'step'],
    // Same millisecond as the second: arrival order still decides.
    ['third.stl', binaryStl(1), '陳小姐', 'stl'],
  ];
  const ids = [];
  for (const [index, [fileName, body, sender]] of sent.entries()) {
    if (index === 1) clock.advance(1500);
    const response = await drop(service.port, { code: service.code, fileName, body, sender: sender || undefined });
    assert.equal(response.status, 201);
    ids.push(response.json.id);
  }
  const response = await station(service.port, service.key, 'GET', '/api/station/inbox');
  assert.equal(response.status, 200);
  assert.deepEqual(Object.keys(response.json), ['items']);
  const { items } = response.json;
  assert.deepEqual(items.map((item) => item.id), ids);
  for (const [index, item] of items.entries()) {
    const [fileName, body, sender, type] = sent[index];
    assert.deepEqual(Object.keys(item).sort(), ['bytes', 'fileName', 'id', 'receivedAt', 'sender', 'sha256', 'type']);
    assert.equal(item.fileName, fileName);
    assert.equal(item.sender, sender);
    assert.equal(item.bytes, body.length);
    assert.equal(item.sha256, sha256(body));
    assert.equal(item.type, type);
    assert.match(item.receivedAt, /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);
  }
  assert.equal(items[0].receivedAt, '2026-10-09T08:00:00Z');
  assert.equal(items[1].receivedAt, '2026-10-09T08:00:01Z');
});

test('a file downloads with its length and SHA-256, and removing it is idempotent', async (t) => {
  const service = await startDrop(t);
  const body = binaryStl(40);
  const created = await drop(service.port, { code: service.code, fileName: "it's (final) *.stl", body });
  assert.equal(created.status, 201);
  const { id } = created.json;

  const download = await station(service.port, service.key, 'GET', `/api/station/files/${id}`);
  assert.equal(download.status, 200);
  assert.equal(download.headers['content-type'], 'application/octet-stream');
  assert.equal(download.headers['content-length'], String(body.length));
  assert.equal(download.headers['x-content-sha256'], sha256(body));
  assert.match(download.headers['content-disposition'], /^attachment; filename\*=UTF-8''it%27s%20%28final%29%20\.stl$/);
  assert.ok(download.body.equals(body));

  const head = await station(service.port, service.key, 'HEAD', `/api/station/files/${id}`);
  assert.equal(head.status, 200);
  assert.equal(head.headers['x-content-sha256'], sha256(body));
  assert.equal(head.body.length, 0);

  const queueDir = path.join(service.dataDir, 'queue');
  assert.deepEqual(fs.readdirSync(queueDir).sort(), [`${id}.bin`, `${id}.json`]);
  for (let attempt = 0; attempt < 2; attempt += 1) {
    const removed = await station(service.port, service.key, 'DELETE', `/api/station/files/${id}`);
    assert.equal(removed.status, 204, `delete ${attempt + 1}`);
    assert.equal(removed.body.length, 0);
  }
  assert.deepEqual(fs.readdirSync(queueDir), []);
  const gone = await station(service.port, service.key, 'GET', `/api/station/files/${id}`);
  assert.equal(gone.status, 404);
  assert.equal(gone.json.ok, false);
  assert.equal(gone.json.error, 'not_found');
  assert.deepEqual((await station(service.port, service.key, 'GET', '/api/station/inbox')).json.items, []);

  for (const bad of ['../station-key', '..%2Fstation-key', 'ABCDEF'.padEnd(32, '0'), 'a'.repeat(31), 'a'.repeat(33)]) {
    const response = await station(service.port, service.key, 'GET', `/api/station/files/${bad}`);
    assert.equal(response.status, 404, bad);
  }
});

test('the drop code can be renewed, unless DROP_CODE fixes it', async (t) => {
  const service = await startDrop(t);
  const old = service.code;
  const renewed = await station(service.port, service.key, 'POST', '/api/station/drop-code');
  assert.equal(renewed.status, 200);
  assert.deepEqual(Object.keys(renewed.json), ['dropCode']);
  assert.match(renewed.json.dropCode, /^\d{6}$/);
  assert.notEqual(renewed.json.dropCode, old);
  assert.equal((await station(service.port, service.key, 'GET', '/api/station/status')).json.dropCode, renewed.json.dropCode);
  assert.equal(fs.readFileSync(path.join(service.dataDir, 'drop-code'), 'utf8').trim(), renewed.json.dropCode);
  assert.equal((await drop(service.port, { code: old, fileName: 'cube.obj', body: OBJ })).status, 401);
  assert.equal((await drop(service.port, { code: renewed.json.dropCode, fileName: 'cube.obj', body: OBJ })).status, 201);
  assert.equal((await station(service.port, service.key, 'GET', '/api/station/drop-code')).status, 405);

  const fixed = await startDrop(t, { env: { DROP_CODE: '4821' } });
  assert.equal(fixed.code, '4821');
  const refused = await station(fixed.port, fixed.key, 'POST', '/api/station/drop-code');
  assert.equal(refused.status, 409);
  assert.equal(refused.json.ok, false);
  assert.equal(refused.json.error, 'fixed_code');
  assert.equal((await station(fixed.port, fixed.key, 'GET', '/api/station/status')).json.dropCode, '4821');
  assert.equal(fs.existsSync(path.join(fixed.dataDir, 'drop-code')), false, 'a fixed code is not written to the volume');
  assert.equal((await drop(fixed.port, { code: '4821', fileName: 'cube.obj', body: OBJ })).status, 201);
});

test('items older than DROP_TTL_HOURS are removed, on the timer and at start', async (t) => {
  const clock = manualClock();
  const dataDir = tempDir(t);
  const first = await startDrop(t, { clock, dataDir, env: { DROP_TTL_HOURS: '1' } });
  const old = await drop(first.port, { code: first.code, fileName: 'old.obj', body: OBJ });
  clock.advance(30 * 60 * 1000);
  const newer = await drop(first.port, { code: first.code, fileName: 'newer.obj', body: OBJ });
  assert.equal(old.status, 201);
  assert.equal(newer.status, 201);

  clock.advance(29 * 60 * 1000);
  assert.equal(await first.drop.service.expire(), 0);
  clock.advance(60 * 1000);
  assert.equal(await first.drop.service.expire(), 1);
  const items = (await station(first.port, first.key, 'GET', '/api/station/inbox')).json.items;
  assert.deepEqual(items.map((item) => item.fileName), ['newer.obj']);
  assert.ok(!fs.existsSync(path.join(dataDir, 'queue', `${old.json.id}.bin`)));
  assert.ok(!fs.existsSync(path.join(dataDir, 'queue', `${old.json.id}.json`)));
  assert.ok(first.logs.some((line) => /removed 1 expired item/.test(line)));
  await first.close();

  // A restart long after the TTL removes what expired while it was stopped,
  // and keeps the key and code.
  clock.advance(2 * 60 * 60 * 1000);
  const second = await startDrop(t, { clock, dataDir, env: { DROP_TTL_HOURS: '1' } });
  assert.equal(second.key, first.key);
  assert.equal(second.code, first.code);
  assert.deepEqual((await station(second.port, second.key, 'GET', '/api/station/inbox')).json.items, []);
  assert.deepEqual(fs.readdirSync(path.join(dataDir, 'queue')), []);
});

test('a restart keeps queued items and clears unfinished uploads', async (t) => {
  const dataDir = tempDir(t);
  const first = await startDrop(t, { dataDir });
  const kept = await drop(first.port, { code: first.code, fileName: 'kept.stl', body: ASCII_STL });
  await first.close();
  const queueDir = path.join(dataDir, 'queue');
  fs.writeFileSync(path.join(queueDir, `${'b'.repeat(32)}.bin.tmp`), 'partial');
  fs.writeFileSync(path.join(queueDir, `${'c'.repeat(32)}.bin`), 'no record');
  fs.writeFileSync(path.join(queueDir, `${'d'.repeat(32)}.json`), '{not json');

  const second = await startDrop(t, { dataDir });
  const items = (await station(second.port, second.key, 'GET', '/api/station/inbox')).json.items;
  assert.deepEqual(items.map((item) => item.id), [kept.json.id]);
  assert.deepEqual(fs.readdirSync(queueDir).sort(), [`${kept.json.id}.bin`, `${kept.json.id}.json`]);
});

test('the generated station key and drop code are stored with mode 0600 and never logged', async (t) => {
  const service = await startDrop(t);
  const keyFile = path.join(service.dataDir, 'station-key');
  const codeFile = path.join(service.dataDir, 'drop-code');
  assert.match(service.key, /^[A-Za-z0-9_-]{43}$/, '32 random bytes as base64url');
  assert.equal(fs.readFileSync(keyFile, 'utf8'), `${service.key}\n`);
  assert.equal(fs.readFileSync(codeFile, 'utf8'), `${service.code}\n`);
  if (!isWindows) {
    assert.equal(fs.statSync(keyFile).mode & 0o777, 0o600);
    assert.equal(fs.statSync(codeFile).mode & 0o777, 0o600);
  }
  await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ });
  await station(service.port, service.key, 'POST', '/api/station/drop-code');
  const log = service.logs.join('\n');
  assert.ok(log.includes('docker compose exec lan-model-drop node server/station-key.mjs'), log);
  assert.ok(!log.includes(service.key), 'the station key is never logged');
  assert.ok(!log.includes(service.code), 'the drop code is never logged');
  assert.ok(!log.includes('cube.obj'), 'file names are not logged');

  // A widened file is made private again at the next start.
  if (!isWindows) {
    await service.close();
    fs.chmodSync(keyFile, 0o644);
    await startDrop(t, { dataDir: service.dataDir });
    assert.equal(fs.statSync(keyFile).mode & 0o777, 0o600);
  }
});

test('DROP_STATION_KEY replaces the generated key and is not written to the volume', async (t) => {
  const key = 'configured-station-key-0123456789';
  const service = await startDrop(t, { env: { DROP_STATION_KEY: key } });
  assert.equal(fs.existsSync(path.join(service.dataDir, 'station-key')), false);
  assert.equal((await station(service.port, key, 'GET', '/api/station/status')).status, 200);
  assert.ok(service.logs.some((line) => line === 'Using the station key from DROP_STATION_KEY.'));
  assert.ok(!service.logs.join('\n').includes(key));
});

test('server/station-key.mjs prints the key on request', async (t) => {
  const script = path.join(dropRoot, 'server', 'station-key.mjs');
  const empty = tempDir(t);
  const missing = spawnSync(process.execPath, [script], { env: { ...process.env, DROP_DATA_DIR: empty, DROP_STATION_KEY: '' }, encoding: 'utf8' });
  assert.equal(missing.status, 1);
  assert.equal(missing.stdout, '');
  assert.match(missing.stderr, /Start the service once/);

  const service = await startDrop(t);
  const printed = execFileSync(process.execPath, [script], { env: { ...process.env, DROP_DATA_DIR: service.dataDir, DROP_STATION_KEY: '' }, encoding: 'utf8' });
  assert.equal(printed, `${service.key}\n`);
  const configured = execFileSync(process.execPath, [script], { env: { ...process.env, DROP_DATA_DIR: empty, DROP_STATION_KEY: 'configured-station-key-0123456789' }, encoding: 'utf8' });
  assert.equal(configured, 'configured-station-key-0123456789\n');
});
