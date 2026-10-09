// The LAN model drop HTTP service: the sender page, the drop endpoint and the
// station API, all on one port. Node.js built-ins only.
import { createHash } from 'node:crypto';
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { AttemptLimiter, clientAddress, LOCKOUT_MS } from './attempts.mjs';
import { LISTEN_PORT, PROTOCOL, SERVICE } from './config.mjs';
import { cleanFileName, cleanSender, NameError } from './names.mjs';
import { DropQueue, ID_PATTERN, newId } from './queue.mjs';
import { generateDropCode, loadDropCode, loadStationKey, secretsEqual, storeDropCode } from './secrets.mjs';
import { ContentProbe, contentMatches, typeForName } from './sniff.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
export const DEFAULT_SITE_DIR = path.resolve(here, '..', 'site');

// Sent on every response, including errors and the API.
export const SECURITY_HEADERS = Object.freeze({
  'Content-Security-Policy': [
    "default-src 'self'",
    "script-src 'self'",
    "style-src 'self'",
    "img-src 'self'",
    "font-src 'self'",
    "connect-src 'self'",
    "object-src 'none'",
    "base-uri 'none'",
    "form-action 'self'",
    "frame-ancestors 'none'",
  ].join('; '),
  'X-Content-Type-Options': 'nosniff',
  'Referrer-Policy': 'no-referrer',
  'X-Frame-Options': 'DENY',
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Resource-Policy': 'same-origin',
  'Permissions-Policy': 'camera=(), microphone=(), geolocation=(), payment=(), usb=()',
});

// Bodies of refused uploads up to this size are read and discarded so the
// connection can carry the answer cleanly; larger ones close the connection.
const DRAIN_LIMIT = 1024 * 1024;

const CONTENT_TYPES = Object.freeze({
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.woff2': 'font/woff2',
  '.txt': 'text/plain; charset=utf-8',
});

function defaultLog(message) {
  process.stdout.write(`${new Date().toISOString()} ${message}\n`);
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]);
}

// Reads the sender site into memory once. Requests can only reach the files
// found here; no request path is ever joined onto a directory.
export function loadSite(siteDir, values) {
  const files = new Map();
  const walk = (dir, prefix, depth) => {
    if (depth > 4) return;
    for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
      if (entry.name.startsWith('.')) continue;
      const full = path.join(dir, entry.name);
      const route = `${prefix}/${entry.name}`;
      if (entry.isDirectory()) {
        walk(full, route, depth + 1);
      } else if (entry.isFile()) {
        const type = CONTENT_TYPES[path.extname(entry.name).toLowerCase()];
        if (type) files.set(route, { type, body: fs.readFileSync(full) });
      }
    }
  };
  walk(siteDir, '', 0);
  const index = files.get('/index.html');
  if (!index) throw new Error(`The sender site is missing ${path.join(siteDir, 'index.html')}.`);
  const rendered = index.body.toString('utf8').replace(/\{\{([A-Z_]+)\}\}/g, (match, name) =>
    (Object.hasOwn(values, name) ? escapeHtml(values[name]) : match));
  const page = { type: index.type, body: Buffer.from(rendered, 'utf8') };
  files.set('/index.html', page);
  files.set('/', page);
  return files;
}

// encodeURIComponent leaves ' ( ) * as they are; a header parameter may not.
function rfc5987(value) {
  return encodeURIComponent(value).replace(/['()*]/g, (c) => `%${c.charCodeAt(0).toString(16).toUpperCase()}`);
}

function sendJson(res, status, body, headers = {}) {
  const text = JSON.stringify(body);
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(text),
    'Cache-Control': 'no-store',
    ...headers,
  });
  res.end(text);
}

function fail(res, status, error, message, headers = {}) {
  sendJson(res, status, { ok: false, error, message }, headers);
}

