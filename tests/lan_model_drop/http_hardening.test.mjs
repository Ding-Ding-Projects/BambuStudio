// Transport hardening of the LAN model drop service: security headers on
// every response, time-limited requests, the running size check on a
// streamed body, and configuration validation.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import { Readable } from 'node:stream';
import test from 'node:test';
import {
  ConfigError, drop, OBJ, readConfig, receiveBody, request, SECURITY_HEADERS, startDrop, station, tempDir,
} from './harness.mjs';

function assertSecurityHeaders(response, label) {
  const csp = response.headers['content-security-policy'];
  assert.ok(csp, `${label}: Content-Security-Policy`);
  for (const directive of ["default-src 'self'", "script-src 'self'", "frame-ancestors 'none'", "object-src 'none'", "base-uri 'none'"]) {
    assert.ok(csp.split('; ').includes(directive), `${label}: ${directive}`);
  }
  assert.doesNotMatch(csp, /unsafe-inline|unsafe-eval|https?:|\*/, `${label}: no inline script, no remote source`);
  assert.equal(response.headers['x-content-type-options'], 'nosniff', label);
  assert.equal(response.headers['referrer-policy'], 'no-referrer', label);
  assert.equal(response.headers['x-frame-options'], 'DENY', label);
}

test('every kind of response carries the security headers', async (t) => {
  const service = await startDrop(t);
  const responses = [
    ['page', await request(service.port, { path: '/' })],
    ['page by name', await request(service.port, { path: '/index.html' })],
    ['page HEAD', await request(service.port, { method: 'HEAD', path: '/' })],
    ['health', await request(service.port, { path: '/healthz' })],
    ['unknown page', await request(service.port, { path: '/nothing-here' })],
    ['unknown API', await request(service.port, { path: '/api/nothing' })],
    ['drop refused', await drop(service.port, { code: 'nope', fileName: 'cube.obj', body: OBJ })],
    ['drop accepted', await drop(service.port, { code: service.code, fileName: 'cube.obj', body: OBJ })],
    ['wrong method', await request(service.port, { method: 'PUT', path: '/' })],
    ['station refused', await request(service.port, { path: '/api/station/inbox' })],
    ['station inbox', await station(service.port, service.key, 'GET', '/api/station/inbox')],
  ];
  for (const [label, response] of responses) assertSecurityHeaders(response, label);
  assert.equal(responses[0][1].status, 200);
  assert.match(responses[0][1].headers['content-type'], /^text\/html; charset=utf-8$/);
  assert.equal(responses[0][1].headers['cache-control'], 'no-store');
  assert.equal(responses[4][1].status, 404);
  assert.equal(responses[8][1].status, 405);
  assert.equal(SECURITY_HEADERS['Content-Security-Policy'], responses[0][1].headers['content-security-policy']);
});

test('requests refused before the handler still carry the security headers', async (t) => {
  const service = await startDrop(t);
  const raw = await new Promise((resolve, reject) => {
    const socket = net.connect(service.port, '127.0.0.1', () => socket.write('NOT HTTP AT ALL\r\n\r\n'));
    let text = '';
    socket.setEncoding('utf8');
    socket.on('data', (chunk) => { text += chunk; });
    socket.on('close', () => resolve(text));
    socket.on('error', reject);
  });
  assert.match(raw, /^HTTP\/1\.1 400 Bad Request\r\n/);
  assert.match(raw, /\r\nContent-Security-Policy: default-src 'self'/);
  assert.match(raw, /\r\nX-Content-Type-Options: nosniff\r\n/);
  assert.match(raw, /\r\nReferrer-Policy: no-referrer\r\n/);
});

test('an upload that stalls is cut off by the request time limit and leaves nothing behind', async (t) => {
  const service = await startDrop(t, {
    serverOptions: { requestTimeoutMs: 800, headersTimeoutMs: 500, connectionsCheckingIntervalMs: 100 },
  });
  const started = Date.now();
  const raw = await new Promise((resolve, reject) => {
    const socket = net.connect(service.port, '127.0.0.1', () => {
      socket.write([
        'POST /api/drop HTTP/1.1',
        'Host: 127.0.0.1',
        `X-Drop-Code: ${service.code}`,
        'X-Drop-Filename: slow.obj',
        'Content-Length: 100000',
        '',
        'v 0 0 0\n',
      ].join('\r\n'));
    });
    let text = '';
    socket.setEncoding('utf8');
    socket.on('data', (chunk) => { text += chunk; });
    socket.on('close', () => resolve(text));
    socket.on('error', (error) => (error.code === 'ECONNRESET' ? resolve(text) : reject(error)));
  });
  const elapsed = Date.now() - started;
  assert.ok(elapsed < 5000, `closed after ${elapsed} ms`);
  assert.match(raw, /^HTTP\/1\.1 408 Request Timeout\r\n|^$/);
  // The partial upload is removed once the connection is gone.
  const queueDir = path.join(service.dataDir, 'queue');
  for (let wait = 0; wait < 50 && fs.readdirSync(queueDir).length; wait += 1) await new Promise((r) => setTimeout(r, 20));
  assert.deepEqual(fs.readdirSync(queueDir), []);
  assert.deepEqual((await station(service.port, service.key, 'GET', '/api/station/inbox')).json.items, []);
});

