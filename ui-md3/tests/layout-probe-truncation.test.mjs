import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// A dialog action drawn as "Left..." went unreported because the layout probe
// measured labels only on wxStaticText and wxControl, and the kit Button is
// neither (clipping inventory CJ-014). These contracts keep the kit Button
// recording every paint that shortens its label, and the probe reporting it.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFile(path.join(repoDir, ...parts), 'utf8');

const stripComments = (source) => source
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^[ \t]*\/\/.*$/gm, '');

const buttonCpp = await read('src', 'slic3r', 'GUI', 'Widgets', 'Button.cpp');
const buttonHpp = await read('src', 'slic3r', 'GUI', 'Widgets', 'Button.hpp');
const probeCpp = await read('src', 'slic3r', 'GUI', 'LayoutProbe.cpp');

test('the kit Button records every paint that shortens its label', () => {
  const render = buttonCpp.match(/void Button::render\(wxDC& dc\)[\s\S]*?\n\}/);
  assert.ok(render, 'Button::render must exist');
  const code = stripComments(render[0]);
  assert.match(code, /m_label_truncated = false;/, 'each paint starts from "not shortened"');
  const ellipsizeCalls = (code.match(/wxControl::Ellipsize\(/g) || []).length;
  const flagSets = (code.match(/m_label_truncated = true;/g) || []).length;
  assert.ok(ellipsizeCalls > 0, 'render() shortens labels with wxControl::Ellipsize');
  assert.equal(flagSets, ellipsizeCalls, 'every Ellipsize path records the shortening');
  assert.match(buttonHpp, /bool LabelTruncated\(\) const \{ return m_label_truncated; \}/);
  assert.match(buttonHpp, /bool AllowsShrink\(\) const \{ return m_allow_shrink; \}/);
});

test('the layout probe reports a shortened kit Button label', () => {
  const writer = probeCpp.match(/void write_window\([\s\S]*?\n\}/);
  assert.ok(writer, 'write_window must exist');
  const code = stripComments(writer[0]);
  assert.match(code, /dynamic_cast<::Button \*>\(w\)/, 'kit Buttons are measured, not skipped');
  assert.match(code, /truncated = shown && btn->LabelTruncated\(\);/);
  assert.match(code, /text_clipped = truncated && !ellipsized;/, 'shrinking is by design only where the button allows it');
  assert.match(code, /\\"truncated\\":/, 'every record carries the truncated field');
  assert.match(code, /\\"type\\":" << json\(type_name_of\(w\)\)/, 'every record names its C++ type');
});

test('a top-level window is never reported as clipped by its parent', () => {
  // A dialog's rectangle is in screen coordinates and it is its own native
  // window, so comparing it with the main frame's client area flagged every
  // dialog placed away from the frame's top-left corner (Setup Wizard, AI ink
  // scanner) as clipped.
  const writer = probeCpp.match(/void write_window\([\s\S]*?\n\}/);
  assert.ok(writer, 'write_window must exist');
  const code = stripComments(writer[0]);
  assert.match(code, /if \(parent && !w->IsTopLevel\(\)\) \{\s*const wxRect parent_client/, 'only child windows are compared with the parent client area');
});
