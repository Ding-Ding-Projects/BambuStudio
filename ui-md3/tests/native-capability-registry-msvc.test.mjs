import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The hosted Windows build (runs 825 and 828) stopped in AutomationBridge.cpp
// and NativeCapabilityRegistry.cpp with C2440 and C2653: MSVC cannot resolve
// the class-scope `Clock` alias inside a lambda used as a default argument of a
// Registry member. The registry therefore keeps its clock default in a
// delegating constructor and a static member function.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const header = readFileSync(path.join(repoDir, 'src/slic3r/GUI/LocalCapabilities/NativeCapabilityRegistry.hpp'), 'utf8');

test('no Registry member takes a lambda as a default argument', () => {
    const lambdaDefault = /\(\s*[^()]*=\s*\[[^\]]*\]\s*(\([^)]*\))?\s*\{/;
    const offenders = header.split('\n').filter((line) => lambdaDefault.test(line));
    assert.deepEqual(offenders, []);
});

test('the one-argument constructor delegates to the steady clock', () => {
    assert.match(header, /explicit Registry\(Random random\) : Registry\(std::move\(random\), Now\(&Registry::steady_now\)\) \{\}/);
    assert.match(header, /static Clock::time_point steady_now\(\) \{ return Clock::now\(\); \}/);
});
