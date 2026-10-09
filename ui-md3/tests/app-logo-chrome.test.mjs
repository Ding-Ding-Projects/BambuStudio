// The app logo surface is compiled, hosted in Preferences and applied to the
// app chrome (title bar tile, window and taskbar icon, About banner, startup
// screen). These are source contracts: the pure selection model is covered by
// tests/app_logo, and the packaged rendering still needs a built-app capture.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = (relative) => fs.readFileSync(path.join(root, relative), 'utf8');
const app = read('src/slic3r/GUI/GUI_App.cpp');

// Body of the first top-level function whose signature contains `marker`.
function functionBody(source, marker) {
  const at = source.indexOf(marker);
  assert.ok(at >= 0, `missing ${marker}`);
  const open = source.indexOf('{', at);
  let depth = 0;
  for (let i = open; i < source.length; ++i) {
    if (source[i] === '{') depth++;
    else if (source[i] === '}' && --depth === 0) return source.slice(open, i + 1);
  }
  assert.fail(`unbalanced body after ${marker}`);
}

test('the logo panel is compiled into the GUI target and extracted for translation', () => {
  const cmake = read('src/slic3r/CMakeLists.txt');
  assert.match(cmake, /^\s*GUI\/AppLogo\/LogoPanel\.cpp\s*$/m);
  assert.match(cmake, /^\s*GUI\/AppLogo\/LogoPanel\.hpp\s*$/m);
  assert.match(cmake, /^\s*GUI\/AppLogo\/LogoRender\.cpp\s*$/m);
  assert.match(cmake, /^\s*GUI\/AppLogo\/LogoRender\.hpp\s*$/m);
  assert.match(read('bbl/i18n/list.txt'), /^src\/slic3r\/GUI\/AppLogo\/LogoPanel\.cpp$/m);
  assert.match(read('tests/CMakeLists.txt'), /^add_subdirectory\(app_logo\)$/m);
});

