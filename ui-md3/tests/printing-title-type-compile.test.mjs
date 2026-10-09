import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync, spawnSync} from 'node:child_process';

// Syntax-only compiler regression against actual installed wxWidgets headers,
// never stand-in widget definitions. Windows: an MSVC developer environment with
// --wx-include and --wx-setup. Elsewhere those may be omitted: wx-config
// (WX_CONFIG) names the installed headers and the host C++ compiler (CXX,
// default c++) checks the same probe.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arg = name => {
  const i = process.argv.indexOf(name);
  return i < 0 ? undefined : process.argv[i + 1];
};
const revision = arg('--source-revision');
const read = file => (revision ? execFileSync('git', ['show', revision + ':' + file], {cwd: root, encoding: 'utf8'}) :
  fs.readFileSync(path.join(root, file), 'utf8')).replaceAll('\r\n', '\n');
const source = read('src/slic3r/GUI/StatusPanel.cpp');
const header = read('src/slic3r/GUI/StatusPanel.hpp');
const printingClass = header.slice(header.indexOf('class PrintingTaskPanel :'), header.indexOf('class StatusBasePanel :'));
const member = printingClass.match(/^\s*(?:wxStaticText|Label)\s*\*\s*m_staticText_printing\s*;/m)?.[0].trim();
assert(member, 'Missing actual PrintingTaskPanel member');
const helper = source.match(/static void layout_printing_title\([^)]*\)\s*\{[^}]*\}/)?.[0];
assert(helper, 'Missing actual title helper');
const calls = [...source.matchAll(/^\s*layout_printing_title\(m_panel_printing_title, m_staticText_printing\);/gm)];
assert.equal(calls.length, 2, 'Creation and rescale both use the measured helper');
const msvc = process.platform === 'win32';
let hostFlags = null;
if (!msvc && !arg('--wx-include') && !arg('--wx-setup')) {
  try {
    hostFlags = execFileSync(process.env.WX_CONFIG ?? 'wx-config', ['--cxxflags'], {encoding: 'utf8'}).trim().split(/\s+/);
  } catch (error) {
    assert.fail('Pass --wx-include and --wx-setup, or provide wx-config for real installed wxWidgets headers: ' + error.message);
  }
}
const hostDirs = (hostFlags ?? []).filter(f => f.startsWith('-I')).map(f => f.slice(2));
const include = arg('--wx-include') ?? hostDirs.find(d => fs.existsSync(path.join(d, 'wx', 'stattext.h')));
const setup = arg('--wx-setup') ?? hostDirs.find(d => fs.existsSync(path.join(d, 'wx', 'setup.h')));
assert(include && setup, 'Pass --wx-include and --wx-setup for real configured wxWidgets headers');
assert(fs.existsSync(path.join(include, 'wx', 'stattext.h')));
assert(fs.existsSync(path.join(setup, 'wx', 'setup.h')));
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'printing-title-type-'));
const file = path.join(directory, 'printing-title.cpp');
fs.writeFileSync(file, `#include <algorithm>
#include <type_traits>
#include <wx/panel.h>
#include <wx/sizer.h>
#include "${path.join(root, 'src/slic3r/GUI/Widgets/Label.hpp').replaceAll('\\', '/')}"
static_assert(std::is_base_of<wxStaticText, Label>::value, "Production Label must derive from wxStaticText");
${helper}
struct PrintingTitleProbe {
    wxPanel *m_panel_printing_title;
    ${member}
    void create() { ${calls[0][0]} }
    void rescale() { ${calls[1][0]} }
};
`);
const result = hostFlags
  ? spawnSync(process.env.CXX ?? 'c++', ['-std=c++17', '-fsyntax-only', '-DUNICODE', '-D_UNICODE', ...hostFlags, file],
    {cwd: directory, encoding: 'utf8'})
  : spawnSync('cl.exe', ['/nologo', '/Zs', '/std:c++17', '/EHsc', '/D__WXMSW__', '/DUNICODE', '/D_UNICODE',
    '/I' + setup, '/I' + include, file], {cwd: directory, encoding: 'utf8'});
if (result.error) throw result.error;
const output = result.stdout + result.stderr;
if (process.argv.includes('--expect-mismatch')) {
  assert.notEqual(result.status, 0, 'Old production contract must fail');
  // MSVC: C2664 at each call. GCC: an invalid wxStaticText* to Label* conversion
  // error at each call. Clang: no viable layout_printing_title call at each site,
  // explained by the same base-to-derived pointer conversion.
  const conversion = /(?:invalid conversion|cannot convert)[^\n]*wxStaticText[^\n]*Label/.test(output);
  const rejected = hostFlags
    ? (conversion ? output.split('\n').filter(line =>
      /error: (?:invalid conversion|cannot convert|no matching function for call to .layout_printing_title.)/.test(line)).length : 0)
    : (output.match(/error C2664:/g) || []).length;
  assert(rejected > 0, 'Expected the real pointer-conversion diagnostic\n' + output);
  assert.equal(rejected, 2, 'Both production calls must reject the old type\n' + output);
  console.log('Old production contract: compiler exit ' + result.status + ', ' + (hostFlags ? 'pointer conversion rejected' : 'C2664 observed') + ' at both calls');
  console.log(output.trim());
} else {
  assert.equal(result.status, 0, output);
  console.log('Production helper, member and both calls: compiler exit 0 using real wxWidgets and Label headers');
}
