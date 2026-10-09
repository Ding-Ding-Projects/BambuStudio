import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync, spawnSync} from 'node:child_process';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arg = name => { const i = process.argv.indexOf(name); return i < 0 ? null : process.argv[i + 1]; };
const revision = arg('--source-revision');
const file = 'src/slic3r/GUI/Widgets/ListBox.cpp';
const source = (revision ? execFileSync('git', ['show', revision + ':' + file], {cwd: root, encoding: 'utf8'})
    : fs.readFileSync(path.join(root, file), 'utf8')).replaceAll('\r\n', '\n');
function body(name) {
    const start = source.indexOf(name), begin = source.indexOf('{', start);
    assert(start >= 0, name);
    let depth = 0;
    for (let i = begin; i < source.length; ++i) {
        if (source[i] === '{') ++depth;
        if (source[i] === '}' && --depth === 0) return source.slice(start, i + 1);
    }
    assert.fail(name);
}
// --extract only writes the production bodies for the documented manual MSVC
// command. Without it the same harness is compiled and executed here: cl.exe
// from an initialized MSVC prompt on Windows, the host C++ compiler elsewhere.
const extractOnly = arg('--extract');
const output = extractOnly ?? fs.mkdtempSync(path.join(os.tmpdir(), 'atlas-listbox-hover-'));
fs.mkdirSync(output, {recursive: true});
const transitions = ['void ListBox::onMotion(', 'void ListBox::onLeave(', 'void ListBox::animateHover('].map(body).join('\n');
const paint = body('void ListBox::OnDrawBackground(');
const progress = paint.match(/const double progress = [^;]+;/)?.[0];
const hover = paint.match(/const double hover = [^;]+;/)?.[0];
assert(progress && hover);
fs.writeFileSync(path.join(output, 'atlas_listbox_hover.inc'), transitions + '\ndouble ListBox::hoverPaint(size_t n) const {\n'
    + progress + '\n' + hover + '\nreturn hover;\n}\n');
console.log('Extracted production motion/leave/transition bodies and paint expression' + (revision ? ' from ' + revision : ''));
if (!extractOnly) {
    const harness = path.join(root, 'tests/native_shared_controls/atlas_listbox_hover_tests.cpp');
    const executable = path.join(output, process.platform === 'win32' ? 'atlas_listbox_hover_tests.exe' : 'atlas_listbox_hover_tests');
    const compiled = process.platform === 'win32'
        ? spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/I' + output, harness,
            '/Fe' + executable, '/Fo' + path.join(output, 'atlas_listbox_hover_tests.obj')], {cwd: output, encoding: 'utf8'})
        : spawnSync(process.env.CXX ?? 'c++', ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + output, harness,
            '-o', executable], {cwd: output, encoding: 'utf8'});
    if (compiled.error) throw compiled.error;
    assert.equal(compiled.status, 0, compiled.stdout + compiled.stderr);
    const run = spawnSync(executable, [], {cwd: output, encoding: 'utf8'});
    console.log(run.stdout.trim());
    assert.equal(run.status, 0, run.stdout + run.stderr);
    assert.match(run.stdout, /6 interrupted-hover cases; 48 assertions; 0 failures/);
}
