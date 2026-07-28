/*
 * Contract tests for the Pages site's data and logic layers.
 *
 * These run without a browser: the modules are loaded into a Node global that
 * carries just enough of a window for their top level, and only the pure parts
 * (copy resolution, regex evaluation, date parsing, changelog data) are called.
 * Anything that needs layout is covered by runtime-layout-clipping.mjs instead.
 */
import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(testDir, '..');
const siteDir = path.join(uiDir, 'site');

// No DOM, no storage, and no Worker: the inline evaluation path is the one
// exercised here, which is also the path a hardened browser falls back to.
globalThis.window = globalThis;
globalThis.Worker = undefined;
globalThis.location = { search: '', href: 'https://example.invalid/', hostname: 'example.invalid' };

await import('../site/copy.js');
await import('../site/changelog.data.js');
await import('../site/dimsum.data.js');
await import('../site/core.js');
await import('../site/regex.js');
await import('../site/changelog.js');

const copy = globalThis.BAMBU_SITE_COPY;
const site = globalThis.BambuSite;
const regex = globalThis.BambuRegex;
const changelog = globalThis.BAMBU_CHANGELOG;
const changelogView = globalThis.BambuChangelog;

const placeholders = (value) => (String(value).match(/\{[A-Za-z0-9_]+\}/g) || []).sort().join(',');

/* ------------------------------------------------------------------ copy */

test('every copy entry carries both languages with a usable ladder length', () => {
  const keys = Object.keys(copy.entries);
  assert.ok(keys.length > 100, `expected a substantial catalog, found ${keys.length}`);
  for (const key of keys) {
    const entry = copy.entries[key];
    for (const language of ['en', 'yue']) {
      assert.ok(Array.isArray(entry[language]), `${key}.${language} must be an array`);
      assert.ok(entry[language].length >= 1 && entry[language].length <= 5,
        `${key}.${language} has ${entry[language].length} variants; 1-5 are supported`);
      for (const variant of entry[language]) {
        assert.equal(typeof variant, 'string');
        assert.ok(variant.trim().length > 0, `${key}.${language} has an empty variant`);
      }
    }
  }
});

test('a ladder never changes the placeholders a message interpolates', () => {
  for (const [key, entry] of Object.entries(copy.entries)) {
    const expected = placeholders(entry.en[0]);
    for (const language of ['en', 'yue']) {
      for (const [index, variant] of entry[language].entries()) {
        assert.equal(placeholders(variant), expected,
          `${key}.${language}[${index}] changes the placeholder set`);
      }
    }
  }
});

test('funny levels select a variant in range for every supported ladder length', () => {
  for (let count = 1; count <= 5; count++) {
    for (let level = 1; level <= 5; level++) {
      const index = site.variantIndex(count, level);
      assert.ok(Number.isInteger(index) && index >= 0 && index < count,
        `variantIndex(${count}, ${level}) returned ${index}`);
    }
    // Level 1 is always the most serious variant and level 5 the most playful.
    assert.equal(site.variantIndex(count, 1), 0);
    assert.equal(site.variantIndex(count, 5), count - 1);
  }
  // Out-of-range levels are clamped rather than producing undefined text.
  assert.equal(site.variantIndex(5, 0), 0);
  assert.equal(site.variantIndex(5, 99), 4);
});

test('warnings keep every fact at every funny level, in both languages', () => {
  const contracts = [
    ['hero.integrity', 'en', [/unsigned/i, /SHA-256/, /per user/i]],
    ['hero.integrity', 'yue', [/未簽名/, /SHA-256/]],
    ['settings.reset.desc', 'en', [/undo/i, /tab order/i, /funny level/i]],
    ['settings.reset.desc', 'yue', [/(無法復原|冇得返轉頭|冇 undo)/, /分頁次序/, /搞笑/]],
    ['settings.storage.blocked', 'en', [/(local storage|storage)/i, /reload/i]],
    ['settings.storage.blocked', 'yue', [/儲存/, /(重新載入|reload)/]],
    ['regex.timeout', 'en', [/\{ms\}/, /(unchanged|untouched)/i]],
    ['regex.timeout', 'yue', [/\{ms\}/, /(原狀|原封不動)/]],
    ['regex.invalid', 'en', [/\{message\}/]],
    ['regex.invalid', 'yue', [/\{message\}/]],
    ['settings.funny.desc', 'en', [/error/i, /warning/i, /never change/i]],
    ['settings.funny.desc', 'yue', [/錯誤/, /警告/]],
    ['changelog.dateinvalid', 'en', [/YYYY-MM-DD/]],
    ['changelog.dateinvalid', 'yue', [/YYYY-MM-DD/]]
  ];
  for (const [key, language, patterns] of contracts) {
    const variants = copy.entries[key][language];
    for (const [index, variant] of variants.entries()) {
      for (const pattern of patterns) {
        assert.match(variant, pattern, `${key}.${language}[${index}] dropped a required fact`);
      }
    }
  }
});

