import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// ScalableButton is the flat icon button behind 53 construction lines in 22
// files: the Prepare sidebar, the object list, the Device tab, the print dialogs
// and the calibration pages. It was a wxButton, so it drew only its bitmap: no
// hover wash, no focus ring, yet a real native button that took the Tab stop and
// answered Space and Enter with nothing to show for it. It is a kit Button now,
// defined at the end of Widgets/Button.hpp (Button derives from StaticBox, which
// includes wxExtensions.hpp, so the class cannot live in that header).

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const srcDir = path.join(repoDir, 'src');
const slic3rDir = path.join(srcDir, 'slic3r');
const gui = path.join(slic3rDir, 'GUI');
const BUTTON_HEADER = path.join(gui, 'Widgets', 'Button.hpp');
// The quoted-include search path of libslic3r_gui (src/slic3r/CMakeLists.txt):
// the including file's own folder first, then src/slic3r/Utils and src.
const includeDirs = [path.join(slic3rDir, 'Utils'), srcDir];

const read = (...parts) => readFile(path.join(gui, ...parts), 'utf8');
// Code only: comments and the contents of string literals removed.
const code = (text) => text.replace(/\r\n/g, '\n')
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/\/\/.*$/gm, '')
  .replace(/"(?:[^"\\\n]|\\.)*"/g, '""');
// One function or class body: from `signature` to the first line that starts with `}`.
const body = (text, signature) => {
  const start = text.indexOf(signature);
  assert.ok(start >= 0, `${signature} is defined`);
  const end = text.indexOf('\n}', start);
  return text.slice(start, end < 0 ? text.length : end + 2);
};

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp|h)$/.test(entry.name)) out.push(full);
  }
  return out;
}

const includeCache = new Map();
async function includesOf(file) {
  if (!includeCache.has(file)) {
    const text = (await readFile(file, 'utf8')).replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '');
    const found = [];
    for (const match of text.matchAll(/^\s*#\s*include\s*"([^"]+)"/gm)) {
      const target = [path.dirname(file), ...includeDirs].map((dir) => path.resolve(dir, match[1])).find((p) => existsSync(p));
      if (target) found.push(target);
    }
    includeCache.set(file, found);
  }
  return includeCache.get(file);
}

async function reaches(file, header) {
  const seen = new Set([file]);
  const queue = [file];
  while (queue.length) {
    for (const next of await includesOf(queue.shift())) {
      if (next === header) return true;
      if (!seen.has(next)) {
        seen.add(next);
        queue.push(next);
      }
    }
  }
  return false;
}

const rel = (file) => path.relative(gui, file).replaceAll('\\', '/');

test('no class under src/slic3r/GUI derives from wxButton, and the unreachable wxButton classes are gone', async () => {
  const hpp = code(await read('wxExtensions.hpp'));
  const cpp = code(await read('wxExtensions.cpp'));
  assert.doesNotMatch(hpp, /class ScalableButton\s*:\s*public wxButton/, 'ScalableButton is a kit Button, not a native button');
  for (const name of ['LockButton', 'ModeButton', 'ModeSizer']) {
    assert.doesNotMatch(hpp, new RegExp(`\\bclass\\s+${name}\\b\\s*[:{]`), `${name} is not declared in wxExtensions.hpp (nothing constructs it)`);
    assert.doesNotMatch(cpp, new RegExp(`\\b${name}::`), `${name} is not defined in wxExtensions.cpp`);
  }
  const offenders = [];
  for (const file of await sources(gui)) {
    const text = code(await readFile(file, 'utf8'));
    if (/\b(?:public|private|protected)\s+wxButton\b/.test(text)) offenders.push(rel(file));
  }
  assert.deepEqual(offenders, [], 'a class with a native button base keeps a native button on screen');
});

