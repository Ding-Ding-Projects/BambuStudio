import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// md3-v231 crashed on every close of the main window. Deleting MainFrame
// destroys its diff_dialog member, whose LabeledCheckBox holds a CheckBox.
// After ~CheckBox had finished, ~wxWindowMSW called ::DestroyWindow on the
// CheckBox window. Windows hides a still-visible child window first and sends
// it WM_SHOWWINDOW(FALSE); wx turns that into wxEVT_SHOW and dispatched it to
// the [this] lambda the CheckBox constructor had bound. The lambda called
// settleSelection() and update() on an object whose CheckBox part no longer
// existed. update()'s virtual GetValue() went through wxWindow's vtable into an
// unrelated slot (wxWindowBase::Destroy), which deleted the object again, 30
// levels deep, until renderBitmap() read a freed vtable pointer.
//
// The same shape exists in every child widget that settles a motion on hide:
// a destroyed MD3::Motion::Anim (a wxTimer) is stopped from inside
// ::DestroyWindow. The OS sends WM_SHOWWINDOW from DestroyWindow only to child
// windows, so the rule pinned here is for child widgets: the show handler is a
// member function, the destructor unbinds it before anything else, and the
// handler itself does nothing once the window is being deleted.
//
// The build cannot run the GUI here, so these checks pin the source.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = path.join(repoDir, 'src', 'slic3r', 'GUI');
const read = async (name) => (await readFile(path.join(gui, name), 'utf8')).replace(/\r\n/g, '\n');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const source = async (name) => stripComments(await read(name));

// Brace-balanced block that starts at the first `{` after `start`.
const blockAfter = (code, start, what) => {
  assert.notEqual(start, -1, `missing ${what}`);
  const open = code.indexOf('{', start);
  assert.notEqual(open, -1, `no body for ${what}`);
  let depth = 0;
  for (let i = open; i < code.length; ++i) {
    if (code[i] === '{') ++depth;
    else if (code[i] === '}' && --depth === 0) return code.slice(open, i + 1);
  }
  assert.fail(`unterminated ${what}`);
};
const block = (code, signature) => blockAfter(code, code.indexOf(signature), signature);
const classBody = (code, name) => {
  const at = code.search(new RegExp(`class\\s+${name}\\b[^;{]*\\{`));
  return blockAfter(code, at, `class ${name}`);
};
const escape = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

