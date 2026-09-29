import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The release job used to mark a default-branch build "latest" only when the
// branch still pointed at that exact commit at publication time. A build takes
// about two hours and the branch moved faster than that, so every build after
// md3-v143 was published as "superseded" and the latest release, which the
// in-app updater reads (releases/latest/download), stayed on md3-v143. A build
// now becomes latest when it is newer than the release that is latest now, so
// an older queued build still cannot replace a newer one.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const workflow = await readFile(path.join(repoDir, '.github', 'workflows', 'build_all.yml'), 'utf8');

const publication = () => {
  const start = workflow.indexOf("$latestArgument = '--latest=false'");
  assert.ok(start > 0, 'the publication step decides the latest flag');
  const end = workflow.indexOf('gh release edit $env:release_tag', start);
  assert.ok(end > start, 'the decision comes before the publishing edit');
  return workflow.slice(start, end);
};

test('a default-branch build becomes latest when it is newer than the current latest release', () => {
  const code = publication();
  assert.match(code, /repos\/\$env:GITHUB_REPOSITORY\/releases\/latest/, 'reads the release that is latest now');
  assert.match(code, /repos\/\$env:GITHUB_REPOSITORY\/compare\/\$currentLatestSha\.\.\.\$env:GITHUB_SHA/, 'compares that release\'s commit with this build\'s');
  assert.match(code, /\$comparison -eq 'ahead' -or \$comparison -eq 'identical'/, 'newer (or the same commit rebuilt) wins');
  assert.match(code, /\$latestArgument = '--latest'/);
  assert.match(code, /\$releaseTitle \+= " \(superseded \$defaultBranch build\)"/, 'an older build is still named superseded');
});

test('the branch head no longer decides the latest release', () => {
  const code = publication();
  assert.doesNotMatch(code, /\$defaultSha -eq \$env:GITHUB_SHA/, 'a busy branch must not keep every build from becoming latest');
});
