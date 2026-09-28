import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(testDir, '..');

await import('../app/i18n.resources.js');
await import('../app/i18n.js');

const i18n = globalThis.BambuI18n;

function memoryStorage(initial = {}) {
  const values = new Map(Object.entries(initial));
  return {
    getItem(key) { return values.has(key) ? values.get(key) : null; },
    setItem(key, value) { values.set(key, String(value)); },
    snapshot() { return Object.fromEntries(values); }
  };
}

test('exposes exactly the three required, self-identifying language choices', () => {
  assert.deepEqual(
    i18n.modes.map(({ id, label }) => ({ id, label })),
    [
      { id: 'en', label: 'English' },
      { id: 'yue_HK', label: '廣東話（香港）' },
      { id: 'bilingual_en_yue_HK', label: 'English + 廣東話' }
    ]
  );
});

test('normalizes QA aliases while invalid values fall back to English', () => {
  assert.equal(i18n.normalizeMode('yue'), 'yue_HK');
  assert.equal(i18n.normalizeMode('zh-HK'), 'yue_HK');
  assert.equal(i18n.normalizeMode('both'), 'bilingual_en_yue_HK');
  assert.equal(i18n.normalizeMode('not-a-language'), 'en');
  assert.equal(i18n.searchOverride('?view=settings&lang=bilingual'), 'bilingual_en_yue_HK');
});

test('uses Cantonese resources and retains English for missing entries', () => {
  assert.equal(i18n.translateText('Home', 'yue_HK'), '主頁');
  assert.equal(i18n.translateText('No inks match your filter.', 'yue_HK'), '沒有墨水符合篩選條件。');
  assert.equal(i18n.translateText('Uncatalogued future control', 'yue_HK'), 'Uncatalogued future control');
  assert.equal(i18n.describe('Uncatalogued future control', 'yue_HK').fallback, true);
});

test('bilingual mode keeps English primary and progressively discloses long Cantonese', () => {
  const shortLabel = i18n.describe('Home', 'bilingual_en_yue_HK');
  assert.equal(shortLabel.text, 'Home');
  assert.equal(shortLabel.secondary, '主頁');
  assert.equal(shortLabel.disclosure, false);

  const longCopy = i18n.describe(
    'Start a new project, open an existing one, or continue where you left off.',
    'bilingual_en_yue_HK'
  );
  assert.equal(longCopy.text.startsWith('Start a new project'), true);
  assert.equal(longCopy.secondary.includes('開個新項目'), true);
  assert.equal(longCopy.disclosure, true);

  // Safety/error text is deliberately literal and remains directly visible.
  const errorCopy = i18n.describe('No inks match your filter.', 'bilingual_en_yue_HK');
  assert.equal(errorCopy.disclosure, false);
  assert.equal(errorCopy.tone, 'literal');
});

test('localizes titles and short placeholders without crowding long inputs', () => {
  assert.equal(i18n.translateAttribute('Clear', 'title', 'bilingual_en_yue_HK'), 'Clear / 清除');
  assert.equal(i18n.translateAttribute('Search files', 'placeholder', 'bilingual_en_yue_HK'), 'Search files / 搜尋檔案');
  assert.equal(
    i18n.translateAttribute('Name containing… (supports regex)', 'placeholder', 'bilingual_en_yue_HK'),
    'Name containing… (supports regex)'
  );
});

test('translates interpolated screen strings and structured messages', () => {
  assert.equal(i18n.translateText('7 of 12 selected', 'yue_HK'), '已選 7 / 12 項');
  assert.equal(i18n.translateText('Sliced · 180 layers', 'yue_HK'), '已切片 · 180 層');
  assert.equal(
    i18n.message('printSent', { printer: 'Bambu Lab X1 Carbon' }, 'yue_HK'),
    '已傳送到 Bambu Lab X1 Carbon · 即將開始列印'
  );
  assert.match(i18n.message('projectSaved', {}, 'bilingual_en_yue_HK'), /^Project saved .+ \/ 項目已儲存/);
});

test('persists a Settings selection and gives a URL override precedence without rewriting storage', () => {
  const storage = memoryStorage();
  assert.equal(i18n.saveStoredMode('yue-HK', storage), true);
  assert.equal(i18n.readStoredMode(storage), 'yue_HK');
  assert.equal(i18n.resolveInitialMode(undefined, storage), 'yue_HK');
  assert.equal(i18n.resolveInitialMode('bilingual', storage), 'bilingual_en_yue_HK');
  assert.equal(i18n.readStoredMode(storage), 'yue_HK');
  assert.equal(i18n.resolveInitialMode('invalid-qa-value', storage), 'en');
});