// `owner` is the text that holds the constructor's Bind calls, `dtor` the
// destructor body and `handler` returns the show handler body. `stop` matches
// the first call that touches state the derived destructor has destroyed.
const checkChildWidget = ({ cls, owner, dtor, handler, bindVia = '', stop }) => {
  const bind = new RegExp(`${escape(bindVia)}Bind\\(\\s*wxEVT_SHOW\\s*,\\s*&${cls}::(\\w+)\\s*,\\s*this\\s*\\)`);
  assert.doesNotMatch(owner, /Bind\(\s*wxEVT_SHOW\s*,\s*\[/, `${cls} binds wxEVT_SHOW to a lambda, which no destructor can unbind`);
  const bound = owner.match(bind);
  assert.ok(bound, `${cls} binds wxEVT_SHOW to a member handler`);

  const unbind = dtor.search(new RegExp(`${escape(bindVia)}Unbind\\(\\s*wxEVT_SHOW\\s*,\\s*&${cls}::${bound[1]}\\s*,\\s*this\\s*\\)`));
  assert.notEqual(unbind, -1, `~${cls} unbinds the show handler`);
  const firstStatement = dtor.slice(1).trim();
  assert.match(firstStatement, /^(?:wxWindow::)?Unbind\(/, `~${cls} unbinds before it stops or destroys anything`);
  const stopAt = dtor.search(stop);
  if (stopAt !== -1) assert.ok(unbind < stopAt, `~${cls} unbinds before it stops its motion`);

  const body = handler();
  assert.match(body, /\.Skip\(\)/, `${cls}'s show handler still lets the event through`);
  const guard = body.search(/IsBeingDeleted\(\)/);
  assert.notEqual(guard, -1, `${cls}'s show handler checks IsBeingDeleted()`);
  const firstUse = body.search(stop);
  assert.notEqual(firstUse, -1, `${cls}'s show handler still settles on hide`);
  assert.ok(guard < firstUse, `${cls} checks IsBeingDeleted() before it touches its own members`);
  return bound[1];
};

test('CheckBox: wxEVT_SHOW cannot reach settleSelection() once ~CheckBox has started', async () => {
  const hpp = await source('Widgets/CheckBox.hpp');
  const cpp = await source('Widgets/CheckBox.cpp');
  const handlerName = checkChildWidget({
    cls: 'CheckBox',
    owner: block(cpp, 'CheckBox::CheckBox(wxWindow *parent, int id)'),
    dtor: block(hpp, '~CheckBox() override'),
    handler: () => block(cpp, 'void CheckBox::onShow(wxShowEvent &e)'),
    stop: /settleSelection\(\)|m_selection_motion\.Stop\(\)/,
  });
  assert.match(hpp, new RegExp(`void ${handlerName}\\(wxShowEvent &e\\);`));
  assert.doesNotMatch(cpp, /wxEVT_SHOW\s*,\s*\[/);
});

test('Slider: neither a hide nor a lost capture reaches settleHalo() once ~Slider has started', async () => {
  const hpp = await source('Widgets/Slider.hpp');
  const cpp = await source('Widgets/Slider.cpp');
  const create = block(cpp, 'bool Slider::Create(');
  const dtor = block(hpp, '~Slider() override');
  checkChildWidget({
    cls: 'Slider',
    owner: create,
    dtor,
    handler: () => block(cpp, 'void Slider::onShow(wxShowEvent &e)'),
    stop: /settleHalo\(\)|m_halo_motion\.Stop\(\)/,
  });
  // ::DestroyWindow also releases a held mouse capture, and wx reports that as
  // wxEVT_MOUSE_CAPTURE_LOST to the window being destroyed.
  assert.doesNotMatch(create, /Bind\(\s*wxEVT_MOUSE_CAPTURE_LOST\s*,\s*\[/);
  assert.match(create, /Bind\(\s*wxEVT_MOUSE_CAPTURE_LOST\s*,\s*&Slider::onCaptureLost\s*,\s*this\s*\)/);
  const unbindLost = dtor.search(/Unbind\(\s*wxEVT_MOUSE_CAPTURE_LOST\s*,\s*&Slider::onCaptureLost\s*,\s*this\s*\)/);
  assert.notEqual(unbindLost, -1, '~Slider unbinds the capture-lost handler');
  assert.ok(unbindLost < dtor.search(/m_halo_motion\.Stop\(\)/), '~Slider unbinds before it stops its motion');
  const lost = block(cpp, 'void Slider::onCaptureLost(wxMouseCaptureLostEvent &');
  assert.ok(lost.search(/IsBeingDeleted\(\)/) !== -1 && lost.search(/IsBeingDeleted\(\)/) < lost.search(/settleHalo\(\)/));
});

test('the Plater filament disclosure header does not settle a destroyed motion', async () => {
  const plater = await source('Plater.cpp');
  const cls = classBody(plater, 'FilamentDisclosureHeader');
  checkChildWidget({
    cls: 'FilamentDisclosureHeader',
    owner: block(cls, 'explicit FilamentDisclosureHeader(wxWindow *parent)'),
    dtor: block(cls, '~FilamentDisclosureHeader() override'),
    handler: () => block(cls, 'void onShow(wxShowEvent &event)'),
    stop: /settle\(\)|m_motion\.Stop\(\)/,
  });
});

test('the device connection disclosure banner does not settle a destroyed motion', async () => {
  const sideTools = await source('Widgets/SideTools.cpp');
  const cls = classBody(sideTools, 'ConnectionDisclosureBanner');
  checkChildWidget({
    cls: 'ConnectionDisclosureBanner',
    owner: block(cls, 'explicit ConnectionDisclosureBanner(wxWindow *parent)'),
    dtor: block(cls, '~ConnectionDisclosureBanner() override'),
    handler: () => block(cls, 'void onShow(wxShowEvent &event)'),
    stop: /settle\(\)|m_motion\.Stop\(\)/,
  });
});

test('the fan motion view does not stop its destroyed timer base on hide', async () => {
  // FanMotionView privately derives from wxTimer after wxWindow, so the timer
  // base is destroyed before ~wxWindowMSW sends the hide.
  const hpp = await source('Widgets/FanControl.hpp');
  const cpp = await source('Widgets/FanControl.cpp');
  checkChildWidget({
    cls: 'FanMotionView',
    owner: block(cpp, 'FanMotionView::FanMotionView('),
    dtor: block(classBody(hpp, 'FanMotionView'), '~FanMotionView() override'),
    handler: () => block(cpp, 'void FanMotionView::OnShow(wxShowEvent& event)'),
    bindVia: 'wxWindow::',
    stop: /\bStop\(\)|UpdateTimer\(\)/,
  });
});
