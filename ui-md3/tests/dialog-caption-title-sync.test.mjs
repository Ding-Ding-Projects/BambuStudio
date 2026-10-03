import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The kit caption (MD3DialogCaption) replaced the native title bar, but it read the title
// once: a dialog that called SetTitle() afterwards changed the window text the user never
// sees and kept its first title on the strip. The Load-to-nozzle dialog showed "Confirm"
// where "Load <tray> to left nozzle" belonged. A caption adopted from the dialog's own
// title now follows it, and a dialog can ask for the update in the same paint.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');
const code = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const read = async (...parts) => code(await readFile(path.join(guiDir, ...parts), 'utf8'));
const fn = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, signature);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};

test('the kit caption can follow its dialog title', async () => {
  const header = await read('Widgets', 'MD3DialogChrome.hpp');
  assert.match(header, /^\s*static void SyncTitle\(wxDialog \*dialog\);/m);
  assert.match(header, /^\s*void FollowDialogTitle\(\);/m);
  assert.match(header, /^\s*wxString\s+m_applied_title;/m, 'the strip remembers the title it last applied, not the label text the decorator may pair');
  assert.match(header, /^\s*bool\s+m_follow_title\s*\{\s*false\s*\};/m);

  const source = await read('Widgets', 'MD3DialogChrome.cpp');
  const ctor = fn(source, 'MD3DialogCaption::MD3DialogCaption(wxDialog *dialog, const wxString &title)');
  assert.match(ctor, /^\s*m_applied_title = title;/m);
  assert.match(ctor, /Bind\(wxEVT_IDLE, \[this\]\(wxIdleEvent &e\) \{\s*if \(m_follow_title\)\s*FollowDialogTitle\(\);\s*e\.Skip\(\);\s*\}\);/,
    'a following caption checks the title on idle');
  const adopt = fn(source, 'void MD3DialogCaption::Adopt(wxDialog *dialog, const wxString &title)');
  assert.match(adopt, /^\s*caption->m_follow_title = title\.empty\(\);/m, 'only a caption that took the dialog title follows it; an explicit literal stays');

  const follow = fn(source, 'void MD3DialogCaption::FollowDialogTitle()');
  assert.match(follow, /if \(m_dialog == nullptr \|\| m_dialog->IsBeingDeleted\(\)\)\s*return;/);
  assert.match(follow, /const wxString title = m_dialog->GetTitle\(\);\s*if \(title == m_applied_title\)\s*return;\s*m_applied_title = title;/);
  assert.match(follow, /^\s*SetName\(title\);/m, 'screen readers hear the new purpose');
  assert.match(follow, /^\s*m_title->SetLabelText\(title\);/m, "'&' stays literal, as in the constructor");
  assert.match(follow, /^\s*Layout\(\);/m);

  const sync = fn(source, 'void MD3DialogCaption::SyncTitle(wxDialog *dialog)');
  assert.match(sync, /for \(wxWindow \*child : dialog->GetChildren\(\)\)\s*if \(auto \*caption = dynamic_cast<MD3DialogCaption \*>\(child\)\)\s*caption->FollowDialogTitle\(\);/);
});

test('the Load-to-nozzle dialog shows the title it sets, from the first paint and after every choice', async () => {
  const source = await read('Widgets', 'AMSItem.cpp');
  const ctor = fn(source, 'FeedDirectionDialog::FeedDirectionDialog(wxWindow* parent,');
  assert.match(ctor, /: wxDialog\(parent, wxID_ANY, _L\("Confirm"\), wxDefaultPosition, wxDefaultSize\),/,
    'the fallback title is the window title, so the strip and the window agree before any mapping is set');
  assert.match(ctor, /^\s*MD3DialogCaption::Adopt\(this\);/m, 'adopted from the dialog title, so the caption follows it');
  assert.doesNotMatch(ctor, /MD3DialogCaption::Adopt\(this, _L\("Confirm"\)\);/);

  for (const signature of ['void FeedDirectionDialog::OnRadioClicked(wxCommandEvent& evt)', 'void FeedDirectionDialog::SetExtruderMapping(MachineObject* obj,']) {
    const body = fn(source, signature);
    const titles = body.match(/^\s*SetTitle\(/gm) ?? [];
    const syncs = body.match(/^\s*MD3DialogCaption::SyncTitle\(this\);/gm) ?? [];
    assert.ok(titles.length > 0, `${signature} sets the title`);
    assert.equal(syncs.length, titles.length, `${signature}: every SetTitle is followed by SyncTitle`);
    for (const match of body.matchAll(/SetTitle\([^\n]*\n(\s*[^\n]*\n)?/g))
      assert.match(match[0], /MD3DialogCaption::SyncTitle\(this\);/, 'the caption updates in the same paint as the title');
  }
});
