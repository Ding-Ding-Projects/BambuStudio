import assert from 'node:assert/strict';
import { readdirSync, readFileSync, statSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// nlohmann::json::items() keeps a reference to its json. In C++17 a range-for
// over items() of a json returned by value (flatten(), unflatten(), patch(),
// diff(), parse()) walks an object that is destroyed before the loop starts.
// The version history comparison did this inside a worker thread, where the
// access violation would end the application. Name the json first.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const byValue = /for\s*\([^;{]*:\s*[^;{]*\b(?:flatten|unflatten|patch|diff|parse)\s*\([^;{]*\)\s*\.\s*items\s*\(\s*\)\s*\)/;

function sources(dir) {
    const out = [];
    for (const name of readdirSync(dir)) {
        const full = path.join(dir, name);
        if (statSync(full).isDirectory()) out.push(...sources(full));
        else if (/\.(cpp|hpp|h)$/.test(name)) out.push(full);
    }
    return out;
}

test('no range-for walks items() of a json returned by value', () => {
    const offenders = [];
    for (const file of [...sources(path.join(repoDir, 'src', 'slic3r')), ...sources(path.join(repoDir, 'src', 'libslic3r'))]) {
        readFileSync(file, 'utf8').split('\n').forEach((line, index) => {
            if (byValue.test(line)) offenders.push(`${path.relative(repoDir, file)}:${index + 1}`);
        });
    }
    assert.deepEqual(offenders, []);
});

test('the version history comparison names its flattened documents', () => {
    const text = readFileSync(path.join(repoDir, 'src/slic3r/GUI/ProjectHistoryDialog.cpp'), 'utf8');
    assert.match(text, /const nlohmann::json left_flat = left_document\.flatten\(\), right_flat = right_document\.flatten\(\);/);
});
