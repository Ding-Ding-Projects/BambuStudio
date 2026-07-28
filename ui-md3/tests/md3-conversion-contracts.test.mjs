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
