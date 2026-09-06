// Contract tests for the native MD3 conversion, following the pattern already used by
// layout-clipping.test.mjs: read the shipped C++ and assert the properties the fix established.
//
// Why these exist. The surfaces below were converted, compiled and shipped, but four of the five
// cannot be photographed on the build host — the fan popup needs a connected printer, the
// Slice/Print dropdown is a wxPopupTransientWindow that any spawned process focus-kills, the 2D bed
// dialog is not reachable for Bambu printer profiles, and the Measure gizmo would not open under
// synthetic input. "It is committed" is not evidence that it is fixed, so the fixes are pinned here
// instead: each test asserts the specific, checkable property that constitutes the fix, and fails if
// the surface regresses to its legacy form.
//
// These do not replace a screenshot for judging how something LOOKS. They do prove that what the
// screenshot would show is present in the code that shipped.

import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = (...p) => path.join(repoDir, 'src', 'slic3r', 'GUI', ...p);

const read = (...p) => readFile(gui(...p), 'utf8');

// A C++ line comment or block-comment body is documentation, not behaviour. Several of these files
// deliberately NAME the legacy thing they replaced ("the toggle_on/toggle_off PNG pair"), so a naive
// grep for the legacy token reports the comment that proves the fix as though it were the defect.
function stripComments(source) {
  return source
    .replace(/\/\*[\s\S]*?\*\//g, ' ')
    .replace(/^[ \t]*\/\/.*$/gm, ' ');
}

const relativeLuminance = (hex) => {
  const [r, g, b] = [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255)
    .map((c) => (c <= 0.03928 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4));
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
};
const contrast = (a, b) => {
  const [hi, lo] = [relativeLuminance(a), relativeLuminance(b)].sort((x, y) => y - x);
  return (hi + 0.05) / (lo + 0.05);
};

test('fan control popup is tokenised and its switches are real controls', async () => {
  const source = stripComments(await read('Widgets', 'FanControl.cpp'));

  // It had zero MD3::Role references across 1161 lines; every colour was a ThemeColor literal.
  assert.equal(source.match(/ThemeColor::/g), null,
    'FanControl.cpp must carry no legacy ThemeColor literals');
  assert.ok(/MD3::Role::/.test(source), 'FanControl.cpp must resolve colours from MD3 roles');
  assert.ok(/MD3::ColorScheme::Device/.test(source),
    'the popup lives in the Device workspace and must thread the teal scheme');

  // The accessibility half: the fan on/off toggles were toggle_on/toggle_off PNGs inside a
  // wxStaticBitmap. A wxStaticBitmap is not a control — it cannot take focus and reports no role,
  // name or checked state, which made the whole popup mouse-only.
  assert.equal(source.match(/toggle_on|toggle_off/g), null,
    'the toggle PNG pseudo-switches must be gone from executable code');
  assert.ok(/new SwitchButton\(/.test(source),
    'the fan toggles must be real SwitchButton controls, which are focusable and expose state');

  // One wxStaticBitmap legitimately survives: the fan artwork is an image, not a control.
  const bitmaps = source.match(/new wxStaticBitmap\(/g) || [];
  assert.equal(bitmaps.length, 1,
    'only the fan artwork may remain a static bitmap; controls must be controls');
});

test('side menu popup paints a real MD3 floating surface', async () => {
  const source = stripComments(await read('Widgets', 'SideMenuPopup.cpp'));

  // It previously drew a single transparent rectangle: no fill, no border, no radius, so the
  // Slice/Print menu read as a detached slab over the 3D viewport.
  assert.ok(/MD3::Light::sc\b/.test(source), 'the popup must fill with a SurfaceContainer tone');
  assert.ok(/MD3::Light::outlineVariant/.test(source), 'the popup must carry an OutlineVariant frame');
  // The radius is a named constant, not a literal, so assert it at its definition and at use.
  assert.ok(/SIDE_POPUP_RADIUS\s*=\s*18/.test(source), 'the popup must use the kit 18px corner radius');
  assert.ok(/FromDIP\(SIDE_POPUP_RADIUS\)/.test(source), 'the corner radius must be DPI-scaled');
  // Stored as light values and resolved per paint, so the menu follows a runtime theme switch
  // instead of freezing at its construction theme.
  assert.ok(/darkModeColorFor/.test(source),
    'popup colours must resolve at paint time so dark mode is not frozen at construction');
});

test('side button no longer defaults to the legacy palette', async () => {
  const source = stripComments(await read('Widgets', 'SideButton.cpp'));
  // The legacy palette was the CONSTRUCTOR DEFAULT, which is why the Slice/Print dropdown rows —
  // which only ever call SetCornerRadius — rendered as solid brand-green bars.
  for (const legacy of ['BrandGreen', 'ThemeColor::White', 'Grey250', 'Grey400']) {
    assert.ok(!source.includes(legacy),
      `SideButton must not default to the legacy ${legacy}`);
  }
  // The file aliases the enum (`using R = MD3::Role;`) and resolves through StateColor.
  assert.ok(/using R = MD3::Role;/.test(source), 'SideButton must alias the MD3 role enum');
  assert.ok(/StateColor::semantic\(R::SurfaceContainer\)/.test(source),
    'SideButton must default to a SurfaceContainer menu-row fill');
});

test('measurement chips are legible in dark mode, by arithmetic', async () => {
  const source = stripComments(await read('Gizmos', 'GLGizmoMeasure.cpp'));

  // Both value chips were filled with 50%-alpha pure white while their text resolves to OnSurface —
  // near-white text on a near-white plate in dark mode.
  assert.ok(/MD3::Role::SurfaceContainer/.test(source), 'chips must fill from SurfaceContainer');
  assert.ok(/MD3::Role::OutlineVariant/.test(source), 'chips must carry an OutlineVariant hairline');
  assert.ok(/MD3::Metrics::radius_tiny/.test(source), 'chips must use the kit corner radius');

  const tokens = await read('Widgets', 'MD3Tokens.hpp');
  const darkBlock = tokens.slice(tokens.indexOf('namespace Dark'));
  const tokenValue = (name) => {
    const m = darkBlock.match(new RegExp(`inline const wxColour ${name}\\{"(#[0-9a-fA-F]{6})"\\}`));
    assert.ok(m, `dark token ${name} must exist`);
    return m[1];
  };

  // The dark SurfaceContainer token is named `sc` in MD3Tokens.hpp.
  const before = contrast('#ffffff', tokenValue('onSurface'));      // the old 50%-white plate
  const after = contrast(tokenValue('sc'), tokenValue('onSurface'));

  // The point of the fix: the label has to be readable on its own plate.
  assert.ok(after > before,
    `SurfaceContainer must beat the old white plate (was ${before.toFixed(2)}:1, now ${after.toFixed(2)}:1)`);
  assert.ok(after >= 4.5,
    `chip text must clear the WCAG AA 4.5:1 body-text threshold, got ${after.toFixed(2)}:1`);
});

test('2D bed keeps its axis colours as exempt data', async () => {
  const source = stripComments(await read('2DBed.cpp'));

  // Red X / green Y is a near-universal 3D convention the user reads as MEANING, and the 3D gizmo
  // still draws pure RGB (GLGizmoBase.cpp AXES_COLOR), so tokenising the 2D side would have made
  // the bed preview disagree with the scene it mirrors. A conversion that did exactly that was
  // reverted; this pins it.
  assert.ok(/wxColour\(255, 0, 0\)/.test(source), 'the X axis must stay pure red');
  assert.ok(/wxColour\(0, 255, 0\)/.test(source), 'the Y axis must stay pure green');
  assert.equal(source.match(/MD3::Viewport::axis/g), null,
    'axis colours are exempt data and must not be tokenised');

  // The 1.05:1 fill-vs-backdrop step has been repeatedly read as an accessibility failure. It is
  // not: WCAG 1.4.11 governs user-interface components and graphical objects required to understand
  // content, and two adjacent decorative surface tones are neither. What must be perceivable is the
  // boundary that says where the printable area IS — the contour ring — and that is what is pinned
  // here. Brightening the slab to raise the fill step would drop the grid below its current
  // separation, which is why the fill is deliberately left at the lowest container.
  const tokens = await read('Widgets', 'MD3Tokens.hpp');
  const namespaceTokens = (name) => {
    const start = tokens.indexOf(`namespace ${name}`);
    const next = tokens.indexOf('namespace', start + 10);
    const block = tokens.slice(start, next === -1 ? undefined : next);
    return Object.fromEntries(
      [...block.matchAll(/inline const wxColour (\w+)\{"(#[0-9a-fA-F]{6})"\}/g)].map((m) => [m[1], m[2]]));
  };

  // Read the roles 2DBed ACTUALLY assigns rather than hard-coding them here. Asserting against
  // the palette alone would pass even if this file stopped using it — a mutation that flattened
  // the contour role slipped through exactly that way before this was bound to the source.
  const roleOf = (local) => {
    const m = source.match(new RegExp(`${local}\\s*=\\s*StateColor::semantic\\(MD3::Role::(\\w+)\\)`));
    assert.ok(m, `2DBed must assign ${local} from an MD3 role`);
    return m[1];
  };
  const roleToToken = {
    Surface: 'surface', SurfaceContainerLowest: 'scLowest', SurfaceContainerLow: 'scLow',
    SurfaceContainer: 'sc', SurfaceContainerHigh: 'scHigh', SurfaceContainerHighest: 'scHighest',
    Outline: 'outline', OutlineVariant: 'outlineVariant',
    OnSurface: 'onSurface', OnSurfaceVariant: 'onSurfaceVariant',
  };
  const tokenFor = (theme, role) => {
    const key = roleToToken[role];
    assert.ok(key, `unmapped role ${role}`);
    const v = namespaceTokens(theme)[key];
    assert.ok(v, `${theme} token ${key} must exist`);
    return v;
  };

  const contourRole = roleOf('contour');
  const fillRole = roleOf('bed_fill');
  const backdropRole = roleOf('backdrop');
  const annotationRole = roleOf('annotation');

  for (const theme of ['Light', 'Dark']) {
    const boundary = contrast(tokenFor(theme, contourRole), tokenFor(theme, fillRole));
    assert.ok(boundary >= 3.0,
      `${theme}: the contour ring (${contourRole} on ${fillRole}) must clear WCAG 1.4.11's 3:1 so the printable area stays perceivable, got ${boundary.toFixed(2)}:1`);
    const label = contrast(tokenFor(theme, annotationRole), tokenFor(theme, backdropRole));
    assert.ok(label >= 4.5,
      `${theme}: bed annotation text (${annotationRole} on ${backdropRole}) must clear WCAG 1.4.3's 4.5:1, got ${label.toFixed(2)}:1`);
  }

  // The surrounding chrome, which is not data, should be tokenised.
  assert.ok(/StateColor::semantic\(MD3::Role::/.test(source),
    'bed chrome (backdrop, grid, contour) must resolve from MD3 roles');
});

test('the prepare action bar cannot starve its primary actions again', async () => {
  const source = await read('MainFrame.cpp');

  // The canvas-alignment spacers are proportion-0 sizer items and the tool row is proportion-1.
  // wxBoxSizer's degenerate branch pays the fixed items in full first, so a full-width spacer
  // starved the row: the Slice pill rendered at 92px ("Slice pl") and the Print pill at 0px —
  // a primary action absent from the UI entirely, with nothing reporting it.
  assert.ok(/spare\s*=\s*std::max\(0,\s*bar_width\s*-\s*row_min\)/.test(source),
    'the spacers must be clamped to what the tool row does not need');
  assert.ok(/left_sidebar_width\s*=\s*std::min\(left_sidebar_width,\s*spare\)/.test(source),
    'the left spacer must never exceed the spare width');
  assert.ok(/right_sidebar_width\s*=\s*std::min\(right_sidebar_width,\s*spare\)/.test(source),
    'the right spacer must never exceed the spare width');

  // wx zero-sizes a starved child in silence, so the shortfall must announce itself.
  assert.ok(/prepare action bar: tool row allocated/.test(source),
    'a still-over-subscribed row must log its shortfall rather than vanish silently');
});

// ---------------------------------------------------------------------------
// Second-pass sweep: generic (stock wx) elements → kit widgets, kit vocabulary.
//
// The design folder (ui-md3/design-system) has no stock-looking control: every
// button is a pill Button, every divider a StaticLine, every progress a
// ProgressBar, every link a LinkLabel, every toggle a CheckBox/Switch, every
// dropdown a ComboBox, every dialog a borderless MD3 shell. These tests pin
// the sweep that converted the remaining stock constructions and keep a
// ratchet on the ones that are still allowed (image holders, dev-only chrome,
// frames, and controls that wrap a native editor).
// ---------------------------------------------------------------------------

import { readdir } from 'node:fs/promises';

async function guiSources() {
  const files = [];
  async function walk(dir) {
    for (const entry of await readdir(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) await walk(full);
      else if (entry.name.endsWith('.cpp')) files.push(full);
    }
  }
  await walk(gui());
  return files;
}

async function sitesOf(pattern) {
  const hits = new Map();
  for (const file of await guiSources()) {
    const source = stripComments(await readFile(file, 'utf8'));
    const count = (source.match(pattern) || []).length;
    if (count) hits.set(path.relative(gui(), file).replace(/\\/g, '/'), count);
  }
  return hits;
}

function assertOnlyAllowed(hits, allowed, what) {
  const offenders = [...hits.keys()].filter((file) => !allowed.has(file));
  assert.deepEqual(offenders, [], `${what} must only remain in the allowlisted files`);
}

test('every user-visible string passes through the Ink / Ink Dispenser vocabulary', async () => {
  const mode = stripComments(await read('LanguageMode.cpp'));
  assert.ok(/wxString vocabulary\(const wxString &text\)/.test(mode), 'LanguageMode.cpp must define vocabulary()');
  for (const [from, to] of [['Filaments', 'Inks'], ['filament', 'ink'], ['Filament', 'Ink'], ['AMS', 'Ink Dispenser']]) {
    assert.ok(new RegExp(`L"${from}",\\s*L"${to}"`).test(mode), `vocabulary must rewrite ${from} -> ${to}`);
  }
  // Identifiers and URLs are never rewritten.
  assert.ok(mode.includes('L"://"') && mode.includes("L'_'"), 'vocabulary must skip URLs and identifiers');

  const i18n = stripComments(await read('I18N.hpp'));
  const translateBodies = i18n.match(/inline (?:wxString|std::string) translate(?:_utf8)?\([^)]*\)\s*\{[^}]*\}/g) || [];
  assert.ok(translateBodies.length >= 20, 'I18N.hpp must keep its translate overloads');
  for (const body of translateBodies) {
    // Either applies the vocabulary itself or delegates to an overload that does.
    assert.ok(body.includes('vocabulary(') || /return translate\(/.test(body),
      `every translate overload must apply the vocabulary:\n${body}`);
  }
  assert.ok(/I18N::vocabulary\(wxGetTranslation/.test(stripComments(await read('GUI_App.cpp'))),
    'the libslic3r translate callback must apply the vocabulary too');
  for (const kind of ['Standard', 'English']) {
    assert.ok(new RegExp(`LanguageModeKind::${kind}\\)\\s*return \\{ vocabulary\\(`).test(mode),
      `LanguageModeService::translate must apply the vocabulary for the ${kind} mode`);
  }
});

test('every label is the kit Label; no stock wxStaticText is constructed anywhere', async () => {
  // Label subclasses wxStaticText and reaches the base through an initializer, never
  // through `new wxStaticText(`, so the allowlist is genuinely empty. 548 sites were
  // retyped by scripts/md3/convert-static-text.mjs; anything reintroducing the stock
  // control renders in the system font and the legacy tone instead of MD3 body type.
  assertOnlyAllowed(await sitesOf(/new wxStaticText\(/g), new Set(), 'wxStaticText');
});

test('the kit Label seeds MD3 roles, not the legacy palette', async () => {
  const source = stripComments(await read('Widgets', 'Label.cpp'));
  assert.match(source, /^\s*SetForegroundColour\(StateColor::semantic\(MD3::Role::OnSurface\)\);/m,
    'Label must seed its text tone from MD3::Role::OnSurface');
  assert.match(source, /^\s*SetForegroundColour\(StateColor::semantic\(MD3::Role::Primary\)\);/m,
    'Label hyperlinks must take MD3::Role::Primary, resolved per call');
  assert.doesNotMatch(source, /ThemeColor::(TextPrimary|BrandGreen)\b/,
    'Label must not reference the legacy ThemeColor palette');
  // The SetLabel early-return must also consult the native label, or a base-class
  // Wrap() through a wxStaticText* leaves stale wrapped text on screen.
  assert.match(source, /m_text == label && \(\(GetWindowStyle\(\) & LB_AUTO_WRAP\) \|\| wxStaticText::GetLabel\(\) == label\)/,
    'Label::SetLabel must compare against wxStaticText::GetLabel() as well as m_text');
});

test('every radio is the kit LabeledRadioButton, which carries the radio role and group navigation', async () => {
  // The earlier native exception existed because the bare RadioBox lost the radio
  // role, checked state, accessible name and arrow keys. LabeledRadioButton and
  // RadioGroup supply all four, so no stock wxRadioButton may remain.
  assertOnlyAllowed(await sitesOf(/new wxRadioButton\(/g), new Set(), 'wxRadioButton');
  const widget = stripComments(await read('Widgets', 'LabeledRadioButton.cpp'));
  assert.match(widget, /wxROLE_SYSTEM_RADIOBUTTON/, 'the accessible peer must report the radio role');
  assert.match(widget, /wxACC_STATE_SYSTEM_CHECKED/, 'the accessible peer must expose the checked state');
  assert.match(widget, /wxCommandEvent event\(wxEVT_RADIOBUTTON, GetId\(\)\);/, 'activation must emit wxEVT_RADIOBUTTON from the row');
  assert.match(widget, /case WXK_UP: case WXK_LEFT:\s+moveTo\(here - 1\)/, 'RadioGroup must move selection with the arrow keys');
  assert.match(widget, /new RadioBox\(this\)/, 'the row must draw the kit RadioBox glyph');
  const cmake = await readFile(path.join(repoDir, 'src', 'slic3r', 'CMakeLists.txt'), 'utf8');
  assert.match(cmake, /^\s*GUI\/Widgets\/LabeledRadioButton\.cpp\s*$/m, 'LabeledRadioButton.cpp must be registered');
  const page = stripComments(await read('CalibrationWizardPage.hpp'));
  assert.doesNotMatch(page, /wxRadioButton/, 'FilamentComboBox must type its slot radio as the kit row');
  assert.match(page, /void SetRadioBox\(LabeledRadioButton\* btn\)/, 'SetRadioBox must take the kit row');
});

test('every text field is a kit TextInput or TextArea; native editors exist only inside kit fields', async () => {
  // A native wxTextCtrl may exist only as the INNER editor of a kit field
  // (TextInput, TextArea, SearchField, TempInput, MD3ColorPicker, the regex
  // builder's own fields) or where the host owns the editor's lifetime.
  assertOnlyAllowed(await sitesOf(/new wxTextCtrl\(/g), new Set([
    'Widgets/TextInput.cpp',
    'Widgets/TextArea.cpp',
    'Widgets/SearchField.cpp',
    'Widgets/TempInput.cpp',
    'Widgets/MD3ColorPicker.cpp',
    'Widgets/RegexBuilderPopup.cpp',   // kit-internal fields drawn by the popup's own field panels
    'ExtraRenderers.cpp',              // wxDataViewCustomRenderer owns the cell editor's lifetime
    'MixedFilamentDialog.cpp',         // inline cell editor inside a hand-drawn table row
    'WebViewDialog.cpp',               // developer-only URL bar, compiled out for public releases
  ]), 'wxTextCtrl');
  const area = stripComments(await read('Widgets', 'TextArea.cpp'));
  assert.match(area, /R::SurfaceContainerLow : R::SurfaceContainerLowest/, 'TextArea must tone read-only and editable fields from MD3 roles');
  assert.match(area, /m_focused \? R::Primary : R::OutlineVariant/, 'TextArea must promote its outline to Primary on focus');
  assert.match(area, /MD3::Metrics::radius_tiny/, 'TextArea must use the kit small radius');
  assert.match(area, /new TextCtrl\(this, wxID_ANY, text/, 'TextArea must host the MSW-colour-safe TextCtrl editor');
  const cmake = await readFile(path.join(repoDir, 'src', 'slic3r', 'CMakeLists.txt'), 'utf8');
  assert.match(cmake, /^\s*GUI\/Widgets\/TextArea\.cpp\s*$/m, 'TextArea.cpp must be registered');
  for (const [file, needle] of [
    ['UpdateDialogs.cpp', 'new TextArea(this, from_u8(update.change_log)'],
    ['MsgDialog.cpp', 'm_script_text = new TextArea('],
    ['SendSystemInfoDialog.cpp', 'new TextArea(this, json'],
    ['NetworkTestDialog.cpp', 'txt_log = new TextArea('],
    ['StatusPanel.cpp', 'm_comment_text = new TextArea('],
    ['UnsavedChangesDialog.cpp', 'new TextArea(this, label'],
  ]) {
    assert.ok(stripComments(await read(file)).includes(needle), `${file} must construct its view as TextArea`);
  }
});

test('every bitmap button is a kit icon Button or RadioBox; swatches ride Button::SetIconBitmap', async () => {
  assertOnlyAllowed(await sitesOf(/new wxBitmapButton\(/g), new Set(), 'wxBitmapButton');
  const button = stripComments(await read('Widgets', 'Button.cpp'));
  assert.match(button, /^void Button::SetIconBitmap\(const wxBitmap &bitmap\)/m, 'Button must accept a ready bitmap for data swatches');
  const ext = stripComments(await read('wxExtensions.cpp'));
  assert.match(ext, /if \(m_icon_name\.empty\(\)\) return;/, 'a wrapped bitmap must survive msw_rescale');
  for (const [file, needle] of [
    ['WebViewDialog.cpp', 'btn->SetIconButton(Button::IconShape::Square, FromDIP(28));'],
    ['Plater.cpp', 'm_hover_btn->SetGlyph(MaterialIcon::FiberManualRecord'],
    ['CapsuleButton.cpp', 'm_btn->SetGlyph(selected ? MaterialIcon::TaskAlt : MaterialIcon::Circle'],
    ['FilamentGroupPopup.cpp', 'radio_btns[idx]          = new RadioBox(this);'],
    ['FilamentPickerDialog.cpp', 'btn->SetIconBitmap(btn_bmp);'],
    ['PresetComboBoxes.cpp', 'clr_picker->SetIconButton(Button::IconShape::Square, FromDIP(28));'],
    ['FilamentMapPanel.cpp', 'm_btn->SetIconBitmap(icon_enabled);'],
  ]) {
    assert.ok(stripComments(await read(file)).includes(needle), `${file} must construct its icon control from the kit`);
  }
  // The picker's selection ring is the kit border, not a second paint handler.
  const picker = stripComments(await read('FilamentPickerDialog.cpp'));
  assert.doesNotMatch(picker, /Bind\(wxEVT_PAINT, &FilamentPickerDialog::OnButtonPaint/, 'the swatch ring must be drawn through the kit border');
});

test('the only list is the kit ListBox, drawn with the DropDown row anatomy', async () => {
  assertOnlyAllowed(await sitesOf(/new wxListBox\(/g), new Set(), 'wxListBox');
  const list = stripComments(await read('Widgets', 'ListBox.cpp'));
  assert.match(list, /class ListBox : public wxVListBox|ListBox::ListBox\(wxWindow \*parent/, 'ListBox must be the owner-drawn wxVListBox');
  assert.match(list, /MD3::Role::SecondaryContainer, m_scheme/, 'the selected pane must be SecondaryContainer in the active scheme');
  assert.match(list, /MD3::Role::SurfaceContainerHigh/, 'the hover pane must be SurfaceContainerHigh');
  assert.match(list, /wxControl::Ellipsize\(m_rows\[n\], dc, wxELLIPSIZE_END/, 'long rows must ellipsize, with the full text in the tooltip');
  assert.match(list, /SetToolTip\(row >= 0/, 'the hovered row must expose its full text as the tooltip');
  const cmake = await readFile(path.join(repoDir, 'src', 'slic3r', 'CMakeLists.txt'), 'utf8');
  assert.match(cmake, /^\s*GUI\/Widgets\/ListBox\.cpp\s*$/m, 'ListBox.cpp must be registered');
  assert.ok(stripComments(await read('SmartHomeDialog.cpp')).includes('m_list = new ListBox(m_scroll'), 'SmartHome must use the kit ListBox');
});

test('no window is sized by an unscaled pixel literal', async () => {
  // SetSize/SetMinSize/SetMaxSize(wxSize(N, M)) with a positive literal is a
  // 100%-only size: at 150% and 200% it clips whatever it holds. The zero and
  // minus-one forms are sizer contracts ("the sizer owns this axis"), not
  // pixel sizes, and stay legal. Everything else goes through FromDIP.
  const hits = new Map();
  for (const [file, count] of await sitesOf(/\bSet(?:Min|Max)?Size\(wxSize\((?!0, *(?:0|-1)\))\d+, *-?\d+\)\)/g)) hits.set(file, count);
  assertOnlyAllowed(hits, new Set(), 'unscaled wxSize literal');
  const prefs = stripComments(await read('Preferences.cpp'));
  assert.match(prefs, /SetSize\(FromDIP\(wxSize\(780, 580\)\)\);/, 'Preferences must scale its 780x580 default size');
  const app = stripComments(await read('GUI_App.cpp'));
  assert.doesNotMatch(app, /SetSize\(wxSize\(270, 158\)\)/, 'the plugin download dialog must be sized through FromDIP on every path');
});

test('no label combines two ellipsize styles', async () => {
  // wxST_ELLIPSIZE_START|MIDDLE|END on one control is contradictory; wx picks
  // one arbitrarily (and asserts in debug builds). MonitorBasePanel shipped that.
  assertOnlyAllowed(await sitesOf(/wxST_ELLIPSIZE_\w+\s*\|\s*wxST_ELLIPSIZE_/g), new Set([
    'LayoutProbe.cpp', // ORs the three flags as a MASK to test a style, never applies them
  ]), 'combined ellipsize styles');
});

test('the runtime layout probe is wired, off by default, and reports the starvation flags', async () => {
  const probe = stripComments(await read('LayoutProbe.cpp'));
  assert.match(probe, /env_value\("BAMBU_LAYOUT_PROBE"\)/, 'the probe must be gated on BAMBU_LAYOUT_PROBE');
  for (const key of ['starved', 'zero_sized', 'oversubscribed', 'text_clipped', 'clipped_by_parent']) {
    assert.ok(probe.includes(`"${key}"`) || probe.includes(`\\"${key}\\"`), `the probe must emit the ${key} flag`);
  }
  assert.match(probe, /required > v\.available/, 'the row verdict must compare required minimum against available size');
  const app = stripComments(await read('GUI_App.cpp'));
  assert.match(app, /copy_data_structure->dwData == 2/, 'WM_COPYDATA must dispatch dwData == 2 to the probe');
  assert.match(app, /^\s*LayoutProbe::install\(mainframe\);/m, 'the probe must be installed after the main frame is shown');
  const cmake = await readFile(path.join(repoDir, 'src', 'slic3r', 'CMakeLists.txt'), 'utf8');
  assert.match(cmake, /^\s*GUI\/LayoutProbe\.cpp\s*$/m, 'LayoutProbe.cpp must be registered in src/slic3r/CMakeLists.txt');
});

test('stock wx controls are gone from the GUI except the allowlisted holders', async () => {
  assertOnlyAllowed(await sitesOf(/new wxGauge\(/g), new Set(), 'wxGauge');
  assertOnlyAllowed(await sitesOf(/new wxStaticLine\(/g), new Set(), 'wxStaticLine');
  assertOnlyAllowed(await sitesOf(/new wxHyperlinkCtrl\(/g), new Set(), 'wxHyperlinkCtrl');
  assertOnlyAllowed(await sitesOf(/new wxChoice\(/g), new Set(), 'wxChoice');
  assertOnlyAllowed(await sitesOf(/new wxCheckBox\(/g), new Set(), 'wxCheckBox');
  // The SmartHome volume trackbar and the option-field slider are on the kit
  // Slider now; nothing else may bring a native trackbar back.
  assertOnlyAllowed(await sitesOf(/new wxSlider\(/g), new Set(), 'wxSlider');
  assertOnlyAllowed(await sitesOf(/new wxComboBox\(/g), new Set([
    'ExtrusionCalibration.cpp', // legacy branch of an #ifdef whose live branch is the kit ComboBox
    'Auxiliary.cpp',            // dead designer-panel code behind a commented member
  ]), 'wxComboBox');
  assertOnlyAllowed(await sitesOf(/new wxButton\(/g), new Set([
    'CalibrationWizardSavePage.cpp', // bitmap holder for the tray thumbnail
    'SyncAmsInfoDialog.cpp',         // two bitmap holders of the compare panel
    'ObjColorDialog.cpp',            // bitmap holders / colour icon wells
    'WebViewDialog.cpp',             // developer-only browser toolbar (!BBL_RELEASE_TO_PUBLIC)
  ]), 'wxButton');
});

test('wgtMsgPanel includes the widget header that owns LinkLabel', async () => {
  const header = stripComments(await read('DeviceTab', 'wgtMsgPanel.h'));
  const includeLine = '#include "slic3r/GUI/Widgets/LinkLabel.hpp"';
  const memberIndex = header.search(/\bLinkLabel[ \t]*\*[ \t]*m_wiki_link\b/);

  assert.match(header,
    /^[ \t]*#pragma once[ \t]*\r?\n(?:[ \t]*\r?\n)*[ \t]*#include[ \t]+"slic3r\/GUI\/Widgets\/LinkLabel[.]hpp"[ \t]*(?:\r?\n|$)/,
    'wgtMsgPanel.h must include LinkLabel.hpp unconditionally after pragma once');
  assert.notEqual(memberIndex, -1,
    'wgtMsgPanel.h must retain the converted LinkLabel member');
  assert.ok(header.indexOf(includeLine) < memberIndex,
    'wgtMsgPanel.h must make LinkLabel visible before the member declaration');
});

test('the kit ProgressBar carries the gauge surface the status bars rely on', async () => {
  const hpp = stripComments(await read('Widgets', 'ProgressBar.hpp'));
  for (const member of ['int          GetValue() const', 'int          GetRange() const', 'void         SetRange(int range);', 'void         Pulse();']) {
    assert.ok(hpp.includes(member), `ProgressBar.hpp must declare ${member.trim()}`);
  }
  const cpp = stripComments(await read('Widgets', 'ProgressBar.cpp'));
  assert.ok(/void ProgressBar::Pulse\(\)/.test(cpp) && /m_indeterminate/.test(cpp), 'Pulse() must drive an indeterminate sweep');
});

test('LabeledCheckBox is a registered kit widget that re-emits wxEVT_CHECKBOX', async () => {
  const cmake = await readFile(path.join(repoDir, 'src', 'slic3r', 'CMakeLists.txt'), 'utf8');
  assert.ok(cmake.includes('GUI/Widgets/LabeledCheckBox.cpp') && cmake.includes('GUI/Widgets/LabeledCheckBox.hpp'),
    'LabeledCheckBox must be part of the libslic3r_gui target');
  const cpp = stripComments(await read('Widgets', 'LabeledCheckBox.cpp'));
  assert.ok(cpp.includes('wxCommandEvent event(wxEVT_CHECKBOX, GetId());'), 'the row must emit wxEVT_CHECKBOX so old handlers keep working');
  assert.ok(cpp.includes('new CheckBox(this)') && cpp.includes('new Label(this, label)'), 'the row is the kit CheckBox glyph plus a Label');
});

test('every owned dialog is on the MD3 caption shell; only frames keep native chrome', async () => {
  const allowedNative = new Set([
    'ModelMall.cpp',              // DPIFrame window, not a dialog
    'ImageDPIFrame.cpp',          // already borderless (!wxCAPTION)
    'BaseTransparentDPIFrame.cpp',// already borderless (!wxCAPTION)
    'MainFrame.cpp',              // SettingsDialog uses wxDEFAULT_FRAME_STYLE (a frame)
    'GUI_App.cpp',                // GuideFrame wizard window
  ]);
  const offenders = [];
  for (const file of await guiSources()) {
    const source = stripComments(await readFile(file, 'utf8'));
    if (!/wxDEFAULT_DIALOG_STYLE|wxCAPTION/.test(source)) continue;
    if (/MD3DialogCaption::Adopt|MD3DialogCaption\(|public MD3Dialog\b|MD3Dialog\(/.test(source)) continue;
    const rel = path.relative(gui(), file).replace(/\\/g, '/');
    if (!allowedNative.has(rel)) offenders.push(rel);
  }
  assert.deepEqual(offenders, [], 'dialogs constructed with native OS chrome must adopt the MD3 caption');
});