// Streams a request body to a temporary file with a running size check,
// hashing it and collecting what the content checks need on the way. On any
// failure the file handle is closed before the promise settles, so the
// caller can remove the temporary file safely.
export function receiveBody(req, temp, limit) {
  return new Promise((resolve) => {
    const out = fs.createWriteStream(temp, { flags: 'wx', mode: 0o600 });
    const hash = createHash('sha256');
    const probe = new ContentProbe();
    let received = 0;
    let settled = false;
    const settle = (result) => {
      if (settled) return;
      settled = true;
      const value = () => ({ ...result, received, sha256: result.ok ? hash.digest('hex') : null, content: probe.result() });
      if (result.ok) {
        resolve(value());
        return;
      }
      req.pause();
      if (out.closed) {
        resolve(value());
      } else {
        out.once('close', () => resolve(value()));
        out.destroy();
      }
    };
    req.on('data', (chunk) => {
      if (settled) return;
      received += chunk.length;
      if (received > limit) {
        settle({ ok: false, error: 'too_large' });
        return;
      }
      hash.update(chunk);
      probe.update(chunk);
      if (!out.write(chunk)) {
        req.pause();
        out.once('drain', () => { if (!settled) req.resume(); });
      }
    });
    req.on('end', () => { if (!settled) out.end(); });
    req.on('error', () => settle({ ok: false, error: 'aborted' }));
    req.on('close', () => { if (!req.complete) settle({ ok: false, error: 'aborted' }); });
    // Success waits for the file to be closed, so it can be renamed on any
    // platform.
    out.on('close', () => settle(out.writableFinished ? { ok: true } : { ok: false, error: 'storage' }));
    out.on('error', () => settle({ ok: false, error: 'storage' }));
  });
}

const MESSAGES = Object.freeze({
  bad_request: 'The request was not understood.',
  wrong_code: 'The drop code is not right. Check the code shown in Bambu Studio.',
  length_required: 'The upload must state its length.',
  too_large: 'The file is larger than this drop box accepts.',
  unsupported_type: 'Only 3MF, STL, STEP, OBJ and AMF models are accepted.',
  too_many_attempts: 'Too many wrong drop codes. Try again in a few minutes.',
  queue_full: 'The drop box is full. Ask the person at the computer to open or discard some models.',
  unauthorized: 'A valid station key is required.',
  not_found: 'Not found.',
  method_not_allowed: 'This method is not allowed here.',
  fixed_code: 'The drop code is fixed by DROP_CODE and cannot be changed here.',
});

