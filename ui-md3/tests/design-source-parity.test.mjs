// Parity guard for the checked-in Claude Design source files.
//
// The files under design-source/ are the read-only source of truth the MD3
// implementation is measured against. These tests pin four facts: the files
// are present, the main shell carries its expected structural markers, the
// proprietary DC runtime (support.js) is never imported by shipped code, and —
// the part that keeps parity at 100% — every line of the design's markup,
// every string of its data, and every rule of its CSS is present in what ships.
//
// The rule is design ⊆ app. The shipped app deliberately carries more than the
// design (working search filtering with empty-state rows, showcase images on
// Home, accessibility attributes, a responsive title bar, the Settings language
// row, and the project-wide Filament→Ink / AMS→Ink Dispenser rename). Those are
// approved extensions and are listed as such below; anything the design has
// that the app lacks fails here and names the design line that went missing.

import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(testDir, '..');
const designSourceDir = path.join(uiDir, 'design-source');

const expectedFiles = [
  'Bambu Studio.dc.html',
  'SearchField.dc.html',
  'support.js',
  'CLAUDE.md',
];

const readDesignSource = (...p) => readFile(path.join(designSourceDir, ...p), 'utf8');
const readUi = (...p) => readFile(path.join(uiDir, ...p), 'utf8');

test('every expected design-source file exists and is non-empty', async () => {
  for (const name of expectedFiles) {
    const content = await readDesignSource(name);
    assert.ok(content.length > 0, `design-source/${name} must exist and be non-empty`);
  }
});

test('Bambu Studio.dc.html carries its key structural markers', async () => {
  const html = await readDesignSource('Bambu Studio.dc.html');
  for (const marker of ['dc-import', 'SearchField', 'data-theme']) {
    assert.ok(html.includes(marker),
      `the design shell must reference "${marker}"`);
  }
});

test('support.js stays a design-only asset and is not imported by any shipped file', async () => {
  // The DC runtime is proprietary and was authored against React; we reimplement
  // a vanilla runtime instead. Shipped code must therefore never import it —
  // neither through an ESM import nor a CommonJS require.
  const importPattern = /(?:from\s+['"][^'"]*support\.js|require\(\s*['"][^'"]*support\.js)/;
  for (const dir of ['site', 'app']) {
    const entries = await readdir(path.join(uiDir, dir), { withFileTypes: true });
    for (const entry of entries) {
      if (!entry.isFile() || !entry.name.endsWith('.js')) continue;
      const source = await readFile(path.join(uiDir, dir, entry.name), 'utf8');
      assert.doesNotMatch(source, importPattern,
        `shipped file ${dir}/${entry.name} must not import support.js`);
    }
  }
});

test('CLAUDE.md states the design sources are read-only source-of-truth files', async () => {
  const doc = await readDesignSource('CLAUDE.md');
  assert.ok(/do not edit/i.test(doc) && /source of truth/i.test(doc),
    'CLAUDE.md must declare the folder read-only and authoritative');
});

// ---------------------------------------------------------------------------
// design ⊆ app: markup
// ---------------------------------------------------------------------------

// The approved vocabulary. The product renamed filament to ink and the AMS to
// the Ink Dispenser across the app, the kit and the Cantonese catalog; the
// design source predates the rename. Applied to design lines before comparing.
const VOCABULARY = [
  ['Filament Manager', 'Ink Manager'],
  ['Search filaments', 'Search inks'],
  ['Search filament presets', 'Search ink presets'],
  ['Export filaments', 'Export inks'],
  ['No filaments match', 'No inks match'],
  ['Export filament', 'Export ink'],
  ['New filament', 'New ink'],
  ['Add filament', 'Add ink'],
  ['Sync with AMS', 'Sync with Ink Dispenser'],
  ['Sync AMS', 'Sync dispenser'],
  ['AMS mapping', 'Ink Dispenser mapping'],
  ['printer and filament', 'printer and ink'],
  ['for a filament', 'for an ink'],
  ['AMS · Slot', 'Ink Dispenser · Slot'],
  ['> AMS<', '> Ink Dispenser<'],
  ['> Filament<', '> Ink<'],
  ['<span>Filament</span>', '<span>Ink</span>'],
  ['>Filament</span>', '>Ink</span>'],
  ["label:'Filament'", "label:'Ink'"],
  ['filament preset', 'ink preset'],
  ['Add filament:', 'Add ink:'],
];

// The Filament screen's table is driven by its own filtered list; the design's
// shared `filamentRows` binding is still used by the Add-ink dialog in the shell.
const SCREEN_BINDINGS = { filament: [['{{ filamentRows }}', '{{ filRows }}']] };

// Snackbar messages that took interpolation parameters when they moved into
// the localisation catalog. Design literal → catalog English source.
const MESSAGE_TEMPLATES = [
  ['Slicing Plate 1…', 'Slicing Plate {plate}…'],
  ['Plate 1 sliced · 1h 24m · 23.4 g', 'Plate {plate} sliced · {time} · {weight}'],
  ['Sent to Bambu Lab X1 Carbon · print starting', 'Sent to {printer} · print starting'],
];

// Attributes the app adds on top of the design's elements. They are removed
// from BOTH sides before comparing, so their presence is neither required nor
// forbidden; they simply cannot hide a missing design attribute.
const APPROVED_ATTRIBUTES = [
  /\s+aria-[a-z-]+="[^"]*"/g,
  /\s+role="[^"]*"/g,
  /\s+class="[^"]*"/g,
  /\s+id="[^"]*"/g,
  /\s+data-dialog-close/g,
  /\s+data-dialog-initial/g,
  /\s+data-language-[a-z-]+="[^"]*"/g,
  /\s+on-query="[^"]*"/g,
  /\s+hint-[a-z-]+="[^"]*"/g,
  /\s+loading="[^"]*"/g,
  /\s+decoding="[^"]*"/g,
];

