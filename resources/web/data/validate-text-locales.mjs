import assert from 'node:assert/strict';
import { readFileSync, readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
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
  281,
  'update the yue_HK catalog and reviewed baseline when English web keys change',
);
assert.deepEqual(
  Object.keys(cantonese).sort(),
  Object.keys(english).sort(),
  'yue_HK must have exact key parity with the authoritative English table',
);
assert.equal(cantonese.t271, '個', 'yue_HK count suffix must use the Cantonese unit');

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

// The printer-connection page reuses nothing: its heading, body and image
// description each have their own key, translated rather than copied.
for (const key of ['t295', 't296', 't297']) {
  assert.notEqual(cantonese[key], english[key], `yue_HK must translate ${key}`);
}

// Every page element marked for translation names a key the English table
// has. A ".trans" node without one keeps whatever language its HTML was
// written in, and an unknown key does the same without any error. Commented
// markup is ignored; generated markup inside the pages' scripts is checked.
const webRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
function* pageSources(directory) {
  for (const name of readdirSync(directory)) {
    const full = path.join(directory, name);
    if (statSync(full).isDirectory()) {
      if (name !== 'node_modules' && name !== 'include') yield* pageSources(full);
    } else if (/\.(html|js)$/.test(name) && full !== fileURLToPath(new URL('./text.js', import.meta.url))) {
      yield full;
    }
  }
}
const keyAttributes = [['tid', 'text'], ['data-ph-tid', 'placeholder'], ['data-alt-tid', 'alt text']];
const unkeyed = [];
let keyedElements = 0;
for (const file of pageSources(webRoot)) {
  let text = readFileSync(file, 'utf8');
  if (file.endsWith('.html')) {
    text = text.replace(/<!--[\s\S]*?-->/g, (comment) => comment.replace(/[^\n]/g, ' '));
  }
  for (const tag of text.matchAll(/<([a-zA-Z][\w-]*)\b([^<>]*)>/g)) {
    const attributes = tag[2];
    const where = `${path.relative(webRoot, file)}:${text.slice(0, tag.index).split('\n').length}`;
    for (const [attribute, role] of keyAttributes) {
      // (?<![\w-]) keeps "tid" from also matching inside "data-ph-tid".
      const key = new RegExp(`(?<![\\w-])${attribute}\\s*=\\s*["']([^"']*)["']`).exec(attributes)?.[1];
      if (key === undefined) continue;
      keyedElements += 1;
      if (!Object.hasOwn(english, key)) unkeyed.push(`${where}: ${role} key "${key}" is not in the English table`);
    }
    const classes = /(?<![\w-])class\s*=\s*["']([^"']*)["']/.exec(attributes)?.[1].split(/\s+/) ?? [];
    if (classes.includes('trans') && !/(?<![\w-])tid\s*=/.test(attributes)) {
      unkeyed.push(`${where}: <${tag[1]} class="trans"> has no tid`);
    }
  }
}
assert.deepEqual(unkeyed, [], 'every translatable web element needs a known key');
assert.ok(keyedElements > 300, `expected the page scan to see the keyed elements, saw ${keyedElements}`);

console.log(
  `Validated yue_HK and bilingual_en_yue_HK for ${Object.keys(english).length} English web keys ` +
    `and ${keyedElements} keyed page elements.`,
);
