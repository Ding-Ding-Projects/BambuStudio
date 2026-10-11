// Settings of the LAN model drop service, read from the environment once at
// start. Every value has a safe default; a value that is present but invalid
// stops the service with a message instead of being guessed at.

export const PROTOCOL = 1;
export const SERVICE = 'lan-model-drop';
export const LISTEN_PORT = 8080;

// What Bambu Studio reads (src/slic3r/GUI/LanModelDrop/LanModelDropModel.hpp): models of at most
// 2 GiB and at most 1000 inbox entries. A larger setting here would let a sender be told a model
// arrived that the station then refuses, or never lists.
export const STATION_MAX_ITEM_BYTES = 2147483648;
export const STATION_MAX_INBOX_ITEMS = 1000;
// DROP_STATION_NAME is cut to this many characters.
export const MAX_STATION_NAME = 60;

export const DEFAULTS = Object.freeze({
  dataDir: '/data',
  stationName: 'Bambu Studio',
  maxBytes: 268435456,
  ttlHours: 24,
  queueMaxFiles: 50,
  queueMaxBytes: 2147483648,
});

export class ConfigError extends Error {
  constructor(message) {
    super(message);
    this.name = 'ConfigError';
  }
}

// Control characters never reach a page, a log line or a file record.
const CONTROL = /[\u0000-\u001f\u007f-\u009f]/gu;

function present(value) {
  return typeof value === 'string' && value.trim() !== '';
}

function wholeNumber(env, name, fallback, { min = 1, max = Number.MAX_SAFE_INTEGER, why = '' } = {}) {
  const raw = env[name];
  if (!present(raw)) return fallback;
  const text = raw.trim();
  if (!/^\d+$/.test(text)) throw new ConfigError(`${name} must be a whole number, not "${text}".`);
  const value = Number(text);
  if (!Number.isSafeInteger(value) || value < min || value > max) {
    throw new ConfigError(`${name} must be between ${min} and ${max}${why}.`);
  }
  return value;
}

function hours(env, name, fallback) {
  const raw = env[name];
  if (!present(raw)) return fallback;
  const text = raw.trim();
  if (!/^\d+(?:\.\d+)?$/.test(text)) throw new ConfigError(`${name} must be a number of hours, not "${text}".`);
  const value = Number(text);
  // At most 30 days: the drop box is a hand-over point, not storage.
  if (!(value > 0) || value > 720) throw new ConfigError(`${name} must be more than 0 and at most 720.`);
  return value;
}

export function cleanStationName(raw) {
  const text = String(raw ?? '').replace(CONTROL, '').trim();
  const characters = [...text];
  return characters.length > MAX_STATION_NAME ? characters.slice(0, MAX_STATION_NAME).join('').trim() : text;
}

// A station key travels in an HTTP header, so it is visible ASCII without
// spaces. It is long enough that guessing it is not a practical attack.
export function validStationKey(value) {
  return typeof value === 'string' && /^[\x21-\x7e]{16,512}$/.test(value);
}

// A fixed drop code is typed on a phone's number pad.
export function validDropCode(value) {
  return typeof value === 'string' && /^\d{4,12}$/.test(value);
}

// A host Bambu Studio takes in an invite address (LanModelDropModel.cpp,
// parse_base_address): a host name of dot-separated labels of letters, digits
// and hyphens, not starting with a dot or a hyphen nor ending with a hyphen,
// with an optional final dot; an IPv4 address (the URL parser writes every
// IPv4 form as four decimal numbers); or an IPv6 address in brackets.
function stationHost(host) {
  if (host.startsWith('[')) return /^\[[0-9a-f:.]{1,45}\]$/u.test(host) && host.includes(':');
  if (host.length > 253 || /^[.-]|-$/u.test(host)) return false;
  return host.replace(/\.$/u, '').split('.').every((label) => /^[a-z0-9-]{1,63}$/u.test(label));
}

