// Source checks for Focus outside the 3D view and Low stimulation's quieter
// colours (docs/features/windows/attention-modes.md):
//
// - The object list mutes a row only while Focus is on and something is
//   selected; a selected row, anything inside it, the rows a selection sits in
//   and the plate headings stay as they are. Muting only colours the text,
//   it never hides a row, and the list repaints when the selection or Focus
//   changes.
// - Preferences: while one setting is explained with Focus on, the other rows
//   are muted, and every row gets its own colour back first.
// - Low stimulation switches the quieter colours on at startup and whenever
//   it changes, with the same repaint a theme change gets.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const list = read('src/slic3r/GUI/GUI_ObjectList.cpp');
const model = read('src/slic3r/GUI/ObjectDataViewModel.cpp');
const prefs = read('src/slic3r/GUI/Preferences.cpp');
const service = read('src/slic3r/GUI/AttentionModes/AttentionService.cpp');

function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, name);
    const begin = source.indexOf('{', start);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, (t) => ' '.repeat(t.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const code = (text) => text.replace(/\/\/[^\n]*/g, '');

function checkList(listSource, modelSource) {
    const muted = code(body(listSource, 'bool ObjectList::muted_by_focus('));
    assert.match(muted, /if \(!AttentionModes::focus_on\(\) \|\| !item\.IsOk\(\)/, 'nothing is muted while Focus is off');
    assert.match(muted, /if \(selected\.empty\(\)\) return false;/, 'nothing is muted without a selection');
    assert.match(muted, /& itPlate\) return false;/, 'plate headings stay');
    assert.match(muted, /if \(IsSelected\(it\)\) return false;/, 'a selected row and what it holds stay');
    assert.match(muted, /if \(it == item\) return false;/, 'the rows a selection sits in stay');
    const attr = code(body(modelSource, 'bool ObjectDataViewModel::GetAttr('));
    assert.match(attr, /if \(!m_muted_rows \|\| !m_muted_rows\(item\)\) return false;\s*attr\.SetColour\(/);
    assert.doesNotMatch(attr, /Hide|Delete|Remove/, 'muting never hides a row');
    assert.match(code(body(listSource, 'void ObjectList::selection_changed(')), /refresh_focus_rows\(\);/);
    assert.match(code(body(listSource, 'void ObjectList::update_selections(')), /refresh_focus_rows\(\);\s*\}$/);
}

function checkPreferences(source) {
    const focus = code(body(source, 'void PreferencesDialog::apply_focus_rows('));
    const restore = focus.indexOf('window->SetForegroundColour(colour);');
    const gate = focus.indexOf('app_config->get(AttentionModes::kFocus) != "true") return;');
    const mute = focus.indexOf('label->SetForegroundColour(muted);');
    assert(restore >= 0 && gate > restore && mute > gate, 'every row gets its colour back, then only Focus mutes');
    assert.match(focus, /if \(info\.row == nullptr \|\| info\.row == focused->row\) continue;/, 'the explained row keeps its colour');
    assert.match(code(body(source, 'void PreferencesDialog::show_info(')), /m_info_key = key;\s*apply_focus_rows\(\);/);
    assert.match(code(body(source, 'void PreferencesDialog::hide_info(')), /m_info_key\.clear\(\);\s*apply_focus_rows\(\);/);
}

function checkQuiet(source) {
    const tick = code(body(source, 'void tick_all('));
    assert.match(tick, /if \(s\.low_stimulation != svc\.quiet\) \{\s*svc\.quiet = s\.low_stimulation;\s*apply_quiet\(svc\.quiet\);/);
    assert.match(code(body(source, 'void init(')), /if \(current\(\)\.low_stimulation\) \{\s*svc\.quiet = true;\s*apply_quiet\(true\);/);
    const apply = code(body(source, 'void apply_quiet('));
    assert.match(apply, /MD3::setQuietColours\(on\);/);
    assert.match(apply, /force_colors_update\(\);/, 'the same repaint a theme change gets');
}

function broken(source, from, to) {
    assert(source.includes(from), `fixture anchor: ${from}`);
    return source.replace(from, to);
}

test('the object list mutes only while Focus is on and something is selected', () => checkList(list, model));
test('Preferences mutes the other rows while one is explained', () => checkPreferences(prefs));
test('Low stimulation turns the quieter colours on and off', () => checkQuiet(service));
test('each checker fails on a broken copy', () => {
    assert.throws(() => checkList(broken(list, '    if (selected.empty()) return false;\n', ''), model));
    assert.throws(() => checkList(list, broken(model, 'if (!m_muted_rows || !m_muted_rows(item)) return false;', 'if (!m_muted_rows) return false;')));
    assert.throws(() => checkPreferences(broken(prefs, ' || app_config->get(AttentionModes::kFocus) != "true") return;', ') return;')));
    assert.throws(() => checkQuiet(broken(service, '        apply_quiet(svc.quiet);\n', '')));
});
