import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import test from 'node:test';

const base = '50715f4355e8b845042bd4809bac2da2ce5c40f4';
const prefix = 'src/slic3r/GUI/DeviceTab/';
const read = (name, ref = process.env.ATLAS_SOURCE_REF) => ref
  ? execFileSync('git', ['show', `${ref}:${prefix}${name}.cpp`], {encoding:'utf8'}).replace(/\r\n/g, '\n')
  : readFileSync(`${prefix}${name}.cpp`, 'utf8').replace(/\r\n/g, '\n');
const rack = read('wgtDeviceNozzleRack');
const update = read('wgtDeviceNozzleRackUpdate');
function body(source, name) {
  const start = source.indexOf(`void ${name}(`);
  assert.ok(start >= 0, name);
  const opening = source.indexOf('{', start);
  let depth = 1, end = opening + 1;
  while (depth && end < source.length) {
    if (source[end] === '{') depth++;
    if (source[end] === '}') depth--;
    end++;
  }
  return source.slice(opening + 1, end - 1);
}

test('tile minimum uses the production measured-content expressions at all display scales', () => {
  const measure = body(rack, 'wgtDeviceNozzleRackNozzleItem::MeasureCard');
  const expression = measure.match(/SetMinSize\(wxSize\((std::max[^;]+)\)\);/)[1];
  const calculate = new Function('FromDIP', 'content', `return [${expression.replaceAll('std::max', 'Math.max')}];`);
  for (const scale of [1, 1.25, 1.5, 2]) {
    for (const content of [{x:72, y:86}, {x:160, y:148}, {x:92, y:190}]) {
      const actual = {x:Math.ceil(content.x * scale), y:Math.ceil(content.y * scale)};
      const [width, height] = calculate(n => Math.round(n * scale), actual);
      assert.ok(width >= actual.x && height >= actual.y);
    }
  }
  assert.ok(measure.indexOf('SetMinSize(wxDefaultSize)') < measure.indexOf('CalcMin()'));
  assert.doesNotMatch(body(rack, 'wgtDeviceNozzleRackNozzleItem::CreateGui'), /SetMaxSize/);
  for (const method of ['CreateGui', 'SetNozzleStatus', 'Rescale'])
    assert.match(body(rack, `wgtDeviceNozzleRackNozzleItem::${method}`), /MeasureCard\(\)/);
});

test('status icon containers receive logical dimensions once', () => {
  for (const source of [rack, update]) {
    assert.match(source, /SetIconButton\(Button::IconShape::Circle, 20\)/);
    assert.doesNotMatch(source, /SetIconButton\([^;]+FromDIP/);
  }
});

test('update text columns have natural heights and refresh measurements after data and DPI updates', () => {
  const create = body(update, 'wgtDeviceNozzleRackHotendUpdate::CreateGui');
  for (const name of ['type_panel', 'info_panel']) {
    assert.doesNotMatch(create, new RegExp(`${name}->SetMaxSize`));
    assert.match(create, new RegExp(`${name}->SetMinSize\\(WX_DIP_SIZE\\(\\d+, -1\\)\\)`));
  }
  for (const method of ['UpdateInfo', 'Rescale'])
    assert.match(body(update, `wgtDeviceNozzleRackHotendUpdate::${method}`), /MeasureRow\(\)/);
  assert.match(body(update, 'wgtDeviceNozzleRackHotendUpdate::MeasureRow'), /InvalidateBestSize\(\)/);
});

test('hardware, mapping, identity and firmware decisions remain byte-identical', () => {
  const groups = [
    ['wgtDeviceNozzleRack', ['wgtDeviceNozzleRackNozzleItem::Update', 'wgtDeviceNozzleRackNozzleItem::EnableSelect', 'wgtDeviceNozzleRackNozzleItem::OnItemSelected', 'wgtDeviceNozzleRackNozzleItem::OnBtnNozzleStatus', 'wgtDeviceNozzleRackPos::OnMoveRackUp', 'wgtDeviceNozzleRackPos::OnMoveRackDown', 'wgtDeviceNozzleRackPos::OnBtnHomingRack', 'wgtDeviceNozzleRackArea::OnBtnReadAll']],
    ['wgtDeviceNozzleSelect', ['wgtDeviceNozzleRackSelect::UpdateNozzleInfos', 'wgtDeviceNozzleRackSelect::UpdatSelectedNozzle', 'wgtDeviceNozzleRackSelect::UpdatSelectedNozzles', 'wgtDeviceNozzleRackSelect::ClearSelection', 'wgtDeviceNozzleRackSelect::SetSelectedNozzle', 'wgtDeviceNozzleRackSelect::OnNozzleItemSelected']],
    ['wgtDeviceNozzleRackUpdate', ['wgtDeviceNozzleRackHotendUpdate::OnStatusIconClick', 'wgtDeviceNozzleRackHotendUpdate::UpdateExtruderNozzleInfo', 'wgtDeviceNozzleRackHotendUpdate::UpdateRackNozzleInfo', 'wgtDeviceNozzleRackHotendUpdate::UpdateInfo', 'wgtDeviceNozzleRackUprade::OnBtnReadAll']]
  ];
  for (const [file, methods] of groups) for (const method of methods) {
    const normalize = text => text.replace('    MeasureRow();\n', '');
    assert.equal(normalize(body(read(file), method)), normalize(body(read(file, base), method)), method);
  }
});
