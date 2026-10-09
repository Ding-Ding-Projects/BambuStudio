// Decisions and messages for the Bambu Studio MD3 download capture extension.
//
// Everything in this module is pure: it reads the values it is given and never
// touches a browser API, so the same rules run in the service worker, in the
// settings page and under `node --test`.

export const HOST_NAME = 'io.github.ding_ding_projects.bambustudio_md3';
export const PROTOCOL_VERSION = 1;
export const SETTINGS_KEY = 'captureSettings';
export const LOG_KEY = 'captureLog';
export const LOG_LIMIT = 25;
export const MAX_URL_LENGTH = 8192;
export const MAX_FILE_NAME_LENGTH = 200;
export const REPLY_TIMEOUT_MS = 30000;

// The file types Bambu Studio MD3 opens on Windows. The first six are captured
// out of the box; the rest are common on the web for other purposes (pictures,
// other programs' scenes, printer jobs), so the person turns them on.
export const MODEL_TYPES = Object.freeze([
  { id: '3mf', suffixes: ['.3mf'], defaultOn: true,
    mimes: ['model/3mf', 'application/vnd.ms-package.3dmanufacturing-3dmodel+xml'] },
  { id: 'stl', suffixes: ['.stl'], defaultOn: true,
    mimes: ['model/stl', 'model/x.stl-binary', 'model/x.stl-ascii', 'application/sla', 'application/vnd.ms-pki.stl'] },
  { id: 'step', suffixes: ['.step', '.stp'], defaultOn: true, mimes: ['model/step', 'application/step'] },
  { id: 'obj', suffixes: ['.obj'], defaultOn: true, mimes: ['model/obj'] },
  { id: 'amf', suffixes: ['.zip.amf', '.amf'], defaultOn: true, mimes: ['application/x-amf'] },
  { id: 'oltp', suffixes: ['.oltp'], defaultOn: true, mimes: [] },
  { id: 'gcode', suffixes: ['.gcode'], defaultOn: false, mimes: ['text/x-gcode'] },
  { id: 'svg', suffixes: ['.svg'], defaultOn: false, mimes: [] },
  { id: 'gltf', suffixes: ['.gltf', '.glb'], defaultOn: false, mimes: ['model/gltf+json', 'model/gltf-binary'] },
  { id: 'fbx', suffixes: ['.fbx'], defaultOn: false, mimes: [] },
].map((type) => Object.freeze({ ...type, suffixes: Object.freeze(type.suffixes), mimes: Object.freeze(type.mimes) })));

const TYPE_BY_ID = new Map(MODEL_TYPES.map((type) => [type.id, type]));

export const LANGUAGE_MODE_IDS = Object.freeze(['auto', 'en', 'yue_HK', 'bilingual_en_yue_HK']);

// Reasons a reply from the application can give for not taking a download.
export const DECLINE_REASONS = Object.freeze(['busy', 'disabled', 'invalid', 'shutting-down', 'unsupported']);

export function defaultSettings() {
  return {
    version: 1,
    enabled: true,
    types: Object.fromEntries(MODEL_TYPES.map((type) => [type.id, type.defaultOn])),
    excludedSites: [],
    notifyOnFallback: true,
    language: 'auto',
  };
}

function isPlainObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

// Turns whatever storage holds into a complete, valid settings object. Unknown
// keys are dropped, missing keys take their defaults, and a corrupt value never
// switches capture on for a type the person turned off.
export function normalizeSettings(raw) {
  const settings = defaultSettings();
  if (!isPlainObject(raw)) return settings;
  if (typeof raw.enabled === 'boolean') settings.enabled = raw.enabled;
  if (typeof raw.notifyOnFallback === 'boolean') settings.notifyOnFallback = raw.notifyOnFallback;
  if (typeof raw.language === 'string' && LANGUAGE_MODE_IDS.includes(raw.language)) settings.language = raw.language;
  if (isPlainObject(raw.types)) {
    for (const type of MODEL_TYPES) {
      if (typeof raw.types[type.id] === 'boolean') settings.types[type.id] = raw.types[type.id];
    }
  }
  if (Array.isArray(raw.excludedSites)) {
    const seen = new Set();
    for (const entry of raw.excludedSites) {
      const pattern = typeof entry === 'string' ? parseSitePattern(entry) : null;
      if (pattern && !seen.has(pattern)) {
        seen.add(pattern);
        settings.excludedSites.push(pattern);
      }
    }
  }
  return settings;
}

