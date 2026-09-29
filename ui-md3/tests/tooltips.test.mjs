import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Every wx tooltip is drawn by one shared Win32 tooltip control, which painted the
// system's pale box in the system font whatever the theme. With its visual style
// removed it takes the colours it is given: the Material plain tooltip.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const strip = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const app = strip(await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'GUI_App.cpp'), 'utf8'));
const checker = (await readFile(path.join(repoDir, 'scripts', 'md3', 'check-tooltips.py'), 'utf8')).replace(/\r\n/g, '\n');
const style = app.slice(app.indexOf('static void style_tooltips_md3(wxWindow *scale_from)\n{'), app.indexOf('void GUI_App::show_message_box('));

test('the shared tooltip control is the Material plain tooltip', () => {
  assert.match(style, /HWND tip = static_cast<HWND>\(wxToolTip::GetToolTipCtrl\(\)\);/);
  assert.match(style, /::SetWindowTheme\(tip, L"", L""\);/, 'without the visual style the control uses the colours below');
  assert.match(style, /TTM_SETTIPBKCOLOR, colorref\(StateColor::semantic\(MD3::Role::InverseSurface\)\)/);
  assert.match(style, /TTM_SETTIPTEXTCOLOR, colorref\(StateColor::semantic\(MD3::Role::InverseOn\)\)/);
  assert.match(style, /::SendMessage\(tip, TTM_SETMARGIN, 0, reinterpret_cast<LPARAM>\(&margin\)\);/);
  assert.match(style, /::SendMessage\(tip, WM_SETFONT, reinterpret_cast<WPARAM>\(::Label::Body_12\.GetHFONT\(\)\), TRUE\);/);
  assert.match(style, /::DwmSetWindowAttribute\(tip, 33 , &round_small, sizeof\(round_small\)\);|::DwmSetWindowAttribute\(tip, 33 \/\* DWMWA_WINDOW_CORNER_PREFERENCE \*\/, &round_small, sizeof\(round_small\)\);/);
});

test('it is applied once the main window exists and again on every theme change', () => {
  assert.match(app, /SetTopWindow\(mainframe\);\s*#ifdef __WXMSW__\s*style_tooltips_md3\(mainframe\);\s*#endif/);
  const colors = app.slice(app.indexOf('void GUI_App::force_colors_update()'), app.indexOf('m_force_colors_update = true;', app.indexOf('void GUI_App::force_colors_update()')));
  assert.match(colors, /#ifdef __WXMSW__\s*style_tooltips_md3\(mainframe\);\s*#endif\s*$/, 'after the dark theme, so the new colours win');
});

test('the runtime check judges the tooltip by its colour', () => {
  assert.match(checker, /INVERSE_SURFACE = \{'light': \(0x2F, 0x30, 0x36\), 'dark': \(0xE3, 0xE2, 0xE9\)\}/);
  assert.match(checker, /'result': 'material tooltip' if distance <= 24 else 'system tooltip'/);
  assert.match(checker, /return 0 if row\.get\('result'\) == 'material tooltip' else 1/);
});