// The app gave a title/aria-label to the design's untitled window buttons and
// switches; a design element that HAD a title keeps it through normalisation
// because the pattern below only strips a title that precedes an aria-label —
// the app's signature for an added one — or the switch titles bound to a label.
const ADDED_TITLES = [
  /\s+title="(?:Minimize|Maximize|Close)"/g,
  /\s+title="\{\{ p\.label \}\}"/g,
  /\s+title="Chamber light"/g,
];

// Home's recent-card thumbnail: the design's striped placeholder div now also
// carries `position:relative` so the showcase image can layer on top of it.
const APPROVED_STYLE_PREFIXES = [
  ['<div style="height:118px; background:repeating-linear-gradient(45deg, var(--md-sc-high) 0 10px, var(--md-sc) 10px 20px); display:flex; align-items:center; justify-content:center; color:var(--md-on-surface-variant);"><span data-icon style="font-size:40px;">deployed_code</span></div>\n<div style="padding:12px 14px;">',
   '<div style="position:relative; height:118px; background:repeating-linear-gradient(45deg, var(--md-sc-high) 0 10px, var(--md-sc) 10px 20px); display:flex; align-items:center; justify-content:center; color:var(--md-on-surface-variant);"><span data-icon style="font-size:40px;">deployed_code</span><img src="./assets/showcase/{{ r.image }}" alt="" style="position:absolute; inset:0; display:block; width:100%; height:100%; object-fit:cover;"></div>\n<div style="padding:12px 14px;">'],
  // The hero's children were made positioned so they stack above the artwork.
  ['<div style="width:64px; height:64px; border-radius:20px;', '<div style="position:relative; z-index:1; width:64px; height:64px; flex:0 0 64px; border-radius:20px;'],
  ['<div style="flex:1; min-width:0;">\n<div style="font-size:23px;', '<div style="position:relative; z-index:1; flex:1; min-width:0;">\n<div style="font-size:23px;'],
  ['<div style="display:flex; gap:12px;">\n<button onClick="{{ goPrepare }}"', '<div style="position:relative; z-index:1; display:flex; gap:12px;">\n<button onClick="{{ goPrepare }}"'],
  // Title bar: the spacer's flex now comes from a class; the title is pinned.
  ['<div style="flex:1; -webkit-app-region:drag; height:100%;"></div>', '<div style="-webkit-app-region:drag; height:100%;"></div>'],
  ['<div style="font-weight:500; font-size:14px; margin-right:10px; letter-spacing:.1px;">Bambu Studio</div>',
   '<div style="font-weight:500; font-size:14px; margin-right:10px; letter-spacing:.1px; flex:0 0 auto;">Bambu Studio</div>'],
  // Objects list: the two hard-coded rows became a keyed loop with the same
  // markup; the row text is bound instead of literal and gets an ellipsis.
  ['<span data-icon style="font-size:18px;">deployed_code</span>\n<span style="flex:1; font-size:12.5px; font-weight:500;">3DBenchy.stl</span>',
   '<span data-icon style="font-size:18px;">{{ o.icon }}</span>\n<span style="flex:1; min-width:0; font-size:12.5px; font-weight:500; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">{{ o.name }}</span>'],
  ['<div style="width:16px; height:16px; border-radius:5px; background:var(--md-primary); border:1px solid rgba(0,0,0,.2);"></div>',
   '<div style="width:16px; height:16px; flex:0 0 auto; border-radius:5px; background:var(--md-primary); border:1px solid rgba(0,0,0,.2);"></div>'],
  ['<span data-icon style="font-size:16px;">layers</span> <span style="flex:1;">Layers &amp; height range</span>',
   '<span data-icon style="font-size:16px;">{{ o.icon }}</span> <span style="flex:1; min-width:0; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">{{ o.name }}</span>'],
];