// Accepts "example.com", "*.example.com" or a pasted address such as
// "https://www.example.com/models", and returns the lower-case pattern, or null
// when the line is not a site.
export function parseSitePattern(line) {
  if (typeof line !== 'string') return null;
  let text = line.trim().toLowerCase();
  if (!text) return null;
  let wildcard = false;
  if (text.startsWith('*.')) {
    wildcard = true;
    text = text.slice(2);
  }
  if (/^[a-z][a-z0-9+.-]*:\/\//.test(text)) {
    if (wildcard) return null;
    try {
      text = new URL(text).hostname;
    } catch {
      return null;
    }
  } else if (/[/?#@:\s\\]/.test(text)) {
    return null;
  }
  let host;
  try {
    host = new URL(`http://${text}/`).hostname;
  } catch {
    return null;
  }
  if (!host || host.startsWith('[') || !/^[a-z0-9.-]+$/.test(host)) return null;
  if (host.startsWith('.') || host.endsWith('.') || host.includes('..')) return null;
  if (host.split('.').some((label) => !label || label.length > 63 || label.startsWith('-') || label.endsWith('-'))) return null;
  return wildcard ? `*.${host}` : host;
}

// Splits the settings text box into valid patterns and the lines that are not
// sites, so the page can name each rejected line instead of dropping it.
export function parseSiteList(text) {
  const patterns = [];
  const invalid = [];
  const seen = new Set();
  for (const line of String(text ?? '').split(/\r?\n/)) {
    if (!line.trim()) continue;
    const pattern = parseSitePattern(line);
    if (!pattern) {
      invalid.push(line.trim());
    } else if (!seen.has(pattern)) {
      seen.add(pattern);
      patterns.push(pattern);
    }
  }
  return { patterns, invalid };
}

export function hostOf(url) {
  try {
    return new URL(url).hostname.toLowerCase();
  } catch {
    return '';
  }
}

// "example.com" covers only that host; "*.example.com" covers the domain and
// every host under it.
export function siteMatches(host, patterns) {
  const name = String(host ?? '').toLowerCase();
  if (!name) return false;
  return (patterns ?? []).some((pattern) => {
    if (pattern.startsWith('*.')) {
      const domain = pattern.slice(2);
      return name === domain || name.endsWith(`.${domain}`);
    }
    return name === pattern;
  });
}

function lastPathSegment(url) {
  try {
    const segments = new URL(url).pathname.split('/');
    const last = segments[segments.length - 1] || '';
    try {
      return decodeURIComponent(last);
    } catch {
      return last;
    }
  } catch {
    return '';
  }
}

function baseName(path) {
  const parts = String(path ?? '').split(/[\\/]/);
  return parts[parts.length - 1] || '';
}

// A proposed file name for the application's Start download dialog: no folder
// parts, no control characters, no characters Windows refuses in a name, and
// no trailing dots or spaces. The application checks the name again.
export function sanitizeFileName(name) {
  let text = baseName(name)
    .replace(/[\u0000-\u001f\u007f]/g, '')
    .replace(/[<>:"/\\|?*]/g, '_')
    .trim()
    .replace(/[. ]+$/g, '');
  if (text.length > MAX_FILE_NAME_LENGTH) {
    const dot = text.lastIndexOf('.');
    const suffix = dot > 0 && text.length - dot <= 16 ? text.slice(dot) : '';
    text = text.slice(0, MAX_FILE_NAME_LENGTH - suffix.length).trimEnd() + suffix;
  }
  return text;
}

export function fileNameFromItem(item) {
  const fromBrowser = baseName(item?.filename);
  if (fromBrowser) return fromBrowser;
  return lastPathSegment(item?.finalUrl || item?.url || '');
}

function suffixType(fileName) {
  const lower = String(fileName ?? '').toLowerCase();
  let best = null;
  let bestLength = 0;
  for (const type of MODEL_TYPES) {
    for (const suffix of type.suffixes) {
      if (lower.endsWith(suffix) && lower.length > suffix.length && suffix.length > bestLength) {
        best = type;
        bestLength = suffix.length;
      }
    }
  }
  return best;
}

// The model type of a download, from its name first and its MIME type second.
// A generic MIME type (application/octet-stream, text/plain) never decides.
export function typeForFile(fileName, mime) {
  const byName = suffixType(fileName);
  if (byName) return byName;
  const media = String(mime ?? '').split(';')[0].trim().toLowerCase();
  if (!media) return null;
  return MODEL_TYPES.find((type) => type.mimes.includes(media)) ?? null;
}

function checkTransferUrl(url) {
  if (typeof url !== 'string' || !url) return 'scheme';
  if (url.length > MAX_URL_LENGTH) return 'too-long';
  let parsed;
  try {
    parsed = new URL(url);
  } catch {
    return 'scheme';
  }
  if (parsed.protocol !== 'https:' && parsed.protocol !== 'http:') return 'scheme';
  if (parsed.username || parsed.password) return 'credentials';
  return null;
}

// Whether the service worker takes a browser download away from the browser.
// The decision never depends on anything but the item and the settings.
export function decideCapture(item, settings) {
  const rules = normalizeSettings(settings);
  const fileName = fileNameFromItem(item);
  const url = item?.finalUrl || item?.url || '';
  const result = (capture, reason, type = null) => ({ capture, reason, type, fileName, url });
  if (!rules.enabled) return result(false, 'disabled');
  if (item?.state && item.state !== 'in_progress') return result(false, 'not-in-progress');
  if (item?.incognito) return result(false, 'incognito');
  if (item?.byExtensionId) return result(false, 'extension');
  const urlProblem = checkTransferUrl(url);
  if (urlProblem) return result(false, urlProblem);
  if (siteMatches(hostOf(url), rules.excludedSites) || siteMatches(hostOf(item?.referrer), rules.excludedSites)) {
    return result(false, 'excluded-site');
  }
  const type = typeForFile(fileName, item?.mime);
  if (!type) return result(false, 'not-model');
  if (!rules.types[type.id]) return result(false, 'type-off', type);
  return result(true, 'capture', type);
}

// The context-menu action is an explicit request for one link, so it ignores
// the automatic switches and the excluded sites but keeps every safety rule.
export function decideLinkCapture(linkUrl) {
  const url = String(linkUrl ?? '');
  const fileName = lastPathSegment(url);
  const result = (capture, reason, type = null) => ({ capture, reason, type, fileName, url });
  const urlProblem = checkTransferUrl(url);
  if (urlProblem) return result(false, urlProblem);
  const type = typeForFile(fileName, '');
  if (!type) return result(false, 'not-model');
  return result(true, 'capture', type);
}

function withoutFragment(url) {
  const parsed = new URL(url);
  parsed.hash = '';
  return parsed.href;
}

// The page a download came from, as the Start download dialog shows it: scheme,
// host and path only, because a page's query string can carry a session token.
export function sourceOf(url) {
  try {
    const parsed = new URL(url);
    if (parsed.protocol !== 'https:' && parsed.protocol !== 'http:') return '';
    return `${parsed.origin}${parsed.pathname}`;
  } catch {
    return '';
  }
}

// The message the native messaging host receives for one capture. The host
// answers with interpretCaptureReply's shape once the application has queued
// the item behind its Start download dialog; nothing is transferred before
// the person confirms there.
export function buildCaptureMessage(capture, { captureId, capturedAt }) {
  const type = capture.type ?? null;
  let fileName = sanitizeFileName(capture.fileName);
  if (type && !suffixType(fileName)) fileName = `${fileName || 'model'}${type.suffixes[0]}`;
  const totalBytes = Number.isSafeInteger(capture.totalBytes) && capture.totalBytes > 0 ? capture.totalBytes : null;
  const mime = typeof capture.mime === 'string' && capture.mime ? capture.mime.split(';')[0].trim().toLowerCase() : null;
  return {
    type: 'capture',
    protocol: PROTOCOL_VERSION,
    captureId,
    origin: capture.origin === 'link' ? 'link' : 'download',
    url: withoutFragment(capture.url),
    source: sourceOf(capture.referrer) || null,
    fileName,
    modelType: type ? type.id : null,
    mime,
    totalBytes,
    capturedAt,
  };
}

// Maps a failure to reach the host (Chrome's runtime.lastError text, or the
// timeout this extension raises) to a stable code the page can translate.
export function classifyHostError(error) {
  if (error && typeof error.code === 'string' && error.code.startsWith('host-')) return error.code;
  const message = String(error?.message ?? error ?? '').toLowerCase();
  if (message.includes('not found')) return 'host-missing';
  if (message.includes('forbidden')) return 'host-forbidden';
  if (message.includes('exited')) return 'host-exited';
  return 'host-error';
}

// The catalogue key that explains a decision or failure code to the person:
// "host-missing" reads "reasonHostMissing".
export function reasonMessageKey(code) {
  return `reason${String(code ?? 'host-error').split('-').map((part) => part.charAt(0).toUpperCase() + part.slice(1)).join('')}`;
}

// Every code a log entry or notification can carry.
export const OUTCOME_CODES = Object.freeze([
  'queued', 'pause-failed', 'host-missing', 'host-forbidden', 'host-exited', 'host-error', 'host-timeout',
  'host-reply', 'host-protocol', ...DECLINE_REASONS.map((reason) => `declined-${reason}`), 'declined-other',
  'scheme', 'credentials', 'too-long', 'not-model',
]);

// The application must name the capture it is answering; an answer for any
// other capture, or one the extension cannot read, is a failure, so the
// browser keeps the download.
export function interpretCaptureReply(reply, captureId) {
  if (!isPlainObject(reply) || reply.type !== 'capture-result' || reply.protocol !== PROTOCOL_VERSION) {
    return { ok: false, code: 'host-reply' };
  }
  if (reply.captureId !== captureId) return { ok: false, code: 'host-reply' };
  if (reply.status === 'queued' && typeof reply.queueItemId === 'string' && reply.queueItemId.trim()) {
    return { ok: true, code: 'queued', queueItemId: reply.queueItemId.trim() };
  }
  if (reply.status === 'declined') {
    const reason = DECLINE_REASONS.includes(reply.reason) ? reply.reason : 'other';
    return { ok: false, code: `declined-${reason}` };
  }
  return { ok: false, code: 'host-reply' };
}

export function buildHelloMessage() {
  return { type: 'hello', protocol: PROTOCOL_VERSION };
}

export function interpretHelloReply(reply) {
  if (!isPlainObject(reply) || reply.type !== 'hello') return { ok: false, code: 'host-reply' };
  if (reply.protocol !== PROTOCOL_VERSION) return { ok: false, code: 'host-protocol', protocol: reply.protocol ?? null };
  const app = typeof reply.app === 'string' && reply.app.trim() ? reply.app.trim().slice(0, 80) : 'Bambu Studio MD3';
  const version = typeof reply.version === 'string' ? reply.version.trim().slice(0, 40) : '';
  return { ok: true, code: 'connected', app, version };
}

// Chrome match patterns for the link context menu: every known model suffix,
// in lower and upper case, with and without a query string.
export function contextMenuPatterns() {
  const patterns = [];
  for (const type of MODEL_TYPES) {
    for (const suffix of type.suffixes) {
      for (const spelled of new Set([suffix, suffix.toUpperCase()])) {
        patterns.push(`*://*/*${spelled}`, `*://*/*${spelled}?*`);
      }
    }
  }
  return patterns;
}

export function modelType(id) {
  return TYPE_BY_ID.get(id) ?? null;
}

// One entry of the recent-handoffs list kept in the extension's local storage.
export function logEntry({ at, fileName, url, referrer, origin, outcome, code, queueItemId }) {
  return {
    at,
    fileName: sanitizeFileName(fileName) || '',
    site: hostOf(referrer) || hostOf(url),
    origin: origin === 'link' ? 'link' : 'download',
    outcome,
    code,
    ...(queueItemId ? { queueItemId } : {}),
  };
}

export function appendToLog(log, entry) {
  const list = Array.isArray(log) ? log.filter(isPlainObject) : [];
  return [entry, ...list].slice(0, LOG_LIMIT);
}
