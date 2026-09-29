import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The splash screen says when the running version was released
// (docs/features/windows/splash-release-date.md). These contracts pin the
// three pieces that have to agree: the build records its moment in UTC, the
// splash draws a date line from it, and the line follows every language mode.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFile(path.join(repoDir, ...parts), 'utf8');

// "must contain" assertions run against code only, so a comment that merely
// names a construct cannot satisfy them.
const stripComments = (source) => source
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/^[ \t]*\/\/.*$/gm, '')
  .replace(/[ \t]+\/\/[^"\n]*$/gm, '');

const versionHeader = await read('src', 'libslic3r', 'libslic3r_version.h.in');
const buildTimeHeader = await read('src', 'libslic3r', 'libslic3r_build_time.h.in');
const libslic3rCmake = await read('src', 'libslic3r', 'CMakeLists.txt');
const guiApp = await read('src', 'slic3r', 'GUI', 'GUI_App.cpp');

test('the build records the moment it was made in UTC', () => {
  assert.match(
    stripComments(libslic3rCmake),
    /string\(TIMESTAMP\s+SLIC3R_BUILD_TIME_UTC\s+"%Y-%m-%dT%H:%M:%SZ"\s+UTC\)/,
    'the local-time build stamp carries no offset, so the release date needs its own UTC stamp'
  );
  assert.match(buildTimeHeader, /#define SLIC3R_BUILD_TIME_UTC "@SLIC3R_BUILD_TIME_UTC@"/);
  assert.match(stripComments(libslic3rCmake), /configure_file\([^)]*libslic3r_build_time\.h\.in[^)]*libslic3r_build_time\.h @ONLY\)/,
    'the build time is generated into its own header');
  assert.match(guiApp, /^#include "libslic3r_build_time\.h"\r?$/m, 'and the splash screen reads it from there');
});

test('the version header carries nothing that changes from one build to the next', () => {
  // libslic3r_version.h is included by libslic3r.h and by the precompiled
  // header, so a per-build value in it made every object out of date and no
  // build could reuse an earlier build's objects (the Windows build cache).
  assert.doesNotMatch(versionHeader, /SLIC3R_BUILD_TIME/, 'the build time lives in libslic3r_build_time.h');
});

test('the splash screen draws the release date under its title', () => {
  const splash = guiApp.match(/class BBLSplashScreen[\s\S]*?\n\};/);
  assert.ok(splash, 'BBLSplashScreen must exist');
  const code = stripComments(splash[0]);
  assert.match(code, /release\s*=\s*splash_release_date_text\(\)/);
  assert.match(code, /DrawLabel\(m_constant_text\.release,/);
});

test('the release date line follows every language mode', () => {
  const helper = guiApp.match(/static wxString splash_release_date_text\(\)[\s\S]*?\n\}/);
  assert.ok(helper, 'splash_release_date_text() must exist');
  const code = stripComments(helper[0]);
  assert.match(code, /SLIC3R_BUILD_TIME_UTC/);
  assert.match(code, /MakeFromUTC\(\)/, 'the stamp is UTC; the splash shows the date in the user\'s time zone');
  assert.match(code, /L\("Released %s"\)/, 'a public release build says Released');
  assert.match(code, /L\("Built %s"\)/, 'any other build says Built');
  assert.match(code, /translate_mode\(/);
  assert.match(code, /render_localized_text_stacked\(/, 'bilingual mode stacks the English and Cantonese lines');
});

test('both release date messages have a Cantonese translation', async () => {
  const po = await read('bbl', 'i18n', 'yue_HK', 'BambuStudio_yue_HK.po');
  for (const id of ['Released %s', 'Built %s']) {
    const entry = po.match(new RegExp(`\\nmsgid "${id}"\\r?\\nmsgstr "([^"]*)"`));
    assert.ok(entry, `${id} needs an entry in the Cantonese catalogue`);
    assert.ok(entry[1].includes('%s') && entry[1] !== id, `${id} needs a Cantonese msgstr that keeps %s`);
  }
});

test('with the layout probe on, the splash leaves the bitmap it shows beside the probe dumps', async () => {
  // The splash lives for well under a second, so a screenshot of its window
  // comes back before it paints. The capture of a built release reads the
  // exact bitmap the splash shows instead.
  const probeHeader = await read('src', 'slic3r', 'GUI', 'LayoutProbe.hpp');
  assert.match(probeHeader, /std::string artifact_path\(const std::string &file_name\);/);
  const probeSource = stripComments(await read('src', 'slic3r', 'GUI', 'LayoutProbe.cpp'));
  assert.match(probeSource, /std::string artifact_path\(const std::string &file_name\)\s*\{/);
  const code = stripComments(guiApp);
  assert.match(
    code,
    /Decorate\(m_main_bitmap\);\s*if \(LayoutProbe::enabled\(\)\)\s*m_main_bitmap\.SaveFile\(wxString::FromUTF8\(LayoutProbe::artifact_path\("splash\.png"\)\), wxBITMAP_TYPE_PNG\);/,
    'the decorated bitmap is saved right after it is drawn, only when the probe is on'
  );
});