test('falls back to same-tab reload persistence when localStorage is unavailable', () => {
  const previousName = globalThis.name;
  const blockedStorage = {
    getItem() { throw new Error('blocked'); },
    setItem() { throw new Error('blocked'); }
  };
  try {
    globalThis.name = 'unrelated-window-state';
    assert.equal(i18n.saveStoredMode('bilingual_en_yue_HK', blockedStorage), true);
    assert.equal(i18n.readStoredMode(blockedStorage), 'bilingual_en_yue_HK');
    assert.match(globalThis.name, /^unrelated-window-state;bambu-language=/);
  } finally {
    if (previousName === undefined) delete globalThis.name;
    else globalThis.name = previousName;
  }
});

test('covers all required navigation, Settings, search, and common-action source strings', () => {
  const required = [
    'File', 'Edit', 'View', 'Objects', 'Help',
    'Home', 'Prepare', 'Preview', 'Device', 'Multi-device', 'Project',
    'Calibration', 'Ink', 'Settings',
    'Appearance', 'General', 'Presets', 'Network', 'Version control', 'About',
    'Language', 'Language mode', 'Theme', 'Density', 'Accent color',
    'Search', 'Search settings', 'Search objects', 'Search inks',
    'Open', 'Cancel', 'Save', 'Send', 'Export', 'Restore this version',
    'New Project', 'Open project', 'Slice plate', 'Send print', 'Export all',
    'Import', 'New ink'
  ];
  const missing = required.filter((source) => !i18n.describe(source, 'yue_HK').localized);
  assert.deepEqual(missing, []);
});

test('maintains broad Cantonese coverage for visible static app copy', async () => {
  const html = await readFile(path.join(rootDir, 'index.html'), 'utf8');
  const decode = (value) => value
    .replaceAll('&amp;', '&')
    .replaceAll('&mdash;', '—')
    .replaceAll('&nbsp;', ' ')
    .replaceAll('&quot;', '"')
    .replaceAll('&#39;', "'");
  const icons = new Set(
    [...html.matchAll(/<span[^>]*data-icon[^>]*>([^<]+)<\/span>/g)]
      .map((match) => decode(match[1].trim()))
  );
  const productOrTechnical = new Set([
    'Bambu Studio', 'Bambu Studio — Material Design 3', 'Bambu Lab X1 Carbon',
    '3DBenchy_project', '3DBenchy.gcode.3mf', '3DBenchy.stl',
    'STL, STEP, 3MF, OBJ', 'Ink Dispenser', 'X', 'Y', 'Z', 'Z +10', 'Z −10', 'Z axis'
  ]);
  const candidates = [...new Set(
    [...html.matchAll(/>([^<>]+)</g)].map((match) => decode(match[1]).trim()).filter(Boolean)
  )].filter((source) => (
    !source.includes('{{') &&
    !icons.has(source) &&
    !productOrTechnical.has(source) &&
    !/^[-#$\d.%°×·→−\s]+$/.test(source) &&
    !/\.(?:stl|3mf|png|pdf|txt)$/i.test(source)
  ));
  const translated = candidates.filter((source) => i18n.describe(source, 'yue_HK').localized);
  assert.ok(translated.length / candidates.length >= 0.9,
    `static Cantonese coverage dropped to ${translated.length}/${candidates.length}`);
});

test('covers every known-gap literal that index.html left untranslated', () => {
  // Regression guard for a fixed batch of gaps: window controls with no
  // catalog entry at all rendered in English even in yue_HK mode, because
  // describe() only ever finds what the catalog actually holds.
  const gaps = ['Minimize', 'Maximize', 'Close', 'Close version history', 'Close dialog'];
  for (const source of gaps) {
    assert.ok(i18n.describe(source, 'yue_HK').localized, `still missing a yue_HK entry for "${source}"`);
  }
  assert.ok(
    i18n.describe('No objects match your search.', 'yue_HK').localized,
    'the empty-objects-search message is still missing a yue_HK entry',
  );
});

test('maintains Cantonese coverage for title, aria-label, placeholder and alt attributes', async () => {
  const html = await readFile(path.join(rootDir, 'index.html'), 'utf8');
  const decode = (value) => value
    .replaceAll('&amp;', '&')
    .replaceAll('&mdash;', '—')
    .replaceAll('&nbsp;', ' ')
    .replaceAll('&quot;', '"')
    .replaceAll('&#39;', "'");
  // Only literal values: a `{{ binding }}` is computed at render time (icon
  // names, dynamic labels) and is not a source string this catalog covers.
  const attributePattern = /\s(?:title|aria-label|placeholder|alt)="([^"{}]*)"/g;
  const productOrTechnical = new Set(['Clear']);
  const candidates = [...new Set(
    [...html.matchAll(attributePattern)]
      .map((match) => decode(match[1]).trim())
      .filter(Boolean)
      .filter((source) => !productOrTechnical.has(source))
  )];
  assert.ok(candidates.length > 0, 'the attribute scan itself found nothing; the regex or the fixture drifted');
  const untranslated = candidates.filter((source) => !i18n.describe(source, 'yue_HK').localized);
  assert.deepEqual(untranslated, [], 'these title/aria-label/placeholder/alt strings have no yue_HK catalog entry');
});

