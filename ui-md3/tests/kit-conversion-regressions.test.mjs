import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Behaviour the stock controls had and the kit conversion must keep. Each
// contract names the stock behaviour, the kit difference that lost it, and the
// call-site rule that restores it without going back to the stock control.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const guiDir = path.resolve(testDir, '..', '..', 'src', 'slic3r', 'GUI');
// Comments go; string literals stay, for the checks that quote a label.
const code = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const read = async (...parts) => code(await readFile(path.join(guiDir, ...parts), 'utf8'));
// A function body: from its signature to the brace that closes it.
const fn = (source, signature) => {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, signature);
  const open = source.indexOf('{', start);
  let depth = 0;
  for (let i = open; i < source.length; ++i) {
    if (source[i] === '{') ++depth;
    if (source[i] === '}' && --depth === 0) return source.slice(start, i + 1);
  }
  assert.fail(signature);
};

test('re-selecting the current schedule value source keeps the consent box', async () => {
  // wxRadioBox sent wxEVT_RADIOBOX only when the selection changed. A kit
  // LabeledRadioButton emits wxEVT_RADIOBUTTON on every activation, the
  // selected row too, so a handler that withdrew consent on every event cleared
  // it when the current source was clicked again. Consent goes only when the
  // source really changes, against the source the panel last showed.
  const panel = await read('ScheduledSettings', 'Panel.cpp');
  const header = await read('ScheduledSettings', 'Panel.hpp');
  assert.match(header, /int m_source_kind=0;/, 'the panel remembers the source it shows');
  assert.match(panel,
    /m_source\.Get\(i\)->Bind\(wxEVT_RADIOBUTTON,\[this,i\]\(wxCommandEvent& event\)\{if\(i!=m_source_kind\)\{m_source_kind=i;m_consent->SetValue\(false\);\}event\.Skip\(\);\}\);/,
    'consent is withdrawn only when another source row is chosen');
  assert.doesNotMatch(panel, /wxEVT_RADIOBUTTON,\[this\]\(wxCommandEvent& event\)\{m_consent->SetValue\(false\)/,
    'no row handler clears consent unconditionally');
  // Every programmatic selection updates the remembered source, so the next
  // user choice is compared with what is on screen.
  const selections = [...panel.matchAll(/m_source\.SetSelection\(([^;]+)\);/g)];
  assert.ok(selections.length >= 2, 'the constructor and load_editor select a source');
  for (const selection of selections) {
    const after = panel.slice(selection.index + selection[0].length);
    assert.ok(after.startsWith(`m_source_kind=${selection[1]};`), `m_source.SetSelection(${selection[1]}) is followed by m_source_kind=${selection[1]}`);
  }
});

test('reloading the version history store filter resets the shown store with the list', async () => {
  // wxChoice::Clear() emptied the shown value with the list. The kit ComboBox
  // clears its rows and selection but keeps the shown text, so after a reload
  // with no stores the field still named the old store while
  // GetStringSelection(), which the filter chip reads, was empty.
  const dialog = await read('ProjectHistoryDialog.cpp');
  const clears = [...dialog.matchAll(/m_store_filter->Clear\(\);/g)];
  assert.ok(clears.length >= 1, 'the store filter is rebuilt on reload');
  for (const clear of clears)
    assert.ok(dialog.slice(clear.index + clear[0].length).trimStart().startsWith('m_store_filter->SetValue(wxEmptyString);'),
      'every Clear() of the store filter also resets its shown value');
});

test('the camera zoom percentage opens the Material menu and names the current zoom', async () => {
  // The stock wxButton's text was its accessible name, so a screen reader heard
  // the zoom ("125%"). The kit Button prefers a SetName() name, which was the
  // constant "Camera zoom percentage". One helper now sets the label and a name
  // that carries it, from the existing catalogue strings.
  const hud = await read('Widgets', 'CameraHUD.cpp');
  const helper = fn(hud, 'void show_zoom_percent(Button *chip, int percent)');
  assert.match(helper, /const wxString label = wxString::Format\("%d%%", percent\);/);
  assert.match(helper, /chip->SetLabel\(label\);/);
  assert.match(helper, /chip->SetName\(wxString::Format\(_L\("%s, %s"\), _L\("Camera zoom percentage"\), label\)\);/,
    'the accessible name carries the shown percentage');
  const outside = hud.replace(helper, '');
  assert.doesNotMatch(outside, /m_zoom_percent->Set(?:Label|Name)\(/, 'the label and name change only together, in the helper');
  assert.match(fn(hud, 'CameraHUD::CameraHUD('), /show_zoom_percent\(m_zoom_percent, 100\);/);
  assert.match(fn(hud, 'void CameraHUD::AttachMedia('),
    /show_zoom_percent\(m_zoom_percent, int\(static_cast<wxMediaCtrl3 \*>\(media\)->GetZoom\(\) \* 100 \+ \.5\)\);/);
  assert.match(fn(hud, 'void CameraHUD::on_view_changed('), /show_zoom_percent\(m_zoom_percent, evt\.GetInt\(\)\);/);
  // The menu is the Material menu named by context-menus.test.mjs; the wxMenu
  // only carries its rows.
  const ctor = fn(hud, 'CameraHUD::CameraHUD(');
  assert.match(ctor, /MD3::PopupMenuBelow\(m_zoom_percent, &menu\);/);
  assert.doesNotMatch(hud, /m_zoom_percent->PopupMenu\(/);
});