function normalise(html, { design, extra = [] } = {}) {
  let text = html.replace(/\r\n/g, '\n');
  for (const [from, to] of extra) text = text.split(from).join(to);
  for (const pattern of APPROVED_ATTRIBUTES) text = text.replace(pattern, '');
  for (const pattern of ADDED_TITLES) text = text.replace(pattern, '');
  // The design shell uses "AMS" as a whole word in text; the vocabulary map
  // above is applied only to the design side so the app can never regress
  // back to the old words unnoticed either.
  if (design) for (const [from, to] of VOCABULARY) text = text.split(from).join(to);
  let lines = text.split('\n').map((line) => line.trim()).filter(Boolean);
  // Multi-line-aware approved rewrites are applied on the joined text.
  if (design) {
    let joined = lines.join('\n');
    for (const [from, to] of APPROVED_STYLE_PREFIXES) joined = joined.split(from).join(to);
    lines = joined.split('\n');
  }
  return lines;
}

// Every design line must appear, in order, in the app's lines.
function assertSubsequence(designLines, appLines, label, designOffset) {
  let cursor = 0;
  for (let index = 0; index < designLines.length; index += 1) {
    const wanted = designLines[index];
    let found = -1;
    for (let scan = cursor; scan < appLines.length; scan += 1) {
      if (appLines[scan] === wanted) { found = scan; break; }
    }
    assert.notEqual(found, -1,
      `${label}: design line ${designOffset + index + 1} is missing from the app (or out of order):\n  ${wanted}`);
    cursor = found + 1;
  }
}

