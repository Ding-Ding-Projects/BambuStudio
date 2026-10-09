import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The browser extension that captures model downloads for Bambu Studio MD3.
// Its rules, its service worker flow and its settings page model are plain ES
// modules; the flow runs here against a recording stand-in for the chrome.*
// API, so the order of pause, handoff, cancel and resume is checked exactly.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const extensionDir = path.join(repoDir, 'browser-extension');
const read = (relative) => readFileSync(path.join(extensionDir, relative), 'utf8');
const readJson = (relative) => JSON.parse(read(relative));

const rules = await import('../../browser-extension/capture-rules.js');
const i18n = await import('../../browser-extension/i18n.js');
const service = await import('../../browser-extension/capture-service.js');
const model = await import('../../browser-extension/options-model.js');

const EXTENSION_ID = 'beapempohkjjpcjdfojdlngcbamofiok';
const manifest = readJson('manifest.json');
const english = readJson('_locales/en/messages.json');
const cantonese = readJson('_locales/yue_HK/messages.json');
const catalogues = await i18n.loadCatalogues(async (relative) => readJson(relative));

// ---------------------------------------------------------------- manifest

test('the manifest is a Manifest V3 extension with only the permissions capture needs', () => {
  assert.equal(manifest.manifest_version, 3);
  assert.deepEqual([...manifest.permissions].sort(), ['contextMenus', 'downloads', 'nativeMessaging', 'notifications', 'storage']);
  for (const key of ['host_permissions', 'optional_permissions', 'optional_host_permissions', 'content_scripts',
    'web_accessible_resources', 'externally_connectable', 'content_security_policy', 'update_url']) {
    assert.equal(manifest[key], undefined, `${key} must not be declared`);
  }
  assert.deepEqual(manifest.background, { service_worker: 'service-worker.js', type: 'module' });
  assert.deepEqual(manifest.options_ui, { page: 'options.html', open_in_tab: true });
  assert.equal(manifest.incognito, 'not_allowed');
  assert.equal(manifest.default_locale, 'en');
  assert.match(manifest.version, /^\d+(\.\d+){0,3}$/);
});

test('the manifest key gives the fixed extension ID the native messaging host must allow', () => {
  const der = Buffer.from(manifest.key, 'base64');
  const id = [...createHash('sha256').update(der).digest('hex').slice(0, 32)]
    .map((digit) => String.fromCharCode(97 + Number.parseInt(digit, 16))).join('');
  assert.equal(id, EXTENSION_ID);
  assert.equal(rules.HOST_NAME, 'io.github.ding_ding_projects.bambustudio_md3');
  assert.match(rules.HOST_NAME, /^[a-z0-9_]+(\.[a-z0-9_]+)*$/, 'Chrome host names allow lower-case letters, digits, _ and single dots');
});

function pngSize(relative) {
  const data = readFileSync(path.join(extensionDir, relative));
  assert.equal(data.subarray(1, 4).toString('latin1'), 'PNG', `${relative} is a PNG`);
  return [data.readUInt32BE(16), data.readUInt32BE(20)];
}

test('every file the manifest names exists, and each icon has its declared size', () => {
  for (const [size, file] of [...Object.entries(manifest.icons), ...Object.entries(manifest.action.default_icon)]) {
    assert.deepEqual(pngSize(file), [Number(size), Number(size)], file);
  }
  for (const file of [manifest.background.service_worker, manifest.options_ui.page]) {
    assert.ok(existsSync(path.join(extensionDir, file)), file);
  }
  assert.ok(existsSync(path.join(extensionDir, service.ICON_PATH)));
});

test('manifest strings resolve in both catalogues', () => {
  for (const value of [manifest.name, manifest.short_name, manifest.description, manifest.action.default_title]) {
    const key = /^__MSG_(\w+)__$/.exec(value)?.[1];
    assert.ok(key, `${value} is a catalogue reference`);
    assert.ok(english[key]?.message, `en ${key}`);
    assert.ok(cantonese[key]?.message, `yue_HK ${key}`);
  }
  assert.ok(english.extShortName.message.length <= 12, 'Chrome truncates short names past 12 characters');
});

// ------------------------------------------------------- code and page safety

const sourceFiles = readdirSync(extensionDir).filter((name) => /\.(js|html|css)$/.test(name));

