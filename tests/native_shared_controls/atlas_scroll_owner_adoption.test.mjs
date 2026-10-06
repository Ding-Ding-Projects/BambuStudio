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
function verify(file, kind, include, count, source = read(file)) {
    const owner = kind === 'scroll' ? 'MD3ScrolledWindow' : 'MD3DataViewListCtrl';
    let restored = source;
    if (include) {
        assert(source.includes(`#include "${include}"`), file);
        restored = restored.replace(`#include "${include}"\n`, file.endsWith('.hpp') ? '#include <wx/scrolwin.h>\n' : '');
    }
    assert.equal((restored.match(new RegExp(`\\b${owner}\\b`, 'g')) || []).length, count, file);
    restored = restored.replaceAll(owner, kind === 'scroll' ? 'wxScrolledWindow' : 'wxDataViewListCtrl');
    assert.equal(restored, old(file), file + ': only the owner type and explicit include may change');
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