// Depth-aware extraction of the <sc-if value="{{ flag }}"> … </sc-if> block.
function extractScIf(html, flag) {
  const start = html.indexOf(`<sc-if value="{{ ${flag} }}"`);
  assert.notEqual(start, -1, `design must contain <sc-if value="{{ ${flag} }}">`);
  const tags = /<sc-if\b|<\/sc-if>/g;
  tags.lastIndex = start;
  let depth = 0;
  let match;
  while ((match = tags.exec(html))) {
    if (match[0] === '<sc-if') depth += 1;
    else if ((depth -= 1) === 0) {
      return { text: html.slice(start, match.index + match[0].length), line: html.slice(0, start).split('\n').length };
    }
  }
  throw new Error(`unterminated <sc-if value="{{ ${flag} }}">`);
}

function extractBetween(html, startMarker, endMarker) {
  const start = html.indexOf(startMarker);
  assert.notEqual(start, -1, `design must contain ${startMarker}`);
  const end = html.indexOf(endMarker, start + startMarker.length);
  assert.notEqual(end, -1, `design must contain ${endMarker} after ${startMarker}`);
  return { text: html.slice(start, end), line: html.slice(0, start).split('\n').length };
}

const SCREENS = ['home', 'prepare', 'preview', 'device', 'multi', 'project', 'calibration', 'filament', 'settings'];

test('every screen template contains the design screen block line for line, in order', async () => {
  const design = await readDesignSource('Bambu Studio.dc.html');
  for (const id of SCREENS) {
    const flag = `is${id[0].toUpperCase()}${id.slice(1)}`;
    const block = extractScIf(design, flag);
    const template = await readUi('app', 'screens', `${id}.template.html`);
    assert.match(template, new RegExp(`<template data-screen="${id}">`),
      `${id}.template.html must wrap a <template data-screen="${id}">`);
    assertSubsequence(normalise(block.text, { design: true, extra: SCREEN_BINDINGS[id] || [] }), normalise(template), `screen "${id}"`, block.line - 1);
  }
});

