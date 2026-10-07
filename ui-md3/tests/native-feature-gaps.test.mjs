import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// design/native-feature-gaps.json records every gap the source audit found
// between the native application and its feature contract. A gap moves from
// open to landed only with the main commit, tests and documentation that
// deliver it, and to verified only with a record of the built Windows
// application doing it. Gaps are never deleted: the audited ids below stay in
// the ledger until they are verified, so a gap that quietly disappears fails
// here.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const ledger = JSON.parse(readFileSync(path.join(repoDir, 'design', 'native-feature-gaps.json'), 'utf8'));

const AUDITED_GAPS_PER_FAMILY = {
    'school-mode': 12,
    'adhd-modes': 11,
    'scheduled-settings': 11,
    'language': 6,
    'funny-emoji': 7,
    'personal-vocabulary': 5,
    'narration': 6,
    'display-name': 3,
    'dim-sum': 8,
    'material-design': 10,
    'motion': 6,
    'appearance-editor': 14,
    'color-picker': 6,
    'logo-customization': 8,
    'overlay-panels': 6,
    'element-locks': 15,
    'support-tickets': 7,
    'unlock-ladder': 7,
    'authenticator': 13,
    'super-confirmation': 6,
    'tabs': 8,
    'regex-search': 8,
    'command-palette': 7,
    'collapse-filters': 5,
    'workflow-navigation': 3,
    'rich-controls': 4,
    'guided-forms': 6,
    'local-history': 13,
    'exports': 7,
    'bulk-actions': 7,
    'changelog': 7,
    'external-editor': 6,
    'blank-editors': 6,
    'notifications': 10,
    'progress-recovery': 10,
    'download-handoff': 8,
    'forge-publishing': 7,
    'file-converter': 13,
    'ollama-suite': 14,
    'offline-docs': 6,
    'front-provenance': 5,
    'product-evidence': 4,
    'completeness-parity': 5,
    'landing-and-docs-site': 10,
    'readme-and-recording': 4,
    'line-count': 4,
    'discord-embed': 4,
    'sanitized-instruction-copy': 2,
    'critic-frameless-window-chrome': 4,
    'critic-accessibility-and-sizing': 6,
    'critic-no-promotional-prompts': 3,
    'critic-no-analytics': 3,
    'critic-bundled-assets-and-fonts': 4,
    'critic-discard-history': 3,
    'critic-article-structure': 3,
    'critic-provider-markup': 3,
    'critic-remote-hosted-pages': 3,
    'critic-funny-level-first-run-disclosure': 2,
    'critic-status-corrections': 4,
};

const SIZES = new Set(['S', 'M', 'L', 'XL']);
const STATUSES = new Set(['open', 'landed', 'verified']);

test('every audited family and gap id is still in the ledger', () => {
    const families = new Set(ledger.families.map((family) => family.id));
    const ids = new Set(ledger.gaps.map((gap) => gap.id));
    for (const [family, count] of Object.entries(AUDITED_GAPS_PER_FAMILY)) {
        assert.ok(families.has(family), `family ${family} is missing`);
        for (let index = 1; index <= count; index += 1) {
            const id = `${family}#${String(index).padStart(2, '0')}`;
            assert.ok(ids.has(id), `gap ${id} is missing`);
        }
    }
    assert.equal(ids.size, ledger.gaps.length, 'gap ids are unique');
});

test('every gap names its family, contract clause, size and status', () => {
    const families = new Set(ledger.families.map((family) => family.id));
    for (const gap of ledger.gaps) {
        assert.match(gap.id, /^[a-z0-9-]+#\d{2}$/, gap.id);
        assert.equal(gap.id.split('#')[0], gap.family, gap.id);
        assert.ok(families.has(gap.family), gap.id);
        assert.ok(gap.title.trim() && gap.contractClause.trim(), gap.id);
        assert.ok(SIZES.has(gap.size), `${gap.id} size ${gap.size}`);
        assert.equal(typeof gap.needsWindowsRuntime, 'boolean', gap.id);
        assert.ok(STATUSES.has(gap.status), `${gap.id} status ${gap.status}`);
        for (const key of ['filesToTouch', 'hotFiles', 'dependsOn', 'testReferences', 'documentationReferences', 'runtimeEvidence']) {
            assert.ok(Array.isArray(gap[key]), `${gap.id} ${key}`);
        }
    }
});

test('a landed gap names its main commit and existing tests and documentation', () => {
    for (const gap of ledger.gaps.filter((item) => item.status !== 'open')) {
        assert.match(gap.landedIn ?? '', /^[0-9a-f]{40}$/, `${gap.id} landedIn`);
        assert.ok(gap.testReferences.length > 0, `${gap.id} has no tests`);
        for (const reference of [...gap.testReferences, ...gap.documentationReferences]) {
            const file = reference.split(/[#:]/)[0];
            assert.ok(existsSync(path.join(repoDir, file)), `${gap.id} reference ${reference} does not exist`);
        }
    }
    for (const gap of ledger.gaps.filter((item) => item.status === 'open')) {
        assert.equal(gap.landedIn, null, `${gap.id} is open but names a commit`);
    }
});

test('a verified gap carries runtime evidence, and completion needs every gap verified', () => {
    for (const gap of ledger.gaps.filter((item) => item.status === 'verified')) {
        assert.ok(gap.runtimeEvidence.length > 0, `${gap.id} has no runtime evidence`);
    }
    if (ledger.completionClaim) {
        assert.ok(ledger.gaps.every((gap) => gap.status === 'verified'), 'completion is claimed with unverified gaps');
    }
});

test('the readable page matches the ledger', () => {
    const result = spawnSync('python3', [path.join(repoDir, 'scripts', 'md3', 'render-feature-gaps.py'), '--check'], { encoding: 'utf8' });
    assert.equal(result.status, 0, result.stderr || result.stdout);
});