test('ScalableButton is defined once, after Button in Widgets/Button.hpp; wxExtensions.hpp only declares it', async () => {
  const button = code(await read('Widgets', 'Button.hpp'));
  const buttonAt = button.search(/class\s+Button\s*:\s*public\s+StaticBox\b/);
  const scalableAt = button.search(/class\s+ScalableButton\s*:\s*public\s+Button\b/);
  assert.ok(buttonAt >= 0, 'class Button is defined');
  assert.ok(scalableAt > buttonAt, 'class ScalableButton : public Button follows class Button');
  const ext = code(await read('wxExtensions.hpp'));
  assert.match(ext, /^class ScalableButton;/m, 'wxExtensions.hpp keeps the forward declaration for pointer members');
  assert.doesNotMatch(ext, /class\s+ScalableButton\s*:/, 'and no definition: Button derives from StaticBox, which includes wxExtensions.hpp');
  const definitions = [];
  for (const file of await sources(gui)) {
    const text = code(await readFile(file, 'utf8'));
    if (/\bclass\s+ScalableButton\s*:/.test(text)) definitions.push(rel(file));
  }
  assert.deepEqual(definitions, ['Widgets/Button.hpp'], 'one definition');
});

// Every file below holds a `new ScalableButton(` line. The class is complete only
// through Widgets/Button.hpp, so each has to reach it through its own includes; the
// forced precompiled header does not bring it in. Hand-written, so that a file
// dropped from the list is noticed; the discovery below catches a file added to it.
const CONSTRUCTING_FILES = [
  'AmsMappingPopup.cpp', 'CalibrationWizardPresetPage.cpp', 'CalibrationWizardPage.cpp',
  'GUI_ObjectTableSettings.cpp', 'GUI_ObjectTable.cpp', 'GUI_ObjectSettings.cpp',
  'DeviceTab/wgtDeviceNozzleRack.cpp', 'FilamentMapPanel.cpp', 'ParamsPanel.cpp',
  'PlateSettingsDialog.cpp', 'PlateMoveDialog.cpp', 'PrePrintChecker.cpp', 'Plater.cpp',
  'PresetComboBoxes.cpp', 'SelectMachine.cpp', 'Tab.cpp', 'SyncAmsInfoDialog.cpp',
  'SendMultiMachinePage.cpp', 'UnsavedChangesDialog.cpp', 'StatusPanel.cpp',
  'Widgets/SideTools.cpp', 'Widgets/StaticGroup.cpp',
];
// Classes that derive from ScalableButton and sit in a header that does not reach
// Widgets/Button.hpp yet. PlusMinusButton (GUI_ObjectLayers.hpp) is converted on its
// own; once that header reaches Button.hpp, or no longer derives from ScalableButton,
// the entry can go.
const DERIVING_WITHOUT_BUTTON_HEADER = new Set([
  'GUI_ObjectLayers.hpp',
]);

