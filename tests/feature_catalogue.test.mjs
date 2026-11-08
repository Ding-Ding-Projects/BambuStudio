// The Every feature page must never offer an Open button that opens nothing.
//
// 1. Compiles and runs tests/feature_catalogue_test.cpp (catalogue integrity,
//    actions, filters, sort orders, recently opened, the command registry).
// 2. Builds it again with SLIC3R_FEATURE_CATALOGUE_NEGATIVE_FIXTURE, which
//    breaks the production recently-opened order; that build must fail.
// 3. Checks the sources: every command id is registered in MainFrame.cpp, every
//    Preferences key is a registered Preferences row, every article exists and
//    is in the offline documentation bundle, and every icon exists in the
//    shipped Material Symbols font. The same scan run on a copy of
//    MainFrame.cpp with one registration removed must report it.
import {existsSync, mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (rel) => readFileSync(path.join(root, rel), 'utf8');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-feature-catalogue-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + read('tests/feature_catalogue_test.cpp'));
  const exe = path.join(output, 'fixture.exe');
  const compile = compileFixture({source, exe, cwd: output, include: root});
  if (compile.error) throw compile.error;
  if (compile.status !== 0) process.exit(compile.status ?? 1);
  return {exe, output};
}

// ---------------------------------------------------------------- native suite
const normal = build('normal', '');
const green = spawnSync(normal.exe, [], {cwd: normal.output, stdio: 'inherit'});
if (green.error) throw green.error;
if (green.status !== 0) process.exit(green.status ?? 1);

const mutated = build('negative', '#define SLIC3R_FEATURE_CATALOGUE_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the suite cannot notice a broken recently-opened order.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (recently opened appended at the back).');

// ---------------------------------------------------------------- source scan
const catalogue = read('src/slic3r/GUI/FeatureFinder/FeatureCatalogue.cpp');
const entries = [...catalogue.matchAll(
  /\{"([a-z0-9-]+)", "([a-z-]+)", 0x([0-9A-F]{4,5}) \/\* ([a-z0-9_]+) \*\/, L\("((?:[^"\\]|\\.)*)"\), L\("((?:[^"\\]|\\.)*)"\), "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)", Badge::(\w+), State::(\w+), "((?:[^"\\]|\\.)*)"\}/g,
)].map((m) => ({id: m[1], glyph: parseInt(m[3], 16), icon: m[4], title: m[5], state: m[10], action: m[11]}));
if (entries.length < 40) {
  console.error(`Only ${entries.length} catalogue entries were parsed; the scan pattern no longer matches the table.`);
  process.exit(1);
}

// Preferences keys whose rows register through a named constant.
const keyTokens = {
  app_display_name: ['AppDisplayName::CONFIG_KEY'],
  app_logo: ['AppLogo::config_key'],
  funny_level_en: ['I18N::FUNNY_LEVEL_ENGLISH_KEY'],
  funny_level_yue: ['I18N::FUNNY_LEVEL_CANTONESE_KEY'],
  action_sounds_enabled: ['ActionSounds::enabled_key'],
  attention_time_awareness: ['AttentionModes::kTimeAwareness'],
};

