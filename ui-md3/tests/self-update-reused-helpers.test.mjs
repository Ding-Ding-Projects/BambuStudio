import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The self-update diagnostic loads its observation helpers from the installer
// first-run diagnostic's source by name. Its first hosted run stopped with
// "The term 'New-ProcessRecord' is not recognized", because a loaded helper
// called a first-run function that was not on the list. Every first-run
// function reachable from the loaded ones must be loaded too.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (file) => readFileSync(path.join(repoDir, file), 'utf8').replace(/\r\n/g, '\n');
const firstRun = read('scripts/ci/Diagnose-InstallerFirstRun.ps1');
const selfUpdate = read('scripts/ci/Diagnose-SelfUpdate.ps1');

function functionBodies(source) {
    const bodies = new Map();
    const starts = [...source.matchAll(/^function ([A-Za-z]+-[A-Za-z0-9]+) \{/gm)];
    starts.forEach((match, index) => {
        const end = index + 1 < starts.length ? starts[index + 1].index : source.length;
        bodies.set(match[1], source.slice(match.index, end));
    });
    return bodies;
}

test('every first-run helper the loaded helpers call is loaded too', () => {
    const bodies = functionBodies(firstRun);
    const listMatch = selfUpdate.match(/\$ReusedFunctions = @\(([\s\S]*?)\)/);
    assert.ok(listMatch, 'the reused function list is present');
    const listed = new Set([...listMatch[1].matchAll(/'([^']+)'/g)].map((m) => m[1]));
    const missing = new Set();
    const pending = [...listed];
    const seen = new Set(pending);
    while (pending.length) {
        const name = pending.pop();
        const body = bodies.get(name);
        assert.ok(body, `${name} is defined in the first-run diagnostic`);
        for (const callee of bodies.keys()) {
            if (callee === name || seen.has(callee)) continue;
            if (new RegExp(`(?<![\\w-])${callee}(?![\\w-])`).test(body.slice(body.indexOf('{')))) {
                seen.add(callee);
                pending.push(callee);
                if (!listed.has(callee)) missing.add(callee);
            }
        }
    }
    assert.deepEqual([...missing], []);
});