test('the extension loads nothing from the network and runs no generated code', () => {
  assert.deepEqual(sourceFiles.sort(), ['capture-rules.js', 'capture-service.js', 'i18n.js', 'options-model.js',
    'options.css', 'options.html', 'options.js', 'service-worker.js']);
  for (const file of sourceFiles) {
    const text = read(file);
    assert.doesNotMatch(text, /\beval\s*\(|new Function\s*\(|importScripts\s*\(|XMLHttpRequest|WebSocket|sendBeacon/, file);
    assert.doesNotMatch(text, /(src|href)\s*=\s*["']https?:/i, file);
    assert.doesNotMatch(text, /@import|url\(\s*["']?https?:/i, file);
    for (const match of text.matchAll(/fetch\(([^)]*)\)/g)) {
      assert.match(match[1], /chrome\.runtime\.getURL/, `${file} fetches only its own files`);
    }
  }
  assert.doesNotMatch(read('capture-service.js'), /downloads\.download\s*\(/, 'the extension never starts a transfer itself');
});

test('the settings page has no inline script, a single heading and a label for every control', () => {
  const html = read('options.html');
  for (const script of html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/g)) {
    assert.match(script[1], /\bsrc="[^"]+"/, 'every script is a file (extension pages forbid inline script)');
    assert.equal(script[2].trim(), '');
  }
  assert.doesNotMatch(html, /\son[a-z]+\s*=/i, 'no inline event handlers');
  assert.equal(html.match(/<h1\b/g)?.length, 1);
  assert.match(html, /<html lang="en">/);
  assert.match(html, /<meta name="viewport"/);
  for (const control of html.matchAll(/<(input|select|textarea)\b[^>]*\bid="([^"]+)"/g)) {
    assert.match(html, new RegExp(`<label for="${control[2]}"`), `${control[2]} has a label`);
  }
  for (const section of html.matchAll(/<section\b([^>]*)>/g)) {
    const labelled = /aria-labelledby="([^"]+)"/.exec(section[1])?.[1];
    assert.ok(labelled && html.includes(`id="${labelled}"`), 'every section is named by its heading');
  }
  assert.match(html, /id="status"[^>]*role="status"[^>]*aria-live="polite"/);
  assert.match(html, /<input type="checkbox" role="switch" id="enabled"/);
});

test('the settings page follows dark mode, reduced motion, forced colours and narrow windows', () => {
  const css = read('options.css');
  assert.match(css, /@media \(prefers-color-scheme: dark\)/);
  assert.match(css, /@media \(prefers-reduced-motion: reduce\)\s*{\s*:root\s*{\s*--md-motion:\s*0s;/);
  assert.match(css, /@media \(forced-colors: active\)/);
  assert.match(css, /@media \(max-width: 600px\)/);
  assert.match(css, /:focus-visible\s*{\s*outline:/);
  assert.doesNotMatch(css.replace(/--md-motion:[^;]+;/g, ''), /transition:[^;]*\d+ms/, 'every transition uses the motion token');
});

// --------------------------------------------------------------- catalogues

function keysUsedBySource() {
  const keys = new Set();
  for (const file of sourceFiles) {
    const text = read(file);
    for (const match of text.matchAll(/data-i18n="(\w+)"/g)) keys.add(match[1]);
    for (const match of text.matchAll(/\bt\.(?:text|parts|lines)\('(\w+)'/g)) keys.add(match[1]);
    for (const match of text.matchAll(/(?:putText\([^,]+, t, |announce\(|labelKey: |key: )'(\w+)'/g)) keys.add(match[1]);
    for (const match of text.matchAll(/notify\(settings, '(\w+)'/g)) keys.add(match[1]);
  }
  for (const match of JSON.stringify(manifest).matchAll(/__MSG_(\w+)__/g)) keys.add(match[1]);
  for (const code of rules.OUTCOME_CODES) keys.add(rules.reasonMessageKey(code));
  for (const type of rules.MODEL_TYPES) keys.add(model.typeMessageKey(type.id));
  for (const choice of i18n.LANGUAGE_CHOICES) if (choice.labelKey) keys.add(choice.labelKey);
  for (const key of ['reasonQueued', 'outcomeQueued', 'outcomeKept', 'outcomeNotSent', 'originLink', 'notifyBody',
    'connectionUnknown', 'connectionChecking', 'connectionOk', 'connectionFailed', 'excludedInvalid', 'badgeOff']) keys.add(key);
  return keys;
}

test('both catalogues hold exactly the messages the extension uses', () => {
  const used = keysUsedBySource();
  assert.deepEqual(Object.keys(cantonese).sort(), Object.keys(english).sort());
  assert.deepEqual([...used].filter((key) => !english[key]), [], 'every used key has an English message');
  assert.deepEqual(Object.keys(english).filter((key) => !used.has(key)), [], 'no catalogue message is unused');
});

test('every Cantonese message is written Cantonese with the same substitutions as the English', () => {
  const disallowed = JSON.parse(readFileSync(path.join(repoDir, 'bbl/i18n/yue_HK/glossary.json'), 'utf8')).disallowed_terms;
  for (const [key, { message }] of Object.entries(english)) {
    const translated = cantonese[key].message;
    assert.ok(message.trim() && translated.trim(), key);
    assert.match(key, /^[A-Za-z0-9_]+$/);
    assert.deepEqual((translated.match(/\$[1-9]/g) ?? []).sort(), (message.match(/\$[1-9]/g) ?? []).sort(), `${key} substitutions`);
    for (const text of [message, translated]) {
      assert.doesNotMatch(text, /\$[A-Za-z0-9_@]+\$/, `${key}: Chrome would read $name$ as an undefined placeholder`);
    }
    if (key !== 'notifyBody') assert.match(translated, /\p{Script=Han}/u, `${key} is translated`);
    for (const term of Object.keys(disallowed)) assert.ok(!translated.includes(term), `${key} uses ${term}`);
  }
  assert.ok([...cantonese.badgeOff.message].length <= 4 && english.badgeOff.message.length <= 4);
});

// ------------------------------------------------------------------- rules

const item = (overrides = {}) => ({
  id: 41,
  url: 'https://makerworld.example/files/benchy.3mf?sig=abc#part',
  finalUrl: 'https://cdn.example/files/benchy.3mf?sig=abc#part',
  referrer: 'https://makerworld.example/models/123?session=secret#top',
  filename: 'benchy.3mf',
  mime: 'application/octet-stream',
  totalBytes: 1048576,
  state: 'in_progress',
  incognito: false,
  ...overrides,
});

test('model downloads are captured by name, then by a model MIME type, and nothing else is', () => {
  const settings = rules.defaultSettings();
  const decide = (overrides) => rules.decideCapture(item(overrides), settings);
  assert.equal(decide({}).capture, true);
  assert.equal(decide({}).type.id, '3mf');
  assert.equal(decide({ filename: 'PART.STEP' }).type.id, 'step');
  assert.equal(decide({ filename: 'plate_1.gcode.3mf' }).type.id, '3mf');
  assert.equal(decide({ filename: 'old.zip.amf' }).type.id, 'amf');
  assert.equal(decide({ filename: 'C:\\Users\\Public\\Downloads\\gear.stl' }).fileName, 'gear.stl');
  assert.equal(decide({ filename: '', finalUrl: 'https://cdn.example/d/hook%20v2.obj?x=1' }).fileName, 'hook v2.obj');
  assert.equal(decide({ filename: 'download', mime: 'model/stl' }).type.id, 'stl');
  assert.equal(decide({ filename: 'download', mime: 'model/3mf; charset=binary' }).type.id, '3mf');
  assert.equal(decide({ filename: 'notes.pdf', mime: 'application/pdf' }).reason, 'not-model');
  assert.equal(decide({ filename: 'archive.zip' }).reason, 'not-model');
  assert.equal(decide({ filename: '.stl' }).reason, 'not-model', 'a bare suffix is not a file name');
  assert.equal(decide({ filename: 'job.gcode' }).reason, 'type-off', 'G-code starts off');
  assert.equal(decide({ filename: 'logo.svg', mime: 'image/svg+xml' }).reason, 'type-off');
});

test('capture refuses unsafe or foreign downloads and honours every switch', () => {
  const settings = rules.defaultSettings();
  const decide = (overrides, custom = settings) => rules.decideCapture(item(overrides), custom).reason;
  assert.equal(decide({ state: 'complete' }), 'not-in-progress');
  assert.equal(decide({ incognito: true }), 'incognito');
  assert.equal(decide({ byExtensionId: 'abcdefghijklmnopabcdefghijklmnop' }), 'extension');
  assert.equal(decide({ url: 'blob:https://x.example/1', finalUrl: '' }), 'scheme');
  assert.equal(decide({ url: 'data:model/stl;base64,AAAA', finalUrl: '' }), 'scheme');
  assert.equal(decide({ url: 'file:///C:/a.stl', finalUrl: '' }), 'scheme');
  assert.equal(decide({ url: 'ftp://x.example/a.stl', finalUrl: '' }), 'scheme');
  assert.equal(decide({ finalUrl: 'https://user:pw@cdn.example/a.stl' }), 'credentials');
  assert.equal(decide({ finalUrl: `https://cdn.example/${'a'.repeat(rules.MAX_URL_LENGTH)}.stl` }), 'too-long');
  assert.equal(decide({}, { ...settings, enabled: false }), 'disabled');
  assert.equal(decide({}, { ...settings, types: { ...settings.types, '3mf': false } }), 'type-off');
  assert.equal(decide({}, { ...settings, excludedSites: ['*.makerworld.example'] }), 'excluded-site', 'the page site counts');
  assert.equal(decide({}, { ...settings, excludedSites: ['cdn.example'] }), 'excluded-site', 'the file site counts');
  assert.equal(decide({}, { ...settings, excludedSites: ['example'] }), 'capture');
});

test('site patterns accept hosts, wildcards and pasted addresses and reject everything else', () => {
  assert.equal(rules.parseSitePattern(' Example.COM '), 'example.com');
  assert.equal(rules.parseSitePattern('*.Printables.com'), '*.printables.com');
  assert.equal(rules.parseSitePattern('https://www.thingiverse.com/thing:1/files'), 'www.thingiverse.com');
  assert.equal(rules.parseSitePattern('例子.香港'), 'xn--fsqu00a.xn--j6w193g');
  for (const bad of ['', 'not a site', 'example.com/path', 'user@example.com', '*.https://x.com', '-bad-.com', 'a..b', 'example.com:8080', '[::1]']) {
    assert.equal(rules.parseSitePattern(bad), null, bad);
  }
  assert.deepEqual(rules.parseSiteList('a.com\n\n*.b.com\r\nnope nope\na.com\n'), { patterns: ['a.com', '*.b.com'], invalid: ['nope nope'] });
  assert.equal(rules.siteMatches('files.b.com', ['*.b.com']), true);
  assert.equal(rules.siteMatches('b.com', ['*.b.com']), true);
  assert.equal(rules.siteMatches('notb.com', ['*.b.com']), false);
  assert.equal(rules.siteMatches('sub.a.com', ['a.com']), false);
});

test('stored settings are normalised so corrupt values never widen capture', () => {
  assert.deepEqual(rules.normalizeSettings('garbage'), rules.defaultSettings());
  const normalised = rules.normalizeSettings({
    enabled: 'yes', notifyOnFallback: false, language: 'klingon', extra: 1,
    types: { stl: false, gcode: 'true', nope: true }, excludedSites: ['A.com', 'a.com', 7, 'bad site'],
  });
  assert.equal(normalised.enabled, true);
  assert.equal(normalised.notifyOnFallback, false);
  assert.equal(normalised.language, 'auto');
  assert.equal(normalised.types.stl, false);
  assert.equal(normalised.types.gcode, false);
  assert.equal('nope' in normalised.types, false);
  assert.equal('extra' in normalised, false);
  assert.deepEqual(normalised.excludedSites, ['a.com']);
  assert.equal(rules.normalizeSettings({ language: 'bilingual_en_yue_HK' }).language, 'bilingual_en_yue_HK');
});

test('the capture message names the file, the transfer address, the source page and nothing private', () => {
  const decision = rules.decideCapture(item(), rules.defaultSettings());
  const message = rules.buildCaptureMessage({
    origin: 'download', url: decision.url, referrer: item().referrer, fileName: decision.fileName,
    type: decision.type, mime: 'Model/3MF; q=1', totalBytes: 1048576,
  }, { captureId: 'cap-1', capturedAt: '2026-10-09T01:02:03.000Z' });
  assert.deepEqual(message, {
    type: 'capture', protocol: 1, captureId: 'cap-1', origin: 'download',
    url: 'https://cdn.example/files/benchy.3mf?sig=abc',
    source: 'https://makerworld.example/models/123',
    fileName: 'benchy.3mf', modelType: '3mf', mime: 'model/3mf', totalBytes: 1048576,
    capturedAt: '2026-10-09T01:02:03.000Z',
  });
  const unnamed = rules.buildCaptureMessage({ url: 'https://x.example/get?id=9', referrer: '', fileName: 'get',
    type: rules.modelType('stl'), totalBytes: -1 }, { captureId: 'c', capturedAt: 't' });
  assert.equal(unnamed.fileName, 'get.stl');
  assert.equal(unnamed.totalBytes, null);
  assert.equal(unnamed.source, null);
  assert.equal(unnamed.origin, 'download');
  assert.equal(rules.sanitizeFileName('..\\evil/na<me>:"x"|?*.stl. '), 'na_me___x____.stl');
  assert.equal(rules.sanitizeFileName(`${'n'.repeat(300)}.3mf`).length, rules.MAX_FILE_NAME_LENGTH);
  assert.ok(rules.sanitizeFileName(`${'n'.repeat(300)}.3mf`).endsWith('.3mf'));
});

test('only a well-formed answer for the same capture hands the download over', () => {
  const reply = (fields) => ({ type: 'capture-result', protocol: 1, captureId: 'cap-1', ...fields });
  assert.deepEqual(rules.interpretCaptureReply(reply({ status: 'queued', queueItemId: ' q7 ' }), 'cap-1'), { ok: true, code: 'queued', queueItemId: 'q7' });
  assert.equal(rules.interpretCaptureReply(reply({ status: 'queued', queueItemId: '' }), 'cap-1').code, 'host-reply');
  assert.equal(rules.interpretCaptureReply(reply({ status: 'queued', queueItemId: 'q', captureId: 'other' }), 'cap-1').code, 'host-reply');
  assert.equal(rules.interpretCaptureReply(reply({ status: 'queued', queueItemId: 'q', protocol: 2 }), 'cap-1').code, 'host-reply');
  assert.equal(rules.interpretCaptureReply(reply({ status: 'declined', reason: 'busy' }), 'cap-1').code, 'declined-busy');
  assert.equal(rules.interpretCaptureReply(reply({ status: 'declined', reason: 'shutting-down' }), 'cap-1').code, 'declined-shutting-down');
  assert.equal(rules.interpretCaptureReply(reply({ status: 'declined', reason: 'whatever' }), 'cap-1').code, 'declined-other');
  assert.equal(rules.interpretCaptureReply(reply({ status: 'done' }), 'cap-1').code, 'host-reply');
  assert.equal(rules.interpretCaptureReply(null, 'cap-1').code, 'host-reply');
  assert.deepEqual(rules.interpretHelloReply({ type: 'hello', protocol: 1, app: 'Bambu Studio MD3', version: '2.3.0' }),
    { ok: true, code: 'connected', app: 'Bambu Studio MD3', version: '2.3.0' });
  assert.equal(rules.interpretHelloReply({ type: 'hello', protocol: 2 }).code, 'host-protocol');
  assert.equal(rules.classifyHostError(new Error('Specified native messaging host not found.')), 'host-missing');
  assert.equal(rules.classifyHostError(new Error('Access to the specified native messaging host is forbidden.')), 'host-forbidden');
  assert.equal(rules.classifyHostError(new Error('Native host has exited.')), 'host-exited');
  assert.equal(rules.classifyHostError(new Error('Error when communicating with the native messaging host.')), 'host-error');
  assert.equal(rules.reasonMessageKey('declined-shutting-down'), 'reasonDeclinedShuttingDown');
});

test('the link menu appears on every known model suffix in either case, with or without a query', () => {
  const patterns = rules.contextMenuPatterns();
  for (const pattern of ['*://*/*.3mf', '*://*/*.3mf?*', '*://*/*.STL', '*://*/*.STL?*', '*://*/*.stp', '*://*/*.ZIP.AMF']) {
    assert.ok(patterns.includes(pattern), pattern);
  }
  assert.equal(rules.decideLinkCapture('https://x.example/a/b.Step?dl=1').type.id, 'step');
  assert.equal(rules.decideLinkCapture('https://x.example/a/b.gcode').capture, true, 'an explicit request ignores the type switches');
  assert.equal(rules.decideLinkCapture('javascript:alert(1)').reason, 'scheme');
  assert.equal(rules.decideLinkCapture('https://x.example/page.html').reason, 'not-model');
});

// --------------------------------------------------------------- languages

test('language choice: the browser decides "auto", bilingual keeps English first', () => {
  assert.equal(i18n.effectiveLanguage('auto', 'zh-HK'), 'yue_HK');
  assert.equal(i18n.effectiveLanguage('auto', 'yue'), 'yue_HK');
  assert.equal(i18n.effectiveLanguage('auto', 'zh-TW'), 'en');
  assert.equal(i18n.effectiveLanguage('auto', 'en-US'), 'en');
  assert.equal(i18n.effectiveLanguage('yue_HK', 'en-US'), 'yue_HK');
  assert.equal(i18n.effectiveLanguage('nonsense', 'en-US'), 'en');
  assert.equal(i18n.formatMessage('$1 costs $$2 and $2', ['A', 'B']), 'A costs $2 and B');

  const yue = i18n.createTranslator(catalogues, 'yue_HK', 'en-US');
  assert.equal(yue.htmlLang, 'yue-HK');
  assert.equal(yue.text('menuOpenLink'), '用 Bambu Studio MD3 開呢條連結');
  const both = i18n.createTranslator(catalogues, 'bilingual_en_yue_HK', 'en-US');
  assert.equal(both.text('menuOpenLink'), 'Open link in Bambu Studio MD3 / 用 Bambu Studio MD3 開呢條連結');
  assert.equal(both.lines('notifyBody', ['a.stl', { key: 'reasonHostMissing' }]),
    "a.stl: Bambu Studio MD3's browser connection is not installed for this browser.\na.stl：呢個瀏覽器未裝 Bambu Studio MD3 嘅瀏覽器連接。");
  assert.deepEqual(both.parts('captureEnabled'), {
    primary: 'Hand model downloads to Bambu Studio MD3', primaryLang: 'en',
    secondary: '將模型下載交俾 Bambu Studio MD3', secondaryLang: 'yue-HK',
  });
  assert.deepEqual(i18n.LANGUAGE_CHOICES.map((choice) => choice.id), rules.LANGUAGE_MODE_IDS);
});

// ---------------------------------------------------- service worker flow

function fakeEvent() {
  const listeners = [];
  return { listeners, addListener: (listener) => listeners.push(listener) };
}

// A recording stand-in for the chrome.* API. `host` decides what the native
// messaging host does with each message: { reply }, { error }, or { silent }.
function fakeChrome({ host = () => ({ silent: true }), pauseError = null, settings, uiLanguage = 'en-US' } = {}) {
  const calls = [];
  const store = settings ? { [rules.SETTINGS_KEY]: settings } : {};
  const chrome = {
    calls,
    store,
    runtime: {
      id: EXTENSION_ID,
      lastError: undefined,
      getURL: (relative) => `chrome-extension://${EXTENSION_ID}/${relative}`,
      onInstalled: fakeEvent(),
      onStartup: fakeEvent(),
      openOptionsPage: () => calls.push(['openOptionsPage']),
      connectNative(name) {
        calls.push(['connectNative', name]);
        const port = {
          onMessage: fakeEvent(),
          onDisconnect: fakeEvent(),
          postMessage(message) {
            calls.push(['postMessage', structuredClone(message)]);
            const behaviour = host(message, calls);
            queueMicrotask(() => {
              if (behaviour.error) {
                chrome.runtime.lastError = { message: behaviour.error };
                for (const listener of port.onDisconnect.listeners) listener(port);
                chrome.runtime.lastError = undefined;
              } else if (behaviour.reply) {
                for (const listener of port.onMessage.listeners) listener(behaviour.reply);
              }
            });
          },
          disconnect: () => calls.push(['disconnect']),
        };
        return port;
      },
    },
    downloads: {
      onDeterminingFilename: fakeEvent(),
      pause: async (id) => {
        calls.push(['pause', id]);
        if (pauseError) throw new Error(pauseError);
      },
      resume: async (id) => { calls.push(['resume', id]); },
      cancel: async (id) => { calls.push(['cancel', id]); },
      erase: async (query) => { calls.push(['erase', query]); },
      download: async () => { calls.push(['download']); },
    },
    storage: {
      local: {
        get: async (keys) => {
          const list = Array.isArray(keys) ? keys : [keys];
          return Object.fromEntries(list.filter((key) => key in store).map((key) => [key, structuredClone(store[key])]));
        },
        set: async (values) => { Object.assign(store, structuredClone(values)); },
        remove: async (key) => { delete store[key]; },
      },
      onChanged: fakeEvent(),
    },
    contextMenus: {
      onClicked: fakeEvent(),
      removeAll: async () => { calls.push(['removeAll']); },
      create: (properties, callback) => { calls.push(['menu', properties]); callback?.(); },
    },
    action: {
      onClicked: fakeEvent(),
      setTitle: async ({ title }) => { calls.push(['title', title]); },
      setBadgeText: async ({ text }) => { calls.push(['badge', text]); },
    },
    notifications: { create: async (id, options) => { calls.push(['notify', options]); } },
    i18n: { getUILanguage: () => uiLanguage },
  };
  return chrome;
}

const queued = (queueItemId = 'queue-7') => (message) => ({
  reply: { type: 'capture-result', protocol: 1, captureId: message.captureId, status: 'queued', queueItemId },
});

function startService(chrome, overrides = {}) {
  let next = 0;
  return service.createCaptureService(chrome, {
    catalogues,
    now: () => new Date('2026-10-09T08:30:00.000Z'),
    newId: () => `cap-${++next}`,
    replyTimeoutMs: 50,
    ...overrides,
  });
}

const names = (calls) => calls.map(([name]) => name);

test('a captured download is paused, handed over, and cancelled in the browser only after the application queues it', async () => {
  let suggestedWhenHostAsked = null;
  let suggestions = 0;
  const chrome = fakeChrome({
    host: (message, calls) => {
      suggestedWhenHostAsked = suggestions;
      assert.deepEqual(names(calls).slice(0, 3), ['pause', 'connectNative', 'postMessage']);
      return queued()(message);
    },
  });
  const capture = startService(chrome);
  capture.install();
  assert.equal(chrome.downloads.onDeterminingFilename.listeners.length, 1);
  const returned = chrome.downloads.onDeterminingFilename.listeners[0](item(), () => {
    suggestions += 1;
    chrome.calls.push(['suggest']);
  });
  assert.equal(returned, true, 'the listener keeps the browser waiting for suggest()');
  await capture.idle();

  assert.equal(suggestedWhenHostAsked, 0, 'the browser download cannot finish while the application is asked');
  assert.equal(suggestions, 1);
  assert.deepEqual(names(chrome.calls), ['pause', 'connectNative', 'postMessage', 'disconnect', 'cancel', 'suggest', 'erase']);
  assert.deepEqual(chrome.calls.find(([name]) => name === 'connectNative')[1], rules.HOST_NAME);
  const sent = chrome.calls.find(([name]) => name === 'postMessage')[1];
  assert.deepEqual(sent, {
    type: 'capture', protocol: 1, captureId: 'cap-1', origin: 'download',
    url: 'https://cdn.example/files/benchy.3mf?sig=abc', source: 'https://makerworld.example/models/123',
    fileName: 'benchy.3mf', modelType: '3mf', mime: 'application/octet-stream', totalBytes: 1048576,
    capturedAt: '2026-10-09T08:30:00.000Z',
  });
  assert.deepEqual(chrome.store[rules.LOG_KEY], [{
    at: '2026-10-09T08:30:00.000Z', fileName: 'benchy.3mf', site: 'makerworld.example', origin: 'download',
    outcome: 'queued', code: 'queued', queueItemId: 'queue-7',
  }]);
  assert.ok(!names(chrome.calls).includes('notify'));
});

for (const [label, host, code] of [
  ['the host is not installed', () => ({ error: 'Specified native messaging host not found.' }), 'host-missing'],
  ['the application declines', (message) => ({ reply: { type: 'capture-result', protocol: 1, captureId: message.captureId, status: 'declined', reason: 'busy' } }), 'declined-busy'],
  ['the answer names another capture', () => ({ reply: { type: 'capture-result', protocol: 1, captureId: 'someone-else', status: 'queued', queueItemId: 'q' } }), 'host-reply'],
  ['no answer arrives in time', () => ({ silent: true }), 'host-timeout'],
]) {
  test(`when ${label}, the browser resumes and keeps the download and the person is told why`, async () => {
    const chrome = fakeChrome({ host, uiLanguage: 'zh-HK' });
    const capture = startService(chrome);
    let suggestions = 0;
    const result = await capture.handleDownload(item(), () => { suggestions += 1; chrome.calls.push(['suggest']); });
    await capture.idle();
    assert.equal(result.code, code);
    assert.equal(suggestions, 1);
    const order = names(chrome.calls);
    assert.ok(!order.includes('cancel') && !order.includes('erase') && !order.includes('download'));
    assert.ok(order.indexOf('resume') > order.indexOf('postMessage'));
    assert.ok(order.indexOf('suggest') > order.indexOf('resume'));
    assert.ok(order.includes('disconnect'), 'the port is closed');
    const notice = chrome.calls.find(([name]) => name === 'notify')[1];
    assert.equal(notice.title, '瀏覽器繼續處理呢個下載', 'auto follows a Hong Kong browser into Cantonese');
    assert.equal(notice.message, `benchy.3mf：${cantonese[rules.reasonMessageKey(code)].message}`);
    assert.equal(notice.iconUrl, `chrome-extension://${EXTENSION_ID}/icons/icon-128.png`);
    assert.equal(chrome.store[rules.LOG_KEY][0].outcome, 'kept');
    assert.equal(chrome.store[rules.LOG_KEY][0].code, code);
  });
}

test('a download the browser will not pause is never sent and stays in the browser', async () => {
  const chrome = fakeChrome({ pauseError: 'Download must be in progress' });
  const capture = startService(chrome);
  let suggestions = 0;
  const result = await capture.handleDownload(item(), () => { suggestions += 1; });
  await capture.idle();
  assert.equal(result.code, 'pause-failed');
  assert.equal(suggestions, 1);
  assert.deepEqual(names(chrome.calls).filter((name) => name !== 'notify'), ['pause']);
  assert.equal(chrome.store[rules.LOG_KEY][0].code, 'pause-failed');
});

test('downloads that are not captured are released at once and leave no trace', async () => {
  for (const [settings, overrides] of [
    [undefined, { filename: 'invoice.pdf', mime: 'application/pdf', finalUrl: 'https://x.example/invoice.pdf' }],
    [{ enabled: false }, {}],
    [{ excludedSites: ['cdn.example'] }, {}],
  ]) {
    const chrome = fakeChrome({ settings, host: queued() });
    const capture = startService(chrome);
    let suggestions = 0;
    await capture.handleDownload(item(overrides), () => { suggestions += 1; });
    await capture.idle();
    assert.equal(suggestions, 1);
    assert.deepEqual(chrome.calls, []);
    assert.equal(chrome.store[rules.LOG_KEY], undefined);
  }
});

test('turning fallback notices off keeps the log but sends no notification', async () => {
  const chrome = fakeChrome({ settings: { notifyOnFallback: false }, host: () => ({ error: 'Native host has exited.' }) });
  const capture = startService(chrome);
  await capture.handleDownload(item(), () => {});
  await capture.idle();
  assert.ok(!names(chrome.calls).includes('notify'));
  assert.equal(chrome.store[rules.LOG_KEY][0].code, 'host-exited');
});

test('two captures finishing together are both logged, newest first, within the limit', async () => {
  const chrome = fakeChrome({ host: queued(), settings: undefined });
  chrome.store[rules.LOG_KEY] = Array.from({ length: rules.LOG_LIMIT }, (_, index) => ({ at: 'old', fileName: `old-${index}.stl`, outcome: 'kept', code: 'host-missing' }));
  const capture = startService(chrome);
  await Promise.all([
    capture.handleDownload(item({ id: 1, filename: 'one.stl' }), () => {}),
    capture.handleDownload(item({ id: 2, filename: 'two.stl' }), () => {}),
  ]);
  await capture.idle();
  const log = chrome.store[rules.LOG_KEY];
  assert.equal(log.length, rules.LOG_LIMIT);
  assert.deepEqual(log.slice(0, 2).map((entry) => entry.fileName).sort(), ['one.stl', 'two.stl']);
  assert.equal(log[2].fileName, 'old-0.stl');
});

test('the link menu hands one link over and always explains a failure', async () => {
  const chrome = fakeChrome({ host: queued('queue-link'), settings: { enabled: false, notifyOnFallback: false } });
  const capture = startService(chrome);
  capture.install();
  const click = chrome.contextMenus.onClicked.listeners[0];
  await click({ menuItemId: service.MENU_ID, linkUrl: 'https://files.example/d/bracket.STL?x=1#y', pageUrl: 'https://models.example/item/9?t=1' });
  await capture.idle();
  assert.deepEqual(names(chrome.calls), ['connectNative', 'postMessage', 'disconnect']);
  const sent = chrome.calls[1][1];
  assert.equal(sent.origin, 'link');
  assert.equal(sent.url, 'https://files.example/d/bracket.STL?x=1');
  assert.equal(sent.source, 'https://models.example/item/9');
  assert.equal(sent.fileName, 'bracket.STL');
  assert.equal(sent.modelType, 'stl');
  assert.equal(chrome.store[rules.LOG_KEY][0].origin, 'link');
  assert.equal(chrome.store[rules.LOG_KEY][0].queueItemId, 'queue-link');

  const failing = fakeChrome({ host: () => ({ error: 'Specified native messaging host not found.' }), settings: { notifyOnFallback: false } });
  const second = startService(failing);
  const result = await second.handleMenuClick({ menuItemId: service.MENU_ID, linkUrl: 'https://files.example/a.3mf', pageUrl: 'https://p.example/' });
  await second.idle();
  assert.equal(result.outcome, 'not-sent');
  assert.equal(failing.calls.find(([name]) => name === 'notify')[1].title, 'Not sent to Bambu Studio MD3');
  assert.equal((await second.handleMenuClick({ menuItemId: 'something-else' })).outcome, 'ignored');
});

test('menu title, toolbar title and badge follow the language and the switch', async () => {
  const chrome = fakeChrome({ settings: { enabled: false, language: 'bilingual_en_yue_HK' } });
  const capture = startService(chrome);
  capture.install();
  for (const listener of chrome.runtime.onInstalled.listeners) listener({ reason: 'install' });
  await capture.idle();
  const menu = chrome.calls.find(([name]) => name === 'menu')[1];
  assert.equal(menu.id, service.MENU_ID);
  assert.deepEqual(menu.contexts, ['link']);
  assert.deepEqual(menu.targetUrlPatterns, rules.contextMenuPatterns());
  assert.equal(menu.title, 'Open link in Bambu Studio MD3 / 用 Bambu Studio MD3 開呢條連結');
  assert.equal(chrome.calls.find(([name]) => name === 'badge')[1], 'off');
  assert.match(chrome.calls.find(([name]) => name === 'title')[1], /capture is off.*關咗/);
  assert.ok(names(chrome.calls).indexOf('removeAll') < names(chrome.calls).indexOf('menu'), 'menus are replaced, not duplicated');

  chrome.calls.length = 0;
  chrome.store[rules.SETTINGS_KEY] = { enabled: true, language: 'yue_HK' };
  for (const listener of chrome.storage.onChanged.listeners) listener({ [rules.SETTINGS_KEY]: {} }, 'local');
  await capture.idle();
  assert.equal(chrome.calls.find(([name]) => name === 'badge')[1], '');
  assert.equal(chrome.calls.find(([name]) => name === 'menu')[1].title, '用 Bambu Studio MD3 開呢條連結');

  chrome.calls.length = 0;
  for (const listener of chrome.action.onClicked.listeners) listener();
  assert.deepEqual(chrome.calls, [['openOptionsPage']]);
});

test('the connection check reports the application or the reason it cannot be reached', async () => {
  const ok = fakeChrome({ host: () => ({ reply: { type: 'hello', protocol: 1, app: 'Bambu Studio MD3', version: '2.3.0.1' } }) });
  assert.deepEqual(await service.checkConnection(ok, 50), { ok: true, code: 'connected', app: 'Bambu Studio MD3', version: '2.3.0.1' });
  assert.deepEqual(ok.calls[1], ['postMessage', { type: 'hello', protocol: 1 }]);
  const missing = fakeChrome({ host: () => ({ error: 'Specified native messaging host not found.' }) });
  assert.deepEqual(await service.checkConnection(missing, 50), { ok: false, code: 'host-missing' });
  const silent = fakeChrome();
  assert.deepEqual(await service.checkConnection(silent, 20), { ok: false, code: 'host-timeout' });
});

// ------------------------------------------------------ settings page model

test('the settings page model lists every type, every language and every outcome in words', () => {
  const t = i18n.createTranslator(catalogues, 'en', 'en-US');
  const rows = model.typeRows(rules.defaultSettings());
  assert.deepEqual(rows.map((row) => row.id), rules.MODEL_TYPES.map((type) => type.id));
  assert.deepEqual(rows.filter((row) => row.checked).map((row) => row.id), ['3mf', 'stl', 'step', 'obj', 'amf', 'oltp']);
  assert.deepEqual(model.languageOptions(t).map((option) => option.label), ['Same as the browser', 'English', '廣東話（香港）', 'English + 廣東話']);
  const log = [
    { at: '2026-10-09T08:30:00.000Z', fileName: 'a.3mf', site: 'm.example', origin: 'download', outcome: 'queued', code: 'queued' },
    { at: '2026-10-09T08:31:00.000Z', fileName: 'b.stl', site: 'n.example', origin: 'link', outcome: 'not-sent', code: 'host-missing' },
    'junk',
  ];
  const view = model.logRows(log, t);
  assert.equal(view.length, 2);
  assert.equal(view[0].outcome.primary, 'Handed to Bambu Studio MD3');
  assert.equal(view[0].detail.primary, english.reasonQueued.message);
  assert.equal(view[1].outcome.primary, 'Not sent');
  assert.equal(view[1].detail.primary, english.reasonHostMissing.message);
  assert.equal(view[1].fromLink, true);
  assert.equal(view[0].dateTime, '2026-10-09T08:30:00.000Z');
  assert.ok(view[0].time.length > 0);
  assert.equal(model.dateLocale('yue_HK'), 'zh-HK');
  assert.deepEqual(model.connectionMessage(null), { key: 'connectionUnknown' });
  assert.deepEqual(model.connectionMessage({ checking: true }), { key: 'connectionChecking' });
  assert.deepEqual(model.connectionMessage({ ok: false, code: 'host-forbidden' }), { key: 'connectionFailed', substitutions: [{ key: 'reasonHostForbidden' }] });
  const yue = i18n.createTranslator(catalogues, 'yue_HK', 'en-US');
  const failed = model.connectionMessage({ ok: false, code: 'host-missing' });
  assert.equal(yue.text(failed.key, failed.substitutions), `未連接。${cantonese.reasonHostMissing.message}`);
});

// ---------------------------------------------------------------- shipping

test('the Windows install puts the extension folder beside the application', () => {
  const cmake = readFileSync(path.join(repoDir, 'CMakeLists.txt'), 'utf8');
  const windowsBlock = /if \(WIN32\)\n([\s\S]*?)\nelseif \(SLIC3R_FHS\)/.exec(cmake)?.[1] ?? '';
  assert.match(windowsBlock, /install\(DIRECTORY "\$\{CMAKE_CURRENT_SOURCE_DIR\}\/browser-extension\/" DESTINATION "\$\{CMAKE_INSTALL_PREFIX\}\/browser-extension"\)/);
});
