import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync, spawnSync } from 'node:child_process';

// On Windows run in an MSVC developer environment (cl.exe); elsewhere the host
// C++ compiler (CXX, default c++) compiles the same probe with warnings as
// errors. Only the production initializer and production density selection
// code are compiled, without creating a window.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const revision = process.argv.indexOf('--source-revision');
const read = file => (revision < 0 ? fs.readFileSync(path.join(root, file), 'utf8') :
    execFileSync('git', ['show', `${process.argv[revision + 1]}:${file}`], {cwd: root, encoding: 'utf8'})).replaceAll('\r\n', '\n');
const header = read('src/slic3r/GUI/Widgets/StaticBox.hpp');
const tokens = read('src/slic3r/GUI/Widgets/MD3Tokens.hpp');
const declaration = header.match(/^\s*double m_default_radius_dip\{[^\n]+/m)?.[0].trim();
assert.ok(declaration, 'Missing production default-radius declaration');
const metrics = tokens.match(/struct DensityMetrics\s*\{[^}]+\};/)?.[0];
assert.ok(metrics, 'Missing production density metrics');
const selectionStart = tokens.indexOf('namespace Metrics {');
const selectionEnd = tokens.indexOf('inline constexpr int top_bar_height', selectionStart);
assert.ok(selectionStart >= 0 && selectionEnd > selectionStart);
const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'static-box-radius-'));
const source = path.join(directory, 'radius.cpp');
const msvc = process.platform === 'win32';
const executable = path.join(directory, msvc ? 'radius.exe' : 'radius');
fs.writeFileSync(source, `#include <cassert>
#include <iostream>
namespace MD3 {
${metrics}
${tokens.slice(selectionStart, selectionEnd)}
} }
struct RadiusProbe { ${declaration} };
int main() {
    MD3::Metrics::setDensity(MD3::Metrics::Density::Comfortable);
    const RadiusProbe comfortable;
    assert(comfortable.m_default_radius_dip == 16.0);
    MD3::Metrics::setDensity(MD3::Metrics::Density::Compact);
    const RadiusProbe compact;
    assert(compact.m_default_radius_dip == 12.0);
    assert(comfortable.m_default_radius_dip == 16.0);
    MD3::Metrics::setDensity(MD3::Metrics::Density::Comfortable);
    const RadiusProbe restored;
    assert(restored.m_default_radius_dip == 16.0);
    std::cout << "Runtime density initialization: 4 assertions passed\\n";
}
`);
const compiled = msvc
    ? spawnSync('cl.exe', ['/nologo', '/std:c++17', '/permissive-', '/EHsc', '/W4', '/WX',
        source, `/Fo${path.join(directory, 'radius.obj')}`, `/Fe${executable}`], {cwd: directory, encoding: 'utf8'})
    : spawnSync(process.env.CXX ?? 'c++', ['-std=c++17', '-pedantic-errors', '-Wall', '-Wextra', '-Werror',
        source, '-o', executable], {cwd: directory, encoding: 'utf8'});
if (compiled.error) throw compiled.error;
const output = compiled.stdout + compiled.stderr;
if (process.argv.includes('--expect-narrowing')) {
    assert.notEqual(compiled.status, 0, 'Old declaration must fail compilation');
    // GCC: narrowing conversion; Clang: cannot be narrowed; MSVC: C2397.
    assert.match(output, msvc ? /C2397/ : /narrowing conversion|cannot be narrowed/, 'Expected the actual narrowing diagnostic');
    console.log(`Old production declaration: compiler exit ${compiled.status}, ${msvc ? 'C2397' : 'narrowing conversion'} observed`);
    console.log(output.trim());
} else {
    assert.equal(compiled.status, 0, output);
    const result = spawnSync(executable, [], {cwd: directory, encoding: 'utf8'});
    assert.equal(result.status, 0, result.stdout + result.stderr);
    console.log('Production declaration: compiler exit 0');
    console.log(result.stdout.trim());
}
