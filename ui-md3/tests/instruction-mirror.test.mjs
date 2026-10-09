import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The shared instruction mirror: README.md and AGENTS.md each carry one
// clearly labelled, generated copy of the maintainer's sanitized shared agent
// instructions, written by scripts/instructions/refresh-instruction-mirror.mjs.
// These tests drive the refresh against a temporary repository root with a
// synthetic instruction file, so no real instruction text is needed here.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const refreshScript = path.join(repoDir, 'scripts', 'instructions', 'refresh-instruction-mirror.mjs');
const libraryUrl = new URL('../../scripts/instructions/instruction-mirror.mjs', import.meta.url);

const README = `# Example project

Intro text for first-time readers.

# How to compile

Build steps.

# Report issue

Use the tracker.

# License

Licence text.
`;

const AGENTS = `# Build dependency policy

Install what the build needs.

## Agent conversation vocabulary

Existing repository-specific block.
`;

const SOURCE = `# Example shared agent instructions

These are example defaults written for this test.

## Working discipline

- Pull the Git remote before starting work.
- Keep changes scoped and report concrete evidence.

## Secrets and sensitive input

- Do not ask the user to paste secrets into chat.
`;

const REVISION = '0123456789abcdef0123456789abcdef01234567';

function fixtureRoot(t) {
  const root = mkdtempSync(path.join(tmpdir(), 'instruction-mirror-'));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writeFileSync(path.join(root, 'README.md'), README);
  writeFileSync(path.join(root, 'AGENTS.md'), AGENTS);
  writeFileSync(path.join(root, 'shared.md'), SOURCE);
  return root;
}

function refresh(root, ...extra) {
  return spawnSync(process.execPath, [refreshScript, '--root', root, '--source', path.join(root, 'shared.md'),
    '--source-revision', REVISION, '--date', '2026-10-08', ...extra], { encoding: 'utf8' });
}

const read = (root, file) => readFileSync(path.join(root, file), 'utf8');