test('the running size check stops a body that grows past its limit while it streams', async (t) => {
  const dir = tempDir(t);
  const chunks = [Buffer.alloc(60, 0x61), Buffer.alloc(60, 0x62), Buffer.alloc(60, 0x63)];
  const body = Readable.from(chunks);
  body.on('end', () => { body.complete = true; });
  const temp = path.join(dir, 'upload.tmp');
  const result = await receiveBody(body, temp, 100);
  assert.equal(result.ok, false);
  assert.equal(result.error, 'too_large');
  assert.equal(result.received, 120, 'reading stops at the chunk that crossed the limit');
  assert.equal(result.sha256, null);

  const fits = Readable.from([Buffer.from('v 1 2 3\n')]);
  fits.on('end', () => { fits.complete = true; });
  const ok = await receiveBody(fits, path.join(dir, 'fits.tmp'), 100);
  assert.equal(ok.ok, true);
  assert.equal(ok.received, 8);
  assert.equal(fs.readFileSync(path.join(dir, 'fits.tmp'), 'utf8'), 'v 1 2 3\n');
  assert.equal(ok.content.text, true);
});

test('static files come only from the bundled site', async (t) => {
  const service = await startDrop(t);
  for (const target of ['/../server/app.mjs', '/..%2Fserver%2Fapp.mjs', '/%2e%2e/server/config.mjs', '/server/app.mjs', '/.env', '/site/index.html']) {
    const response = await request(service.port, { path: target });
    assert.equal(response.status, 404, target);
    assert.doesNotMatch(response.body.toString('utf8'), /import |createServer/, target);
  }
});

test('invalid settings stop the service with a message instead of being guessed', () => {
  const invalid = [
    [{ DROP_MAX_BYTES: 'lots' }, /DROP_MAX_BYTES/],
    [{ DROP_MAX_BYTES: '0' }, /DROP_MAX_BYTES/],
    [{ DROP_TTL_HOURS: '0' }, /DROP_TTL_HOURS/],
    [{ DROP_TTL_HOURS: '-3' }, /DROP_TTL_HOURS/],
    [{ DROP_QUEUE_MAX_FILES: '1.5' }, /DROP_QUEUE_MAX_FILES/],
    [{ DROP_MAX_BYTES: '5000', DROP_QUEUE_MAX_BYTES: '4000' }, /DROP_QUEUE_MAX_BYTES/],
    [{ DROP_CODE: '12ab' }, /DROP_CODE/],
    [{ DROP_CODE: '123' }, /DROP_CODE/],
    [{ DROP_STATION_KEY: 'too-short' }, /DROP_STATION_KEY/],
    [{ DROP_STATION_KEY: 'has a space in the middle of it' }, /DROP_STATION_KEY/],
  ];
  for (const [env, message] of invalid) {
    assert.throws(() => readConfig(env), (error) => error instanceof ConfigError && message.test(error.message), JSON.stringify(env));
  }
  // The key is never repeated in the message.
  assert.throws(() => readConfig({ DROP_STATION_KEY: 'secret value' }), (error) => !error.message.includes('secret value'));

  const defaults = readConfig({});
  assert.deepEqual({ ...defaults }, {
    dataDir: '/data',
    stationName: 'Bambu Studio',
    maxBytes: 268435456,
    ttlHours: 24,
    queueMaxFiles: 50,
    queueMaxBytes: 2147483648,
    stationKey: null,
    fixedCode: null,
  });
  // Empty values, as compose passes unset variables, mean the default.
  assert.deepEqual({ ...readConfig({ DROP_STATION_KEY: '', DROP_CODE: '', DROP_STATION_NAME: ' ' }) }, { ...defaults });
  assert.equal(readConfig({ DROP_STATION_NAME: 'Lab\u0007 PC' }).stationName, 'Lab PC');
});
