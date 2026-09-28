import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {
  buildEnglishCantoneseTranslation,
  languageFallbacks,
  splitStructuredTranslation,
  toInlineBilingual,
} from '../src/i18nResources.ts';

// A yue_HK value identical to its English source is only acceptable for a
// protocol name, an acronym, or a product/brand name that genuinely has no
// Cantonese form. Every other key must carry a real translation, so a lazy
// copy-paste (or i18next-parser seeding English as the "default value") is
// caught instead of silently shipping English-only copy in Cantonese mode.
const IDENTICAL_TRANSLATION_ALLOWLIST = new Set([
  'HTTP', 'HTTPS', 'RFID', 'C++↔Web', 'Bambu Lab', 'AMS',
]);

type TranslationTable = Record<string, string>;

const localesDirectory = fileURLToPath(new URL('../locales/', import.meta.url));
const english = JSON.parse(
  readFileSync(`${localesDirectory}/en.json`, 'utf8'),
) as TranslationTable;
const cantonese = JSON.parse(
  readFileSync(`${localesDirectory}/yue_HK.json`, 'utf8'),
) as TranslationTable;

assert.deepEqual(languageFallbacks, {
  yue_HK: ['en'],
  bilingual_en_yue_HK: ['en'],
  default: ['en'],
});

const placeholders = (value: string): string[] =>
  value.match(/{{\s*[^{}]+\s*}}/g)?.sort() ?? [];

assert.deepEqual(
  Object.keys(cantonese).sort(),
  Object.keys(english).sort(),
  'yue_HK must translate every English key and may not introduce orphan keys',
);

for (const key of Object.keys(english)) {
  assert.ok(cantonese[key].trim(), `yue_HK translation is empty: ${key}`);
  assert.deepEqual(
    placeholders(cantonese[key]),
    placeholders(english[key]),
    `interpolation placeholders differ: ${key}`,
  );
  if (cantonese[key] === english[key]) {
    assert.ok(
      IDENTICAL_TRANSLATION_ALLOWLIST.has(key),
      `yue_HK repeats the English source untranslated for "${key}"; either translate it or add it to `
      + 'IDENTICAL_TRANSLATION_ALLOWLIST when it is genuinely a protocol name, acronym, or product name',
    );
  }
}

const bilingual = buildEnglishCantoneseTranslation(english, cantonese);
assert.deepEqual(Object.keys(bilingual).sort(), Object.keys(english).sort());

for (const key of Object.keys(english)) {
  if (cantonese[key] === english[key]) {
    // Identical strings (the allowlist above) render once, with no
    // redundant "粵語：" line repeating the same text back.
    assert.equal(
      bilingual[key], english[key],
      `identical en/yue_HK text must not gain a duplicate secondary line: ${key}`,
    );
    continue;
  }
  assert.ok(
    bilingual[key].startsWith(`${english[key]}\n粵語：`),
    `English must remain the primary bilingual line: ${key}`,
  );
  assert.ok(
    bilingual[key].endsWith(cantonese[key]),
    `Cantonese must remain the accessible secondary line: ${key}`,
  );
  assert.deepEqual(
    placeholders(bilingual[key]),
    [...placeholders(english[key]), ...placeholders(cantonese[key])].sort(),
    `bilingual interpolation placeholders differ: ${key}`,
  );
}

assert.equal(
  bilingual['Remain {{percent}}%'],
  'Remain {{percent}}%\n粵語：剩餘 {{percent}}%',
);
assert.deepEqual(splitStructuredTranslation('Remain 50%'), { primary: 'Remain 50%' });
assert.deepEqual(splitStructuredTranslation('Remain 50%\n粵語：剩餘 50%'), {
  primary: 'Remain 50%',
  secondary: '粵語：剩餘 50%',
});

// toInlineBilingual(): the "English / 粵語" compact form used inside an
// <option> or any other surface that cannot host a second JSX element.
assert.equal(toInlineBilingual('Remain 50%'), 'Remain 50%');
assert.equal(toInlineBilingual('Remain 50%\n粵語：剩餘 50%'), 'Remain 50% / 剩餘 50%');
assert.equal(toInlineBilingual(bilingual['RFID']), 'RFID');
assert.equal(
  toInlineBilingual(bilingual['Remain {{percent}}%']),
  'Remain {{percent}}% / 剩餘 {{percent}}%',
);
