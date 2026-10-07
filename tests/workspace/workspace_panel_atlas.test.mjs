import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const file = 'src/slic3r/GUI/WorkspacePanel.cpp';
const baselineCommit = '3c8fe2708ba03470c1b171e2fce12fcbdfaab0fc';
const original = execFileSync('git', ['show', `${baselineCommit}:${file}`], { cwd: root, encoding: 'utf8' }).replace(/\r\n/g, '\n');
const current = (process.env.WORKSPACE_ATLAS_SOURCE_REVISION
  ? execFileSync('git', ['show', `${process.env.WORKSPACE_ATLAS_SOURCE_REVISION}:${file}`], { cwd: root, encoding: 'utf8' })
  : fs.readFileSync(path.join(root, file), 'utf8')).replace(/\r\n/g, '\n');

// Small lexical scanner for these source boundaries, preserving strings and comments.
function balanced(source, start, open, close) {
  let depth = 0;
  let quote = null;
  let lineComment = false;
  let blockComment = false;
  for (let i = start; i < source.length; ++i) {
    const c = source[i], next = source[i + 1];
    if (lineComment) { if (c === '\n') lineComment = false; continue; }
    if (blockComment) { if (c === '*' && next === '/') { blockComment = false; ++i; } continue; }
    if (quote) { if (c === '\\') ++i; else if (c === quote) quote = null; continue; }
    if (c === '/' && next === '/') { lineComment = true; ++i; continue; }
    if (c === '/' && next === '*') { blockComment = true; ++i; continue; }
    if (c === '"' || c === "'") { quote = c; continue; }
    if (c === open) ++depth;
    if (c === close && --depth === 0) return source.slice(start, i + 1);
  }
  assert.fail(`Unclosed ${open} at ${start}`);
}
function method(source, name) {
  const start = source.indexOf(`WorkspacePanel::${name}(`);
  assert.ok(start >= 0, `Missing method ${name}`);
  const brace = source.indexOf('{', start);
  return source.slice(start, brace) + balanced(source, brace, '{', '}');
}
function bindings(source) {
  return [...source.matchAll(/(?:[A-Za-z_][A-Za-z_0-9]*->)?Bind\(/g)].map(match => {
    const paren = match.index + match[0].length - 1;
    return match[0].slice(0, -1) + balanced(source, paren, '(', ')');
  });
}
const functionalMethods = '~WorkspacePanel WorkspacePanel refresh_overview refresh_files refresh_checklist refresh_calendar refresh_all create_new edit_preferences choose_open open_bundle reset_reminder_cursor check_reminders choose_save save_bundle stage_member_file selected_member open_selected_member save_member add_member add_source add_checklist edit_checklist move_checklist add_slot snooze_selected_slot dismiss_selected_slot toggle_selected_slot export_checklist export_calendar'.split(' ');
function preserveCallbacks(source) {
  const baseline = bindings(original).sort();
  const behavior = bindings(source).filter(item => !/Bind\(wxEVT_(?:SIZE|DPI_CHANGED|SYS_COLOUR_CHANGED),/.test(item)).sort();
  assert.deepEqual(behavior, baseline, 'Workspace event callbacks or binding multiplicity changed');
}
function visualOnly(source) {
  for (const name of ['refresh_appearance', 'reflow']) {
    assert.ok(!/m_workspace|m_dirty|save_bundle|refresh_all|m_notes->(?:SetValue|ChangeValue)|m_month->SetDate/.test(method(source, name)), `${name} must not change user data, dates or drafts`);
  }
}

test('thirty workspace methods retain behavior with only the Overview presentation hook allowed', () => {
  assert.equal(functionalMethods.length, 30);
  for (const name of functionalMethods) {
    const observed = method(current, name);
    // Permit only one trailing presentation call, never a rewritten label,
    // dirty flag, data operation or callback hidden by a broad normalization.
    const preserved = name === 'refresh_overview' ? observed.replace(/\n    reflow\(\);(?=\n})/, '') : observed;
    assert.equal(preserved, method(original, name), name);
  }
});
test('every existing workspace event callback is retained unchanged', () => {
  const candidate = process.env.WORKSPACE_ATLAS_MUTATE_CALLBACK === '1'
    ? current.replace('m_workspace.notes = utf8(m_notes->GetValue()); m_dirty = true;', 'm_workspace.notes.clear(); m_dirty = true;')
    : current;
  preserveCallbacks(candidate);
});
test('all page identities, dispatch targets and table columns are retained', () => {
  const pages = [...current.matchAll(/add_section\([a-z_]+, _L\("([^"]+)"\)\);/g)].map(match => match[1]);
  assert.deepEqual(pages, ['Overview','Files','Checklist','Notes','Calendar']);
  for (const expression of [/\b(?:button|list_button|calendar_button)\(_L\("[^"]+"\), &WorkspacePanel::[a-z_]+\);/g, /m_(?:files|agenda)->AppendTextColumn\([^;]+;/g]) {
    assert.deepEqual(current.match(expression), original.match(expression));
  }
});
test('visual lifecycle has no workspace, date or draft mutation and waits for construction', () => {
  visualOnly(current);
  assert.ok(method(current, 'reflow').includes('if (!m_ui_ready || m_reflowing || GetClientSize().x <= 0) return;'));
  const create = method(current, 'create_ui');
  assert.ok(create.indexOf('m_ui_ready = true;') > create.indexOf('add_section(calendar_page, _L("Calendar"));'));
  assert.ok(create.indexOf('m_ui_ready = true;') < create.lastIndexOf('refresh_appearance();'));
});
test('existing translation keys are reused without new uncoordinated copy', () => {
  const keys = source => [...new Set([...source.matchAll(/_L\("((?:[^"\\]|\\.)*)"\)/g)].map(match => match[1]))].sort();
  assert.deepEqual(keys(current), keys(original));
});
test('deliberate callback and visual-data mutations are rejected', () => {
  const brokenCallback = current.replace('m_workspace.notes = utf8(m_notes->GetValue()); m_dirty = true;', 'm_workspace.notes.clear(); m_dirty = true;');
  assert.notEqual(brokenCallback, current);
  assert.throws(() => preserveCallbacks(brokenCallback), /event callbacks/);
  const brokenVisual = current.replace('void WorkspacePanel::reflow()\n{', 'void WorkspacePanel::reflow()\n{\n    m_workspace.notes.clear();');
  assert.notEqual(brokenVisual, current);
  assert.throws(() => visualOnly(brokenVisual), /must not change user data/);
});

function contentLayoutProtocol(source) {
  const overview = method(source, 'refresh_overview');
  assert.ok(overview.indexOf('reflow();') > overview.indexOf('m_overview->SetLabel('), 'Overview content must trigger reflow without a size event');
  const layout = method(source, 'reflow');
  const guard = layout.indexOf('if (!m_ui_ready || m_reflowing || GetClientSize().x <= 0) return;');
  const invalidate = layout.indexOf('overview_card->InvalidateBestSize();');
  const cardLayout = layout.indexOf('overview_card->Layout();');
  const extent = layout.indexOf('page->FitInside();');
  const pageLayout = layout.indexOf('page->Layout();');
  assert.ok(guard >= 0 && invalidate > guard && cardLayout > invalidate && extent > cardLayout && pageLayout > extent,
    'Guard, Overview invalidation/layout and scroll extent/layout must occur in order');
  assert.ok(layout.includes('auto *overview_card = m_overview->GetParent();'));
  return ['content', 'invalidate-card', 'layout-card', 'fit-page', 'layout-page'];
}

test('stable-width short-long-short content follows the actual source layout protocol', () => {
  const protocol = contentLayoutProtocol(current);
  // Source-derived lifecycle model only: no native widget or font measurement
  // is claimed. Width never changes and no size event is supplied.
  const model = { width: 20, labelHeight: 1, cachedCardHeight: 1, extent: 1 };
  const heights = [];
  for (const text of ['Short title', 'Long workspace title '.repeat(12), 'Short title']) {
    for (const operation of protocol) {
      if (operation === 'content') model.labelHeight = Math.ceil(text.length / model.width);
      if (operation === 'invalidate-card') model.cachedCardHeight = null;
      if (operation === 'layout-card') model.cachedCardHeight = model.labelHeight;
      if (operation === 'fit-page') model.extent = model.cachedCardHeight;
    }
    heights.push(model.extent);
  }
  assert.ok(heights[1] > heights[0]);
  assert.equal(heights[2], heights[0]);
  assert.equal(model.width, 20);
  // Rename and bundle-open/refresh routes must still reach this common hook.
  assert.ok(bindings(current).some(binding => binding.includes('m_workspace.title = utf8(title); m_dirty = true; refresh_overview();')));
  assert.ok(method(current, 'refresh_all').includes('refresh_overview();'));
  assert.ok(method(current, 'open_bundle').includes('refresh_all();'));
});

test('removing the content hook or Overview layout invalidates the lifecycle contract', () => {
  assert.throws(() => contentLayoutProtocol(current.replace('    reflow();\n}\n\nvoid WorkspacePanel::refresh_files', '}\n\nvoid WorkspacePanel::refresh_files')), /trigger reflow/);
  assert.throws(() => contentLayoutProtocol(current.replace('    overview_card->Layout();', '')), /must occur in order/);
});