export function createDropService(config, options = {}) {
  const now = options.now ?? Date.now;
  const log = options.log ?? defaultLog;
  const ttlMs = config.ttlHours * 60 * 60 * 1000;

  fs.mkdirSync(config.dataDir, { recursive: true, mode: 0o700 });
  const stationKey = loadStationKey(config.dataDir, config.stationKey);
  const dropCode = loadDropCode(config.dataDir, config.fixedCode);
  const queue = new DropQueue({ dataDir: config.dataDir, maxFiles: config.queueMaxFiles, maxBytes: config.queueMaxBytes, now, log });
  queue.load();
  const site = loadSite(options.siteDir ?? DEFAULT_SITE_DIR, {
    STATION_NAME: config.stationName,
    MAX_BYTES: config.maxBytes,
    TTL_HOURS: config.ttlHours,
    PROTOCOL,
  });
  const limiter = new AttemptLimiter({ now });

  const keyFile = path.join(config.dataDir, 'station-key');
  const readKey = 'docker compose exec lan-model-drop node server/station-key.mjs';
  if (stationKey.source === 'environment') log('Using the station key from DROP_STATION_KEY.');
  else if (stationKey.source === 'generated') log(`Generated a station key in ${keyFile}. Read it with: ${readKey}`);
  else log(`Using the station key in ${keyFile}. Read it with: ${readKey}`);
  log(dropCode.fixed ? 'The drop code is fixed by DROP_CODE.' : 'The drop code is shown in Bambu Studio and can be renewed there.');

  // Refuses an upload. A small body is read and discarded so the answer
  // arrives cleanly; a large or unread one closes the connection afterwards.
  // A body that was partly read is never left half-way on a kept-alive
  // connection: that connection is closed after the answer.
  function refuse(req, res, status, error, headers = {}, { started = false } = {}) {
    if (req.socket.destroyed) return;
    const declared = Number(req.headers['content-length']);
    const drain = !started && Number.isSafeInteger(declared) && declared <= DRAIN_LIMIT;
    const close = req.complete || drain ? {} : { Connection: 'close' };
    fail(res, status, error, MESSAGES[error], { ...close, ...headers });
  }

  // Streams the body to a temporary file with a running size check, hashing
  // and collecting what the content checks need on the way.
  async function handleDrop(req, res) {
    const address = clientAddress(req.socket);
    const locked = limiter.lockedFor(address);
    if (locked > 0) return refuse(req, res, 429, 'too_many_attempts', { 'Retry-After': String(Math.ceil(locked / 1000)) });

    const length = req.headers['content-length'];
    if (length === undefined) return refuse(req, res, 411, 'length_required');
    if (!/^\d+$/.test(length) || !Number.isSafeInteger(Number(length))) return refuse(req, res, 400, 'bad_request');
    const declared = Number(length);

    const code = req.headers['x-drop-code'];
    if (typeof code !== 'string' || !secretsEqual(code.trim(), dropCode.code)) {
      limiter.recordWrongCode(address);
      if (limiter.lockedFor(address) > 0) log(`locked out ${address} for ${LOCKOUT_MS / 60000} minutes after repeated wrong drop codes`);
      return refuse(req, res, 401, 'wrong_code');
    }

    let fileName;
    let sender;
    try {
      fileName = cleanFileName(req.headers['x-drop-filename']);
      sender = cleanSender(req.headers['x-drop-sender']);
    } catch (error) {
      if (error instanceof NameError) return refuse(req, res, 400, 'bad_request');
      throw error;
    }
    const type = typeForName(fileName);
    if (!type) return refuse(req, res, 415, 'unsupported_type');
    if (declared > config.maxBytes) return refuse(req, res, 413, 'too_large');

    const release = queue.reserve(declared);
    if (!release) return refuse(req, res, 507, 'queue_full');
    const id = newId();
    const temp = queue.tempBinPath(id);
    try {
      const result = await receiveBody(req, temp, Math.min(declared, config.maxBytes));
      if (!result.ok || result.received !== declared || !contentMatches(type, result.content)) {
        await fsp.rm(temp, { force: true });
        const started = { started: true };
        if (!result.ok && result.error === 'too_large') return refuse(req, res, 413, 'too_large', {}, started);
        if (!result.ok && result.error === 'storage') {
          log(`could not store an upload in ${queue.dir}`);
          return refuse(req, res, 507, 'queue_full', {}, started);
        }
        if (!result.ok || result.received !== declared) return refuse(req, res, 400, 'bad_request', {}, started);
        return refuse(req, res, 415, 'unsupported_type', {}, started);
      }
      await queue.commit(id, { fileName, sender, bytes: result.received, sha256: result.sha256, type });
      log(`received ${id} (${type}, ${result.received} bytes)`);
      return sendJson(res, 201, { ok: true, id });
    } finally {
      release();
    }
  }

  // The bearer token is always compared, even when it is missing, so a
  // missing and a wrong key take the same path.
  function stationAuthorized(req) {
    const header = req.headers.authorization;
    const match = typeof header === 'string' ? /^Bearer +(\S+) *$/i.exec(header) : null;
    return secretsEqual(match ? match[1] : '', stationKey.key);
  }

  async function handleStation(req, res, route) {
    if (!stationAuthorized(req)) {
      return fail(res, 401, 'unauthorized', MESSAGES.unauthorized, { 'WWW-Authenticate': 'Bearer realm="lan-model-drop"' });
    }
    const { method } = req;
    if (route === 'status') {
      if (method !== 'GET') return fail(res, 405, 'method_not_allowed', MESSAGES.method_not_allowed, { Allow: 'GET' });
      const { files, bytes } = queue.stats();
      return sendJson(res, 200, {
        protocol: PROTOCOL,
        stationName: config.stationName,
        dropCode: dropCode.code,
        queued: files,
        queuedBytes: bytes,
        maxBytes: config.maxBytes,
        ttlHours: config.ttlHours,
      });
    }
    if (route === 'inbox') {
      if (method !== 'GET') return fail(res, 405, 'method_not_allowed', MESSAGES.method_not_allowed, { Allow: 'GET' });
      return sendJson(res, 200, { items: queue.list() });
    }
    if (route === 'drop-code') {
      if (method !== 'POST') return fail(res, 405, 'method_not_allowed', MESSAGES.method_not_allowed, { Allow: 'POST' });
      if (dropCode.fixed) return fail(res, 409, 'fixed_code', MESSAGES.fixed_code);
      const next = generateDropCode(dropCode.code);
      storeDropCode(config.dataDir, next);
      dropCode.code = next;
      log('renewed the drop code');
      return sendJson(res, 200, { dropCode: next });
    }
    const file = /^files\/([^/]+)$/.exec(route);
    if (file) {
      const id = file[1];
      if (method === 'DELETE') {
        if (!ID_PATTERN.test(id)) return fail(res, 404, 'not_found', MESSAGES.not_found);
        if (await queue.remove(id)) log(`removed ${id}`);
        res.writeHead(204, { 'Cache-Control': 'no-store' });
        return res.end();
      }
      if (method !== 'GET' && method !== 'HEAD') {
        return fail(res, 405, 'method_not_allowed', MESSAGES.method_not_allowed, { Allow: 'GET, HEAD, DELETE' });
      }
      const item = queue.get(id);
      if (!item) return fail(res, 404, 'not_found', MESSAGES.not_found);
      let handle;
      try {
        handle = await fsp.open(queue.binPath(id), 'r');
      } catch {
        return fail(res, 404, 'not_found', MESSAGES.not_found);
      }
      res.writeHead(200, {
        'Content-Type': 'application/octet-stream',
        'Content-Length': String(item.bytes),
        'Content-Disposition': `attachment; filename*=UTF-8''${rfc5987(item.fileName)}`,
        'X-Content-SHA256': item.sha256,
        'Cache-Control': 'no-store',
      });
      if (method === 'HEAD') {
        await handle.close();
        return res.end();
      }
      const stream = handle.createReadStream();
      stream.on('error', () => res.destroy());
      stream.pipe(res);
      return undefined;
    }
    return fail(res, 404, 'not_found', MESSAGES.not_found);
  }

  function serveSite(req, res, pathname) {
    const file = site.get(pathname);
    if (!file) return fail(res, 404, 'not_found', MESSAGES.not_found);
    if (req.method !== 'GET' && req.method !== 'HEAD') {
      return fail(res, 405, 'method_not_allowed', MESSAGES.method_not_allowed, { Allow: 'GET, HEAD' });
    }
    res.writeHead(200, {
      'Content-Type': file.type,
      'Content-Length': String(file.body.length),
      'Cache-Control': pathname === '/' || pathname === '/index.html' ? 'no-store' : 'no-cache',
    });
    return res.end(req.method === 'HEAD' ? undefined : file.body);
  }

  async function handle(req, res) {
    for (const [name, value] of Object.entries(SECURITY_HEADERS)) res.setHeader(name, value);
    let pathname;
    try {
      if (!req.url.startsWith('/')) throw new Error('not an origin-form request target');
      pathname = new URL(req.url, 'http://drop.invalid').pathname;
    } catch {
      return refuse(req, res, 400, 'bad_request');
    }
    if (pathname === '/healthz') {
      if (req.method !== 'GET' && req.method !== 'HEAD') {
        return fail(res, 405, 'method_not_allowed', MESSAGES.method_not_allowed, { Allow: 'GET, HEAD' });
      }
      return sendJson(res, 200, { ok: true, service: SERVICE, protocol: PROTOCOL });
    }
    if (pathname === '/api/drop') {
      if (req.method !== 'POST') return refuse(req, res, 405, 'method_not_allowed', { Allow: 'POST' });
      return handleDrop(req, res);
    }
    if (pathname.startsWith('/api/station/')) return handleStation(req, res, pathname.slice('/api/station/'.length));
    if (pathname.startsWith('/api/')) return fail(res, 404, 'not_found', MESSAGES.not_found);
    return serveSite(req, res, pathname);
  }

  const sweepEvery = Math.max(1000, Math.min(10 * 60 * 1000, Math.floor(ttlMs / 4)));
  let timer = null;
  const expire = async () => {
    try {
      return await queue.expire(ttlMs);
    } catch (error) {
      log(`could not remove expired items: ${error.code ?? error.message}`);
      return 0;
    }
  };

  return {
    config,
    queue,
    handle,
    expire,
    get dropCode() { return dropCode.code; },
    get stationKeySource() { return stationKey.source; },
    async start() {
      await expire();
      if (!timer) {
        timer = setInterval(expire, sweepEvery);
        timer.unref();
      }
    },
    stop() {
      if (timer) clearInterval(timer);
      timer = null;
    },
  };
}

