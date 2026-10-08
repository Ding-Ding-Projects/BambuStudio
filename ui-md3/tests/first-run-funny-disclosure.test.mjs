import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';

// The funny level must be disclosed at first run, not only in Preferences:
// the first-run setup guide has a step that states plainly that the level
// styles every message including errors and warnings, that English and
// Cantonese each start at level 5, and that either can be changed or reset at
// any time (resources/web/guide/12, wired by GuideFrame in WebGuideDialog.cpp).

const repoDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (relative) => readFileSync(path.join(repoDir, relative), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const stripMarkupComments = (text) => text.replace(/<!--[\s\S]*?-->/g, '');

const guide = 'resources/web/guide';
const textSource = read('resources/web/data/text.js');
const disclosureSource = read(`${guide}/12/funny-disclosure.js`);

// Loads the web text catalog and the step's copy logic into one context whose
// language mode is chosen the way the guide chooses it: the lang query value.
function loadDisclosure(lang, mutateCatalog) {
  const sandbox = {
    GetQueryString: (name) => (name === 'lang' ? lang : null),
    localStorage: { getItem: () => null, setItem: () => {} },
  };
  vm.createContext(sandbox);
  vm.runInContext(textSource, sandbox);
  if (mutateCatalog) mutateCatalog(sandbox.LangText);
  vm.runInContext(disclosureSource, sandbox);
  return sandbox;
}

const hostPayload = (english, cantonese, extra = {}) => ({
  command: 'response_funny_disclosure',
  available: true,
  min: 1,
  max: 5,
  default: 5,
  en: { level: english, stored: true, sample: `english sample ${english}` },
  yue: { level: cantonese, stored: true, sample: `cantonese sample ${cantonese}` },
  ...extra,
});

const plain = (html) => html.replace(/<[^>]*>/g, '');

test('the guide routes region -> funny levels -> experience programme, both ways', () => {
  const region = read(`${guide}/11/11.js`);
  assert.match(stripComments(region), /window\.location\.href="\.\.\/12\/index\.html\?region="\+encodeURIComponent\(RegionFinal\)/);
  assert.doesNotMatch(stripComments(region), /\.\.\/3\/index\.html/, 'the region page must not skip the funny-level step');

  const programme = stripMarkupComments(read(`${guide}/3/index.html`));
  assert.match(programme, /id="PreBtn" onclick="window\.open\('\.\.\/12\/index\.html\?dir=back&amp;'\+GetGetStr\(\),'_self'\)"/);

  const sandbox = loadDisclosure('en');
  assert.equal(sandbox.FunnyDisclosureRoute('back', 'Europe'), '../11/index.html');
  assert.equal(sandbox.FunnyDisclosureRoute('next', 'North America'), '../3/index.html?region=North%20America');
  assert.equal(sandbox.FunnyDisclosureRoute('next', null), '../3/index.html');
});

test('the step page loads the catalog and its logic and uses native, named controls', () => {
  const page = stripMarkupComments(read(`${guide}/12/index.html`));
  for (const script of ['../../data/text.js', '../js/globalapi.js', '../js/common.js', 'funny-disclosure.js', '12.js']) {
    assert.ok(page.includes(`src="${script}"`), `page must load ${script}`);
  }
  assert.ok(page.indexOf('src="../../data/text.js"') < page.indexOf('src="funny-disclosure.js"'));
  assert.match(page, /href="\.\.\/css\/dark\.css"/, 'dark theme sheet must be switchable like every guide page');
  for (const side of ['English', 'Cantonese']) {
    assert.match(page, new RegExp(`<label id="Funny${side}Label" for="Funny${side}Range">`));
    assert.match(page, new RegExp(`<input type="range" id="Funny${side}Range" min="1" max="5" step="1"`));
    assert.match(page, new RegExp(`<output id="Funny${side}Value" for="Funny${side}Range" aria-live="polite">`));
    assert.match(page, new RegExp(`<button type="button" class="GrayBtn FunnyReset" id="Funny${side}Reset">`));
  }
  assert.match(page, /<button type="button" class="GrayBtn trans" tid="t8" id="PreBtn" onclick="GotoPreviousPage\(\)">/);
  assert.match(page, /<button type="button" class="NormalBtn trans" tid="t9" id="AcceptBtn" onclick="GotoNextPage\(\)">/);
  // Nothing about funny levels is visible until the application has answered.
  assert.match(page, /<h1 id="FunnyTitle" hidden>/);
  assert.match(page, /<div id="FunnyDisclosure" hidden>/);
  assert.match(page, /<div id="FunnyControls" hidden>/);
  assert.match(read(`${guide}/12/12.css`), /\[hidden\]\s*\{\s*display:\s*none !important;/);

  const script = stripComments(read(`${guide}/12/12.js`));
  assert.match(script, /SendFunnyMessage\(\{ 'command': 'request_funny_disclosure' \}\)/);
  assert.match(script, /'command': 'save_funny_level', 'language': language, 'level': FunnyDisclosureClampLevel\(value\)/);
  assert.match(script, /'command': 'reset_funny_level', 'language': language/);
  assert.match(script, /'command': 'acknowledge_funny_disclosure'/);
  assert.match(script, /SaveFunnyLevel\('en', this\.value\)/);
  assert.match(script, /SaveFunnyLevel\('yue', this\.value\)/);
  assert.match(script, /ResetFunnyLevel\('en'\)/);
  assert.match(script, /ResetFunnyLevel\('yue'\)/);
  assert.match(script, /attr\('aria-valuetext', row\.valuePlain\)/);
  // School mode: the step leaves in the direction of travel without rendering.
  assert.match(script, /if \(!state\.available\) \{\s*window\.location\.replace\(FunnyDisclosureRoute\(GetQueryString\('dir'\) === 'back' \? 'back' : 'next', m_FunnyRegion\)\);\s*return;/);
  // The sample is application text and is inserted as text, never as markup.
  assert.match(script, /find\('\.FunnySampleText'\)\.text\(row\.sample\)/);
});

test('GuideFrame answers, persists and resets through the shared model', () => {
  const source = read('src/slic3r/GUI/WebGuideDialog.cpp');
  const code = stripComments(source);
  assert.match(code, /#include "FirstRunFunnyDisclosure\.hpp"/);
  assert.match(code, /#include "PersonalModes\/SchoolMode\.hpp"/);
  assert.match(code, /static_assert\(Disclosure::LEVEL_MIN == I18N::FUNNY_LEVEL_MIN/);
  assert.match(code, /static_assert\(Disclosure::LEVEL_MAX == I18N::FUNNY_LEVEL_MAX/);
  assert.match(code, /static_assert\(Disclosure::LEVEL_DEFAULT == I18N::FUNNY_LEVEL_DEFAULT/);
  assert.match(code, /else if \(const std::optional<FirstRunFunnyDisclosure::Request> funny_request = FirstRunFunnyDisclosure::parse_request\(j\)\)/);

  const apply = code.match(/wxString apply_funny_disclosure_request\(const Disclosure::Request &request\)\n\{[\s\S]*?\n\}/);
  assert.ok(apply, 'apply_funny_disclosure_request must exist');
  const body = apply[0];
  assert.match(body, /const bool available = !PersonalModes::school_presentation_suppressed\.load\(\);/);
  assert.match(body, /config->set_bool\(I18N::FUNNY_LEVEL_DISCLOSED_KEY, true\);/);
  assert.match(body, /config->set\(key, std::to_string\(request\.level\)\);/);
  assert.match(body, /config->erase\("app", key\);\s*config->set_dirty\(\);/);
  assert.match(body, /config->save\(\);/);
  assert.match(body, /I18N::language_mode_service\(\)\.set_funny_level\(disclosure_funny_language\(request\.language\), request\.level\);/);
  assert.match(body, /if \(available && config != nullptr && request\.action != Disclosure::Action::Describe\)/);
  assert.match(body, /Disclosure::payload\(state\)\.dump\(-1, ' ', true\)/);
  assert.match(code, /I18N::FUNNY_LEVEL_ENGLISH_KEY : I18N::FUNNY_LEVEL_CANTONESE_KEY/);
  assert.match(code, /I18N::funny_copy_variant\(wxString::FromUTF8\(Disclosure::SAMPLE_SOURCE\), funny, state\.level\)/);

  const model = read('src/slic3r/GUI/FirstRunFunnyDisclosure.hpp');
  assert.match(model, /inline constexpr int LEVEL_DEFAULT = 5;/);
  assert.match(model, /inline constexpr const char \*SAMPLE_SOURCE = "Slicing complete";/);
  assert.match(read('src/slic3r/GUI/LanguageMode.hpp'), /inline constexpr int FUNNY_LEVEL_DEFAULT = 5;/);
  assert.match(disclosureSource, /var FUNNY_DISCLOSURE_DEFAULT_LEVEL = 5;/);
  assert.match(read('src/slic3r/GUI/LanguageMode.cpp'), /\{"Slicing complete",\s*\{"Slicing complete"/, 'the sample source must have a funny-level ladder');
});

test('the catalog carries every disclosure string in English and Cantonese', () => {
  const sandbox = loadDisclosure('en');
  const keys = Object.values(sandbox.FUNNY_DISCLOSURE_KEYS).concat(sandbox.FUNNY_DISCLOSURE_INTRO_LADDER);
  assert.equal(new Set(keys).size, keys.length, 'every string has its own key');
  for (const key of keys) {
    assert.equal(typeof sandbox.LangText.en[key], 'string', `English ${key}`);
    assert.equal(typeof sandbox.LangText.yue_HK[key], 'string', `Cantonese ${key}`);
    assert.notEqual(sandbox.LangText.yue_HK[key], sandbox.LangText.en[key], `Cantonese ${key} must be translated`);
  }
  const { en, yue_HK: yue } = sandbox.LangText;
  assert.match(en.t304, /styles every message Bambu Studio shows, including errors and warnings/);
  assert.match(en.t305, /English and Cantonese each have their own funny level/);
  assert.match(en.t305, /Both start at level \{default\}\./);
  assert.match(en.t306, /change or reset either level/);
  assert.match(en.t306, /at any time/);
  assert.match(en.t307, /change or reset either level at any time in Preferences > General/);
  assert.match(yue.t304, /包括錯誤同警告/);
  assert.match(yue.t305, /英文同廣東話各自有自己嘅搞笑程度/);
  assert.match(yue.t305, /兩個都由第 \{default\} 級開始/);
  assert.match(yue.t306, /更改或者重設任何一個程度/);
  assert.match(yue.t306, /任何時候/);
  assert.match(yue.t307, /任何時候都可以喺「偏好設定 > 一般」更改或者重設任何一個程度/);
  for (const key of ['t305', 't311', 't312']) {
    assert.ok(yue[key].includes('{default}'), `Cantonese ${key} keeps the {default} placeholder`);
  }
  assert.ok(yue.t310.includes('{level}'));
});

test('a fresh profile reads the three facts with the level 5 defaults', () => {
  const sandbox = loadDisclosure('en');
  const state = sandbox.FunnyDisclosureState({
    command: 'response_funny_disclosure', available: true, min: 1, max: 5, default: 5,
    en: { level: 5, stored: false, sample: 'Slicing done. Every layer counted and nothing left behind.' },
    yue: { level: 5, stored: false, sample: '切片搞掂晒，一層都冇走漏。' },
  });
  const copy = sandbox.BuildFunnyDisclosureCopy(state);
  assert.equal(copy.title, 'Funny levels');
  assert.equal(copy.facts[0], sandbox.LangText.en.t304);
  assert.equal(copy.facts[1], 'English and Cantonese each have their own funny level, from 1 (fully serious) to 5 (maximum playfulness). Both start at level 5.');
  assert.equal(copy.facts[2], sandbox.LangText.en.t306);
  assert.equal(copy.controls, true);
  assert.equal(copy.english.value, 'Level 5 of 5');
  assert.equal(copy.english.reset, 'Reset English to level 5');
  assert.equal(copy.cantonese.reset, 'Reset Cantonese to level 5');
  assert.equal(copy.cantonese.label, 'Funny level (Cantonese)');
  assert.equal(copy.english.sample, 'Slicing done. Every layer counted and nothing left behind.');
});