test('every source that constructs a ScalableButton reaches Widgets/Button.hpp', async () => {
  assert.equal(CONSTRUCTING_FILES.length, 22, 'the include-closure scan found 22 constructing files');
  const missing = [];
  for (const file of CONSTRUCTING_FILES) {
    const full = path.join(gui, file);
    const text = await readFile(full, 'utf8');
    assert.ok(text.includes('new ScalableButton('), `${file} constructs a ScalableButton`);
    if (!await reaches(full, BUTTON_HEADER)) missing.push(file);
  }
  assert.deepEqual(missing, [], 'these construct a ScalableButton with no include path to Widgets/Button.hpp');

  // Discovery: any other source that constructs, casts to or derives from one.
  const listed = new Set(CONSTRUCTING_FILES);
  const unreached = [];
  let users = 0;
  for (const file of await sources(slic3rDir)) {
    if (file === BUTTON_HEADER) continue;
    const text = code(await readFile(file, 'utf8'));
    const uses = /new\s+ScalableButton\s*\(|\bpublic\s+ScalableButton\b|dynamic_cast<\s*ScalableButton\b|static_cast<\s*ScalableButton\b/.test(text);
    if (!uses) continue;
    users += 1;
    const name = path.relative(gui, file).replaceAll('\\', '/');
    if (DERIVING_WITHOUT_BUTTON_HEADER.has(name)) continue;
    if (!await reaches(file, BUTTON_HEADER)) unreached.push(name);
    if (/new\s+ScalableButton\s*\(/.test(text) && !listed.has(name) && !['ModelMall.cpp', 'PhysicalPrinterDialog.cpp'].includes(name))
      unreached.push(`${name} (not in the hand-written list)`);
  }
  assert.ok(users >= 24, `expected the constructing, casting and deriving sources, found ${users}`);
  assert.deepEqual(unreached, [], 'a source that uses ScalableButton must reach Widgets/Button.hpp');
});

test('the new class keeps the public names of the old one, and the wxButton bitmap calls its callers make', async () => {
  const button = code(await read('Widgets', 'Button.hpp'));
  const cls = button.slice(button.search(/class\s+ScalableButton\s*:\s*public\s+Button\b/));
  for (const name of ['SetBitmap_', 'SetBitmapDisabled_', 'GetBitmapHeight', 'UseDefaultBitmapDisabled', 'msw_rescale', 'UpdateDarkUI'])
    assert.ok(cls.includes(name), `ScalableButton keeps ${name}`);
  // What GUI_ObjectSettings, GUI_ObjectTable, GUI_ObjectTableSettings, AmsMappingPopup,
  // the glyph buttons of Plater/SelectMachine and PlateMoveDialog call on an instance.
  for (const name of ['SetBitmap', 'GetBitmap', 'SetBitmapDisabled', 'SetBitmapFocus', 'SetBitmapCurrent', 'SetBitmapHover',
    'SetBitmapPressed', 'SetBitmapMargins', 'SetBitmapPosition'])
    assert.match(cls, new RegExp(`\\b${name}\\(`), `ScalableButton keeps the wxButton call ${name}`);
  assert.match(cls, /ScalableButton\(\s*wxWindow \*\s*parent,\s*wxWindowID\s*id,\s*const std::string&\s*icon_name/, 'the icon name constructor');
  assert.match(cls, /ScalableButton\(\s*wxWindow \*\s*parent,\s*wxWindowID\s*id,\s*const ScalableBitmap&\s*bitmap/, 'the bitmap constructor');

  const ext = code(await read('wxExtensions.cpp'));
  // The window style of a kit Button is a border style; the wxButton flags of the
  // callers are not forwarded to it.
  const baseInits = [...ext.matchAll(/^\s*Button\(parent, .*\),?$/gm)].map((m) => m[0].trim());
  assert.equal(baseInits.length, 2, 'both constructors build their Button base');
  for (const init of baseInits)
    assert.doesNotMatch(init, /style|wxBU_|wxNO_BORDER|wxBORDER/, `${init} must not forward a wxButton flag as the window style`);
  assert.doesNotMatch(ext, /\bButton::Create\(|\bSetWindowStyle(?:Flag)?\(/, 'the style flags are not applied to the window some other way');
  assert.match(body(ext, 'void ScalableButton::msw_rescale()'), /\bRescale\(\);/, 'a rescale re-derives the kit geometry and colours');
});

test('an empty label makes a flat icon button, a label makes a small outlined button', async () => {
  const ext = code(await read('wxExtensions.cpp'));
  const style = body(ext, 'void ScalableButton::init_style(');
  assert.match(style, /if \(label\.IsEmpty\(\)\) \{[\s\S]*SetIconButton\(container > 36 \? Button::IconShape::Square : Button::IconShape::Circle, container\);/,
    'icon only: the kit icon button, so the hover wash and focus ring exist');
  assert.match(style, /int container = m_px_cnt \+ 6;/, 'sized from the icon so layouts keep their footprint');
  assert.match(style, /if \(size\.x > 0 && size\.y > 0\)\s*container = std::max\(ToDIP\(size\.x\), ToDIP\(size\.y\)\);/, 'a size the caller gave wins');
  assert.match(style, /SetButtonSize\(Button::Size::Small\);\s*SetVariant\(Button::Variant::Outlined\);/, 'labelled: small outlined');
  assert.match(style, /if \(style & wxBU_LEFT\)\s*SetCenter\(false\);/, 'wxBU_LEFT is the one flag that is read');
  assert.doesNotMatch(style, /wxNO_BORDER|wxBU_EXACTFIT/, 'the border and exact-fit flags mean nothing to the kit button');
  // Icons are loaded by the class itself and handed over as ready bitmaps, so the kit
  // never rebuilds them at its own glyph size and msw_rescale reloads them by name.
  assert.match(body(ext, 'void ScalableButton::apply_bitmap('), /SetIconBitmap\(bitmap\);/);
  assert.match(body(ext, 'void ScalableButton::msw_rescale()'), /create_scaled_bitmap\(m_current_icon_name, m_parent, m_px_cnt\)/);
});

test('a disabled bitmap reaches the kit Button, which draws it only while disabled', async () => {
  const hpp = code(await read('Widgets', 'Button.hpp'));
  const cpp = code(await read('Widgets', 'Button.cpp'));
  assert.match(hpp, /void SetIconBitmapDisabled\(const wxBitmap& bitmap\);/, 'Button has the disabled slot');
  assert.match(hpp, /ScalableBitmap m_disabled_icon;/);
  assert.match(cpp, /^void Button::SetIconBitmapDisabled\(const wxBitmap &bitmap\)/m);
  assert.match(cpp, /if \(m_disabled_icon\.bmp\(\)\.IsOk\(\) && !IsEnabled\(\)\)\s*icon = m_disabled_icon;/,
    'render() swaps the icon only when a disabled bitmap exists and the button is disabled');
  // Every other Button path is untouched: the icon pick is still active when selected or hovered.
  assert.match(cpp, /if \(m_selected \|\| \(\(states & \(int\)StateColor::State::Hovered\) != 0\)\)\s*icon = active_icon;\s*else\s*icon = inactive_icon;/);
  const ext = code(await read('wxExtensions.cpp'));
  assert.match(body(ext, 'void ScalableButton::SetBitmapDisabled('), /SetIconBitmapDisabled\(bitmap\);/, 'SetBitmapDisabled reaches the slot');
  assert.match(body(ext, 'void ScalableButton::SetBitmapDisabled_('), /SetBitmapDisabled\(bmp\.bmp\(\)\);/, 'and so does SetBitmapDisabled_');
  assert.match(body(ext, 'void ScalableButton::update_disabled_bitmap('), /m_bitmap\.ConvertToDisabled\(\)/,
    'a bitmap without a caller supplied disabled one is disabled as the native button did it');
});

test('a colour or size a caller gave survives the kit restyle that a new bitmap triggers', async () => {
  const hpp = code(await read('Widgets', 'Button.hpp'));
  assert.match(hpp, /bool SetBackgroundColour\(const wxColour& colour\) override;/);
  assert.match(hpp, /void SetMinSize\(const wxSize& size\) override;/);
  const ext = code(await read('wxExtensions.cpp'));
  const colour = body(ext, 'bool ScalableButton::SetBackgroundColour(');
  assert.match(colour, /background_color\.setColorForStates\(colour, StateColor::Normal\)/,
    'the resting fill follows the surface the caller names');
  assert.match(colour, /if \(!m_restyling\)\s*m_backdrop = colour;/,
    'only a colour a caller named is kept; the kit passes the parent colour through here while it restyles');
  // Every place that makes the kit restyle marks it, and puts the caller's choices back afterwards.
  assert.match(body(ext, 'void ScalableButton::init_style('), /m_restyling = true;[\s\S]*m_restyling = false;/);
  assert.match(body(ext, 'void ScalableButton::apply_bitmap('), /m_restyling = true;\s*SetIconBitmap\(bitmap\);\s*m_restyling = false;\s*reassert_style\(\);/,
    'a new bitmap makes the kit restyle, then the remembered surface and minimum size are put back');
  assert.match(body(ext, 'void ScalableButton::msw_rescale()'), /m_restyling = true;\s*Rescale\(\);\s*m_restyling = false;[\s\S]*reassert_style\(\);/);
  const again = body(ext, 'void ScalableButton::reassert_style()');
  assert.match(again, /StateColor::isDarkMode\(\) \? StateColor::darkModeColorFor\(m_backdrop\) : StateColor::lightModeColorFor\(m_backdrop\)/,
    'the surface follows the theme as the old UpdateDarkUI mapping of the button colour did');
  assert.match(again, /Button::SetMinSize\(m_min_size\)/);
});

test('the wrapped-bitmap guard of ScalableBitmap::msw_rescale is kept', async () => {
  // A bitmap wrapped from a ready wxBitmap has no resource to reload (the glyph
  // buttons pass one through SetBitmap); md3-conversion-contracts pins the same line.
  assert.match(code(await read('wxExtensions.cpp')), /if \(m_icon_name\.empty\(\)\) return;/);
});
