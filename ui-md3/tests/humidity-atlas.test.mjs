import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

// Source-derived geometry and preservation checks, not native window evidence.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const baseline = '0f034f2fed3c2b913f4c308d0130cce9eeee763f';
const historical = (file, ref = baseline) => execFileSync('git', ['show', `${ref}:${file}`], { cwd: root, encoding: 'utf8', maxBuffer: 10e6 }).replace(/\r\n/g, '\n');
const read = file => process.env.ATLAS_SOURCE_REF ? historical(file, process.env.ATLAS_SOURCE_REF) : readFileSync(path.join(root, file), 'utf8').replace(/\r\n/g, '\n');
const classicPath = 'src/slic3r/GUI/AmsMappingPopup.cpp';
const percentPath = 'src/slic3r/GUI/DeviceTab/uiAmsHumidityPopup.cpp';
const classic = read(classicPath);
const percent = read(percentPath);
const body = (source, signature) => {
  const start = source.indexOf(signature);
  assert.ok(start >= 0, `Missing ${signature}`);
  const open = source.indexOf('{', start);
  let depth = 1;
  for (let i = open + 1; i < source.length; ++i) {
    if (source[i] === '{') ++depth;
    if (source[i] === '}' && --depth === 0) return source.slice(open + 1, i);
  }
  assert.fail(signature);
};

test('level and percentage telemetry assignments and formatting remain unchanged', () => {
  for (const signature of ['void AmsHumidityTipPopup::set_humidity_level('])
    assert.equal(body(classic, signature), body(historical(classicPath), signature));
  assert.equal(body(percent, 'void uiAmsPercentHumidityDryPopup::Update('), body(historical(percentPath), 'void uiAmsPercentHumidityDryPopup::Update('));
  const removeFinalLayout = s => s.replace(/^\s*(?:Fit|Layout|LayoutReadouts)\(\);/gm, '').replace(/\n(?:[ \t]*\n)+/g, '\n').trim();
  assert.equal(removeFinalLayout(body(percent, 'void uiAmsPercentHumidityDryPopup::UpdateContents()')),
    removeFinalLayout(body(historical(percentPath), 'void uiAmsPercentHumidityDryPopup::UpdateContents()')));
  assert.notEqual(removeFinalLayout(body(percent, 'void uiAmsPercentHumidityDryPopup::UpdateContents()').replace('m_humidity_percent', 'm_humidity_level')),
    removeFinalLayout(body(historical(percentPath), 'void uiAmsPercentHumidityDryPopup::UpdateContents()')));
});

test('native and embedded capability routing is byte-identical to the preserved baseline', () => {
  for (const file of ['src/slic3r/GUI/Widgets/AMSControl.cpp', 'src/slic3r/GUI/DeviceWeb/ViewModels/DevicePage/AmsControlWeb/ViewModelActions.cpp'])
    assert.equal(read(file), historical(file));
});

test('production legend geometry contains every ordered bitmap at supported widths and scales', () => {
  const columnsBody = body(classic, 'static int humidity_legend_columns(')
    .replace(/std::clamp/g, 'clamp').replace(/std::max/g, 'Math.max');
  // All operands are integers in C++; truncate before the clamp as C++ does.
  const columnsAt = new Function('width', 'icon_width', 'gap', 'clamp', columnsBody);
  const size = body(classic, 'void AmsHumidityLevelList::set_available_width(');
  const render = body(classic, 'void AmsHumidityLevelList::doRender(');
  const heightExpr = size.match(/const wxSize size\(width, ([^;]+)\);/)[1];
  const xExpr = render.match(/const int x = ([^;]+);/)[1].replace(/GetSize\(\).x/g, 'width');
  const yExpr = render.match(/const int y = ([^;]+);/)[1].replace(/\(i \/ columns\)/g, 'Math.floor(i / columns)');
  const heightAt = new Function('text_height', 'gap', 'rows', 'icon_height', `return ${heightExpr};`);
  const xAt = new Function('width', 'row_width', 'i', 'columns', 'icon_width', 'gap', `return Math.trunc(${xExpr});`);
  const yAt = new Function('text_height', 'gap', 'i', 'columns', 'icon_height', `return ${yExpr};`);
  for (const scale of [1, 1.25, 1.5, 2]) for (const logicalWidth of [220, 288, 400, 680]) for (const logicalGap of [7, 12]) {
    const width = Math.round(logicalWidth * scale), icon = Math.round(54 * scale), gap = Math.round(logicalGap * scale);
    const columns = columnsAt(width, icon, gap, (n, lo, hi) => Math.max(lo, Math.min(hi, Math.trunc(n))));
    const height = heightAt(Math.round(28 * scale), gap, Math.ceil(5 / columns), icon);
    for (let i = 0; i < 5; ++i) {
      const count = Math.min(columns, 5 - Math.floor(i / columns) * columns);
      const rowWidth = count * icon + (count - 1) * gap;
      const x = xAt(width, rowWidth, i, columns, icon, gap);
      const y = yAt(Math.round(28 * scale), gap, i, columns, icon);
      assert.ok(x >= 0 && x + icon <= width);
      assert.ok(y >= 0 && y + icon <= height);
    }
  }
  assert.match(render, /hum_level_img_dark\[i\].bmp\(\) : hum_level_img_light\[i\].bmp\(\)/);
  assert.match(size, /SetMinSize\(size\);\s*SetMaxSize\(size\);\s*SetSize\(size\)/);
});

