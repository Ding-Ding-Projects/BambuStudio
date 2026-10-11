import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';
import {execFileSync, spawnSync} from 'node:child_process';

// The kit SpinInput replaced wxSpinCtrl, so its keyboard and typing rules must
// match it: Up reaches the maximum and Down the minimum, and a minus sign can
// be typed (once, first) exactly when the range reaches below zero. The
// production bodies are extracted from SpinInput.cpp and compiled against
// non-window stubs (spin_input_keys_tests.cpp): cl.exe from an initialized MSVC
// prompt on Windows, the host C++ compiler (CXX, default c++) elsewhere.
// --source-revision <rev> runs the same harness against an older SpinInput.cpp.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const at = process.argv.indexOf('--source-revision');
const file = 'src/slic3r/GUI/Widgets/SpinInput.cpp';
const source = (at < 0 ? fs.readFileSync(path.join(root, file), 'utf8')
    : execFileSync('git', ['show', `${process.argv[at + 1]}:${file}`], {cwd: root, encoding: 'utf8'})).replaceAll('\r\n', '\n');
const code = source.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');

function body(signature) {
    const start = source.indexOf(signature);
    assert.ok(start >= 0, `SpinInput.cpp has no ${signature}`);
    const open = source.indexOf('{', start);
    let depth = 0;
    for (let i = open; i < source.length; ++i) {
        if (source[i] === '{') ++depth;
        if (source[i] === '}' && --depth === 0) return source.slice(start, i + 1);
    }
    assert.fail(signature);
}

function run(define, bodies, summary) {
    const output = fs.mkdtempSync(path.join(os.tmpdir(), 'spin-input-keys-'));
    fs.writeFileSync(path.join(output, 'spin_input_keys.inc'), bodies.map(body).join('\n') + '\n');
    const harness = path.join(root, 'tests/native_shared_controls/spin_input_keys_tests.cpp');
    const executable = path.join(output, process.platform === 'win32' ? 'spin_input_keys.exe' : 'spin_input_keys');
    const compiled = process.platform === 'win32'
        ? spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/D' + define, '/I' + output, harness,
            '/Fe' + executable, '/Fo' + path.join(output, 'spin_input_keys.obj')], {cwd: output, encoding: 'utf8'})
        : spawnSync(process.env.CXX ?? 'c++', ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-D' + define, '-I' + output, harness,
            '-o', executable], {cwd: output, encoding: 'utf8'});
    if (compiled.error) throw compiled.error;
    assert.equal(compiled.status, 0, compiled.stdout + compiled.stderr);
    const result = spawnSync(executable, [], {cwd: output, encoding: 'utf8'});
    console.log(result.stdout.trim());
    assert.equal(result.status, 0, result.stdout + result.stderr);
    assert.match(result.stdout, summary);
}

test('Up reaches the maximum and Down the minimum, and Enter keeps the value in range', () => {
    run('SPIN_KEYS', ['void SpinInput::SetValue(int value)', 'void SpinInput::keyPressed(', 'void SpinInput::onTextEnter('],
        /^8 key cases; 16 assertions; 0 failures$/m);
});

test('a typed minus is accepted only first and only when the minimum is below zero', () => {
    run('SPIN_CHARS', ['bool spinAcceptsChar('], /^5 character cases; 33 assertions; 0 failures$/m);
});

test('every typed character of the editor goes through that rule', () => {
    // wxFILTER_DIGITS refused the sign outright, and the wxFILTER_NUMERIC swap
    // for a negative minimum let ".", ",", "e", "+" and a minus anywhere through,
    // which the whole-number field then silently dropped. One rule replaces both,
    // reading the current minimum, so a later SetRange needs no validator swap.
    assert.doesNotMatch(code, /wxTextValidator|SetValidator/, 'no text validator on the spin editor');
    assert.match(body('void SpinInput::Create('),
        /text_ctrl->Bind\(wxEVT_CHAR, \[this\]\(wxKeyEvent &event\) \{\s*long from = 0, to = 0;\s*text_ctrl->GetSelection\(&from, &to\);\s*if \(spinAcceptsChar\(text_ctrl->GetValue\(\), from, to, int\(event\.GetUnicodeKey\(\)\), this->min\)\)\s*event\.Skip\(\);\s*\}\);/);
});