test('Preferences hosts the panel as a searchable appearance row', () => {
  const prefs = read('src/slic3r/GUI/Preferences.cpp');
  assert.match(prefs, /#include "AppLogo\/LogoPanel\.hpp"/);
  const tab = functionBody(prefs, 'wxWindow *PreferencesDialog::create_appearance_tab()');
  assert.match(tab, /new AppLogoUI::LogoPanel\(/);
  assert.match(tab, /wxGetApp\(\)\.set_app_logo_settings\(/);
  assert.match(tab, /register_option_row\(AppLogo::config_key,/);
});

test('the panel source picker is semantic, searchable and never fakes custom import', () => {
  const panel = read('src/slic3r/GUI/AppLogo/LogoPanel.cpp');
  assert.match(panel, /new SearchField\(/, 'adjacent search field with the regex builder');
  assert.match(panel, /SearchField::MatchPass/);
  assert.match(panel, /new LabeledRadioButton\(/, 'radio rows carry the radio role and checked state');
  assert.match(panel, /RadioGroup/, 'arrow-key radiogroup navigation');
  const render = read('src/slic3r/GUI/AppLogo/LogoRender.cpp');
  for (const source of [panel, render])
    assert.doesNotMatch(source, /wxFileDialog|wxFile\b|wxFFile|fopen|ifstream|decode_bmp|LoadFile/, 'no file is read until the isolated decoder ships');
  assert.match(panel, /->Disable\(\)/, 'custom import stays visibly disabled');
  assert.doesNotMatch(panel, /\btr\("/, 'every label goes through the catalogue macros');
});

test('the app owns persistence and tells the chrome about changes', () => {
  const getter = functionBody(app, 'AppLogo::Settings GUI_App::app_logo_settings() const');
  assert.match(getter, /AppLogo::resolve\(app_config->get\(AppLogo::config_key\)\)/);
  const setter = functionBody(app, 'bool GUI_App::set_app_logo_settings(');
  assert.match(setter, /AppLogo::stored_value\(/);
  assert.match(setter, /app_config->set\(AppLogo::config_key, stored\)/);
  assert.match(setter, /app_config->save\(\)/);
  assert.match(setter, /mainframe->on_app_logo_changed\(\)/);
});

test('every chrome surface draws the selected logo and keeps the shipped mark as fallback', () => {
  const frame = read('src/slic3r/GUI/MainFrame.cpp');
  const changed = functionBody(frame, 'void MainFrame::on_app_logo_changed()');
  assert.match(changed, /m_topbar->RefreshBrandTile\(\)/);
  assert.match(changed, /apply_app_logo_icon\(\)/);
  const icon = functionBody(frame, 'void MainFrame::apply_app_logo_icon()');
  assert.match(icon, /AppLogoUI::icon_bundle\(/);
  assert.match(icon, /main_frame_icon\(/, 'shipped and failed renders keep the installed icon');
  assert.equal(frame.split('SetIcon(main_frame_icon(').length - 1, 1,
    'the constructor routes through apply_app_logo_icon, the only caller of the shipped icon');

  const topbar = read('src/slic3r/GUI/BBLTopbar.cpp');
  const tile = functionBody(topbar, 'static wxBitmap topbar_brand_tile_bitmap(wxWindow *ref)');
  assert.match(tile, /app_logo_settings\(\)/);
  assert.match(tile, /AppLogoUI::scaled_bitmap\(/);
  assert.match(functionBody(topbar, 'void BBLTopbar::RefreshBrandTile()'), /topbar_brand_tile_bitmap\(this\)/);

  const about = read('src/slic3r/GUI/AboutDialog.cpp');
  const banner = functionBody(about, 'static wxBitmap about_banner_with_logo(');
  assert.match(banner, /app_logo_settings\(\)/);
  assert.match(banner, /AppLogo::Target::About/);
  assert.match(about, /about_banner_with_logo\(m_logo_bitmap\.bmp\(\)\)/);

  const splash = functionBody(app, 'void Decorate(wxBitmap& bmp)');
  assert.match(splash, /app_logo_settings\(\)/);
  assert.match(splash, /AppLogo::Target::StartupScreen/);
  assert.match(splash, /load_svg\("splash_logo"/, 'the shipped startup artwork stays the default');
});

test('identity never reads the presentation-only logo key', () => {
  const allowed = new Set([
    'src/libslic3r/AppLogo/Logo.hpp',
    'src/slic3r/GUI/GUI_App.cpp',
    'src/slic3r/GUI/Preferences.cpp',
  ]);
  const offenders = [];
  const walk = (dir) => {
    for (const entry of fs.readdirSync(path.join(root, dir), { withFileTypes: true })) {
      const relative = `${dir}/${entry.name}`;
      if (entry.isDirectory()) walk(relative);
      else if (/\.(c|cc|cpp|h|hpp)$/.test(entry.name)) {
        const text = read(relative);
        if (/AppLogo::config_key|"app_logo"/.test(text) && !allowed.has(relative)) offenders.push(relative);
      }
    }
  };
  walk('src');
  assert.deepEqual(offenders, []);
  // GUI_App.cpp also hosts the updater, data folder and instance identity, so
  // the logo may appear there only in its getter, its setter and the splash.
  let rest = app.replace('#include "libslic3r/AppLogo/Logo.hpp"', '').replace('#include "AppLogo/LogoRender.hpp"', '');
  for (const marker of ['AppLogo::Settings GUI_App::app_logo_settings() const', 'bool GUI_App::set_app_logo_settings(',
                        'void Decorate(wxBitmap& bmp)']) {
    const body = functionBody(rest, marker);
    const start = rest.indexOf(marker);
    rest = rest.slice(0, start) + rest.slice(rest.indexOf(body, start) + body.length);
  }
  assert.doesNotMatch(rest, /AppLogo|app_logo/);
});

test('every logo panel string has a Cantonese entry and the article has its twin', () => {
  const panel = read('src/slic3r/GUI/AppLogo/LogoPanel.cpp');
  const po = read('bbl/i18n/yue_HK/BambuStudio_yue_HK.po');
  const ids = [...panel.matchAll(/_L\("((?:[^"\\]|\\.)*)"\)/g)].map((m) => m[1]);
  assert.ok(ids.length >= 20, 'expected the panel copy to be wrapped for translation');
  const missing = ids.filter((id) => !po.includes(`msgid "${id}"`));
  assert.deepEqual(missing, []);
  assert.ok(fs.existsSync(path.join(root, 'docs/features/appearance/app-logo.yue_HK.md')));
});
