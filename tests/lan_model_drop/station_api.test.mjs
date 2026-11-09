// The station side of the LAN model drop protocol (version 1): status, inbox,
// file download and removal, the retired drop code and expiry. The station
// needs no key.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {
  ASCII_STL, binaryStl, drop, dropRoot, manualClock, OBJ, request, startDrop, station, STEP, tempDir, zipLike,
} from './harness.mjs';

const sha256 = (buffer) => createHash('sha256').update(buffer).digest('hex');

test('/healthz answers without a key', async (t) => {
  const service = await startDrop(t);
  const response = await request(service.port, { path: '/healthz' });
  assert.equal(response.status, 200);
  assert.deepEqual(response.json, { ok: true, service: 'lan-model-drop', protocol: 1 });
});

test('station endpoints answer without a key and ignore an Authorization header', async (t) => {
  const service = await startDrop(t);
  const id = 'a'.repeat(32);
  const targets = [
    ['GET', '/api/station/status', 200],
    ['GET', '/api/station/inbox', 200],
    ['GET', `/api/station/files/${id}`, 404],
    ['DELETE', `/api/station/files/${id}`, 204],
    ['POST', '/api/station/drop-code', 410],
    ['GET', '/api/station/unknown', 404],
  ];
  // An older Bambu Studio still sends a Bearer header, and anything else may arrive too: none of it matters.
  const headerSets = [
    {},
    { Authorization: 'Bearer ' + 'k'.repeat(43) },
    { Authorization: 'Bearer x' },
    { Authorization: 'Basic abc' },
    { Authorization: '' },
  ];
  for (const [method, target, expected] of targets) {
    for (const headers of headerSets) {
      const response = await request(service.port, { method, path: target, headers });
      assert.equal(response.status, expected, `${method} ${target} with ${JSON.stringify(headers)}`);
      assert.notEqual(response.json?.error, 'unauthorized');
      assert.equal(response.headers['www-authenticate'], undefined);
    }
  }
  assert.equal((await station(service.port, 'GET', '/api/station/status')).status, 200);
});

test('status reports the protocol, station and drop box', async (t) => {
  const service = await startDrop(t, { env: { DROP_STATION_NAME: 'Workshop PC', DROP_MAX_BYTES: '5000', DROP_QUEUE_MAX_BYTES: '50000', DROP_TTL_HOURS: '6' } });
  assert.equal((await drop(service.port, { fileName: 'a.3mf', body: zipLike(300) })).status, 201);
  assert.equal((await drop(service.port, { fileName: 'b.obj', body: OBJ })).status, 201);
  const response = await station(service.port, 'GET', '/api/station/status');
  assert.equal(response.status, 200);
  assert.deepEqual(response.json, {
    protocol: 1,
    stationName: 'Workshop PC',
    directSend: true,
    dropCode: 'not-required',
    queued: 2,
    queuedBytes: 300 + OBJ.length,
    maxBytes: 5000,
    ttlHours: 6,
    publicUrl: null,
  });
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
    const response = await station(service.port, 'GET', '/api/station/status');
    assert.equal(response.status, 200);
    assert.equal(response.json.publicUrl, expected, configured);
    // The invite link the station builds from it reaches the sender page.
    const link = new URL(`${response.json.publicUrl}/`);
    assert.equal(link.hash, '');
    assert.ok(link.pathname.endsWith('/'), link.href);
    await service.close();
  }
  const unset = await startDrop(t, { env: { DROP_PUBLIC_URL: '' } });
  assert.equal((await station(unset.port, 'GET', '/api/station/status')).json.publicUrl, null);
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
    const response = await drop(service.port, { fileName, body, sender: sender || undefined });
    assert.equal(response.status, 201);
    ids.push(response.json.id);
  }
  const response = await station(service.port, 'GET', '/api/station/inbox');
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
  const created = await drop(service.port, { fileName: "it's (final) *.stl", body });
  assert.equal(created.status, 201);
  const { id } = created.json;

  const download = await station(service.port, 'GET', `/api/station/files/${id}`);
  assert.equal(download.status, 200);
  assert.equal(download.headers['content-type'], 'application/octet-stream');
  assert.equal(download.headers['content-length'], String(body.length));
  assert.equal(download.headers['x-content-sha256'], sha256(body));
  assert.match(download.headers['content-disposition'], /^attachment; filename\*=UTF-8''it%27s%20%28final%29%20\.stl$/);
  assert.ok(download.body.equals(body));

  const head = await station(service.port, 'HEAD', `/api/station/files/${id}`);
  assert.equal(head.status, 200);
  assert.equal(head.headers['x-content-sha256'], sha256(body));
  assert.equal(head.body.length, 0);

  const queueDir = path.join(service.dataDir, 'queue');
  assert.deepEqual(fs.readdirSync(queueDir).sort(), [`${id}.bin`, `${id}.json`]);
  for (let attempt = 0; attempt < 2; attempt += 1) {
    const removed = await station(service.port, 'DELETE', `/api/station/files/${id}`);
    assert.equal(removed.status, 204, `delete ${attempt + 1}`);
    assert.equal(removed.body.length, 0);
  }
  assert.deepEqual(fs.readdirSync(queueDir), []);
  const gone = await station(service.port, 'GET', `/api/station/files/${id}`);
  assert.equal(gone.status, 404);
  assert.equal(gone.json.ok, false);
  assert.equal(gone.json.error, 'not_found');
  assert.deepEqual((await station(service.port, 'GET', '/api/station/inbox')).json.items, []);

  for (const bad of ['../station-key', '..%2Fstation-key', 'ABCDEF'.padEnd(32, '0'), 'a'.repeat(31), 'a'.repeat(33)]) {
    const response = await station(service.port, 'GET', `/api/station/files/${bad}`);
    assert.equal(response.status, 404, bad);
  }
});

