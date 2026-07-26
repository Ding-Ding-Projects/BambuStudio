import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const source = readFileSync(new URL('./text.js', import.meta.url), 'utf8');
const sandbox = {
  GetQueryString: () => null,
  localStorage: {
    getItem: () => null,
    setItem: () => {},
  },
};
vm.createContext(sandbox);
vm.runInContext(source, sandbox);

const english = sandbox.LangText.en;
const cantonese = sandbox.LangText.yue_HK;
assert.equal(
  Object.keys(english).length,
  168,
  'update the yue_HK catalog and reviewed baseline when English web keys change',
);
assert.deepEqual(
  Object.keys(cantonese).sort(),
  Object.keys(english).sort(),
  'yue_HK must have exact key parity with the authoritative English table',
);

const markup = (value) => value.match(/<[^>]+>/g) ?? [];
for (const key of Object.keys(english)) {
  assert.equal(typeof cantonese[key], 'string', `missing yue_HK string: ${key}`);
  assert.deepEqual(markup(cantonese[key]), markup(english[key]), `markup differs: ${key}`);
}

sandbox.GetQueryString = () => 'bilingual_en_yue_HK';
const bilingual = sandbox.GetCurrentTextByKey('t40');
assert.match(bilingual, /^<span lang="en">Network disconnect/);
assert.match(bilingual, /<span class="BilingualSecondary"[^>]*lang="yue-Hant-HK"[^>]*>粵語：網絡已中斷/);
// Compact secondary line: single block, ellipsized, never a bare <br/> break.
assert.doesNotMatch(bilingual, /<br\s*\/?>/);
assert.match(bilingual, /text-overflow:ellipsis/);

// Untranslated (or identical) strings degrade to English-only, no annotation.
const identical = Object.keys(english).find(
  (key) => english[key] !== '' && cantonese[key] === english[key],
);
if (identical) {
  assert.equal(sandbox.GetCurrentTextByKey(identical), english[identical]);
}

// Plain-text variant for title/placeholder/innerText contexts: no markup.
const plain = sandbox.GetCurrentPlainTextByKey('t40');
assert.doesNotMatch(plain, /[<>]/);
assert.match(plain, /粵語：/);

console.log(`Validated yue_HK and bilingual_en_yue_HK for ${Object.keys(english).length} English web keys.`);