test('every copy key referenced by the site exists in the catalog', async () => {
  const sources = [
    path.join(uiDir, 'landing.html'),
    ...(await readdir(siteDir))
      .filter((name) => name.endsWith('.js'))
      .map((name) => path.join(siteDir, name))
  ];
  const missing = [];
  for (const source of sources) {
    const text = await readFile(source, 'utf8');
    const referenced = new Set();
    for (const match of text.matchAll(/data-copy="([^"{]+)"/g)) referenced.add(match[1]);
    for (const match of text.matchAll(/data-copy-attr="([^"]+)"/g)) {
      for (const rule of match[1].split(';')) {
        const key = rule.split(':')[1];
        if (key && !key.includes('{')) referenced.add(key.trim());
      }
    }
    for (const match of text.matchAll(/site\.(?:text|pair|preview)\(\s*'([a-z][A-Za-z0-9._]+)'/g)) {
      referenced.add(match[1]);
    }
    for (const match of text.matchAll(/notify\(\s*'[a-z]+',\s*'([A-Za-z0-9._]+)'/g)) referenced.add(match[1]);
    for (const key of referenced) {
      // Keys assembled at runtime (`'build.step' + index`) are covered by the
      // per-family test below; only literal keys can be checked here.
      if (!/^[a-z][A-Za-z0-9]*(\.[A-Za-z0-9]+)+$/.test(key)) continue;
      if (!site.known(key)) missing.push(`${path.basename(source)} -> ${key}`);
    }
  }
  assert.deepEqual(missing, [], 'copy keys referenced but not defined');
});

test('every copy key the site assembles at runtime exists too', () => {
  const families = [
    ...['added', 'fixed', 'changed', 'removed', 'documented'].map((c) => `changelog.cat.${c}`),
    ...[1, 2, 3, 4].flatMap((n) => [`build.step${n}.title`, `build.step${n}.body`]),
    ...['group.product', 'group.tools', 'group.project'],
    ...['home', 'prepare', 'preview', 'device', 'multidevice', 'project', 'calibration',
      'filament', 'settings', 'smarthome'].flatMap((s) => [`screen.${s}`, `screen.${s}.body`]),
    ...['tab.overview', 'tab.screens', 'tab.materialyou', 'tab.download', 'tab.changelog',
      'tab.regex', 'tab.settings', 'tab.build'],
    ...['notify.saved', 'notify.reset.done', 'notify.copied', 'notify.copyfailed',
      'notify.exported', 'notify.tab.pinned', 'notify.tab.unpinned', 'notify.tab.reset',
      'notify.dimsum.off'],
    ...['changelog.baseline', 'changelog.nochanges', 'settings.search.empty', 'settings.search.elsewhere']
  ];
  const missing = families.filter((key) => !site.known(key));
  assert.deepEqual(missing, [], 'runtime-assembled copy keys missing from the catalog');
});

test('language modes render primary and companion text as documented', () => {
  site.set('funnyEn', 1);
  site.set('funnyYue', 1);
  const both = site.pair('tab.settings');
  assert.equal(both.en, 'Settings');
  assert.equal(both.yue, '設定');
  // Without a language runtime the site stays on English and never blanks out.
  assert.equal(site.text('tab.settings'), 'Settings');
  assert.equal(site.preview('hero.headline', 'yue', 5).length > 0, true);
  assert.notEqual(site.preview('hero.headline', 'en', 1), site.preview('hero.headline', 'en', 5));
  assert.notEqual(site.preview('hero.headline', 'yue', 1), site.preview('hero.headline', 'yue', 5));
});

/* ----------------------------------------------------------------- regex */

test('literal escaping neutralises every ECMAScript metacharacter', () => {
  const escaped = regex.escapeLiteral('a.b?c*d+e(f)g[h]i{j}k|l^m$n\\o/p');
  const compiled = new RegExp(escaped);
  assert.equal(compiled.test('a.b?c*d+e(f)g[h]i{j}k|l^m$n\\o/p'), true);
  assert.equal(compiled.test('aXbYcZ'), false);
});

test('plain text is the default matcher and regex is opt-in', () => {
  const plain = regex.createMatcher('v2.', {});
  assert.equal(plain.test('release v2.7'), true);
  assert.equal(plain.test('release v27'), false, 'plain mode must not treat . as a wildcard');
  const asRegex = regex.createMatcher('v2.', { regex: true, flags: 'i' });
  assert.equal(asRegex.test('release v27'), true);
});

test('an invalid pattern reports the engine message and matches everything', () => {
  const broken = regex.createMatcher('([unclosed', { regex: true });
  assert.equal(broken.ok, false);
  assert.ok(broken.message.length > 0);
  assert.equal(broken.test('anything'), true, 'a broken pattern must not empty the list');
});

test('evaluation reports ok, nomatch and invalid without throwing', async () => {
  const ok = await regex.evaluate('md3-v(\\d+)', 'g', 'md3-v26 md3-v27');
  assert.equal(ok.status, 'ok');
  assert.equal(ok.matches.length, 2);
  assert.equal(ok.matches[0].groups[0], '26');

  const none = await regex.evaluate('zzz', 'g', 'md3-v27');
  assert.equal(none.status, 'nomatch');

  const bad = await regex.evaluate('(', 'g', 'md3-v27');
  assert.equal(bad.status, 'invalid');
  assert.ok(bad.message.length > 0);

  const empty = await regex.evaluate('', 'g', 'sample');
  assert.equal(empty.status, 'empty');
});

test('a zero-width match cannot spin forever', async () => {
  const result = await regex.evaluate('a*', 'g', 'aaa bbb');
  assert.equal(result.status, 'ok');
  assert.ok(result.matches.length <= 500);
});

test('the sample is bounded before matching', async () => {
  const result = await regex.evaluate('x', 'g', 'x'.repeat(50000));
  assert.ok(result.sampleTruncated, 'an oversized sample must be reported as truncated');
  assert.ok(result.matches.length <= 500);
});

/* ------------------------------------------------------------- changelog */

test('typed dates accept ISO input and refuse impossible ones', () => {
  assert.equal(changelogView.parseTypedDate('2026-07-28'), '2026-07-28');
  assert.equal(changelogView.parseTypedDate('2026-7-8'), '2026-07-08');
  assert.equal(changelogView.parseTypedDate(''), '');
  assert.equal(changelogView.parseTypedDate('2026-02-30'), null);
  assert.equal(changelogView.parseTypedDate('2026-13-01'), null);
  assert.equal(changelogView.parseTypedDate('not a date'), null);
});

test('the generated changelog carries only sourced facts', () => {
  assert.ok(changelog.releases.length >= 30, 'every published release must be present');
  assert.equal(changelog.releaseCount, changelog.releases.length);
  const seen = new Set();
  for (const release of changelog.releases) {
    assert.match(release.published, /^\d{4}-\d{2}-\d{2}T/);
    assert.ok(release.tag && !seen.has(release.tag), `duplicate or missing tag: ${release.tag}`);
    seen.add(release.tag);
    assert.match(release.url, /^https:\/\/github\.com\//);
    for (const change of release.changes) {
      assert.match(change.sha, /^[0-9a-f]{40}$/);
      assert.equal(change.short, change.sha.slice(0, 9));
      assert.ok(['added', 'fixed', 'changed', 'removed', 'documented'].includes(change.category));
    }
    for (const asset of release.assets) {
      assert.ok(asset.name.length > 0);
      assert.equal(typeof asset.bytes, 'number');
    }
  }
  // Newest first, so the viewer never has to sort at render time.
  const dates = changelog.releases.map((release) => release.published);
  assert.deepEqual(dates, [...dates].sort().reverse());
});

/* ---------------------------------------------------------------- dimsum */

test('every dim sum dish is bundled, named in both languages, and drawn locally', () => {
  const catalogue = globalThis.BAMBU_DIM_SUM;
  assert.equal(catalogue.chance, 0.01, 'the stated one-in-a-hundred odds are the implemented odds');
  assert.ok(catalogue.dishes.length >= 8);
  const ids = new Set();
  for (const dish of catalogue.dishes) {
    assert.ok(dish.en.length > 0 && dish.yue.length > 0, `${dish.id} needs both names`);
    assert.ok(!ids.has(dish.id));
    ids.add(dish.id);
    assert.ok(dish.art.length > 0);
    assert.doesNotMatch(dish.art, /https?:|url\(|<script|<image/i, `${dish.id} must be self-contained artwork`);
  }
});
