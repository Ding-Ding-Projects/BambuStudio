/*
 * The layer-by-layer redesign of the Pages site (docs/features/pages/README.md,
 * "The redesign"): every feature has a card with a drawing and an article,
 * the Docs tab carries every article and the wiki, the Tools tab carries the
 * palette, the Base64 toolkit, the accessibility tools and the Animation and
 * transition settings, and reduced motion is a design of its own.
 *
 * Each checker also runs against a broken copy of the source and must fail.
 */
import assert from 'node:assert/strict';
import test from 'node:test';
import vm from 'node:vm';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(testDir, '..');
const repoDir = path.resolve(uiDir, '..');
const read = (file) => readFile(path.join(uiDir, file), 'utf8');

const features = await read('site/features.js');
const docs = await read('site/docs.js');
const tools = await read('site/tools.js');
const motion = await read('site/motion.js');
const css = await read('site/site.css');
const landing = await read('landing.html');
const boot = await read('site/boot.js');
const compose = await read('scripts/compose-site.mjs');
const builder = await read('scripts/build-docs-data.mjs');

function load(source, globals) {
  const context = vm.createContext(Object.assign({ document: { addEventListener() {}, documentElement: { setAttribute() {}, style: { setProperty() {} } }, querySelectorAll: () => [] }, navigator: {} }, globals));
  context.window = context;
  vm.runInContext(source, context);
  return context;
}

function checkFeatures(source) {
  const context = load(source, { BambuSite: {} });
  const list = context.BambuFeatures.FEATURES;
  assert.equal(list.length, 16, 'sixteen features, as the hero badge says');
  const ids = new Set(list.map((f) => f.id));
  for (const wanted of ['localtools', 'livecharts', 'elementlocks', 'attention', 'drawingboard', 'virtualdisks',
    'minecraft', 'extensions', 'base64', 'wizard', 'preferences', 'everyfeature', 'languages', 'dimsum', 'multislice', 'landrop'])
    assert.ok(ids.has(wanted), `feature ${wanted} is listed`);
  for (const feature of list) {
    assert.match(feature.article, /^[a-z0-9-]+$/, `${feature.id} names its article`);
    const drawing = context.BambuFeatures.art(feature.id);
    assert.match(drawing, /^<svg class="feature-art"/, `${feature.id} has a drawing`);
    assert.doesNotMatch(drawing, /<text/, `${feature.id} draws with shapes, never text (the clipping gate measures text)`);
    assert.ok(/fx-/.test(drawing), `${feature.id} animates`);
  }
}

async function checkArticlesExist(source) {
  const context = load(source, { BambuSite: {} });
  const files = [];
  async function walk(directory, prefix) {
    for (const entry of await readdir(directory, { withFileTypes: true })) {
      if (entry.isDirectory()) await walk(path.join(directory, entry.name), `${prefix}${entry.name}/`);
      else if (entry.name.endsWith('.md') && !entry.name.endsWith('.yue_HK.md')) files.push(`${prefix}${entry.name}`);
    }
  }
  await walk(path.join(repoDir, 'docs', 'features'), '');
  const ids = new Set(files.map((f) => f.replace(/\.md$/, '').replace(/[^A-Za-z0-9]+/g, '-').replace(/^-|-$/g, '').toLowerCase()));
  for (const feature of context.BambuFeatures.FEATURES)
    assert.ok(ids.has(feature.article), `${feature.id} points at an article that exists: ${feature.article}`);
  for (const match of source.matchAll(/article: '([a-z0-9-]+)'/g)) assert.ok(ids.has(match[1]));
}