test('both popups bound a real scroll owner and refresh geometry after content or DPI changes', () => {
  for (const [source, signature] of [[classic, 'void AmsHumidityTipPopup::layout_content()'], [percent, 'void uiAmsPercentHumidityDryPopup::LayoutReadouts()']]) {
    const layout = body(source, signature);
    assert.match(layout, /wxDisplay\(display\).GetClientArea\(\)/);
    assert.match(layout, /m_body->SetMinSize\(wxSize\(-1, 1\)\)/);
    assert.match(layout, /work.y - FromDIP\(32\)/);
    assert.match(layout, /SetClientSize\(width, height\);\s*Layout\(\);\s*m_body->FitInside\(\)/);
    assert.match(layout, /Wrap\(/);
  }
  const popup = body(classic, 'void AmsHumidityTipPopup::Popup(');
  assert.ok(popup.indexOf('layout_content();') < popup.indexOf('PopupWindow::Popup(focus)'));
  assert.match(body(classic, 'void AmsHumidityTipPopup::msw_rescale()'), /layout_content\(\)/);
  assert.match(body(percent, 'void uiAmsPercentHumidityDryPopup::UpdateContents()'), /LayoutReadouts\(\)/);
  assert.match(body(percent, 'void uiAmsPercentHumidityDryPopup::msw_rescale()'), /UpdateContents\(\)/);
  const create = body(percent, 'void uiAmsPercentHumidityDryPopup::Create()');
  assert.match(create, /m_body->SetSizer\(m_sizer\)/);
  assert.match(create, /root->Add\(m_body, 1, wxEXPAND\)/);
  const order = [...create.matchAll(/grid_sizer->Add\((\w+)/g)].map(m => m[1]);
  assert.deepEqual(order, ['m_humidity_header', 'm_humidity_label', 'm_temperature_header', 'm_temperature_label', 'left_dry_time_header', 'left_dry_time_label']);
});

test('actual Fit then Position then Popup sequence clamps the final measured rectangle', () => {
  const caller = read('src/slic3r/GUI/DeviceWeb/ViewModels/DevicePage/AmsControlWeb/ViewModelActions.cpp');
  const show = body(caller, 'void show_ams_level_humidity_tip(');
  assert.ok(show.indexOf('popup->Fit()') < show.indexOf('popup->Position('));
  assert.ok(show.indexOf('popup->Position(') < show.indexOf('popup->Popup()'));
  let source = body(classic, 'void AmsHumidityTipPopup::Popup(')
    .replace(/\/\/[^\n]*/g, '').replace(/const wx(?:Point|Size|Rect)\s+/g, 'const ')
    .replace(/const int\s+/g, 'const ').replace(/wxDisplay::GetFromWindow/g, 'displayFor')
    .replace(/wxDisplay\(display\)/g, 'displayObject(display)')
    .replace(/std::clamp/g, 'clamp').replace(/std::max/g, 'Math.max')
    .replace(/PopupWindow::Popup/g, 'showPopup');
  const execute = new Function('GetPosition', 'layout_content', 'GetParent', 'displayFor', 'displayObject',
    'GetSize', 'Move', 'showPopup', 'clamp', 'wxNOT_FOUND', 'focus', source);
  for (const area of [{x: 0, y: 0, width: 1000, height: 700}, {x: -1200, y: 80, width: 1200, height: 800}]) {
    area.GetRight = () => area.x + area.width - 1;
    area.GetBottom = () => area.y + area.height - 1;
    for (const request of [{x: area.x + area.width - 150, y: area.y + area.height - 80}, {x: area.x - 40, y: area.y - 30}]) {
      // Caller Fit observes the heading-only minimum before it positions the popup.
      let size = {x: 150, y: 80};
      let position = {...request};
      let shown = false;
      execute(() => ({...position}), () => { size = {x: 740, y: 600}; }, () => ({}), () => 0,
        () => ({GetClientArea: () => area}), () => size,
        (x, y) => { position = {x, y}; }, () => { shown = true; },
        (n, lo, hi) => Math.max(lo, Math.min(hi, n)), -1, null);
      assert.ok(shown);
      assert.ok(position.x >= area.x && position.y >= area.y);
      assert.ok(position.x + size.x <= area.x + area.width);
      assert.ok(position.y + size.y <= area.y + area.height);
    }
  }
});
