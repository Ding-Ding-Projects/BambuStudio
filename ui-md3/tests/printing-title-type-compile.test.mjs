import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync, spawnSync} from 'node:child_process';

// Syntax-only compiler regression. Requires an MSVC developer environment and
// actual configured wxWidgets headers, never stand-in widget definitions.
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
const include = arg('--wx-include');
const setup = arg('--wx-setup');
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
const result = spawnSync('cl.exe', ['/nologo', '/Zs', '/std:c++17', '/EHsc', '/D__WXMSW__', '/DUNICODE', '/D_UNICODE',
  '/I' + setup, '/I' + include, file], {cwd: directory, encoding: 'utf8'});
if (result.error) throw result.error;
const output = result.stdout + result.stderr;
if (process.argv.includes('--expect-mismatch')) {
  assert.notEqual(result.status, 0, 'Old production contract must fail');
  assert.match(output, /C2664/, 'Expected the real pointer-conversion diagnostic');
  assert.equal((output.match(/error C2664:/g) || []).length, 2, 'Both production calls must reject the old type');
  console.log('Old production contract: compiler exit ' + result.status + ', C2664 observed at both calls');
  console.log(output.trim());
} else {
  assert.equal(result.status, 0, output);
  console.log('Production helper, member and both calls: compiler exit 0 using real wxWidgets and Label headers');
}