function checkDocs(source) {
  assert.match(source, /script\.src = '\.\/site\/docs\.data\.js'/, 'the bundle loads on demand');
  assert.match(source, /createSearchField\(\{\s*labelKey: 'docs\.search'/, 'the shared search field with the regex builder');
  assert.match(source, /a\.en \+ '\\n' \+ a\.yue/, 'the search covers both languages');
  assert.match(source, /showYue \? current\.yue : current\.en/, 'the Cantonese twin is read when chosen');
  assert.match(source, /SpeechSynthesisUtterance/, 'read aloud');
  assert.match(source, /function escapeHtml/, 'markdown is escaped before rendering');
  const context = load(source, { BambuSite: {}, BambuRegex: {} });
  const html = context.BambuDocs.render('# Title\n\nA **bold** `code` [link](https://example.invalid)\n\n- one\n- two\n\n| a | b |\n| --- | --- |\n| 1 | 2 |\n\n<script>alert(1)</script>\n');
  assert.match(html, /<h2>Title<\/h2>/);
  assert.match(html, /<strong>bold<\/strong>/);
  assert.match(html, /<ul><li>one<\/li><li>two<\/li><\/ul>/);
  assert.match(html, /<table class="doc-table">/);
  assert.doesNotMatch(html, /<script>/, 'markup in an article never runs');
  assert.match(html, /&lt;script&gt;/);
}

function checkTools(source) {
  assert.match(source, /event\.ctrlKey && event\.shiftKey && \(event\.key === 'F' \|\| event\.key === 'f'\)/, 'Ctrl+Shift+F opens the palette');
  assert.match(source, /class="palette-switch"/, 'settings are live switches in the results');
  assert.match(source, /heading\.classList\.add\('teleported'\)/, 'a section result teleports to the element');
  assert.match(source, /reader\.readAsDataURL\(file\)/, 'files are read in the browser');
  assert.match(source, /file\.size > 8 \* 1024 \* 1024/, 'a file over 8 MB is refused');
  assert.match(source, /'base64History'/, 'the history is kept');
  for (const key of ['highContrast', 'focusAlways', 'largerTargets', 'motionHero', 'motionHover'])
    assert.match(source, new RegExp(`switchRow\\('${key}'`), `${key} has a switch`);
  assert.match(source, /data-motion>/, 'reduced motion has system, on and off');
  assert.match(source, /BambuMotion\.setPreview\(preview\.checked\)/, 'preview reduced motion');
  const context = load(source, { BambuSite: {}, BambuRegex: {}, TextEncoder, TextDecoder, btoa, atob, Uint8Array });
  assert.equal(context.BambuTools.utf8ToBase64('Har Gow 蝦餃'), 'SGFyIEdvdyDonabppIM=');
  assert.equal(context.BambuTools.base64ToUtf8('SGFyIEdvdyDonabppIM='), 'Har Gow 蝦餃');
  assert.equal(context.BambuTools.urlSafe('a+b/c=='), 'a-b_c');
  assert.equal(context.BambuTools.base64ToUtf8(context.BambuTools.urlSafe(context.BambuTools.utf8ToBase64('??>'))), '??>');
}

function checkMotion(source, stylesheet) {
  assert.match(source, /if \(choice === 'on'\) return true;\s*if \(choice === 'off'\) return false;\s*return systemReduced\(\);/, 'reduced motion follows the system until chosen');
  assert.match(source, /if \(off\) revealEverything\(\);/, 'reduced motion shows every section at once');
  assert.match(source, /navigator\.webdriver/, 'an automated browser sees everything at once');
  assert.match(source, /function transition\(update\) \{\s*update\(\);/, 'the tab switch itself is synchronous');
  assert.match(stylesheet, /\[data-reduced-motion="on"\] \.hero-paths \.p2, \[data-reduced-motion="on"\] \.hero-paths \.p3 \{ stroke: var\(--md-primary\); \}/, 'reduced motion settles the hero on one hue');
  assert.match(stylesheet, /\[data-reduced-motion="on"\] \.feature-art \* \{ animation: none !important; stroke-dashoffset: 0 !important; \}/, 'drawings are complete under reduced motion');
  assert.match(stylesheet, /\[data-reduced-motion="on"\] \.reveal \{ opacity: 1; transform: none; transition: none; \}/);
  assert.match(stylesheet, /\.panel\.tab-enter \{ animation: tab-fade/);
  assert.match(stylesheet, /\[data-motion-tab="slide"\] \.panel\.tab-enter \{ animation: tab-slide/);
}

function checkShell(landingSource, bootSource, composeSource, builderSource) {
  for (const module of ['motion', 'features', 'docs', 'tools'])
    assert.match(landingSource, new RegExp(`<script src="\\./site/${module}\\.js"></script>`), `${module}.js is loaded`);
  assert.match(landingSource, /id="paletteButton"/);
  assert.match(landingSource, /id="accessButton"/);
  for (const id of ['features', 'docs', 'tools'])
    assert.match(bootSource, new RegExp(`\\{ id: '${id}', copy: 'tab\\.${id}'`), `the ${id} tab is registered`);
  assert.match(composeSource, /build-docs-data\.mjs/, 'the compose step builds the documentation bundle');
  assert.match(composeSource, /BambuStudio\.wiki\.git/, 'the wiki is cloned into the bundle');
  assert.match(composeSource, /wikiDir = clone\.status === 0 \? scratch : '';/, 'a cut network never fails the compose');
  assert.match(builderSource, /yue_HK\.md/, 'the Cantonese twin rides with its article');
  assert.match(builderSource, /window\.BAMBU_DOCS = /);
}

test('every feature has a card, a drawing and an article', () => checkFeatures(features));
test('every feature card points at an article that exists', () => checkArticlesExist(features));
test('the Docs tab loads the bundle, searches both languages and renders safely', () => checkDocs(docs));
test('the Tools tab: palette, Base64, accessibility, animation', () => checkTools(tools));
test('reduced motion is a design, not a slower one', () => checkMotion(motion, css));
test('the shell loads the modules, registers the tabs and composes the bundle', () =>
  checkShell(landing, boot, compose, builder));

test('each checker fails on a broken copy', async () => {
  assert.throws(() => checkFeatures(features.replace("{ id: 'landrop'", "{ id: 'landrop2'")));
  await assert.rejects(() => checkArticlesExist(features.replace("article: 'device-live-charts'", "article: 'device-live-chart'")));
  assert.throws(() => checkDocs(docs.replace('function escapeHtml', 'function escapeHtmlx')));
  assert.throws(() => checkTools(tools.replace('file.size > 8 * 1024 * 1024', 'false')));
  assert.throws(() => checkMotion(motion.replace('if (off) revealEverything();', ''), css));
  assert.throws(() => checkMotion(motion, css.replace('stroke-dashoffset: 0 !important; }', '}')));
  assert.throws(() => checkShell(landing, boot.replace("{ id: 'docs', copy: 'tab.docs'", "{ id: 'doc', copy: 'tab.docs'"), compose, builder));
});
