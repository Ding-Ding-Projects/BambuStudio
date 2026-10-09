import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync, spawnSync} from 'node:child_process';

// Syntax-only regression using the real Button and installed wxWidgets headers.
// Windows: an MSVC developer environment with --wx-include and --wx-setup.
// Elsewhere those may be omitted: wx-config (WX_CONFIG) names the installed
// headers and the host C++ compiler (CXX, default c++) checks the same probe.
// No executable or window is created.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arg = name => {const i = process.argv.indexOf(name); return i < 0 ? undefined : process.argv[i + 1];};
const base = 'ff8a0728f02c1e66bb9df2dd2e6220b3eb2e4bb3';
const revision = arg('--source-revision');
const cpp = 'src/slic3r/GUI/DeviceTab/wgtDeviceNozzleRackUpdate.cpp';
const hpp = 'src/slic3r/GUI/DeviceTab/wgtDeviceNozzleRackUpdate.h';
const fromGit = (rev, file) => execFileSync('git', ['show', rev + ':' + file], {cwd: root, encoding: 'utf8'}).replaceAll('\r\n', '\n');
const read = file => revision ? fromGit(revision, file) : fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const source = read(cpp);
const header = read(hpp);
const member = header.match(/^\s*Button\*\s*m_status_bitmap\{ nullptr \};/m)?.[0].trim();
assert(member, 'Actual production Button pointer declaration is required');
const start = source.indexOf('void wgtDeviceNozzleRackHotendUpdate::Rescale()');
assert(start >= 0);
const call = source.slice(start).match(/^\s*m_status_bitmap->(?:msw_rescale|Rescale)\(\);/m)?.[0].trim();
assert(call, 'Actual production scaling call is required');
if (!revision) {
  // The entire file must remain unchanged apart from this single API correction,
  // including hardware commands, event bodies, glyph/color choices and layout.
  assert.equal(source.replace('m_status_bitmap->Rescale();', 'm_status_bitmap->msw_rescale();'), fromGit(base, cpp));
}
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
const include = arg('--wx-include') ?? hostDirs.find(d => fs.existsSync(path.join(d, 'wx', 'window.h')));
const setup = arg('--wx-setup') ?? hostDirs.find(d => fs.existsSync(path.join(d, 'wx', 'setup.h')));
assert(include && setup, 'Pass --wx-include and --wx-setup for real configured wxWidgets headers');
assert(fs.existsSync(path.join(include, 'wx', 'window.h')));
assert(fs.existsSync(path.join(setup, 'wx', 'setup.h')));
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'nozzle-status-button-'));
const file = path.join(directory, 'button-api.cpp');
fs.writeFileSync(file, `#include <type_traits>
#include "${path.join(root, 'src/slic3r/GUI/Widgets/Button.hpp').replaceAll('\\', '/')}"
static_assert(std::is_base_of<wxWindow, Button>::value, "Use the actual native widget hierarchy");
struct StatusButtonProbe {
    ${member}
    void rescale() { ${call} }
};
`);
const result = hostFlags
  ? spawnSync(process.env.CXX ?? 'c++', ['-std=c++17', '-fsyntax-only', '-DUNICODE', '-D_UNICODE', ...hostFlags,
    '-I' + path.join(root, 'src'), file], {cwd: directory, encoding: 'utf8'})
  : spawnSync('cl.exe', ['/nologo', '/Zs', '/std:c++17', '/EHsc', '/D__WXMSW__', '/DUNICODE', '/D_UNICODE',
    '/I' + setup, '/I' + include, '/I' + path.join(root, 'src'), file], {cwd: directory, encoding: 'utf8'});
if (result.error) throw result.error;
const output = result.stdout + result.stderr;
if (process.argv.includes('--expect-missing-member')) {
  assert.notEqual(result.status, 0, 'Old production call must fail');
  // GCC: 'class Button' has no member named 'msw_rescale'; Clang: no member
  // named 'msw_rescale' in 'Button'; MSVC: C2039.
  const missing = hostFlags
    ? output.split('\n').some(line => /error: .*no member named .msw_rescale./.test(line) && line.includes('Button'))
    : /error C2039:.*msw_rescale.*Button/.test(output);
  assert(missing, 'Expected actual missing Button member');
  console.log('Old production call: compiler exit ' + result.status + ', ' + (hostFlags ? 'missing Button member' : 'C2039') + ' observed');
  console.log(output.trim());
} else {
  assert.equal(result.status, 0, output);
  console.log('Production Button declaration and scaling call: compiler exit 0; remaining production file preserved exactly');
}
