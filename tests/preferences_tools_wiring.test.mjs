// Source checks for the Preferences tools that change settings from somewhere
// other than the setting's own row (src/slic3r/GUI/Preferences.cpp):
//
// - Reset this section changes nothing before the two-key confirmation, and
//   only what plan_section_reset put in its reset list.
// - Import changes nothing before you agree to the preview, and only what
//   plan_import put in its apply list.
// - Every change goes through set_value, which refuses a control that is
//   disabled (locked), and the setters drive the row's own control.
// - The info panel's Reset and the Related chips act after their click
//   returns, because acting rebuilds the panel the button lives in.
// - The consent row (privacyuse) gets no setter.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const source = fs.readFileSync(path.join(root, 'src/slic3r/GUI/Preferences.cpp'), 'utf8').replaceAll('\r\n', '\n');

function body(text, name) {
    const start = text.indexOf(name);
    assert(start >= 0, name);
    const begin = text.indexOf('{', start);
    const masked = text.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g,
        part => ' '.repeat(part.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return text.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const code = text => text.replace(/\/\/[^\n]*/g, '');

function checkReset(text) {
    const reset = code(body(text, 'void PreferencesDialog::reset_section('));
    const plan = reset.indexOf('PreferencePlans::plan_section_reset(settings_on_page(page))');
    const gate = reset.indexOf('if (!SuperConfirmGate::Run(m_reset_section, spec)) return;');
    const first = reset.indexOf('set_value(');
    assert(plan >= 0 && gate > plan && first > gate, 'nothing is set before both keys and the slide');
    assert.match(reset.slice(gate), /for \(const std::string &key : plan\.reset\)/, 'only the planned settings change');
    assert.doesNotMatch(reset, /plan\.by_hand\)\s*\{[^}]*set_value/, 'settings left for their own controls are never set');
}

function checkImport(text) {
    const imp = code(body(text, 'void PreferencesDialog::import_preferences('));
    const plan = imp.indexOf('PreferencePlans::plan_import(text, settings_on_page(-1))');
    const ask = imp.indexOf('ShowModal() != wxID_OK) return;', plan);
    const first = imp.indexOf('set_value(');
    assert(plan >= 0 && ask > plan && first > ask, 'nothing is imported before you agree');
    assert.match(imp.slice(ask), /for \(const PreferencePlans::ImportItem &item : plan\.apply\)/, 'only the planned settings change');
    assert.match(imp, /size > PreferencePlans::kImportLimit/, 'a large file is refused before it is read');
}

function checkSetters(text) {
    const can = code(body(text, 'bool PreferencesDialog::can_set('));
    assert.match(can, /it->second\.control->IsEnabled\(\)/, 'a locked control is never driven');
    const set = code(body(text, 'bool PreferencesDialog::set_value('));
    assert.match(set, /if \(!can_set\(key\)\) return false;/);
    const sw = code(body(text, 'bool drive_switch('));
    assert.match(sw, /ProcessEvent\(toggled\)/, 'a switch change runs the row handler');
    const combo = code(body(text, 'bool drive_combo('));
    assert.match(combo, /GetDropDown\(\)\.GetEventHandler\(\)->ProcessEvent\(picked\)/, 'a choice change runs the row handler');
    const checkbox = code(body(text, 'wxBoxSizer *PreferencesDialog::create_item_checkbox('));
    assert.match(checkbox, /if \(param != "privacyuse" && param != "auto_stop_liveview"\)\s*register_setter\(/, 'the consent row has no setter');
    const undo = code(body(text, 'bool PreferencesDialog::undo_last_change('));
    assert.match(undo, /PreferencePlans::plan_undo\(/);
    assert.match(undo, /!set_value\(plan\.key, plan\.value\)/);
}

function checkPanel(text) {
    const info = code(body(text, 'void PreferencesDialog::show_info('));
    assert.match(info, /reset->Bind\(wxEVT_BUTTON, \[this, key, target\]\(wxCommandEvent &\) \{\s*CallAfter\(/, 'Reset acts after its click returns');
    assert.match(info, /if \(can_set\(key\)\)/, 'a locked row offers no Reset');
    const related = code(body(text, 'void PreferencesDialog::add_related('));
    assert.match(related, /chip->Bind\(wxEVT_BUTTON, \[this, other\]\(wxCommandEvent &\) \{\s*CallAfter\(/, 'a chip acts after its click returns');
}

function broken(text, from, to) {
    assert(text.includes(from), `fixture anchor: ${from}`);
    return text.replace(from, to);
}

test('Reset this section waits for both keys and changes only the plan', () => checkReset(source));
test('Import waits for your answer and changes only the plan', () => checkImport(source));
test('every change goes through the row control, never a locked one', () => checkSetters(source));
test('the panel acts after the click that rebuilds it', () => checkPanel(source));

test('each checker fails on a broken copy', () => {
    assert.throws(() => checkReset(broken(source, '    if (!SuperConfirmGate::Run(m_reset_section, spec)) return;\n', '')));
    assert.throws(() => checkImport(broken(source,
        "    if (MessageDialog(this, question, _L(\"Import preferences\"), wxOK | wxCANCEL | wxICON_QUESTION).ShowModal() != wxID_OK) return;\n",
        '')));
    assert.throws(() => checkSetters(broken(source, ' && (it->second.control == nullptr || it->second.control->IsEnabled())', '')));
    assert.throws(() => checkSetters(broken(source, 'if (param != "privacyuse" && param != "auto_stop_liveview")\n        register_setter(',
                                            'if (true)\n        register_setter(')));
    assert.throws(() => checkPanel(broken(source, `            reset->Bind(wxEVT_BUTTON, [this, key, target](wxCommandEvent &) {
                CallAfter([this, key, target]() {
                    set_value(key, target);
                    poll_changes();
                });
            });`, `            reset->Bind(wxEVT_BUTTON, [this, key, target](wxCommandEvent &) {
                set_value(key, target);
                poll_changes();
            });`)));
});
