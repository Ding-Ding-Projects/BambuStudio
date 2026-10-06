import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

// Execute expressions and the layout helper extracted from production sources.
// This is a source-derived geometry/lifecycle model, not a native window test.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => readFileSync(path.join(root, 'src/slic3r/GUI', file), 'utf8');
const media = read('MediaPlayCtrl.cpp');
const status = read('StatusPanel.cpp');
const button = read('Widgets/Button.cpp');
const body = (source, signature) => {
  const start = source.indexOf(signature);
  assert.ok(start >= 0, `Missing production function: ${signature}`);
  const open = source.indexOf('{', start);
  let depth = 1;
  for (let i = open + 1; i < source.length; ++i) {
    if (source[i] === '{') ++depth;
    if (source[i] === '}' && --depth === 0) return source.slice(open + 1, i);
  }
  assert.fail('Unclosed production function');
};

test('camera icon uses one DPI conversion and fits the actual footer with margins', () => {
  const argument = media.match(/m_button_play->SetIconButton\(Button::IconShape::Square, (.+)\);/)[1];
  const margin = Number(media.match(/sizer->Add\(m_button_play[^;]*FromDIP\((\d+)\)/)[1]);
  const footer = Number(status.match(/new MediaPlayCtrl\([^;]*FromDIP\((\d+)\)/)[1]);
  const widthExpression = button.match(/const int wpx = ([^;]+);/)[1]
    .replace('m_icon_shape == IconShape::Square', 'true');
  assert.match(button, /minSize\s*= wxSize\(FromDIP\(wpx\), FromDIP\(container\)\)/);
  for (const scale of [1, 1.25, 1.5, 2]) {
    const FromDIP = n => Math.round(n * scale);
    const container = new Function('FromDIP', `return ${argument};`)(FromDIP);
    const width = new Function('container', `return ${widthExpression};`)(container);
    assert.equal(FromDIP(width), FromDIP(36));
    assert.equal(FromDIP(container), FromDIP(32));
    assert.ok(FromDIP(container) + 2 * FromDIP(margin) <= FromDIP(footer));
  }
});

test('production title helper remeasures after font refresh and overwrites stale minimums', () => {
  const source = body(status, 'static void layout_printing_title(')
    .replace(/\/\/[^\n]*/g, '').replace(/->/g, '.')
    .replace(/Label::Head_16/g, 'headingFont').replace(/std::max/g, 'Math.max')
    .replace(/\bconst int\b/g, 'const').replace(/wxSize\(/g, 'size(');
  const layout = new Function('panel', 'label', 'headingFont', 'size', source);
  let scale = 1;
  let textHeight = 20;
  let measured = false;
  let fontSet = false;
  const sizer = { minimum: null, SetMinSize(value) { this.minimum = value; } };
  const label = {
    SetFont() { fontSet = true; measured = false; },
    InvalidateBestSize() { assert.ok(fontSet); measured = true; },
    GetBestSize() { assert.ok(measured); return { y: textHeight }; },
  };
  const panel = {
    minimum: null, layouts: 0, invalidations: 0,
    FromDIP: n => Math.round(n * scale), GetSizer: () => sizer,
    SetMinSize(value) { this.minimum = value; },
    InvalidateBestSize() { ++this.invalidations; }, Layout() { ++this.layouts; },
  };
  for (const [nextScale, nextText] of [[1, 20], [2, 76], [1.25, 48], [1, 20]]) {
    scale = nextScale; textHeight = nextText; fontSet = false;
    layout(panel, label, 'heading', (x, y) => ({ x, y }));
    const expected = Math.max(Math.round(40 * scale), textHeight + Math.round(16 * scale));
    assert.deepEqual(sizer.minimum, { x: -1, y: expected });
    assert.deepEqual(panel.minimum, sizer.minimum);
  }
  assert.equal(panel.layouts, 4);
  assert.equal(panel.invalidations, 4);
});

test('construction and DPI lifecycle share the measured title helper', () => {
  for (const signature of ['void PrintingTaskPanel::create_panel(', 'void PrintingTaskPanel::msw_rescale()']) {
    assert.match(body(status, signature), /layout_printing_title\(m_panel_printing_title, m_staticText_printing\)/);
  }
  const rescale = body(status, 'void PrintingTaskPanel::msw_rescale()');
  assert.doesNotMatch(rescale, /m_panel_printing_title->SetSize/);
  assert.match(rescale, /InvalidateBestSize\(\);\s*Layout\(\);/);
});

test('Control heading measures its whole sizer and clears stale DPI floors', () => {
  const rescale = body(status, 'void StatusPanel::msw_rescale()');
  assert.doesNotMatch(rescale, /FromDIP\(PAGE_TITLE_HEIGHT\)/);
  const source = body(status, 'static void layout_control_title(');
  assert.match(source, /label->SetFont\(Label::Head_16\);\s*label->InvalidateBestSize\(\)/);
  assert.match(source, /sizer->SetMinSize\(wxDefaultSize\)/);
  assert.match(source, /sizer->GetItem\(label\)->SetBorder\(panel->FromDIP\(8\)\)/);
  const expression = source.match(/const int height = ([^;]+);/)[1]
    .replace(/std::max/g, 'Math.max').replace(/panel->FromDIP/g, 'FromDIP')
    .replace(/sizer->CalcMin\(\).y/g, 'measuredHeight');
  const heightAt = new Function('FromDIP', 'measuredHeight', `return ${expression};`);
  for (const scale of [1, 2, 1.25, 1]) {
    const fromDIP = n => Math.round(n * scale);
    for (const measuredHeight of [fromDIP(24), fromDIP(76)])
      assert.equal(heightAt(fromDIP, measuredHeight), Math.max(fromDIP(40), measuredHeight));
  }
  assert.match(source, /panel->SetMinSize\(wxSize\(-1, height\)\)/);
  assert.match(source, /panel->InvalidateBestSize\(\);\s*panel->Layout\(\)/);
  assert.match(rescale, /layout_control_title\(m_panel_control_title, m_staticText_control\)/);
  assert.match(body(status, 'wxBoxSizer *StatusBasePanel::create_machine_control_page('),
    /layout_control_title\(m_panel_control_title, m_staticText_control\)/);
});
