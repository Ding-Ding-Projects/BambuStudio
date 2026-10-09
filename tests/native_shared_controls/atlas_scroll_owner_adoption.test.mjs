import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import test from 'node:test';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const baseline = 'a271602901c1b079a342dc9ea89edca57c5345c6';
const files = [
    ['Schedule/ScheduledSettingsPanel.cpp', 'scroll', null, 2],
    ['Schedule/ScheduledSettingsPanel.hpp', 'scroll', '../Widgets/MD3ScrolledWindow.hpp', 1],
    ['WorkflowPrintPanel.cpp', 'scroll', null, 1],
    ['WorkflowPrintPanel.hpp', 'scroll', 'Widgets/MD3ScrolledWindow.hpp', 1],
    ['WorkspacePanel.cpp', 'scroll', 'Widgets/MD3ScrolledWindow.hpp', 1],
    ['Bulk/BulkActionPreviewDialog.cpp', 'table', '../Widgets/MD3DataView.hpp', 1],
    ['Bulk/BulkRenameDialog.cpp', 'table', '../Widgets/MD3DataView.hpp', 1],
];
const read = file => fs.readFileSync(path.join(root, 'src/slic3r/GUI', file), 'utf8').replaceAll('\r\n', '\n');
const old = file => execFileSync('git', ['show', baseline + ':src/slic3r/GUI/' + file], {cwd: root, encoding: 'utf8'}).replaceAll('\r\n', '\n');
// Later deliberate feature changes, each owned by its own contract and left out
// of this owner-adoption comparison on both sides:
//   * Preferences > Schedules > Add rule starts from a preset (blank-editor
//     presets, ui-md3/tests/blank-editor-presets.test.mjs owns add_rule()).
//   * The print review page's text is the kit Label and the review and workspace
//     labels take the zero-width sizer contract instead of a 1px literal
//     (ui-md3/tests/md3-conversion-contracts.test.mjs owns both: no stock
//     wxStaticText, no unscaled wxSize). Each exact converted line is mapped back
//     to the line it replaced, so every other line is still compared.
const restore = (pairs) => (text) => pairs.reduce((t, [now, before]) => t.split(now).join(before), text);
const later = {
    'Schedule/ScheduledSettingsPanel.cpp': text => text
        .replace('#include "ScheduleRuleStart.hpp"\n', '')
        .replace(/\nvoid ScheduledSettingsPanel::add_rule\(\)\n\{\n[\s\S]*?\n\}\n/, '\n'),
    'WorkflowPrintPanel.cpp': text => restore([
        ['#include "Widgets/StateColor.hpp"\n#include <wx/wrapsizer.h>\n', '#include "Widgets/StateColor.hpp"\n#include <wx/stattext.h>\n#include <wx/wrapsizer.h>\n'],
        ['Label* WorkflowPrintPanel::AddText(', 'wxStaticText* WorkflowPrintPanel::AddText('],
        ['    auto* label = new Label(parent, text, wxST_NO_AUTORESIZE);\n    // The sizer owns the width; Reflow() wraps the text to whatever it is given.\n    label->SetMinSize(wxSize(0, -1));\n',
         '    auto* label = new wxStaticText(parent, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);\n    label->SetMinSize(wxSize(1, -1));\n'],
        // A wxString on both arms: a const wxChar* / wxString ternary does not compile under strict string conversion.
        ['button->IsEnabled() ? wxString() : slice_reason', 'button->IsEnabled() ? wxEmptyString : slice_reason'],
        ['void WorkflowPrintPanel::SetText(Label* label', 'void WorkflowPrintPanel::SetText(wxStaticText* label'],
    ])(text),
    'WorkflowPrintPanel.hpp': text => restore([
        ['class StaticBox;\nclass Label;\n', 'class StaticBox;\nclass wxStaticText;\n'],
        ['Label*', 'wxStaticText*'],
    ])(text),
    'WorkspacePanel.cpp': text => restore([
        ['        // The sizer owns the width; the label wraps to whatever it is given.\n        title->SetMinSize(wxSize(0, -1));\n', '        title->SetMinSize(wxSize(1, -1));\n'],
        ['    m_overview->SetMinSize(wxSize(0, -1));\n', '    m_overview->SetMinSize(wxSize(1, -1));\n'],
    ])(text),
};
function verify(file, kind, include, count, source = read(file)) {
    const owner = kind === 'scroll' ? 'MD3ScrolledWindow' : 'MD3DataViewListCtrl';
    let restored = source;
    if (include) {
        assert(source.includes(`#include "${include}"`), file);
        restored = restored.replace(`#include "${include}"\n`, file.endsWith('.hpp') ? '#include <wx/scrolwin.h>\n' : '');
    }
    assert.equal((restored.match(new RegExp(`\\b${owner}\\b`, 'g')) || []).length, count, file);
    restored = restored.replaceAll(owner, kind === 'scroll' ? 'wxScrolledWindow' : 'wxDataViewListCtrl');
    const adjust = later[file] ?? (text => text);
    assert.equal(adjust(restored), adjust(old(file)), file + ': only the owner type and explicit include may change');
}
for (const entry of files)
    test(entry[0] + ' retains complete caller logic, sizing, flags and callbacks', () => verify(...entry));

test('owner adoption guard rejects a raw owner, changed identifier or callback body', () => {
    const entry = files[0];
    const source = read(entry[0]);
    assert.throws(() => verify(...entry, source.replace('new MD3ScrolledWindow(', 'new wxScrolledWindow(')));
    assert.throws(() => verify(...entry, source.replace('wxID_ANY', 'wxID_HIGHEST')));
    assert.throws(() => verify(...entry, source.replace('delete_selected();', 'toggle_selected();')));
});

test('kit owners retain base pointer compatibility and bulk member declarations', () => {
    assert.match(read('Widgets/MD3ScrolledWindow.hpp'), /class MD3ScrolledWindow : public wxScrolledWindow/);
    assert.match(read('Widgets/MD3DataView.hpp'), /class MD3DataViewListCtrl : public wxDataViewListCtrl/);
    for (const file of ['Bulk/BulkActionPreviewDialog.hpp', 'Bulk/BulkRenameDialog.hpp']) {
        assert.equal(read(file), old(file));
        assert.match(read(file), /wxDataViewListCtrl\s*\*/);
    }
    assert.equal(read('WorkspacePanel.hpp'), old('WorkspacePanel.hpp'));
    assert.match(read('WorkspacePanel.hpp'), /wxScrolledWindow\s*\*/);
});