test('routes the free-text search summaries through BambuI18n.message, not raw string concatenation', async () => {
  const settingsLogic = await readFile(path.join(rootDir, 'app', 'screens', 'settings.logic.js'), 'utf8');
  const filamentLogic = await readFile(path.join(rootDir, 'app', 'screens', 'filament.logic.js'), 'utf8');
  const mainLogic = await readFile(path.join(rootDir, 'app', 'main.logic.js'), 'utf8');

  // These four screen-logic strings interpolate a user-typed query or a
  // filament name, so they can only ever be built at runtime; the catalog
  // cannot key on them directly. this.msg(...) is the one indirection this
  // runtime offers for that case (see BambuI18n.message in app/i18n.js).
  assert.match(settingsLogic, /this\.msg\('noSettingsMatchInvalidRegex'/);
  assert.match(settingsLogic, /this\.msg\('noSettingsMatchRegex'/);
  assert.match(settingsLogic, /this\.msg\('noSettingsMatch'/);
  assert.doesNotMatch(settingsLogic, /'No settings match/,
    'a raw English literal crept back into settings.logic.js instead of BambuI18n.message');

  assert.match(filamentLogic, /this\.msg\(re \? 'filamentSearchedRegex' : 'filamentSearchedPlain'/);
  assert.doesNotMatch(filamentLogic, /'Searched/,
    'a raw English literal crept back into filament.logic.js instead of BambuI18n.message');

  assert.match(mainLogic, /this\.msg\('exportedSingle'/);
  assert.doesNotMatch(mainLogic, /'Exported \\u201C/,
    'exportFilament reverted to a raw English-only literal instead of BambuI18n.message');

  // The messages the two screens now call must actually resolve, in both
  // directions, with the exact query round-tripped through {query}.
  assert.equal(
    i18n.message('noSettingsMatch', { query: 'Xyz' }, 'yue_HK'),
    '搵唔到符合「Xyz」嘅設定。',
  );
  assert.equal(
    i18n.message('filamentSearchedPlain', { query: 'PLA' }, 'en'),
    'Searched “PLA” · plain text',
  );
  assert.equal(
    i18n.message('exportedSingle', { name: 'Bambu PLA Basic', format: '.bbsflmt' }, 'yue_HK'),
    '已匯出「Bambu PLA Basic」→ 墨水預設（.bbsflmt）',
  );
});

test('gives the Pages tab strip a localized landmark label instead of a hardcoded one', async () => {
  const landing = await readFile(path.join(rootDir, 'landing.html'), 'utf8');
  const tabsSource = await readFile(path.join(rootDir, 'site', 'tabs.js'), 'utf8');
  const copySource = await readFile(path.join(rootDir, 'site', 'copy.js'), 'utf8');

  assert.doesNotMatch(landing, /aria-label="Site sections"/,
    'the static nav reverted to a hardcoded aria-label the site runtime never touches');
  assert.match(landing, /id="tabstrip" data-copy-attr="aria-label:shell\.sections"/);
  assert.doesNotMatch(tabsSource, /setAttribute\('aria-label', 'Site sections'\)/,
    'the JS-built tablist reverted to a hardcoded aria-label the language switch never touches');
  assert.match(tabsSource, /data-copy-attr', 'aria-label:shell\.sections'/);
  assert.match(copySource, /'shell\.sections':\s*\{\s*en:\s*\['Site sections'\]/);
});

test('gives the no-JS landing fallback its own hardcoded Cantonese, since the runtime cannot translate it', async () => {
  const landing = await readFile(path.join(rootDir, 'landing.html'), 'utf8');
  const noscriptMatch = landing.match(/<noscript>([\s\S]*?)<\/noscript>/);
  assert.ok(noscriptMatch, 'landing.html has no <noscript> fallback to check');
  const noscript = noscriptMatch[1];
  // A real Cantonese/Chinese character somewhere near each English sentence,
  // not just an EN string repeated: this is static markup, not a call
  // through i18n.describe(), so the only thing worth asserting is that a
  // reader with JavaScript off still sees Hong Kong Cantonese at all.
  assert.match(noscript, /概念版/);
  assert.match(noscript, /JavaScript.*停用/s);
  assert.match(noscript, /一部枱面 3D 打印機/);
  assert.match(noscript, /最新版本同 Windows 安裝程式/);
  assert.match(noscript, /呢個網站同原型嘅原始碼/);
  assert.match(noscript, /所有已發佈版本/);
});

test('keeps every modular screen template synchronized with index.html', async () => {
  const index = (await readFile(path.join(rootDir, 'index.html'), 'utf8')).replace(/\r\n/g, '\n');
  const ids = ['home', 'prepare', 'preview', 'device', 'multi', 'project', 'calibration', 'filament', 'settings'];
  for (const id of ids) {
    const source = (await readFile(path.join(rootDir, 'app', 'screens', `${id}.template.html`), 'utf8'))
      .replace(/\r\n/g, '\n')
      .trim()
      .split('\n')
      .map((line) => `  ${line}`)
      .join('\n');
    assert.equal(index.includes(source), true, `${id} template is not assembled into index.html`);
  }
});

test('loads localization before the renderer and exposes exactly three Settings options', async () => {
  const index = await readFile(path.join(rootDir, 'index.html'), 'utf8');
  const settings = await readFile(path.join(rootDir, 'app', 'screens', 'settings.template.html'), 'utf8');
  const resourcesAt = index.indexOf('./app/i18n.resources.js');
  const runtimeAt = index.indexOf('./runtime/mini-dc.js');
  assert.ok(resourcesAt > 0 && resourcesAt < index.indexOf('./app/i18n.js'));
  assert.ok(index.indexOf('./app/i18n.js') < runtimeAt);
  assert.match(index, /data-language-mode="\{\{ language \}\}"/);
  assert.match(settings, /list="\{\{ languageModes \}\}"/);
  assert.match(settings, /hint-placeholder-count="3"/);
  assert.match(settings, /data-language-option="\{\{ l\.id \}\}"/);
  assert.match(settings, /aria-pressed="\{\{ l\.selected \}\}"/);
});

test('shares canonical persisted modes with the Pages landing surface', async () => {
  const landing = await readFile(path.join(rootDir, 'landing.html'), 'utf8');
  const core = await readFile(path.join(rootDir, 'site', 'core.js'), 'utf8');
  const settingsModule = await readFile(path.join(rootDir, 'site', 'settings.js'), 'utf8');

  const optionValues = [...landing.matchAll(/<option value="([^"]+)"/g)].map((match) => match[1]);
  assert.deepEqual(optionValues, ['en', 'yue_HK', 'bilingual_en_yue_HK']);

  // The landing page selects and persists its mode through the app's runtime,
  // so a mode chosen on the site survives into the prototype and back.
  assert.ok(landing.indexOf('./app/i18n.resources.js') < landing.indexOf('./app/i18n.js'));
  assert.ok(landing.indexOf('./app/i18n.js') < landing.indexOf('./site/core.js'));
  assert.match(core, /global\.BambuI18n/);
  assert.match(core, /i18n\.initialize\(\{ search: global\.location\.search \}\)/);
  assert.match(core, /i18n\.setActiveMode\(mode, \{ persist: true \}\)/);
  assert.match(settingsModule, /site\.setLanguageMode\(option\[0\]\)/);

  // Every line of the site's script is a separate file, so a strict inline
  // script policy cannot blank the page.
  assert.equal([...landing.matchAll(/<script>([\s\S]*?)<\/script>/g)].length, 0);
});
