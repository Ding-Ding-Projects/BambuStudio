import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The appearance editor's Animation and transition tab. The hand-written
// inventory (design/native-appearance/motion-capability-inventory.json) and
// the table the tab lists (ElementMotion.cpp) must agree row for row; every
// wired row names a real renderer consumer found in source, and every
// unavailable row carries its exact reason and stays visible, disabled, on the
// tab. Each check below is also run against a deliberately broken copy of the
// sources (a removed tab, setting, property, consumer or inventory row) and
// must turn red there.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (relative) => readFileSync(path.join(repoDir, relative), 'utf8').replace(/\r\n/g, '\n');

const FILES = {
  popover: 'src/slic3r/GUI/Appearance/AppearanceEditorPopover.cpp',
  popoverHeader: 'src/slic3r/GUI/Appearance/AppearanceEditorPopover.hpp',
  page: 'src/slic3r/GUI/Appearance/AppearanceMotionPage.cpp',
  model: 'src/slic3r/GUI/Appearance/ElementMotion.cpp',
  modelHeader: 'src/slic3r/GUI/Appearance/ElementMotion.hpp',
  style: 'src/slic3r/GUI/Appearance/ElementStyle.cpp',
  stateHandler: 'src/slic3r/GUI/Widgets/StateHandler.cpp',
  chrome: 'src/slic3r/GUI/Widgets/MD3DialogChrome.cpp',
  motion: 'src/slic3r/GUI/Widgets/MD3Motion.cpp',
  presets: 'src/slic3r/GUI/Presets/BlankEditorPresets.cpp',
  history: 'src/slic3r/GUI/ProjectHistoryDialog.cpp',
  inventory: 'design/native-appearance/motion-capability-inventory.json',
};

function load() {
  const sources = {};
  for (const [name, file] of Object.entries(FILES)) sources[name] = read(file);
  return sources;
}

// Every capability the contract's Animation and transition clauses name.
const REQUIRED = [
  // Tracks, properties and keyframes
  'layers', 'tracks', 'target-element-state', 'property-state-colours', 'property-window-opacity', 'property-list',
  'property-geometry', 'property-control-opacity', 'property-discrete', 'custom-properties', 'computed-start',
  'explicit-values', 'interpolation', 'keyframes', 'motion-paths',
  // Timing and easing
  'duration', 'automatic-duration', 'delay', 'negative-delay', 'end-delay', 'repeat', 'direction-fill', 'playback-rate',
  'global-authority', 'global-speed-level', 'easing-named', 'easing-bezier', 'easing-bezier-handles',
  'easing-linear-stops', 'easing-steps', 'easing-spring', 'timing-summary', 'validation', 'precision',
  // Timeline and playback
  'preview-curve', 'preview-scrub', 'preview-playback', 'preview-reverse', 'preview-speed', 'preview-compare',
  'preview-zoom-loop', 'document-timelines', 'sequencing', 'overlap', 'interruption', 'cancellation', 'completion',
  // Transitions and triggers
  'trigger-hover', 'trigger-focus', 'trigger-press', 'trigger-selection', 'trigger-enable', 'trigger-entrance',
  'trigger-exit', 'trigger-status-states', 'trigger-other-events', 'trigger-thresholds', 'trigger-priority',
  'inheritance', 'discrete-transitions', 'visual-only',
  // Reversibility and access
  'reset-value', 'reset-element-motion', 'undo-redo', 'local-history', 'presets', 'import-export', 'preserve-unknown',
  'copy-paste-style', 'scheduled', 'keyboard-screen-reader', 'search', 'command-palette', 'reduced-motion',
  'non-motion-equivalent', 'editor-own-controls',
];

const unescape = (text) => text.replace(/\\(.)/g, '$1');

function sourceTable(model) {
  const rows = [];
  const pattern = /\{"([a-z0-9-]+)", k(\w+), L\("((?:[^"\\]|\\.)*)"\), S::(Wired|Unavailable),\s*L\("((?:[^"\\]|\\.)*)"\)\}/g;
  for (const m of model.matchAll(pattern)) {
    rows.push({ id: m[1], area: m[2].toLowerCase(), title: unescape(m[3]), status: m[4] === 'Wired' ? 'wired' : 'unavailable', detail: unescape(m[5]) });
  }
  return rows;
}

