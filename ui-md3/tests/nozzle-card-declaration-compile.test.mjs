import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync, spawnSync} from 'node:child_process';

// Syntax-only compilation of the complete production header and both production
// definitions. All widget, device and wx headers are real; no substitute class
// exists. Windows: an MSVC developer environment with the configured GUI project
// (--project). Elsewhere --project may be omitted: the host C++ compiler (CXX,
// default c++) uses the repository sources, the installed wx headers named by
// wx-config (WX_CONFIG), and libslic3r_version.h configured from its template
// with the values in version.inc, as CMake's configure_file does.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arg = name => process.argv[process.argv.indexOf(name) + 1];
const host = process.platform !== 'win32' && !process.argv.includes('--project');
assert(host || process.argv.includes('--project'), 'Pass the configured libslic3r_gui.vcxproj using --project');
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'nozzle-card-declaration-'));
function hostIncludes() {
  let flags;
  try {
    flags = execFileSync(process.env.WX_CONFIG ?? 'wx-config', ['--cxxflags'], {encoding: 'utf8'}).trim().split(/\s+/);
  } catch (error) {
    assert.fail('Pass --project, or provide wx-config for real installed wx headers: ' + error.message);
  }
  const values = Object.fromEntries([...fs.readFileSync(path.join(root, 'version.inc'), 'utf8')
    .matchAll(/^\s*set\((\w+) "([^"]*)"\)/gm)].map(m => [m[1], m[2]]));
  const generated = path.join(temporary, 'generated');
  fs.mkdirSync(generated);
  fs.writeFileSync(path.join(generated, 'libslic3r_version.h'),
    fs.readFileSync(path.join(root, 'src/libslic3r/libslic3r_version.h.in'), 'utf8').replace(/@(\w+)@/g, (_, name) => values[name] ?? ''));
  return {flags: flags.filter(f => !f.startsWith('-I')), includes: [...flags.filter(f => f.startsWith('-I')).map(f => f.slice(2)),
    path.join(root, 'src/slic3r'), path.join(root, 'src/eigen'), generated]};
}
const hostBuild = host ? hostIncludes() : null;
const includes = hostBuild ? hostBuild.includes : [...fs.readFileSync(arg('--project'), 'utf8').matchAll(/<AdditionalIncludeDirectories>([^<]+)</g)]
  .map(m => m[1].split(';').filter(p => !p.startsWith('%(')))
  .sort((a, b) => b.length - a.length)[0];
assert(includes?.some(p => fs.existsSync(path.join(p, 'wx', 'setup.h'))), 'Real configured wx setup required');
assert(includes.some(p => fs.existsSync(path.join(p, 'wx', 'window.h'))), 'Real wx headers required');
const revision = process.argv.includes('--source-revision') ? arg('--source-revision') : undefined;
const read = file => (revision ? execFileSync('git', ['show', `${revision}:${file}`], {cwd:root, encoding:'utf8'})
  : fs.readFileSync(path.join(root, file), 'utf8')).replaceAll('\r\n', '\n');
const prefix = 'src/slic3r/GUI/DeviceTab/wgtDeviceNozzleRack';
const header = read(prefix + '.h');
const source = read(prefix + '.cpp');
const definitions = ['MeasureCard', 'UpdateCardPresentation'].map(name => {
  const start = source.indexOf(`void wgtDeviceNozzleRackNozzleItem::${name}()`);
  assert(start >= 0, name);
  let end = source.indexOf('{', start) + 1, depth = 1;
  while (depth && end < source.length) {
    if (source[end] === '{') depth++;
    if (source[end] === '}') depth--;
    end++;
  }
  assert.equal(depth, 0);
  return source.slice(start, end);
}).join('\n');
function compile(name, contents) {
  fs.writeFileSync(path.join(temporary, name + '.h'), contents);
  const file = path.join(temporary, name + '.cpp');
  fs.writeFileSync(file, `#include <algorithm>
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "${name}.h"
namespace Slic3r::GUI {
${definitions}
}
`);
  const result = hostBuild
    ? spawnSync(process.env.CXX ?? 'c++', ['-std=c++17', '-fsyntax-only', ...hostBuild.flags,
      '-DUNICODE', '-D_UNICODE', '-DwxDEBUG_LEVEL=0', '-DBOOST_ALL_NO_LIB',
      '-I' + path.join(root, 'src'), ...includes.map(p => '-I' + p), file],
      {cwd:temporary, encoding:'utf8', timeout:120000})
    : spawnSync('cl.exe', ['/nologo', '/Zs', '/std:c++17', '/EHsc', '/utf-8', '/bigobj',
      '/D__WXMSW__', '/DUNICODE', '/D_UNICODE', '/DNOMINMAX', '/DwxDEBUG_LEVEL=0', '/DBOOST_ALL_NO_LIB',
      '/I' + path.join(root, 'src'), ...includes.map(p => '/I' + p), file],
      {cwd:temporary, encoding:'utf8', timeout:60000});
  if (result.error) throw result.error;
  return {status:result.status, output:result.stdout + result.stderr};
}
// MSVC: C2039. GCC: no declaration matches. Clang: out-of-line definition does
// not match any declaration.
const undeclared = name => hostBuild
  ? new RegExp(`error: (?:no declaration matches|out-of-line definition of)[^\\n]*${name}`)
  : new RegExp(`error C2039:.*${name}`);
const missing = header.replace(/^\s*void (?:MeasureCard|UpdateCardPresentation)\(\);\n/gm, '');
const red = compile('missing-declarations', missing);
assert.notEqual(red.status, 0, 'Missing production declarations must fail');
assert.match(red.output, undeclared('MeasureCard'));
assert.match(red.output, undeclared('UpdateCardPresentation'));
console.log(`Negative: ${hostBuild ? 'host compiler' : 'MSVC C2039'} rejects both absent production declarations`);
const current = compile('production-declarations', header);
if (process.argv.includes('--expect-mismatch')) {
  assert.notEqual(current.status, 0);
  assert.match(current.output, undeclared('MeasureCard'));
  assert.match(current.output, undeclared('UpdateCardPresentation'));
  console.log('Baseline: both methods rejected on their actual production owner');
} else {
  assert.equal(current.status, 0, current.output);
  console.log(`Positive: complete production header and both actual method definitions compile, ${hostBuild ? 'host compiler' : 'MSVC'} exit 0`);
}
