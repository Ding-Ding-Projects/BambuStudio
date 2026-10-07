import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync, spawnSync} from 'node:child_process';

// Syntax-only compilation of the complete production header and both production
// definitions. Run from an MSVC developer environment with the configured GUI
// project. All widget, device and wx headers are real; no substitute class exists.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arg = name => process.argv[process.argv.indexOf(name) + 1];
assert(process.argv.includes('--project'), 'Pass the configured libslic3r_gui.vcxproj using --project');
const project = fs.readFileSync(arg('--project'), 'utf8');
const includes = [...project.matchAll(/<AdditionalIncludeDirectories>([^<]+)</g)]
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
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'nozzle-card-declaration-'));
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
  const result = spawnSync('cl.exe', ['/nologo', '/Zs', '/std:c++17', '/EHsc', '/utf-8', '/bigobj',
    '/D__WXMSW__', '/DUNICODE', '/D_UNICODE', '/DNOMINMAX', '/DwxDEBUG_LEVEL=0', '/DBOOST_ALL_NO_LIB',
    '/I' + path.join(root, 'src'), ...includes.map(p => '/I' + p), file],
    {cwd:temporary, encoding:'utf8', timeout:60000});
  if (result.error) throw result.error;
  return {status:result.status, output:result.stdout + result.stderr};
}
const missing = header.replace(/^\s*void (?:MeasureCard|UpdateCardPresentation)\(\);\n/gm, '');
const red = compile('missing-declarations', missing);
assert.notEqual(red.status, 0, 'Missing production declarations must fail');
assert.match(red.output, /error C2039:.*MeasureCard/);
assert.match(red.output, /error C2039:.*UpdateCardPresentation/);
console.log('Negative: MSVC C2039 observed for both absent production declarations');
const current = compile('production-declarations', header);
if (process.argv.includes('--expect-mismatch')) {
  assert.notEqual(current.status, 0);
  assert.match(current.output, /error C2039:.*MeasureCard/);
  assert.match(current.output, /error C2039:.*UpdateCardPresentation/);
  console.log('Baseline: both methods rejected on their actual production owner');
} else {
  assert.equal(current.status, 0, current.output);
  console.log('Positive: complete production header and both actual method definitions compile, MSVC exit 0');
}
