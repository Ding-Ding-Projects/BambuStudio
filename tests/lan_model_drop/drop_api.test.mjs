// The sender side of the LAN model drop protocol (version 1): accepted
// types and their content checks, every refusal code, and file names.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {
  AMF_XML, ASCII_STL, binaryStl, drop, manualClock, OBJ, request, startDrop, station, STEP, zipLike,
} from './harness.mjs';

function errorOf(response) {
  assert.equal(response.json?.ok, false, `expected a JSON error, got ${response.status} ${response.body}`);
  assert.equal(typeof response.json.message, 'string');
  return response.json.error;
}

async function inbox(service) {
  const response = await station(service.port, service.key, 'GET', '/api/station/inbox');
  assert.equal(response.status, 200);
  return response.json.items;
}

test('every accepted type is stored when its content matches', async (t) => {
  const service = await startDrop(t);
  const cases = [
    ['plate.3mf', zipLike(), '3mf'],
    ['cube-ascii.stl', ASCII_STL, 'stl'],
    ['cube-binary.stl', binaryStl(3), 'stl'],
    ['bracket.step', STEP, 'step'],
    ['bracket.STP', STEP, 'step'],
    ['cube.obj', OBJ, 'obj'],
    ['cube.amf', AMF_XML, 'amf'],
    ['cube-zipped.amf', zipLike(120), 'amf'],
    ['bom.step', Buffer.concat([Buffer.from([0xef, 0xbb, 0xbf]), STEP]), 'step'],
  ];
  for (const [fileName, body] of cases) {
    const response = await drop(service.port, { code: service.code, fileName, body });
    assert.equal(response.status, 201, `${fileName}: ${response.body}`);
    assert.equal(response.json.ok, true);
    assert.match(response.json.id, /^[0-9a-f]{32}$/);
  }
  const items = await inbox(service);
  assert.deepEqual(items.map((item) => [item.fileName, item.type]), cases.map(([name, , type]) => [name, type]));
});

test('content that does not match its extension is refused with 415', async (t) => {
  const service = await startDrop(t);
  const cases = [
    ['not-a-zip.3mf', Buffer.from('this is plain text, not a zip archive')],
    ['random.stl', Buffer.from(Array.from({ length: 300 }, (_, i) => (i * 7) & 0xff))],
    ['wrong-count.stl', Buffer.concat([binaryStl(3).subarray(0, 84 + 50 * 3), Buffer.alloc(1)])],
    ['empty-solid.stl', Buffer.from('solid nothing\nendsolid nothing\n')],
    ['solid-with-nul.stl', Buffer.concat([ASCII_STL, Buffer.from([0])])],
    ['no-header.step', Buffer.from('HEADER;\nENDSEC;\n')],
    ['binary.obj', Buffer.concat([OBJ, Buffer.from([0, 1, 2, 3])])],
    ['no-vertices.obj', Buffer.from('# only a comment\nf 1 2 3\n')],
    ['plain.amf', Buffer.from('<?xml version="1.0"?>\n<model></model>\n')],
    ['empty.stl', Buffer.alloc(0)],
  ];
  for (const [fileName, body] of cases) {
    const response = await drop(service.port, { code: service.code, fileName, body });
    assert.equal(response.status, 415, `${fileName}: ${response.body}`);
    assert.equal(errorOf(response), 'unsupported_type');
  }
  assert.deepEqual(await inbox(service), []);
  assert.deepEqual(fs.readdirSync(path.join(service.dataDir, 'queue')), [], 'refused uploads leave nothing on disk');
});

test('other extensions are refused with 415 before any byte is stored', async (t) => {
  const service = await startDrop(t);
  for (const fileName of ['setup.exe', 'notes.txt', 'model', 'archive.zip', 'model.stl.exe', 'page.html']) {
    const response = await drop(service.port, { code: service.code, fileName, body: zipLike() });
    assert.equal(response.status, 415, fileName);
    assert.equal(errorOf(response), 'unsupported_type');
  }
});

test('a wrong drop code is 401, and the fifth wrong code in a minute locks the address out for five minutes', async (t) => {
  const clock = manualClock();
  const service = await startDrop(t, { clock });
  const wrong = service.code === '000000' ? '111111' : '000000';
  for (let attempt = 1; attempt <= 5; attempt += 1) {
    const response = await drop(service.port, { code: wrong, fileName: 'cube.obj', body: OBJ });
    assert.equal(response.status, 401, `attempt ${attempt}`);
    assert.equal(errorOf(response), 'wrong_code');
  }
  // Locked: even the right code is refused, with a Retry-After.
  const locked = await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ });
  assert.equal(locked.status, 429);
  assert.equal(errorOf(locked), 'too_many_attempts');
  assert.equal(locked.headers['retry-after'], '300');
  const noCode = await drop(service.port, { fileName: 'cube.obj', body: OBJ });
  assert.equal(noCode.status, 429);

  clock.advance(4 * 60 * 1000 + 59 * 1000);
  assert.equal((await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ })).status, 429);
  clock.advance(1000);
  assert.equal((await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ })).status, 201);
  assert.ok(service.logs.some((line) => /locked out 127\.0\.0\.1/.test(line)));
});