const STATUS_TEXT = { 400: 'Bad Request', 408: 'Request Timeout', 413: 'Content Too Large', 431: 'Request Header Fields Too Large' };

export function createDropServer(config, options = {}) {
  const service = createDropService(config, options);
  const log = options.log ?? defaultLog;
  const server = http.createServer({
    maxHeaderSize: 16 * 1024,
    requestTimeout: options.requestTimeoutMs ?? 15 * 60 * 1000,
    headersTimeout: options.headersTimeoutMs ?? 20 * 1000,
    keepAliveTimeout: 5 * 1000,
    connectionsCheckingInterval: options.connectionsCheckingIntervalMs ?? 5 * 1000,
  }, (req, res) => {
    service.handle(req, res).catch((error) => {
      log(`request failed: ${error.code ?? error.message}`);
      if (!res.headersSent && !req.socket.destroyed) fail(res, 500, 'internal_error', 'Something went wrong on the drop box.', { Connection: 'close' });
      else res.destroy();
    });
  });
  server.maxHeadersCount = 64;
  // Requests Node refuses before they reach the handler (malformed, too
  // slow, oversized headers) still get the security headers.
  server.on('clientError', (error, socket) => {
    const response = socket._httpMessage;
    if (socket.writable && (!response || !response.headersSent)) {
      const status = error.code === 'ERR_HTTP_REQUEST_TIMEOUT' ? 408
        : error.code === 'HPE_HEADER_OVERFLOW' ? 431
          : error.code === 'HPE_CHUNK_EXTENSIONS_OVERFLOW' ? 413 : 400;
      const headers = Object.entries(SECURITY_HEADERS).map(([name, value]) => `${name}: ${value}\r\n`).join('');
      socket.end(`HTTP/1.1 ${status} ${STATUS_TEXT[status]}\r\nConnection: close\r\nContent-Length: 0\r\n${headers}\r\n`);
      setTimeout(() => socket.destroy(), 1000).unref();
    } else {
      socket.destroy();
    }
  });
  server.on('close', () => service.stop());

  return {
    server,
    service,
    async listen(port = LISTEN_PORT, host = '0.0.0.0') {
      await service.start();
      await new Promise((resolve, reject) => {
        server.once('error', reject);
        server.listen(port, host, () => {
          server.off('error', reject);
          resolve();
        });
      });
      return server.address();
    },
    close() {
      service.stop();
      return new Promise((resolve) => {
        server.close(() => resolve());
        server.closeAllConnections();
      });
    },
  };
}