test('refresh writes one labelled, identical mirror into README.md and AGENTS.md', async (t) => {
  const { extractMirror, MIRROR_BEGIN, MIRROR_END, bodyDigest } = await import(libraryUrl);
  const root = fixtureRoot(t);
  const run = refresh(root);
  assert.equal(run.status, 0, run.stderr + run.stdout);

  const readme = read(root, 'README.md');
  const agents = read(root, 'AGENTS.md');
  for (const text of [readme, agents]) {
    assert.equal(text.split(MIRROR_BEGIN).length - 1, 1, 'exactly one mirror block');
    assert.equal(text.split(MIRROR_END).length - 1, 1, 'exactly one mirror end');
    assert.match(text, /^# Shared agent instructions \(mirror\)$/m);
    assert.match(text, /generated mirror of the maintainer's shared agent instructions/);
    assert.match(text, /Do not\s+edit it here/);
    assert.match(text, /\[Shared instruction mirror\]\(docs\/features\/documentation\/instruction-mirror\.md\)/);
    assert.match(text, new RegExp(`Source revision \`${REVISION}\``));
    assert.match(text, /mirrored 2026-10-08/);
  }

  const fromReadme = extractMirror(readme);
  const fromAgents = extractMirror(agents);
  assert.equal(fromReadme.body, fromAgents.body, 'both files carry the same body');
  assert.deepEqual(fromReadme.meta, fromAgents.meta, 'both files record the same source');
  assert.equal(fromReadme.meta.sourceRevision, REVISION);
  assert.equal(fromReadme.meta.mirroredOn, '2026-10-08');
  assert.equal(fromReadme.meta.bodySha256, bodyDigest(fromReadme.body));
  assert.match(fromReadme.meta.bodySha256, /^[0-9a-f]{64}$/);

  // The source's own title is replaced by the mirror heading; its sections stay.
  assert.doesNotMatch(fromReadme.body, /Example shared agent instructions/);
  assert.match(fromReadme.body, /^## Working discipline$/m);
  assert.match(fromReadme.body, /^- Do not ask the user to paste secrets into chat\.$/m);

  // The README folds the long reference text; AGENTS.md shows it directly.
  assert.match(readme, /<details>\n<summary>Show the mirrored shared agent instructions<\/summary>/);
  assert.doesNotMatch(agents, /<details>/);
});

test('refresh keeps the surrounding files intact and places each block predictably', async (t) => {
  const root = fixtureRoot(t);
  assert.equal(refresh(root).status, 0);
  const readme = read(root, 'README.md');
  const agents = read(root, 'AGENTS.md');
  const mirrorAt = readme.indexOf('# Shared agent instructions (mirror)');
  assert.ok(mirrorAt > readme.indexOf('# How to compile'), 'README mirror follows the build section');
  assert.ok(mirrorAt < readme.indexOf('# Report issue'), 'README mirror precedes Report issue');
  assert.ok(readme.startsWith(README.slice(0, README.indexOf('# Report issue'))), 'README text before the block is unchanged');
  assert.ok(readme.endsWith(README.slice(README.indexOf('# Report issue'))), 'README text after the block is unchanged');
  assert.ok(agents.startsWith(AGENTS), 'AGENTS.md keeps its repository-specific rules first');
  assert.ok(agents.trimEnd().endsWith('<!-- shared-instructions-mirror:end -->'), 'AGENTS.md mirror is appended');
});

test('refresh is idempotent and replaces an existing mirror in place', async (t) => {
  const { extractMirror } = await import(libraryUrl);
  const root = fixtureRoot(t);
  assert.equal(refresh(root).status, 0);
  const first = read(root, 'README.md');
  assert.equal(refresh(root).status, 0);
  assert.equal(read(root, 'README.md'), first, 'an unchanged source rewrites nothing');

  writeFileSync(path.join(root, 'shared.md'), `${SOURCE}\n## Build entrypoints\n\n- Every build runs the root build script.\n`);
  const next = refresh(root, '--date', '2026-10-09');
  assert.equal(next.status, 0, next.stderr);
  for (const file of ['README.md', 'AGENTS.md']) {
    const text = read(root, file);
    assert.equal(text.split('<!-- shared-instructions-mirror:begin -->').length - 1, 1, `${file} keeps one block`);
    const mirror = extractMirror(text);
    assert.match(mirror.body, /^## Build entrypoints$/m);
    assert.equal(mirror.meta.mirroredOn, '2026-10-09');
  }
});

test('an unchanged source keeps the recorded mirror date', async (t) => {
  const { extractMirror } = await import(libraryUrl);
  const root = fixtureRoot(t);
  assert.equal(refresh(root).status, 0);
  const again = spawnSync(process.execPath, [refreshScript, '--root', root, '--source', path.join(root, 'shared.md'),
    '--source-revision', REVISION], { encoding: 'utf8' });
  assert.equal(again.status, 0, again.stderr);
  assert.equal(extractMirror(read(root, 'AGENTS.md')).meta.mirroredOn, '2026-10-08');
});

test('check mode reports a stale mirror without writing', async (t) => {
  const root = fixtureRoot(t);
  const before = read(root, 'README.md');
  const stale = refresh(root, '--check');
  assert.equal(stale.status, 1, 'a missing mirror is stale');
  assert.match(stale.stdout + stale.stderr, /README\.md/);
  assert.equal(read(root, 'README.md'), before, 'check mode never writes');
  assert.equal(refresh(root).status, 0);
  const current = refresh(root, '--check');
  assert.equal(current.status, 0, current.stdout + current.stderr);
});

test('refresh refuses malformed input instead of writing a misleading mirror', async (t) => {
  const root = fixtureRoot(t);
  const cases = [
    ['--source-revision', 'not-a-revision'],
    ['--date', '8 October 2026'],
  ];
  for (const extra of cases) {
    const run = refresh(root, ...extra);
    assert.notEqual(run.status, 0, extra.join(' '));
  }
  for (const source of [
    `${SOURCE}\n# A second top-level heading\n`,
    `${SOURCE}\n<!-- shared-instructions-mirror:end -->\n`,
    '# Title only\n',
  ]) {
    writeFileSync(path.join(root, 'shared.md'), source);
    const run = refresh(root);
    assert.notEqual(run.status, 0, source);
  }
  assert.equal(read(root, 'README.md'), README, 'refused input leaves README.md unchanged');
  assert.equal(read(root, 'AGENTS.md'), AGENTS, 'refused input leaves AGENTS.md unchanged');
});

test('a Windows checkout with CRLF line endings keeps them and stays idempotent', async (t) => {
  const { extractMirror } = await import(libraryUrl);
  const root = fixtureRoot(t);
  const crlf = (text) => text.replace(/\n/g, '\r\n');
  writeFileSync(path.join(root, 'README.md'), crlf(README));
  writeFileSync(path.join(root, 'AGENTS.md'), crlf(AGENTS));
  // Windows editors may also prefix the export with a byte-order mark.
  const byteOrderMark = String.fromCharCode(0xfeff);
  writeFileSync(path.join(root, 'shared.md'), byteOrderMark + crlf(SOURCE));
  const run = refresh(root);
  assert.equal(run.status, 0, run.stderr + run.stdout);
  for (const file of ['README.md', 'AGENTS.md']) {
    const text = read(root, file);
    assert.doesNotMatch(text, /[^\r]\n/, `${file} keeps CRLF on every line`);
    const mirror = extractMirror(text.replace(/\r\n/g, '\n'));
    assert.ok(mirror, `${file} carries the mirror`);
    assert.ok(!text.includes(byteOrderMark), `${file} carries no byte-order mark`);
    assert.match(mirror.body, /^These are example defaults/, 'the export title is still replaced');
  }
  const first = read(root, 'README.md');
  assert.equal(refresh(root).status, 0);
  assert.equal(read(root, 'README.md'), first, 'a CRLF checkout is not rewritten by a repeated refresh');
  assert.equal(refresh(root, '--check').status, 0, 'check mode accepts the CRLF checkout');
});

test('a fenced level-one heading inside the source is content, not a title', async (t) => {
  const { extractMirror } = await import(libraryUrl);
  const root = fixtureRoot(t);
  writeFileSync(path.join(root, 'shared.md'), `${SOURCE}\n\`\`\`markdown\n# Quoted heading\n\`\`\`\n`);
  assert.equal(refresh(root).status, 0);
  assert.match(extractMirror(read(root, 'AGENTS.md')).body, /^# Quoted heading$/m);
});

test('the mirror procedure is documented in English and Cantonese and indexed', () => {
  const article = path.join(repoDir, 'docs', 'features', 'documentation', 'instruction-mirror.md');
  const twin = path.join(repoDir, 'docs', 'features', 'documentation', 'instruction-mirror.yue_HK.md');
  assert.ok(existsSync(article), 'English article');
  assert.ok(existsSync(twin), 'Cantonese twin');
  const english = readFileSync(article, 'utf8');
  assert.match(english, /refresh-instruction-mirror\.mjs/);
  assert.match(english, /--source-revision/);
  assert.match(readFileSync(twin, 'utf8'), /^translation-of: instruction-mirror\.md$/m);
  const index = readFileSync(path.join(repoDir, 'docs', 'features', 'documentation', 'README.md'), 'utf8');
  assert.match(index, /\(instruction-mirror\.md\)/);
});