test('wrong codes spread over more than a minute do not lock the address', async (t) => {
  const clock = manualClock();
  const service = await startDrop(t, { clock });
  const wrong = service.code === '000000' ? '111111' : '000000';
  for (let round = 0; round < 3; round += 1) {
    for (let attempt = 0; attempt < 4; attempt += 1) {
      assert.equal((await drop(service.port, { code: wrong, fileName: 'cube.obj', body: OBJ })).status, 401);
    }
    clock.advance(61 * 1000);
  }
  assert.equal((await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ })).status, 201);
});

test('a missing drop code counts as a wrong code', async (t) => {
  const service = await startDrop(t);
  const response = await drop(service.port, { fileName: 'cube.obj', body: OBJ });
  assert.equal(response.status, 401);
  assert.equal(errorOf(response), 'wrong_code');
});

test('a lockout belongs to one client address', async (t) => {
  const service = await startDrop(t);
  const wrong = service.code === '000000' ? '111111' : '000000';
  for (let attempt = 0; attempt < 5; attempt += 1) {
    await drop(service.port, { code: wrong, fileName: 'cube.obj', body: OBJ });
  }
  assert.equal((await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ })).status, 429);
  let other;
  try {
    other = await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ, localAddress: '127.0.0.2' });
  } catch (error) {
    if (['EADDRNOTAVAIL', 'EINVAL'].includes(error.code)) {
      t.skip('this system cannot send from 127.0.0.2');
      return;
    }
    throw error;
  }
  assert.equal(other.status, 201);
});

test('an upload without a Content-Length is 411', async (t) => {
  const service = await startDrop(t);
  const response = await request(service.port, {
    method: 'POST',
    path: '/api/drop',
    headers: { 'X-Drop-Code': service.code, 'X-Drop-Filename': 'cube.obj' },
    body: OBJ,
    chunked: true,
  });
  assert.equal(response.status, 411);
  assert.equal(errorOf(response), 'length_required');
  assert.equal(response.headers.connection, 'close');
});

test('a file larger than DROP_MAX_BYTES is 413 before it is stored', async (t) => {
  const service = await startDrop(t, { env: { DROP_MAX_BYTES: '1000', DROP_QUEUE_MAX_BYTES: '100000' } });
  const atLimit = await drop(service.port, { code: service.code, fileName: 'fits.3mf', body: zipLike(1000) });
  assert.equal(atLimit.status, 201);
  const over = await drop(service.port, { code: service.code, fileName: 'big.3mf', body: zipLike(1001) });
  assert.equal(over.status, 413);
  assert.equal(errorOf(over), 'too_large');
  assert.equal((await inbox(service)).length, 1);
});

test('a full drop box is 507, by file count and by bytes', async (t) => {
  const byCount = await startDrop(t, { env: { DROP_QUEUE_MAX_FILES: '2' } });
  for (let i = 0; i < 2; i += 1) {
    assert.equal((await drop(byCount.port, { code: byCount.code, fileName: `cube${i}.obj`, body: OBJ })).status, 201);
  }
  const third = await drop(byCount.port, { code: byCount.code, fileName: 'cube2.obj', body: OBJ });
  assert.equal(third.status, 507);
  assert.equal(errorOf(third), 'queue_full');

  // Taking an item out makes room again.
  const [first] = await inbox(byCount);
  assert.equal((await station(byCount.port, byCount.key, 'DELETE', `/api/station/files/${first.id}`)).status, 204);
  assert.equal((await drop(byCount.port, { code: byCount.code, fileName: 'cube2.obj', body: OBJ })).status, 201);

  const byBytes = await startDrop(t, { env: { DROP_MAX_BYTES: '500', DROP_QUEUE_MAX_BYTES: '800' } });
  assert.equal((await drop(byBytes.port, { code: byBytes.code, fileName: 'a.3mf', body: zipLike(500) })).status, 201);
  const full = await drop(byBytes.port, { code: byBytes.code, fileName: 'b.3mf', body: zipLike(400) });
  assert.equal(full.status, 507);
  assert.equal(errorOf(full), 'queue_full');
  assert.equal((await drop(byBytes.port, { code: byBytes.code, fileName: 'c.3mf', body: zipLike(300) })).status, 201);
});

