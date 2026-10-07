import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// source_group(TREE <root>) stops the configure step on every generator when a
// listed file lies outside <root>. libslic3r_gui compiles shared
// ../libslic3r service sources, and the hosted Windows build failed with
// "source_group ROOT: .../src/slic3r is not a prefix of file:
// .../src/libslic3r/ScheduledSettings/Schedule.cpp". Every source tree is
// grouped through cmake/modules/SourceGroupTree.cmake instead, which sends files
// outside the root to a separate prefixed group.
//
// The fixture project uses no languages, so it runs on any host with CMake.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const cmakePath = (file) => file.replaceAll('\\', '/');
const modulePath = cmakePath(path.join(repoDir, 'cmake', 'modules', 'SourceGroupTree.cmake'));
const cmakeMissing = spawnSync('cmake', ['--version']).status !== 0 && 'cmake is not installed';

function makeFixture(groupCall) {
    const root = mkdtempSync(path.join(tmpdir(), 'source group tree '));
    const gui = path.join(root, 'src', 'slic3r');
    for (const dir of ['src/slic3r/GUI/Panels', 'src/libslic3r/Services', 'elsewhere']) {
        mkdirSync(path.join(root, dir), { recursive: true });
    }
    for (const file of ['src/slic3r/GUI/Panels/Panel.cpp', 'src/slic3r/Main.cpp', 'src/libslic3r/Services/Service.cpp', 'elsewhere/Extra.cpp']) {
        writeFileSync(path.join(root, file), '');
    }
    const extra = cmakePath(path.join(root, 'elsewhere', 'Extra.cpp'));
    writeFileSync(path.join(gui, 'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.15)
project(SourceGroupTreeFixture NONE)
include("${modulePath}")
set(fixture_sources GUI/Panels/Panel.cpp Main.cpp ../libslic3r/Services/Service.cpp "${extra}")
${groupCall}
`);
    return { root, gui, build: path.join(root, 'build') };
}

function configure(fixture, extraArgs = []) {
    return spawnSync('cmake', [...extraArgs, '-S', fixture.gui, '-B', fixture.build], { encoding: 'utf8' });
}

function sourceGroupCalls(traceText) {
    return traceText
        .split('\n')
        .filter((line) => line.startsWith('{'))
        .map((line) => JSON.parse(line))
        .filter((entry) => entry.cmd && entry.cmd.toLowerCase() === 'source_group')
        .map((entry) => entry.args.flatMap((arg) => arg.split(';')));
}

test('a plain source_group(TREE) rejects a sibling-directory source (the hosted failure)', { skip: cmakeMissing }, () => {
    const fixture = makeFixture('source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${fixture_sources})');
    try {
        const result = configure(fixture);
        assert.notEqual(result.status, 0);
        assert.match(result.stderr, /source_group ROOT:\s.*\sis\s+not\s+a\s+prefix\s+of\s+file:/s);
    } finally {
        rmSync(fixture.root, { recursive: true, force: true });
    }
});

test('bambu_source_group_tree groups inside, sibling and unrelated files without failing', { skip: cmakeMissing }, () => {
    const fixture = makeFixture(`bambu_source_group_tree(ROOT \${CMAKE_CURRENT_SOURCE_DIR}
    OUTSIDE_ROOT \${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTSIDE_PREFIX "Shared"
    FILES \${fixture_sources})`);
    try {
        const result = configure(fixture, ['--trace-expand', '--trace-format=json-v1', '--trace-redirect=' + path.join(fixture.root, 'trace.json')]);
        assert.equal(result.status, 0, result.stderr);
        const calls = sourceGroupCalls(readFileSync(path.join(fixture.root, 'trace.json'), 'utf8'));
        const src = cmakePath(path.join(fixture.root, 'src'));
        assert.deepEqual(calls, [
            ['TREE', `${src}/slic3r`, 'FILES', `${src}/slic3r/GUI/Panels/Panel.cpp`, `${src}/slic3r/Main.cpp`],
            ['TREE', src, 'PREFIX', 'Shared', 'FILES', `${src}/libslic3r/Services/Service.cpp`],
            ['Shared', 'FILES', cmakePath(path.join(fixture.root, 'elsewhere', 'Extra.cpp'))],
        ]);
    } finally {
        rmSync(fixture.root, { recursive: true, force: true });
    }
});

test('bambu_source_group_tree refuses a call without its roots and prefix', { skip: cmakeMissing }, () => {
    const fixture = makeFixture('bambu_source_group_tree(ROOT ${CMAKE_CURRENT_SOURCE_DIR} FILES ${fixture_sources})');
    try {
        const result = configure(fixture);
        assert.notEqual(result.status, 0);
        assert.match(result.stderr, /needs ROOT, OUTSIDE_ROOT and OUTSIDE_PREFIX/);
    } finally {
        rmSync(fixture.root, { recursive: true, force: true });
    }
});

test('no project CMake file calls source_group(TREE) directly', () => {
    const offenders = [];
    const skip = new Set(['.git', 'node_modules', 'deps', 'build', 'deps_src', '3rdparty']);
    const walk = (dir) => {
        for (const name of readdirSync(dir)) {
            if (skip.has(name)) continue;
            const full = path.join(dir, name);
            const stat = statSync(full);
            if (stat.isDirectory()) {
                walk(full);
            } else if (name === 'CMakeLists.txt' || name.endsWith('.cmake')) {
                const relative = cmakePath(path.relative(repoDir, full));
                if (relative === 'cmake/modules/SourceGroupTree.cmake') continue;
                if (/source_group\s*\(\s*TREE\b/i.test(readFileSync(full, 'utf8'))) offenders.push(relative);
            }
        }
    };
    walk(path.join(repoDir, 'src'));
    walk(path.join(repoDir, 'cmake'));
    assert.deepEqual(offenders, []);
});

test('the GUI and core libraries group their sources through the helper', () => {
    for (const file of ['src/slic3r/CMakeLists.txt', 'src/libslic3r/CMakeLists.txt']) {
        const text = readFileSync(path.join(repoDir, file), 'utf8');
        assert.match(text, /include\(SourceGroupTree\)/, file);
        assert.match(text, /bambu_source_group_tree\(ROOT \$\{CMAKE_CURRENT_SOURCE_DIR\}\s+OUTSIDE_ROOT \$\{CMAKE_CURRENT_SOURCE_DIR\}\/\.\.\s+OUTSIDE_PREFIX "Shared"/, file);
    }
});