// --- Checks: each returns a list of problems (empty when the contract holds) ---

function tabProblems(s) {
  const problems = [];
  const names = s.popover.match(/const wxString names\[\] = \{([\s\S]*?)\};/);
  if (!names || !names[1].includes('_L("Animation and transition")')) problems.push('the section tabs do not name Animation and transition');
  const pages = [...s.popover.matchAll(/^\s*add_page\(_L\("([^"]+)"\), &AppearanceEditorPopover::(\w+)\);/gm)];
  const tabLabels = names ? [...names[1].matchAll(/_L\("([^"]+)"\)/g)].map((m) => m[1]) : [];
  if (JSON.stringify(pages.map((m) => m[1])) !== JSON.stringify(tabLabels)) problems.push('every section tab needs exactly one page, in order');
  if (!pages.some((m) => m[1] === 'Animation and transition' && m[2] === 'build_motion')) problems.push('no Animation and transition page is added');
  if (!/for \(int i = 0; i < static_cast<int>\(std::size\(names\)\); \+\+i\)/.test(s.popover)) problems.push('the tab loop must follow the names array');
  const build = s.popover.match(/void AppearanceEditorPopover::build_motion\(wxWindow \*page\)\n\{([\s\S]*?)\n\}/);
  if (!build || !/m_motion = new AppearanceMotionPage\(page, m_id, \[this\]\(\) \{ return m_anchor\.get\(\); \}\);/.test(build[1])) problems.push('build_motion must build the motion page for this element');
  if (!/void build_motion\(wxWindow \*page\);/.test(s.popoverHeader)) problems.push('build_motion is not declared');
  if (!/if \(m_motion\)\s*m_motion->retarget\(\);/.test(s.popover)) problems.push('retargeting must refresh the motion page');
  return problems;
}

function inventoryProblems(s) {
  const problems = [];
  let inventory;
  try { inventory = JSON.parse(s.inventory); } catch (error) { return [`inventory is not JSON: ${error.message}`]; }
  const rows = inventory.rows ?? [];
  const table = sourceTable(s.model);
  const ids = rows.map((row) => row.id);
  if (new Set(ids).size !== ids.length) problems.push('inventory ids repeat');
  for (const id of REQUIRED) {
    if (!ids.includes(id)) problems.push(`inventory row missing: ${id}`);
    if (!table.some((row) => row.id === id)) problems.push(`tab capability missing: ${id}`);
  }
  if (JSON.stringify(ids) !== JSON.stringify(table.map((row) => row.id))) problems.push('inventory rows and the tab table differ in ids or order');
  for (const row of rows) {
    const twin = table.find((entry) => entry.id === row.id);
    if (!twin) continue;
    if (row.capability !== twin.title) problems.push(`${row.id}: title differs from the tab`);
    if (row.status !== twin.status) problems.push(`${row.id}: status differs from the tab`);
    if (row.area !== twin.area) problems.push(`${row.id}: area differs from the tab`);
    if (row.status === 'wired') {
      if (row.behaviour !== twin.detail) problems.push(`${row.id}: behaviour differs from the tab`);
      if (!Array.isArray(row.consumers) || row.consumers.length === 0) problems.push(`${row.id}: a wired row names no consumer`);
      for (const consumer of row.consumers ?? []) {
        const name = Object.entries(FILES).find(([, file]) => file === consumer.file)?.[0];
        if (!name) { problems.push(`${row.id}: consumer file ${consumer.file} is not tracked by this contract`); continue; }
        if (!s[name].includes(consumer.evidence)) problems.push(`${row.id}: consumer evidence is gone from ${consumer.file}: ${consumer.evidence}`);
      }
      for (const key of ['persistence', 'undoHistory', 'presetsImportExport', 'accessibility', 'builtAppEvidence']) {
        if (typeof row[key] !== 'string' || row[key].length < 10) problems.push(`${row.id}: ${key} is not stated`);
      }
    } else if (row.status === 'unavailable') {
      if (typeof row.reason !== 'string' || row.reason.length < 40) problems.push(`${row.id}: an unavailable row needs its exact reason`);
      if (row.reason !== twin.detail) problems.push(`${row.id}: the reason differs from the tab`);
      if (!/kept verbatim/.test(row.portableInput ?? '')) problems.push(`${row.id}: portable input handling is not stated`);
    } else {
      problems.push(`${row.id}: unknown status ${row.status}`);
    }
  }
  return problems;
}

function visibilityProblems(s) {
  const problems = [];
  const build = s.page.match(/void AppearanceMotionPage::build_capabilities\([\s\S]*?\n\}/);
  if (!build) return ['the tab does not build its capability list'];
  if (!/for \(const ElementMotion::Capability &row : ElementMotion::capabilities\(\)\) \{/.test(build[0])) problems.push('the tab must list every capability');
  if (/continue;/.test(build[0])) problems.push('no capability may be skipped');
  if (!/if \(!wired_row\)\s*t->Disable\(\);/.test(build[0])) problems.push('an unavailable capability must be a disabled entry');
  if (!/Label \*d = note\(parent, _L\(row\.detail\)\);/.test(build[0])) problems.push('every capability shows its reason');
  if (!/_L\("Unavailable: %s"\)/.test(build[0])) problems.push('unavailable entries are named as such');
  return problems;
}

function consumerProblems(s) {
  const problems = [];
  // State transitions: the element's own timing for the state that changed.
  const transition = s.stateHandler.match(/void StateHandler::transition\(int previous_states\)\n\{[\s\S]*?\n\}/)?.[0] ?? '';
  if (!/ElementStyle::motion_for\(\s*Slic3r::GUI::ElementStyle::element_id_of\(owner_\.get\(\)\),\s*Motion::trigger_for_change\(previous_states, states\(\)\), MD3::Motion::short2\)/.test(transition)) problems.push('StateHandler must read the element motion for the changed state');
  if (!/motion_\.Play\(timing\.total_ms\(\), \[this, timing\]\(double t\) \{\s*progress_ = timing\.progress\(t\);/.test(transition)) problems.push('StateHandler must play the resolved timing');
  if (!/&MD3::Motion::easeLinear, owner_\.get\(\)\);/.test(transition)) problems.push('StateHandler must shape progress itself through a linear animator');
  if (!/progress_ >= 1\.0 \|\| MD3::Motion::reduced\(\)/.test(s.stateHandler)) problems.push('StateHandler must keep painting final colours under reduced motion');
  // Entrances: adopted top-level windows and this editor.
  if (!/Slic3r::GUI::ElementStyle::entrance_for\(owner, Slic3r::GUI::ElementStyle::own_id_of\(owner\), duration_ms\);/.test(s.chrome)) problems.push('the entrance helper must record its window');
  const begin = s.chrome.match(/void MD3TransientEntrance::Begin\(int duration_ms\)\n\{[\s\S]*?\n\}/)?.[0] ?? '';
  if (!/if \(!m_owner \|\| MD3::Motion::reduced\(\)\) return;/.test(begin)) problems.push('the entrance must stay off under reduced motion');
  if (!/m_anim\.Play\(timing\.total_ms\(\), \[this, timing\]\(double t\)/.test(begin)) problems.push('the entrance must play the window timing');
  if (!/ElementStyle::entrance_for\(s_current, card, MD3::Motion::short2\);\s*MD3::Motion::FadeIn\(s_current, entrance\.total_ms\(\), \[entrance\]\(double t\) \{ return entrance\.progress\(t\); \}\);/.test(s.popover)) problems.push('the editor card must fade in with its own entrance timing');
  if (!/void FadeIn\(wxWindow \*window, int total_ms, std::function<double\(double\)> progress\)/.test(s.motion)) problems.push('FadeIn must accept a progress mapping');
  if (!/if \(reduced\(\)\) \{\s*\/\/ Ensure fully opaque and untouched\./.test(s.motion)) problems.push('FadeIn must stay opaque under reduced motion');
  // One curve implementation for the model and the widgets.
  if (!/return Math::standard\(t\);/.test(s.motion) || !/return Math::emphasized\(t\);/.test(s.motion)) problems.push('the widgets must use the shared Material curves');
  return problems;
}

function persistenceProblems(s) {
  const problems = [];
  if (!/return std::find\(keys\.begin\(\), keys\.end\(\), key\) != keys\.end\(\) \|\| ElementMotion::is_motion_key\(key\);/.test(s.style)) problems.push('motion keys must be known style properties');
  if (!/ElementMotion::validate_key\(it\.key\(\), it\.value\(\)\); !check\.ok\)/.test(s.style)) problems.push('import must report invalid motion values');
  for (const trigger of ['hover', 'focus', 'press', 'selection', 'enable', 'entrance']) {
    for (const field of ['duration', 'delay', 'easing']) {
      if (!s.presets.includes(`{"motion.${trigger}.${field}", L("`)) problems.push(`preset statements must label motion.${trigger}.${field}`);
    }
  }
  for (const action of ['Reset element motion', 'Undo element motion', 'Redo element motion']) {
    if (!s.history.includes(`L("${action}: %s")`)) problems.push(`Version history must translate ${action}`);
  }
  if (!/forward \? "Redo element motion" : "Undo element motion"/.test(s.page)) problems.push('undo and redo must be named history actions');
  if (!/history_label\("Reset element motion", m_id\)/.test(s.page)) problems.push('reset element motion must be a named history action');
  if (!/m_history\.record\(\{key, \{\{id, key, before, value\}\}\}\);/.test(s.page)) problems.push('every write must be undoable');
  return problems;
}

const CHECKS = { tabProblems, inventoryProblems, visibilityProblems, consumerProblems, persistenceProblems };

test('the source state satisfies every motion-tab check', () => {
  const s = load();
  for (const [name, check] of Object.entries(CHECKS)) assert.deepEqual(check(s), [], name);
});

test('the inventory covers every contract capability, both wired and unavailable', () => {
  const inventory = JSON.parse(read(FILES.inventory));
  const statuses = new Set(inventory.rows.map((row) => row.status));
  assert.deepEqual([...statuses].sort(), ['unavailable', 'wired']);
  assert.equal(inventory.rows.length, REQUIRED.length, 'one row per contract capability');
  assert.match(inventory.globalAuthority, /Interface motion/);
});

// --- Negative completeness: each removal must turn its check red ---

function mutate(change) {
  const s = load();
  change(s);
  return s;
}

const MUTATIONS = [
  ['removing the tab', 'tabProblems', (s) => { s.popover = s.popover.replace(/\n\s*add_page\(_L\("Animation and transition"\), &AppearanceEditorPopover::build_motion\);/, ''); }],
  ['removing the tab label', 'tabProblems', (s) => { s.popover = s.popover.replace(',\n                              _L("Animation and transition")', ''); }],
  ['dropping an inventory row', 'inventoryProblems', (s) => {
    const doc = JSON.parse(s.inventory);
    doc.rows = doc.rows.filter((row) => row.id !== 'easing-steps');
    s.inventory = JSON.stringify(doc);
  }],
  ['dropping a tab capability', 'inventoryProblems', (s) => { s.model = s.model.replace(/\n\s*\{"keyframes",[\s\S]*?\)\},/, ''); }],
  ['changing an unavailable reason on one side', 'inventoryProblems', (s) => {
    const doc = JSON.parse(s.inventory);
    doc.rows.find((row) => row.id === 'easing-spring').reason = 'Springs are coming soon to this renderer, honestly.';
    s.inventory = JSON.stringify(doc);
  }],
  ['marking an unavailable capability as wired', 'inventoryProblems', (s) => {
    const doc = JSON.parse(s.inventory);
    const row = doc.rows.find((entry) => entry.id === 'keyframes');
    row.status = 'wired';
    s.inventory = JSON.stringify(doc);
  }],
  ['removing the state-transition consumer', 'inventoryProblems', (s) => { s.stateHandler = s.stateHandler.replace('progress_ = timing.progress(t);', 'progress_ = t;'); }],
  ['removing the entrance consumer', 'inventoryProblems', (s) => { s.chrome = s.chrome.replace('static_cast<BYTE>(64 + 191 * shown)', 'static_cast<BYTE>(64 + 191 * t)'); }],
  ['removing the steps setting from the model', 'inventoryProblems', (s) => { s.model = s.model.replace('case Kind::Steps: return steps_value(t, std::max(1, steps), jump);', 'case Kind::Steps: return t;'); }],
  ['hiding unavailable capabilities', 'visibilityProblems', (s) => { s.page = s.page.replace('const bool wired_row = row.support == ElementMotion::Support::Wired;', 'const bool wired_row = row.support == ElementMotion::Support::Wired;\n        if (!wired_row)\n            continue;'); }],
  ['enabling unavailable entries', 'visibilityProblems', (s) => { s.page = s.page.replace(/if \(!wired_row\)\s*t->Disable\(\);/, ''); }],
  ['StateHandler ignoring the element', 'consumerProblems', (s) => { s.stateHandler = s.stateHandler.replace('Slic3r::GUI::ElementStyle::element_id_of(owner_.get())', 'std::string()'); }],
  ['an entrance ignoring reduced motion', 'consumerProblems', (s) => { s.chrome = s.chrome.replace('if (!m_owner || MD3::Motion::reduced()) return;', 'if (!m_owner) return;'); }],
  ['the editor card ignoring its timing', 'consumerProblems', (s) => { s.popover = s.popover.replace('MD3::Motion::FadeIn(s_current, entrance.total_ms(), [entrance](double t) { return entrance.progress(t); });', 'MD3::Motion::FadeIn(s_current, MD3::Motion::short2);'); }],
  ['motion keys unknown to the registry', 'persistenceProblems', (s) => { s.style = s.style.replace(' || ElementMotion::is_motion_key(key);', ';'); }],
  ['a preset label dropped', 'persistenceProblems', (s) => { s.presets = s.presets.replace('{"motion.press.easing", L("Press easing")},', ''); }],
  ['an unrecorded write', 'persistenceProblems', (s) => { s.page = s.page.replace('m_history.record({key, {{id, key, before, value}}});', ''); }],
];

for (const [what, check, change] of MUTATIONS) {
  test(`negative regression: ${what} turns ${check} red`, () => {
    const broken = mutate(change);
    assert.notDeepEqual(broken, load(), 'the mutation must change the sources');
    assert.ok(CHECKS[check](broken).length > 0, `${what} was not detected`);
  });
}

// --- The pure motion model, compiled and run when a C++ compiler is present ---

test('the motion model passes its pure g++ test', (t) => {
  const probe = spawnSync('g++', ['--version'], { encoding: 'utf8' });
  if (probe.status !== 0) {
    t.skip('g++ is not available here; tests/appearance/CMakeLists.txt builds element_motion_tests instead');
    return;
  }
  const dir = mkdtempSync(path.join(tmpdir(), 'element-motion-'));
  try {
    const binary = path.join(dir, 'element_motion_tests');
    const build = spawnSync('g++', ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', path.join(repoDir, 'src'),
      path.join(repoDir, 'tests/appearance/element_motion_standalone.cpp'),
      path.join(repoDir, 'src/slic3r/GUI/Appearance/ElementMotion.cpp'), '-o', binary], { encoding: 'utf8' });
    assert.equal(build.status, 0, build.stderr);
    const run = spawnSync(binary, [], { encoding: 'utf8' });
    assert.equal(run.status, 0, run.stderr + run.stdout);
    assert.match(run.stdout, /element motion tests passed/);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('CMake builds the pure motion test and links the model into the appearance tests', () => {
  const cmake = read('tests/appearance/CMakeLists.txt');
  assert.match(cmake, /add_executable\(element_motion_tests element_motion_standalone\.cpp/);
  assert.match(cmake, /add_test\(NAME element_motion_tests COMMAND element_motion_tests\)/);
  assert.match(cmake, /appearance_tests_main\.cpp\n\s*\$\{PROJECT_SOURCE_DIR\}\/src\/slic3r\/GUI\/Appearance\/ElementStyle\.cpp\n\s*\$\{PROJECT_SOURCE_DIR\}\/src\/slic3r\/GUI\/Appearance\/ElementMotion\.cpp/);
  const gui = read('src/slic3r/CMakeLists.txt');
  for (const file of ['ElementMotion.cpp', 'AppearanceMotionPage.cpp']) assert.match(gui, new RegExp(`GUI/Appearance/${file.replace('.', '\\.')}`));
  const list = read('bbl/i18n/list.txt');
  for (const file of ['AppearanceMotionPage.cpp', 'ElementMotion.cpp']) assert.ok(list.includes(`src/slic3r/GUI/Appearance/${file}`), `${file} strings are extracted`);
});
