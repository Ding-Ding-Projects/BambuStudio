// The sender site of the LAN model drop: its text in both languages, the
// invite link reader, the answer handling, and source contracts for what the
// page may load and how it is built (Material Design 3 tokens, bundled
// fonts and icons, no external address, no inline script).
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { dropRoot, repoRoot, request, startDrop } from './harness.mjs';

const siteDir = path.join(dropRoot, 'site');
const i18n = await import(path.join(siteDir, 'i18n.js'));
const invite = await import(path.join(siteDir, 'invite.js'));
const outcome = await import(path.join(siteDir, 'outcome.js'));
const read = (name) => fs.readFileSync(path.join(siteDir, name), 'utf8');

function siteFiles(dir = siteDir, prefix = '') {
  const files = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const relative = prefix ? `${prefix}/${entry.name}` : entry.name;
    if (entry.isDirectory()) files.push(...siteFiles(path.join(dir, entry.name), relative));
    else files.push(relative);
  }
  return files.sort();
}

// ------------------------------------------------------------------- text

test('every message exists in English and Cantonese with the same substitutions', () => {
  const english = i18n.MESSAGES.en;
  const cantonese = i18n.MESSAGES.yue_HK;
  assert.deepEqual(Object.keys(cantonese).sort(), Object.keys(english).sort());
  const slots = (text) => [...text.matchAll(/\$([1-9])/g)].map((match) => match[1]).sort().join(',');
  for (const key of Object.keys(english)) {
    assert.equal(typeof english[key], 'string', key);
    assert.ok(english[key].trim() !== '', `${key} is empty in English`);
    assert.ok(cantonese[key].trim() !== '', `${key} is empty in Cantonese`);
    assert.equal(slots(cantonese[key]), slots(english[key]), `${key} substitutions`);
  }
  // Cantonese text is written Hong Kong Cantonese in Traditional characters,
  // not a relabelled formal or Simplified translation.
  const all = Object.values(cantonese).join('\n');
  for (const avoided of ['線材', '耗材', '印表機', '軟體', '網路', '专案', '專案', '文件夹', '的模型']) {
    assert.ok(!all.includes(avoided), `Cantonese text uses ${avoided}`);
  }
  assert.match(cantonese.codeFromLink, /嘅/);
});