test('file names are reduced to a clean base name and never reach the disk', async (t) => {
  const service = await startDrop(t);
  const accepted = [
    ['../../evil.stl', 'evil.stl'],
    ['..\\..\\Windows\\System32\\drivers.stl', 'drivers.stl'],
    ['/etc/passwd.stl', 'passwd.stl'],
    ['D:\\models\\part.stl', 'part.stl'],
    ['a\u0000b\u001fc.stl', 'abc.stl'],
    ['what?<is>this|"*:.stl', 'whatisthis.stl'],
    ['  spaced name.stl  ', 'spaced name.stl'],
    ['trailing.stl. . ', 'trailing.stl'],
    ['con.stl', '_con.stl'],
    ['LPT1.stl', '_LPT1.stl'],
    ['模型 零件.stl', '模型 零件.stl'],
  ];
  for (const [sent] of accepted) {
    const response = await drop(service.port, { code: service.code, fileName: sent, body: ASCII_STL });
    assert.equal(response.status, 201, `${JSON.stringify(sent)}: ${response.body}`);
  }
  const items = await inbox(service);
  assert.deepEqual(items.map((item) => item.fileName), accepted.map(([, stored]) => stored));

  const long = `${'x'.repeat(300)}.stl`;
  const longResponse = await drop(service.port, { code: service.code, fileName: long, body: ASCII_STL });
  assert.equal(longResponse.status, 201);
  const longName = (await inbox(service)).at(-1).fileName;
  assert.equal([...longName].length, 200);
  assert.ok(longName.endsWith('.stl'));

  // Only random ids are ever written.
  const stored = fs.readdirSync(path.join(service.dataDir, 'queue'));
  assert.ok(stored.every((name) => /^[0-9a-f]{32}\.(?:bin|json)$/.test(name)), stored.join(', '));
  assert.ok(!fs.existsSync(path.join(service.dataDir, 'evil.stl')));
  assert.ok(!fs.existsSync(path.join(path.dirname(service.dataDir), 'evil.stl')));
});

test('names that clean down to nothing, malformed encodings and long senders are 400', async (t) => {
  const service = await startDrop(t);
  const badNames = ['..', '../..', '/', '\\', '.stl', '   .stl', '...', ' ', '\u0000', '<>:"|?*'];
  for (const fileName of badNames) {
    const response = await drop(service.port, { code: service.code, fileName, body: ASCII_STL });
    assert.equal(response.status, 400, JSON.stringify(fileName));
    assert.equal(errorOf(response), 'bad_request');
  }
  const missing = await drop(service.port, { code: service.code, body: ASCII_STL });
  assert.equal(missing.status, 400);
  const malformed = await drop(service.port, { code: service.code, body: ASCII_STL, headers: { 'X-Drop-Filename': 'cube%E0%A4%A.stl' } });
  assert.equal(malformed.status, 400);
  assert.equal(errorOf(malformed), 'bad_request');

  const longSender = await drop(service.port, { code: service.code, fileName: 'cube.stl', body: ASCII_STL, sender: 'a'.repeat(41) });
  assert.equal(longSender.status, 400);
  const badSender = await drop(service.port, { code: service.code, fileName: 'cube.stl', body: ASCII_STL, headers: { 'X-Drop-Sender': '%ZZ' } });
  assert.equal(badSender.status, 400);
  assert.deepEqual(await inbox(service), []);
});

test('sender names are optional, cleaned and limited to 40 characters', async (t) => {
  const service = await startDrop(t);
  const senders = [
    [undefined, ''],
    ['Ada', 'Ada'],
    ['  Mei\tLing\u0007 ', 'Mei Ling'],
    ['陳大文'.repeat(13) + '陳', '陳大文'.repeat(13) + '陳'],
  ];
  for (const [sender] of senders) {
    assert.equal((await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ, sender })).status, 201);
  }
  assert.deepEqual((await inbox(service)).map((item) => item.sender), senders.map(([, stored]) => stored));
});

test('only POST reaches the drop endpoint', async (t) => {
  const service = await startDrop(t);
  for (const method of ['GET', 'PUT', 'DELETE', 'OPTIONS']) {
    const response = await request(service.port, { method, path: '/api/drop' });
    assert.equal(response.status, 405, method);
    assert.equal(response.headers.allow, 'POST');
    // No cross-origin permission is ever granted.
    assert.equal(response.headers['access-control-allow-origin'], undefined);
  }
});