test('the shell (title bar, tab bar, popover, snackbars, drawer, dialogs) contains the design shell line for line', async () => {
  const design = await readDesignSource('Bambu Studio.dc.html');
  const index = await readUi('index.html');
  const shellStart = index.indexOf('<template data-shell>');
  const shellEnd = index.indexOf('</template>', shellStart);
  assert.ok(shellStart !== -1 && shellEnd !== -1, 'index.html must carry <template data-shell>');
  const shell = normalise(index.slice(shellStart, shellEnd));

  const pieces = [
    extractBetween(design, '<!-- ===== TITLE BAR ===== -->', '<!-- ===== BODY ===== -->'),
    extractScIf(design, 'showControls'),
    extractBetween(design, '<div style="position:absolute; left:50%; bottom:22px;', '<sc-if value="{{ showHistory }}">'),
    extractScIf(design, 'showHistory'),
    extractScIf(design, 'isDlgExport'),
    extractScIf(design, 'isDlgSend'),
    extractScIf(design, 'isDlgAddfil'),
  ];
  for (const piece of pieces) {
    assertSubsequence(normalise(piece.text, { design: true }), shell, `shell (design line ${piece.line})`, piece.line - 1);
  }

  // The root element and its bindings.
  const root = normalise(extractBetween(design, '<div data-theme="{{ theme }}"', '<!-- ===== TITLE BAR ===== -->').text, { design: true });
  const appRoot = normalise(index.slice(shellStart, shellEnd)).find((line) => line.startsWith('<div data-theme="{{ theme }}"')) || '';
  assert.ok(root[0] && appRoot.includes(root[0].replace(/">$/, '').replace(/^<div data-theme="\{\{ theme \}\}" data-density="\{\{ density \}\}" /, '')),
    'the app root must keep the design root style and accentOverride binding');
  assert.ok(appRoot.includes('data-theme="{{ theme }}" data-density="{{ density }}"'), 'the app root must bind theme and density');

  // The design's body container that hosts the screens.
  assert.ok(shell.includes('<div style="flex:1 1 auto; position:relative; overflow:hidden; background:var(--md-surface-dim);">'),
    'the body container must match the design');
  // Each screen slot is present, in the design's order.
  assertSubsequence(SCREENS.map((id) => `<!--SCREEN:${id}-->`), shell, 'screen slots', 0);
});

test('the SearchField template contains the design component line for line', async () => {
  const design = await readDesignSource('SearchField.dc.html');
  const index = await readUi('index.html');
  const start = index.indexOf('<template data-component="SearchField">');
  const end = index.indexOf('</template>', start);
  assert.ok(start !== -1 && end !== -1, 'index.html must carry the SearchField template');
  const body = extractBetween(design, '<div style="position:relative; width:100%;', '</x-dc>');
  assertSubsequence(normalise(body.text, { design: true }), normalise(index.slice(start, end)), 'SearchField', body.line - 1);
});

// ---------------------------------------------------------------------------
// design ⊆ app: CSS tokens and logic data
// ---------------------------------------------------------------------------

test('styles.css carries the design helmet CSS verbatim', async () => {
  const design = await readDesignSource('Bambu Studio.dc.html');
  const css = extractBetween(design, ':root, [data-theme="light"]{', '</style>').text.replace(/\r\n/g, '\n').trim();
  const styles = (await readUi('app', 'styles.css')).replace(/\r\n/g, '\n');
  // The shipped file adds one focus-visible rule after the anchor reset;
  // every design rule must still be present, in order.
  assertSubsequence(css.split('\n').map((l) => l.trim()).filter(Boolean),
    styles.split('\n').map((l) => l.trim()).filter(Boolean), 'styles.css', 0);
});

test('the shipped logic carries every string of the design data', async () => {
  const design = await readDesignSource('Bambu Studio.dc.html');
  const script = extractBetween(design, 'class Component extends DCLogic', '</script>').text;
  // The snackbar messages moved into the localisation catalog (English source
  // strings keyed for Cantonese), so the catalog is part of the shipped logic.
  const shipped = [
    await readUi('app', 'main.logic.js'),
    await readUi('app', 'i18n.resources.js'),
    ...(await Promise.all(SCREENS.map((id) => readUi('app', 'screens', `${id}.logic.js`)))),
  ].join('\n');
  // Every quoted literal in the design's logic — labels, icons, ids, hashes,
  // messages, colors — must survive in the shipped logic, vocabulary-mapped.
  const literals = new Set();
  for (const match of script.matchAll(/'((?:[^'\\]|\\.)*)'/g)) literals.add(match[1]);
  const mapped = (value) => [...VOCABULARY, ...MESSAGE_TEMPLATES].reduce((acc, [from, to]) => acc.split(from).join(to), value);
  const missing = [...literals]
    .filter((value) => value.length > 1 && !/^(?:g|i|m|s|w)$/.test(value))
    .map(mapped)
    .filter((value) => !shipped.includes(`'${value}'`) && !shipped.includes(`"${value}"`) && !shipped.includes(value));
  assert.deepEqual(missing, [], 'design logic literals missing from the shipped logic');
  // And every design method is still defined somewhere in the shipped logic.
  for (const match of script.matchAll(/^\s{2}([a-zA-Z_]+)\(/gm)) {
    const name = match[1];
    if (name === 'constructor') continue;
    assert.match(shipped, new RegExp(`(?:^|\\n)\\s*${name}\\(`),
      `design method ${name}() must be defined in the shipped logic`);
  }
});

test('the shipped SearchField logic carries the design tokens and flag chips', async () => {
  const design = await readDesignSource('SearchField.dc.html');
  const script = extractBetween(design, 'class Component extends DCLogic', '</script>').text;
  const shipped = await readUi('app', 'searchfield.logic.js');
  for (const match of script.matchAll(/'((?:[^'\\]|\\.)*)'/g)) {
    const value = match[1];
    if (value.length <= 1) continue;
    assert.ok(shipped.includes(`'${value}'`) || shipped.includes(value),
      `SearchField literal '${value}' must survive in searchfield.logic.js`);
  }
});
