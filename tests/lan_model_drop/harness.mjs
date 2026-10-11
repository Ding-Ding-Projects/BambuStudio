// Shared helpers for the LAN model drop tests: an in-process service on an
// ephemeral loopback port with a temporary data directory, a raw HTTP client
// and small sample models of every accepted type.
import fs from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
export const repoRoot = path.resolve(here, '..', '..');
export const dropRoot = path.join(repoRoot, 'lan-model-drop');

const { createDropServer, receiveBody, SECURITY_HEADERS } = await import(path.join(dropRoot, 'server', 'app.mjs'));
const config = await import(path.join(dropRoot, 'server', 'config.mjs'));
export { createDropServer, receiveBody, SECURITY_HEADERS };
export const { readConfig, ConfigError } = config;

export function tempDir(t, prefix = 'lan-drop-') {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), prefix));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  return dir;
}

// A clock the tests move by hand.
export function manualClock(start = Date.UTC(2026, 9, 9, 8, 0, 0)) {
  const clock = { time: start, now: () => clock.time, advance(ms) { clock.time += ms; } };
  return clock;
}

export async function startDrop(t, { env = {}, dataDir, clock, serverOptions = {} } = {}) {
  const dir = dataDir ?? tempDir(t);
  const settings = readConfig({ DROP_DATA_DIR: dir, ...env });
  const logs = [];
  const drop = createDropServer(settings, {
    log: (message) => logs.push(message),
    ...(clock ? { now: clock.now } : {}),
    ...serverOptions,
  });
  const address = await drop.listen(0, '127.0.0.1');
  let closed = false;
  const close = async () => {
    if (closed) return;
    closed = true;
    await drop.close();
  };
  t.after(close);
  const keyFile = path.join(dir, 'station-key');
  const key = settings.stationKey ?? fs.readFileSync(keyFile, 'utf8').trim();
  return {
    port: address.port,
    dataDir: dir,
    drop,
    logs,
    key,
    close,
    get code() { return drop.service.dropCode; },
  };
}

// One HTTP request on its own connection. `body` is a Buffer or string;
// `chunked` sends it without a Content-Length.
export function request(port, { method = 'GET', path: target = '/', headers = {}, body, chunked = false, localAddress } = {}) {
  return new Promise((resolve, reject) => {
    const payload = body === undefined ? null : Buffer.from(body);
    const finalHeaders = { ...headers };
    if (payload && !chunked && finalHeaders['Content-Length'] === undefined) finalHeaders['Content-Length'] = String(payload.length);
    const req = http.request({ host: '127.0.0.1', port, method, path: target, headers: finalHeaders, agent: false, localAddress }, (res) => {
      const parts = [];
      res.on('data', (chunk) => parts.push(chunk));
      res.on('end', () => {
        const raw = Buffer.concat(parts);
        let json = null;
        if (/application\/json/.test(res.headers['content-type'] ?? '')) json = JSON.parse(raw.toString('utf8'));
        resolve({ status: res.statusCode, headers: res.headers, body: raw, json });
      });
      res.on('error', reject);
    });
    req.on('error', reject);
    if (payload) req.write(payload);
    req.end();
  });
}

export function drop(port, { code, fileName, body, sender, headers = {}, localAddress } = {}) {
  const all = { ...headers };
  if (code !== undefined) all['X-Drop-Code'] = code;
  if (fileName !== undefined) all['X-Drop-Filename'] = encodeURIComponent(fileName);
  if (sender !== undefined) all['X-Drop-Sender'] = encodeURIComponent(sender);
  return request(port, { method: 'POST', path: '/api/drop', headers: all, body, localAddress });
}

export function station(port, key, method, target) {
  return request(port, { method, path: target, headers: { Authorization: `Bearer ${key}` } });
}

// ------------------------------------------------------------ sample models

const ZIP_MAGIC = Buffer.from([0x50, 0x4b, 0x03, 0x04]);

export function zipLike(size = 200) {
  const rest = Buffer.alloc(size - 4);
  for (let i = 0; i < rest.length; i += 1) rest[i] = (i * 31 + 7) & 0xff;
  return Buffer.concat([ZIP_MAGIC, rest]);
}

export const ASCII_STL = Buffer.from([
  'solid cube',
  ' facet normal 0 0 1',
  '  outer loop',
  '   vertex 0 0 0',
  '   vertex 1 0 0',
  '   vertex 0 1 0',
  '  endloop',
  ' endfacet',
  'endsolid cube',
  '',
].join('\n'));

// A binary STL whose 80-byte header starts with "solid", as some exporters
// write it: only the exact length rule identifies it.
export function binaryStl(triangles = 2, { headerText = 'solid exported by a binary writer' } = {}) {
  const buffer = Buffer.alloc(84 + 50 * triangles);
  buffer.write(headerText, 0, 'latin1');
  buffer.writeUInt32LE(triangles, 80);
  for (let i = 84; i < buffer.length; i += 1) buffer[i] = i & 0xff;
  return buffer;
}

export const STEP = Buffer.from('ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION((\'cube\'),\'2;1\');\nENDSEC;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n');
export const OBJ = Buffer.from('# cube\nmtllib cube.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n');
export const AMF_XML = Buffer.from('<?xml version="1.0" encoding="UTF-8"?>\n<amf unit="millimeter">\n <object id="0"><mesh><vertices/></mesh></object>\n</amf>\n');

export function bytesOf(value) {
  return Buffer.isBuffer(value) ? value : Buffer.from(value);
}
