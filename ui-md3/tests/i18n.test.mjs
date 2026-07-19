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
  assert.equal(i18n.translateText('No filaments match your filter.', 'yue_HK'), '沒有耗材符合篩選條件。');
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
  const errorCopy = i18n.describe('No filaments match your filter.', 'bilingual_en_yue_HK');
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
    'Calibration', 'Filament', 'Settings',
    'Appearance', 'General', 'Presets', 'Network', 'Version control', 'About',
    'Language', 'Language mode', 'Theme', 'Density', 'Accent color',
    'Search', 'Search settings', 'Search objects', 'Search filaments',
    'Open', 'Cancel', 'Save', 'Send', 'Export', 'Restore this version',
    'New Project', 'Open project', 'Slice plate', 'Send print', 'Export all',
    'Import', 'New filament'
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
    'STL, STEP, 3MF, OBJ', 'AMS', 'X', 'Y', 'Z', 'Z +10', 'Z −10', 'Z axis'
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
  const optionValues = [...landing.matchAll(/<option value="([^"]+)"/g)].map((match) => match[1]);
  assert.deepEqual(optionValues, ['en', 'yue_HK', 'bilingual_en_yue_HK']);
  assert.ok(landing.indexOf('./app/i18n.resources.js') < landing.indexOf('./app/i18n.js'));
  assert.match(landing, /BambuI18n/);
  assert.match(landing, /setActiveMode\(event\.target\.value,\{persist:true\}\)/);
  const inlineScripts = [...landing.matchAll(/<script>([\s\S]*?)<\/script>/g)];
  assert.ok(inlineScripts.length > 0);
  inlineScripts.forEach((match) => assert.doesNotThrow(() => new Function(match[1])));
});
