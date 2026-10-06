import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Execute the arithmetic and decisions extracted from the production C++ bodies.
// This is a source-derived model, not a compiled native or hardware test.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => readFileSync(path.join(root, file), 'utf8');
const model = read('src/slic3r/GUI/DeviceCore/DevFilaSystem.cpp');
const panel = read('src/slic3r/GUI/StatusPanel.cpp');
const definitions = read('src/slic3r/GUI/DeviceCore/DevDefs.h');
const types = Object.fromEntries([...definitions.matchAll(/^\s*(EXT_SPOOL|AMS|AMS_LITE|N3F|N3S|AMS_LITE_MIXED)\s*=\s*(\d+)/gm)]
  .map(([, name, value]) => [name, Number(value)]));
assert.equal(Object.keys(types).length, 6);

function body(source, marker) {
  const start = source.indexOf(marker);
  assert.ok(start >= 0, `Production function missing: ${marker}`);
  const open = source.indexOf('{', start);
  let depth = 1;
  for (let i = open + 1; i < source.length; ++i) {
    if (source[i] === '{') ++depth;
    if (source[i] === '}' && --depth === 0) return source.slice(open + 1, i);
  }
  assert.fail(`Unclosed production function: ${marker}`);
}

function translate(source) {
  return source.replace(/\/\/[^\n]*/g, '')
    .replace(/DevAmsType::/g, 'types.')
    .replace(/std::numeric_limits<std::uint32_t>::digits/g, '32')
    .replace(/\bconst\s+(?:auto\s*&?|long long|std::int64_t|int)\s+/g, 'const ')
    .replace(/\b(?:auto|long long|std::int64_t|int)\s+/g, 'let ')
    .replace(/tray->/g, 'tray.');
}

const bitBody = translate(body(model, 'static long long sGetAmsFlagBit('));
const bitIndex = new Function('tray', 'types', bitBody);
const readBody = translate(body(model, 'bool DevAmsTray::is_reading('))
  .replace(/sGetAmsFlagBit\(this\)/g, 'bitIndex(tray, types)')
  .replace(/DevUtil::get_flag_bits/g, 'flagBits');
const readFlag = new Function('tray', 'tray_reading_bits', 'types', 'bitIndex', 'flagBits', readBody);

function flagBits(bits, start) {
  assert.ok(Number.isInteger(start) && start >= 0 && start < 32,
    `The 32-bit telemetry field cannot be shifted by ${start}`);
  return Number((BigInt.asUintN(32, BigInt(bits)) >> BigInt(start)) & 1n);
}

function tray(type, unit, slot) {
  const result = { ams_type: types[type] ?? type,
    get_ams_slot_id: () => ({ first: unit, second: slot }) };
  result.is_reading = bits => Boolean(readFlag(result, bits, types, bitIndex, flagBits));
  return result;
}

// Extract the actual native tray-loop decision, including the historical branch
// so that the same tests can be run before the repair, not just on a hand model.
const loop = body(panel, 'for (auto tray_it = ams_it->second->GetTrays().begin();');
const nativeBody = translate(loop)
  .replace(/std::string\s+tray_id\s*=\s*tray_it->first;/, '')
  .replace(/let\s+tray_id_int\s*=\s*atoi\(tray_id\.c_str\(\)\);/, '')
  .replace(/ams_it->second->GetAmsType\(\)/g, 'amsType')
  .replace(/ams_it->second->IsAmsLiteMixed\(\)/g, 'mixed')
  .replace(/tray_it->second->/g, 'tray.')
  .replace(/tray_it->second/g, 'tray')
  .replace(/obj->tray_reading_bits/g, 'bits')
  .replace(/m_ams_control->StopRridLoading\(ams_id, tray_id\);/g, 'return false;')
  .replace(/m_ams_control->PlayRridLoading\(ams_id, tray_id\);/g, 'return true;');
const nativeDecision = new Function('tray', 'bits', 'ams_id_int', 'tray_id_int', 'amsType', 'mixed', 'types', nativeBody);
function native(type, unit, slot, bits) {
  const mixed = type === 'AMS_LITE_MIXED';
  return nativeDecision(tray(type, unit, slot), bits, unit, slot,
    mixed ? types.AMS_LITE : types[type], mixed, types);
}

test('mixed Lite reads its own high bit without requiring an ordinary AMS bit', () => {
  for (let slot = 0; slot < 4; ++slot) {
    assert.equal(native('AMS_LITE_MIXED', 0, slot, 2 ** (24 + slot)), true);
    assert.equal(native('AMS_LITE_MIXED', 0, slot, 2 ** slot), false);
  }
});

test('ordinary AMS, Lite and 2 Pro trays remain independent', () => {
  for (const type of ['AMS', 'AMS_LITE', 'N3F']) {
    for (let unit = 0; unit < 4; ++unit) {
      for (let slot = 0; slot < 4; ++slot) {
        for (let active = 0; active < 4; ++active)
          assert.equal(native(type, unit, slot, 2 ** (unit * 4 + active)), slot === active);
      }
    }
  }
});

test('single-slot HT keeps consecutive unit bits, not the obsolete four-bit stride', () => {
  assert.equal(native('N3S', 128, 0, 2 ** 16), true);
  assert.equal(native('N3S', 129, 0, 2 ** 17), true);
  assert.equal(native('N3S', 129, 0, 2 ** 20), false);
});

test('idle telemetry stops every supported animation', () => {
  for (const type of ['AMS', 'AMS_LITE', 'N3F', 'AMS_LITE_MIXED', 'N3S'])
    assert.equal(native(type, type === 'N3S' ? 128 : 0, 0, 0), false);
});

test('canonical reader bounds invalid indices before unsigned shifts', () => {
  for (const [type, unit, slot] of [
    ['AMS', -1, 0], ['AMS', 0, -1], ['AMS', 0, 4], ['AMS', 8, 0],
    ['AMS', 2147483647, 0], ['N3S', 127, 0], ['N3S', 128, 1],
    ['N3S', 144, 0], ['AMS_LITE_MIXED', 0, 4], ['EXT_SPOOL', 0, 0], [99, 0, 0],
  ]) {
    assert.equal(tray(type, unit, slot).is_reading(0x7fffffff), false, `${type}/${unit}/${slot}`);
    assert.equal(native(type, unit, slot, 0x7fffffff), false, `native ${type}/${unit}/${slot}`);
  }
  // Bit 31 is the arithmetic boundary, not a claim that stol accepts unsigned
  // high-bit telemetry strings on Windows. Negative stored values retain it.
  assert.equal(tray('N3S', 143, 0).is_reading(-2147483648), true);
  assert.equal(tray('N3S', 143, 0).is_reading(0x7fffffff), false);
});

test('native and web displays share the canonical reader', () => {
  assert.equal(nativeDecision(null, 0x7fffffff, 0, 0, types.AMS, false, types), false);
  assert.match(loop, /tray_it->second->is_reading\(obj->tray_reading_bits\)/);
  assert.doesNotMatch(loop, /<<|check_flag|IsAmsLiteMixed/);
  assert.match(read('src/slic3r/GUI/DeviceWeb/ViewModels/DevicePage/AmsControlWeb/ViewModelDataBuilder.cpp'),
    /tray->is_reading\(machine_obj->tray_reading_bits\)/);
});