// A path Bambu Studio takes in an invite address (normalize_public_url):
// non-empty segments of unreserved and sub-delimiter characters, ':', '@' and
// %XX escapes.
const STATION_PATH = /^(?:\/(?:[A-Za-z0-9\-._~!$&'()*+,;=:@]|%[0-9A-Fa-f]{2})+)*$/u;

// DROP_PUBLIC_URL: the address people open when the service sits behind a
// reverse proxy or has a fixed name. Bambu Studio builds invite links as
// `<publicUrl>/#code=<drop code>`, so the URL is an http or https address
// with a host and nothing that could carry a secret or change the page:
// no user name or password, no query and no fragment. The answer has no
// trailing slash: https://drop.example.org/ becomes https://drop.example.org.
// Bambu Studio puts exactly this address into the invite link and its QR
// code, and uses it only in the form it accepts itself: a host as stationHost
// describes, a port from 1 to 65535, a path as STATION_PATH describes and at
// most 1024 characters. Anything else is refused here, with a message, rather
// than reported and then quietly left out of the invite by the station.
export function normalizePublicUrl(raw) {
  const text = String(raw ?? '').trim();
  const problem = 'DROP_PUBLIC_URL must be an http:// or https:// address without a user name, password, query or fragment, for example http://192.0.2.20:8833';
  const parts = /^(https?):\/\/([^/\\]*)(.*)$/iu.exec(text);
  if (!parts || text.length > 2048 || /[\s\u0000-\u001f\u007f]/u.test(text) || /[?#@\\]/u.test(text) || parts[2] === '') {
    throw new ConfigError(problem);
  }
  let url;
  try {
    url = new URL(text);
  } catch {
    throw new ConfigError(problem);
  }
  if ((url.protocol !== 'http:' && url.protocol !== 'https:') || !url.hostname || url.username !== '' || url.password !== '') {
    throw new ConfigError(problem);
  }
  const href = url.href.replace(/\/+$/u, '');
  if (!stationHost(url.hostname) || url.port === '0' || !STATION_PATH.test(url.pathname.replace(/\/+$/u, '')) || href.length > 1024) {
    throw new ConfigError(`${problem}. The host must be a name of letters, digits and hyphens, an IPv4 address or an IPv6 address in brackets, the port 1 to 65535, the path plain address characters without empty parts, and the whole address at most 1024 characters.`);
  }
  return href;
}

export function readConfig(env = process.env) {
  const stationName = present(env.DROP_STATION_NAME) ? cleanStationName(env.DROP_STATION_NAME) : DEFAULTS.stationName;
  const maxBytes = wholeNumber(env, 'DROP_MAX_BYTES', DEFAULTS.maxBytes,
    { max: STATION_MAX_ITEM_BYTES, why: ', the largest model Bambu Studio takes' });
  const ttlHours = hours(env, 'DROP_TTL_HOURS', DEFAULTS.ttlHours);
  const queueMaxFiles = wholeNumber(env, 'DROP_QUEUE_MAX_FILES', DEFAULTS.queueMaxFiles,
    { max: STATION_MAX_INBOX_ITEMS, why: ', the most models Bambu Studio lists at once' });
  const queueMaxBytes = wholeNumber(env, 'DROP_QUEUE_MAX_BYTES', DEFAULTS.queueMaxBytes);
  if (queueMaxBytes < maxBytes) {
    throw new ConfigError('DROP_QUEUE_MAX_BYTES must be at least DROP_MAX_BYTES, or no full-size model would fit.');
  }

  let stationKey = null;
  if (present(env.DROP_STATION_KEY)) {
    stationKey = env.DROP_STATION_KEY.trim();
    // The key itself is never repeated in a message.
    if (!validStationKey(stationKey)) {
      throw new ConfigError('DROP_STATION_KEY must be 16 to 512 visible ASCII characters without spaces.');
    }
  }

  let fixedCode = null;
  if (present(env.DROP_CODE)) {
    fixedCode = env.DROP_CODE.trim();
    if (!validDropCode(fixedCode)) throw new ConfigError('DROP_CODE must be 4 to 12 digits.');
  }

  const publicUrl = present(env.DROP_PUBLIC_URL) ? normalizePublicUrl(env.DROP_PUBLIC_URL) : null;

  return Object.freeze({
    dataDir: present(env.DROP_DATA_DIR) ? env.DROP_DATA_DIR.trim() : DEFAULTS.dataDir,
    stationName: stationName || DEFAULTS.stationName,
    maxBytes,
    ttlHours,
    queueMaxFiles,
    queueMaxBytes,
    stationKey,
    fixedCode,
    publicUrl,
  });
}