test('legacy renewal is retired without revocation and old configuration does not gate senders', async (t) => {
  const dataDir = tempDir(t);
  fs.writeFileSync(path.join(dataDir, 'drop-code'), 'legacy-value');
  const service = await startDrop(t, { dataDir, env: { DROP_CODE: 'obsolete' } });
  assert.equal((await drop(service.port, { fileName: 'cube.obj', body: OBJ })).status, 201);
  for (let attempt = 0; attempt < 2; ++attempt) {
    const renewed = await station(service.port, 'POST', '/api/station/drop-code');
    assert.equal(renewed.status, 410);
    assert.equal(renewed.json.error, 'retired');
    assert.equal(Object.hasOwn(renewed.json, 'dropCode'), false);
    assert.match(renewed.json.message, /Existing links still work/);
  }
  assert.equal(fs.readFileSync(path.join(dataDir, 'drop-code'), 'utf8'), 'legacy-value');
  const status = (await station(service.port, 'GET', '/api/station/status')).json;
  assert.equal(status.dropCode, 'not-required');
  assert.equal(status.directSend, true);
  assert.equal((await drop(service.port, {code: 'obsolete', fileName: 'old-client.stl', body: ASCII_STL})).status, 201);
});

test('items older than DROP_TTL_HOURS are removed, on the timer and at start', async (t) => {
  const clock = manualClock();
  const dataDir = tempDir(t);
  const first = await startDrop(t, { clock, dataDir, env: { DROP_TTL_HOURS: '1' } });
  const old = await drop(first.port, { fileName: 'old.obj', body: OBJ });
  clock.advance(30 * 60 * 1000);
  const newer = await drop(first.port, { fileName: 'newer.obj', body: OBJ });
  assert.equal(old.status, 201);
  assert.equal(newer.status, 201);

  clock.advance(29 * 60 * 1000);
  assert.equal(await first.drop.service.expire(), 0);
  clock.advance(60 * 1000);
  assert.equal(await first.drop.service.expire(), 1);
  const items = (await station(first.port, 'GET', '/api/station/inbox')).json.items;
  assert.deepEqual(items.map((item) => item.fileName), ['newer.obj']);
  assert.ok(!fs.existsSync(path.join(dataDir, 'queue', `${old.json.id}.bin`)));
  assert.ok(!fs.existsSync(path.join(dataDir, 'queue', `${old.json.id}.json`)));
  assert.ok(first.logs.some((line) => /removed 1 expired item/.test(line)));
  await first.close();

  // A restart long after the TTL removes what expired while it was stopped,
  // and keeps the queue.
  clock.advance(2 * 60 * 60 * 1000);
  const second = await startDrop(t, { clock, dataDir, env: { DROP_TTL_HOURS: '1' } });
  assert.deepEqual((await station(second.port, 'GET', '/api/station/inbox')).json.items, []);
  assert.deepEqual(fs.readdirSync(path.join(dataDir, 'queue')), []);
});

test('a restart keeps queued items and clears unfinished uploads', async (t) => {
  const dataDir = tempDir(t);
  const first = await startDrop(t, { dataDir });
  const kept = await drop(first.port, { fileName: 'kept.stl', body: ASCII_STL });
  await first.close();
  const queueDir = path.join(dataDir, 'queue');
  fs.writeFileSync(path.join(queueDir, `${'b'.repeat(32)}.bin.tmp`), 'partial');
  fs.writeFileSync(path.join(queueDir, `${'c'.repeat(32)}.bin`), 'no record');
  fs.writeFileSync(path.join(queueDir, `${'d'.repeat(32)}.json`), '{not json');

  const second = await startDrop(t, { dataDir });
  const items = (await station(second.port, 'GET', '/api/station/inbox')).json.items;
  assert.deepEqual(items.map((item) => item.id), [kept.json.id]);
  assert.deepEqual(fs.readdirSync(queueDir).sort(), [`${kept.json.id}.bin`, `${kept.json.id}.json`]);
});

test('the service keeps no station key: no key file, no key command, no key log line', async (t) => {
  const service = await startDrop(t);
  await drop(service.port, { fileName: 'cube.obj', body: OBJ });
  await station(service.port, 'GET', '/api/station/inbox');
  assert.equal(fs.existsSync(path.join(service.dataDir, 'station-key')), false);
  assert.equal(fs.existsSync(path.join(dropRoot, 'server', 'station-key.mjs')), false);
  assert.equal(fs.existsSync(path.join(dropRoot, 'server', 'secrets.mjs')), false);
  const log = service.logs.join('\n');
  assert.ok(!/station key|station-key/i.test(log), log);
  assert.ok(!log.includes('cube.obj'), 'file names are not logged');
});

test('a DROP_STATION_KEY left in an old .env is ignored and nothing is written for it', async (t) => {
  for (const value of ['configured-station-key-0123456789', 'too-short', 'has a space in it']) {
    const service = await startDrop(t, { env: { DROP_STATION_KEY: value } });
    assert.equal(fs.existsSync(path.join(service.dataDir, 'station-key')), false);
    assert.equal((await station(service.port, 'GET', '/api/station/status')).status, 200);
    assert.ok(!service.logs.join('\n').includes(value));
    await service.close();
  }
});