test('every message key the page uses exists', () => {
  const sources = ['app.js', 'outcome.js', 'index.html'].map(read).join('\n');
  const used = new Set([
    ...[...sources.matchAll(/key: '([A-Za-z0-9]+)'/g)].map((match) => match[1]),
    ...[...sources.matchAll(/data-i18n="([A-Za-z0-9]+)"/g)].map((match) => match[1]),
    ...[...sources.matchAll(/\.text\('([A-Za-z0-9]+)'/g)].map((match) => match[1]),
  ]);
  assert.ok(used.size > 30, `found ${used.size} keys`);
  for (const key of used) assert.ok(Object.hasOwn(i18n.MESSAGES.en, key), `missing message ${key}`);
  // And nothing in the catalogue is dead weight.
  for (const key of Object.keys(i18n.MESSAGES.en)) {
    const sizeUnit = /^unit/.test(key);
    assert.ok(used.has(key) || sizeUnit, `unused message ${key}`);
  }
});

test('the language follows the browser first and offers three modes', () => {
  assert.equal(i18n.modeFromLanguages(['yue-HK', 'en']), 'yue_HK');
  assert.equal(i18n.modeFromLanguages(['zh-HK']), 'yue_HK');
  assert.equal(i18n.modeFromLanguages(['zh-Hant-HK']), 'yue_HK');
  assert.equal(i18n.modeFromLanguages(['en-GB', 'yue-HK']), 'en');
  assert.equal(i18n.modeFromLanguages(['fr-FR']), 'en');
  assert.equal(i18n.modeFromLanguages([]), 'en');
  assert.deepEqual(i18n.LANGUAGE_MODES.map((mode) => mode.id), ['en', 'yue_HK', 'bilingual']);
  assert.equal(i18n.normalizeMode('bilingual'), 'bilingual');
  assert.equal(i18n.normalizeMode('<script>'), null);

  const size = i18n.sizeParts(268435456);
  assert.deepEqual(size, { key: 'unitMB', value: '256' });
  const both = i18n.createTranslator('bilingual');
  const hint = both.parts('dropHint', [{ key: size.key, values: [size.value] }]);
  assert.equal(hint.primary, '3MF, STL, STEP, OBJ or AMF, up to 256 MB each');
  assert.equal(hint.secondary, '3MF、STL、STEP、OBJ 或者 AMF，每個最多 256 MB');
  assert.equal(hint.secondaryLang, 'yue-HK');
  assert.equal(both.text('send'), 'Send / 傳送');
  const yue = i18n.createTranslator('yue_HK');
  assert.equal(yue.htmlLang, 'yue-HK');
  assert.equal(yue.text('stateNotSent', [{ key: 'reasonWrongCode' }]), '冇傳送：投遞碼唔啱');
});

// ------------------------------------------------------------ invite links

test('an invite fragment yields its code; other fragments are left alone', () => {
  assert.deepEqual(invite.codeFromFragment('#code=123456'), { present: true, code: '123456' });
  assert.deepEqual(invite.codeFromFragment('code=0042'), { present: true, code: '0042' });
  assert.deepEqual(invite.codeFromFragment('#code=123%20456'), { present: true, code: '123456' });
  assert.deepEqual(invite.codeFromFragment('#code=123-456'), { present: true, code: '123456' });
  assert.deepEqual(invite.codeFromFragment('#code=123456789012'), { present: true, code: '123456789012' });
  assert.deepEqual(invite.codeFromFragment('#code=1234567890123'), { present: true, code: null });
  assert.deepEqual(invite.codeFromFragment('#code=12ab56'), { present: true, code: null });
  assert.deepEqual(invite.codeFromFragment('#code='), { present: true, code: null });
  assert.deepEqual(invite.codeFromFragment('#code=<img src=x>'), { present: true, code: null });
  assert.deepEqual(invite.codeFromFragment(''), { present: false, code: null });
  assert.deepEqual(invite.codeFromFragment('#'), { present: false, code: null });
  assert.deepEqual(invite.codeFromFragment('#main'), { present: false, code: null });
  assert.deepEqual(invite.codeFromFragment('#lang=en'), { present: false, code: null });
  assert.equal(invite.validCode(' 12 34 56 '), true);
  assert.equal(invite.validCode('123'), false);
});

function fakeWindow(href) {
  const url = new URL(href);
  const calls = [];
  const win = {
    location: {
      get hash() { return url.hash; },
      get pathname() { return url.pathname; },
      get search() { return url.search; },
      get href() { return url.href; },
    },
    history: {
      state: { kept: true },
      replaceState(state, unused, target) {
        calls.push({ state, unused, target });
        const next = new URL(target, url);
        url.hash = next.hash;
        url.pathname = next.pathname;
        url.search = next.search;
      },
    },
  };
  return { win, calls, url };
}

test('the invite code is taken out of the address bar without a reload or a new history entry', () => {
  const opened = fakeWindow('http://drop.example.org:8833/#code=654321');
  assert.deepEqual(invite.takeInviteCode(opened.win), { present: true, code: '654321' });
  assert.deepEqual(opened.calls, [{ state: { kept: true }, unused: '', target: '/' }]);
  assert.equal(opened.url.href, 'http://drop.example.org:8833/');

  // A reverse proxy path and query survive; only the fragment goes.
  const proxied = fakeWindow('https://drop.example.org/models/?lang=en#code=0042');
  assert.equal(invite.takeInviteCode(proxied.win).code, '0042');
  assert.equal(proxied.url.href, 'https://drop.example.org/models/?lang=en');

  // An unusable code is still removed; it is never kept in the history.
  const broken = fakeWindow('http://drop.example.org/#code=abc');
  assert.deepEqual(invite.takeInviteCode(broken.win), { present: true, code: null });
  assert.equal(broken.url.hash, '');

  // Nothing to take: the address is not touched.
  for (const href of ['http://drop.example.org/', 'http://drop.example.org/#main']) {
    const plain = fakeWindow(href);
    assert.deepEqual(invite.takeInviteCode(plain.win), { present: false, code: null });
    assert.deepEqual(plain.calls, []);
    assert.equal(plain.url.href, href);
  }

  // A browser that refuses replaceState still gets the code.
  const refusing = fakeWindow('http://drop.example.org/#code=777777');
  refusing.win.history.replaceState = () => { throw new Error('SecurityError'); };
  assert.equal(invite.takeInviteCode(refusing.win).code, '777777');
});

test('the page reads the invite before anything else and fills an editable field', () => {
  const app = read('app.js');
  const firstStatement = app.split('\n').find((line) => line.trim() !== '' && !line.startsWith('//') && !line.startsWith('import '));
  assert.equal(firstStatement, 'const invite = takeInviteCode(window);');
  assert.match(app, /window\.addEventListener\('hashchange', \(\) => applyInvite\(takeInviteCode\(window\)\)\)/);
  assert.match(app, /\$\('code'\)\.value = code;/);
  const html = read('index.html');
  const field = /<input id="code"[^>]*>/.exec(html)?.[0] ?? '';
  assert.ok(field, 'the code field exists');
  assert.doesNotMatch(field, /readonly|disabled/);
  assert.match(field, /inputmode="numeric"/);
  assert.match(field, /aria-describedby="code-help code-error"/);
  // The code never goes into the query or storage.
  assert.doesNotMatch(app, /localStorage\.setItem\([^)]*code/i);
  assert.doesNotMatch(app, /[?&]code=/);
});

// ---------------------------------------------------------- answer handling

test('each drop box answer maps to a file result and, when needed, a stop', () => {
  const max = 1000;
  assert.deepEqual(outcome.outcomeFor({ status: 201, body: { ok: true, id: 'x' } }, max), { sent: true });
  const wrong = outcome.outcomeFor({ status: 401, body: { ok: false, error: 'wrong_code' } }, max);
  assert.equal(wrong.sent, false);
  assert.equal(wrong.stop, true);
  assert.deepEqual(wrong.live, { key: 'liveWrongCode' });
  assert.deepEqual(wrong.codeError, { key: 'codeWrong' });
  const locked = outcome.outcomeFor({ status: 429, body: { ok: false, error: 'too_many_attempts' }, retryAfter: '299' }, max);
  assert.deepEqual(locked.live, { key: 'liveLocked', values: [5] });
  assert.equal(locked.stop, true);
  assert.deepEqual(outcome.outcomeFor({ status: 429, body: null, retryAfter: '61' }, max).live.values, [2]);
  assert.deepEqual(outcome.outcomeFor({ status: 507, body: { error: 'queue_full' } }, max), {
    sent: false, reason: { key: 'reasonFull' }, stop: true, live: { key: 'liveFull' },
  });
  assert.deepEqual(outcome.outcomeFor({ status: 413, body: { error: 'too_large' } }, max), {
    sent: false, reason: { key: 'reasonTooLarge', size: max },
  });
  assert.deepEqual(outcome.outcomeFor({ status: 415, body: { error: 'unsupported_type' } }, max), { sent: false, reason: { key: 'reasonType' } });
  assert.deepEqual(outcome.outcomeFor({ status: 400, body: { error: 'bad_request' } }, max), { sent: false, reason: { key: 'reasonName' } });
  assert.deepEqual(outcome.outcomeFor({ status: 0, body: null }, max).live, { key: 'liveNetwork' });
  const server = outcome.outcomeFor({ status: 502, body: null }, max);
  assert.equal(server.stop, true);
  assert.deepEqual(server.reason, { key: 'reasonServer', values: [502] });
  assert.equal(outcome.outcomeFor({ status: 404, body: null }, max).stop, false);
});

test('files are checked before sending: accepted extension, not empty, not too large', () => {
  assert.equal(outcome.precheck({ name: 'part.STL', size: 10 }, 100), null);
  for (const name of ['a.3mf', 'b.stl', 'c.step', 'd.stp', 'e.obj', 'f.amf']) assert.equal(outcome.precheck({ name, size: 1 }, 100), null, name);
  assert.deepEqual(outcome.precheck({ name: 'notes.txt', size: 10 }, 100), { key: 'reasonType' });
  assert.deepEqual(outcome.precheck({ name: 'model', size: 10 }, 100), { key: 'reasonType' });
  assert.deepEqual(outcome.precheck({ name: 'empty.stl', size: 0 }, 100), { key: 'reasonEmpty' });
  assert.deepEqual(outcome.precheck({ name: 'big.stl', size: 101 }, 100), { key: 'reasonTooLarge', size: 100 });
  // The page's list matches the drop box's.
  const sniff = fs.readFileSync(path.join(dropRoot, 'server', 'sniff.mjs'), 'utf8');
  const serverExtensions = [...sniff.matchAll(/'(\.[a-z0-9]+)': '/g)].map((match) => match[1]).sort();
  assert.deepEqual([...outcome.ACCEPTED_EXTENSIONS].sort(), serverExtensions);
});

// ---------------------------------------------------- what the page loads

test('the site has no external address and no inline script or style', () => {
  for (const name of siteFiles()) {
    if (/\.(woff2|txt)$/.test(name)) continue;
    let text = read(name);
    // An SVG file names its XML namespace; that is an identifier, not a fetch.
    if (name.endsWith('.svg')) text = text.replace(' xmlns="http://www.w3.org/2000/svg"', '');
    assert.doesNotMatch(text, /https?:\/\//i, `${name} names an external address`);
    assert.doesNotMatch(text, /(?:src|href|url)\s*[=(]\s*["']?\/\//i, `${name} loads a scheme-relative address`);
    assert.doesNotMatch(text, /@import/i, `${name} imports a stylesheet`);
  }
  const html = read('index.html');
  for (const tag of html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/g)) {
    assert.match(tag[1], /\bsrc="[a-z0-9-]+\.js"/, 'every script is a bundled file');
    assert.equal(tag[2].trim(), '', 'no inline script');
  }
  assert.doesNotMatch(html, /\son[a-z]+\s*=/i, 'no inline event handlers');
  assert.doesNotMatch(html, /\sstyle\s*=/i, 'no inline style attributes');
  assert.doesNotMatch(html, /<style\b/i, 'no style elements');
  // The script talks to the drop endpoint only, through a relative address
  // so a reverse proxy path keeps working.
  const scripts = ['app.js', 'i18n.js', 'invite.js', 'outcome.js'].map(read).join('\n');
  assert.deepEqual([...scripts.matchAll(/\.open\('([A-Z]+)', '([^']+)'\)/g)].map((m) => `${m[1]} ${m[2]}`), ['POST api/drop']);
  assert.doesNotMatch(scripts, /\bfetch\(|sendBeacon|WebSocket|EventSource|import\(|\beval\(|new Function|innerHTML|insertAdjacentHTML|document\.write/);
  assert.doesNotMatch(scripts, /analytics|telemetry|gtag|tracking/i);
});

test('every bundled file the page references is served, from this site only', async (t) => {
  const service = await startDrop(t);
  const page = await request(service.port, { path: '/' });
  assert.equal(page.status, 200);
  const html = page.body.toString('utf8');
  const references = new Set([...html.matchAll(/\b(?:src|href)="([^"#]+)"/g)].map((m) => m[1]));
  const css = ['tokens.css', 'styles.css'].map(read).join('\n');
  for (const match of css.matchAll(/url\('([^']+)'\)/g)) references.add(match[1]);
  for (const match of read('app.js').matchAll(/from '\.\/([^']+)'/g)) references.add(match[1]);
  assert.ok(references.size >= 10, [...references].join(', '));
  for (const reference of references) {
    assert.doesNotMatch(reference, /^[a-z]+:|^\//i, `${reference} is relative`);
    const response = await request(service.port, { path: `/${reference}` });
    assert.equal(response.status, 200, reference);
  }
  // The station name reaches the page escaped, never as markup.
  const named = await startDrop(t, { env: { DROP_STATION_NAME: '<b>Lab</b> "PC"' } });
  const namedPage = (await request(named.port, { path: '/' })).body.toString('utf8');
  assert.match(namedPage, /<meta name="drop-station-name" content="&lt;b&gt;Lab&lt;\/b&gt; &quot;PC&quot;">/);
});

test('the bundled fonts are the design system fonts and every icon is in the subset list', () => {
  for (const name of ['Roboto-Regular.woff2', 'Roboto-Medium.woff2', 'Roboto-Bold.woff2', 'RobotoMono-Regular.woff2', 'Apache-2.0-LICENSE.txt']) {
    const bundled = fs.readFileSync(path.join(siteDir, 'fonts', name));
    const source = fs.readFileSync(path.join(repoRoot, 'ui-md3', 'assets', 'fonts', name));
    assert.ok(bundled.equals(source), `${name} matches ui-md3/assets/fonts`);
  }
  const script = fs.readFileSync(path.join(repoRoot, 'scripts', 'md3', 'subset_lan_drop_icons.py'), 'utf8');
  const listed = new Set([...(/ICONS = \(([\s\S]*?)\)/.exec(script)?.[1] ?? '').matchAll(/"([a-z_]+)"/g)].map((m) => m[1]));
  const html = read('index.html');
  const app = read('app.js');
  const used = new Set([
    ...[...html.matchAll(/data-icon[^>]*>([a-z_]+)</g)].map((m) => m[1]),
    ...[...app.matchAll(/icon\('([a-z_]+)'\)/g)].map((m) => m[1]),
    ...Object.values(Object.fromEntries([...(/STATUS_ICONS = \{([\s\S]*?)\}/.exec(app)?.[1] ?? '').matchAll(/(\w+): '([a-z_]+)'/g)].map((m) => [m[1], m[2]]))),
  ]);
  assert.ok(used.size >= 15, [...used].join(', '));
  for (const name of used) assert.ok(listed.has(name), `icon ${name} is not in scripts/md3/subset_lan_drop_icons.py`);
  for (const name of listed) assert.ok(used.has(name), `icon ${name} is listed but unused`);
});

test('the colour roles and type scale are the design system tokens', () => {
  const tokens = read('tokens.css');
  const kit = fs.readFileSync(path.join(repoRoot, 'ui-md3', 'design-system', 'tokens', 'colors.css'), 'utf8');
  const roles = (css) => Object.fromEntries([...css.matchAll(/(--md-[a-z-]+):([^;]+);/g)].map((m) => [m[1], m[2].trim()]));
  const kitLight = roles(/:root, \[data-theme="light"\]\{([\s\S]*?)\}/.exec(kit)[1]);
  const kitDark = roles(/\[data-theme="dark"\]\{([\s\S]*?)\}/.exec(kit)[1]);
  const pageLight = roles(/\/\* Light colour roles \(brand scheme\)\. \*\/\n:root \{([\s\S]*?)\}/.exec(tokens)[1]);
  const pageDark = roles(/@media \(prefers-color-scheme: dark\) \{\n  :root \{([\s\S]*?)\}/.exec(tokens)[1]);
  assert.deepEqual(pageLight, kitLight);
  assert.deepEqual(pageDark, kitDark);
  const typography = fs.readFileSync(path.join(repoRoot, 'ui-md3', 'design-system', 'tokens', 'typography.css'), 'utf8');
  const scale = (css) => Object.fromEntries([...css.matchAll(/(--md-type-[a-z-]+):\s*([0-9.]+px)/g)].map((m) => [m[1], m[2]]));
  const kitScale = scale(typography);
  for (const [name, value] of Object.entries(scale(tokens))) assert.equal(value, kitScale[name], name);
  const density = fs.readFileSync(path.join(repoRoot, 'ui-md3', 'design-system', 'tokens', 'density.css'), 'utf8');
  for (const name of ['--md-elev-1', '--md-elev-2', '--md-elev-3']) {
    const pick = (css) => new RegExp(`${name}:([^;]+);`).exec(css)?.[1].trim();
    assert.equal(pick(tokens), pick(density), name);
  }
  // Light, dark, more contrast, forced colours and reduced motion are all
  // handled, and the layout starts narrow.
  const styles = read('styles.css');
  assert.match(tokens, /@media \(prefers-contrast: more\)/);
  assert.match(styles, /@media \(forced-colors: active\)/);
  assert.match(styles, /@media \(prefers-reduced-motion: reduce\)/);
  assert.match(styles, /@media \(min-width: 760px\)/);
  assert.doesNotMatch(styles, /@media \(max-width/, 'phone first: wide rules are added, narrow ones are the base');
  assert.match(styles, /:focus-visible \{\n  outline: 3px solid var\(--md-primary\);/);
  // Colours come from the roles, never literals, outside the forced-colours block.
  const outsideForced = styles.replace(/@media \(forced-colors: active\) \{[\s\S]*?\n\}/, '');
  assert.doesNotMatch(outsideForced, /#[0-9a-f]{3,8}\b|rgba?\(/i);
});

test('the page has visible labels, a live region and a keyboard way to choose files', () => {
  const html = read('index.html');
  for (const id of ['code', 'sender']) assert.match(html, new RegExp(`<label for="${id}"`), `${id} has a visible label`);
  assert.match(html, /<p id="live" class="live" role="status" aria-live="polite" aria-atomic="true"><\/p>/);
  assert.match(html, /<button type="button" id="choose" class="button tonal"/);
  assert.match(html, /<input type="file" id="files" multiple hidden>/);
  assert.match(html, /<div class="language" role="group" aria-labelledby="language-label">/);
  assert.equal([...html.matchAll(/<button type="button" data-mode="[a-z_A-Z]+"[^>]*aria-pressed=/g)].length, 3);
  assert.match(html, /<meta name="viewport" content="width=device-width, initial-scale=1">/);
  const app = read('app.js');
  assert.match(app, /track\.setAttribute\('role', 'progressbar'\)/);
  assert.match(app, /remove\.setAttribute\('aria-label', text\(\{ key: 'removeFile'/);
  assert.match(app, /input\.setAttribute\('aria-invalid', 'true'\)/);
  // Browser storage is optional and only holds the language.
  assert.deepEqual([...app.matchAll(/localStorage\.(\w+)\(/g)].map((m) => m[1]).sort(), ['getItem', 'setItem']);
  assert.match(app, /const LANGUAGE_KEY = 'lan-model-drop\.language';/);
});