function scan({mainframe, preferences, bundleRoutes}) {
  const problems = [];
  const prefLines = preferences.split('\n');
  for (const e of entries) {
    if (e.state === 'NotBuilt') {
      if (e.action !== '') problems.push(`${e.id}: not built yet but has an action`);
      continue;
    }
    const [kind, ...rest] = e.action.split(':');
    const target = rest.join(':');
    if (kind === 'command') {
      const registered = mainframe.includes(`feature_command("${target}"`) ||
        mainframe.includes(`Commands::instance().add("${target}"`);
      if (!registered) problems.push(`${e.id}: command "${target}" is not registered in MainFrame.cpp`);
    } else if (kind === 'prefs') {
      const tokens = [`"${target}"`, ...(keyTokens[target] ?? [])];
      const registered = prefLines.some((line, i) => tokens.some((t) => line.includes(t)) &&
        prefLines.slice(Math.max(0, i - 6), i + 1).some((l) => /register_option_row\(|create_item_/.test(l)));
      if (!registered) problems.push(`${e.id}: Preferences key "${target}" is not a registered Preferences row`);
    } else if (kind === 'docs') {
      if (!existsSync(path.join(root, 'docs/features', target))) problems.push(`${e.id}: article docs/features/${target} does not exist`);
      if (!bundleRoutes.has(target)) problems.push(`${e.id}: article ${target} is not in the offline documentation bundle`);
    } else if (e.action !== 'palette') {
      problems.push(`${e.id}: ready but its action "${e.action}" is not one the page can run`);
    }
  }
  return problems;
}

const sources = {
  mainframe: read('src/slic3r/GUI/MainFrame.cpp'),
  preferences: read('src/slic3r/GUI/Preferences.cpp'),
  bundleRoutes: new Set(JSON.parse(read('src/slic3r/GUI/Documentation/bundle-manifest.json')).articles.map((a) => a.route)),
};
const problems = scan(sources);

// Negative scan: drop one registration and the scan has to say so.
const sample = entries.find((e) => e.state === 'Ready' && e.action.startsWith('command:'));
const sampleId = sample.action.slice('command:'.length);
const broken = sources.mainframe.split(`feature_command("${sampleId}"`).join('[](wxCommandEvent&) { /* removed */ }; (void)(');
if (!scan({...sources, mainframe: broken}).some((p) => p.includes(`"${sampleId}"`))) {
  problems.push(`negative scan: removing the "${sampleId}" registration was not noticed`);
}

// ---------------------------------------------------------------- font glyphs
function cmapCodepoints(buffer) {
  const view = new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);
  const numTables = view.getUint16(4);
  let cmapOffset = -1;
  for (let i = 0; i < numTables; i++) {
    const rec = 12 + i * 16;
    if (String.fromCharCode(...buffer.subarray(rec, rec + 4)) === 'cmap') cmapOffset = view.getUint32(rec + 8);
  }
  if (cmapOffset < 0) throw new Error('font has no cmap table');
  const codepoints = new Set();
  const subtables = view.getUint16(cmapOffset + 2);
  for (let i = 0; i < subtables; i++) {
    const offset = cmapOffset + view.getUint32(cmapOffset + 4 + i * 8 + 4);
    const format = view.getUint16(offset);
    if (format === 4) {
      const segX2 = view.getUint16(offset + 6);
      const ends = offset + 14, starts = ends + segX2 + 2, deltas = starts + segX2, ranges = deltas + segX2;
      for (let s = 0; s < segX2 / 2; s++) {
        const end = view.getUint16(ends + s * 2), start = view.getUint16(starts + s * 2);
        const delta = view.getInt16(deltas + s * 2), rangeOffset = view.getUint16(ranges + s * 2);
        for (let c = start; c <= end && c !== 0xFFFF; c++) {
          let glyph;
          if (rangeOffset === 0) glyph = (c + delta) & 0xFFFF;
          else {
            const addr = ranges + s * 2 + rangeOffset + (c - start) * 2;
            glyph = view.getUint16(addr);
            if (glyph !== 0) glyph = (glyph + delta) & 0xFFFF;
          }
          if (glyph !== 0) codepoints.add(c);
        }
      }
    } else if (format === 12) {
      const groups = view.getUint32(offset + 12);
      for (let g = 0; g < groups; g++) {
        const base = offset + 16 + g * 12;
        const start = view.getUint32(base), end = view.getUint32(base + 4);
        for (let c = start; c <= end; c++) codepoints.add(c);
      }
    }
  }
  return codepoints;
}
const font = cmapCodepoints(readFileSync(path.join(root, 'resources/fonts/MaterialSymbolsOutlined.ttf')));
for (const e of entries) {
  if (!font.has(e.glyph)) problems.push(`${e.id}: icon ${e.icon} (U+${e.glyph.toString(16).toUpperCase()}) is not in the shipped icon font`);
}

if (problems.length) {
  for (const p of problems) console.error(p);
  process.exit(1);
}
console.log(`Every feature sources: ${entries.length} entries; commands, Preferences rows, articles and icons all resolve.`);
console.log(`Negative scan noticed the removed "${sampleId}" registration.`);
