import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {
  buildEnglishCantoneseTranslation,
  languageFallbacks,
} from '../src/i18nResources.ts';

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
}

const bilingual = buildEnglishCantoneseTranslation(english, cantonese);
assert.deepEqual(Object.keys(bilingual).sort(), Object.keys(english).sort());

for (const key of Object.keys(english)) {
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
