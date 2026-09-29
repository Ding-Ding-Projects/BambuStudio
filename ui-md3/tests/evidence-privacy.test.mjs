import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Evidence under docs/screenshots is public. A layout dump records the text of
// every field on screen, and Preferences shows the download folder, so the
// capture profile's folders ended up in twelve dumps with the Windows account
// name in the path; the Config profiles dialog showed the data folder in two
// captures the same way. Capture profiles now live under C:\Users\Public, and
// no text evidence may name a folder under any other user profile.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
// C:\Users\<name>, in plain, JSON-escaped or forward-slash form; C:\Users\Public is allowed.
export const USER_PROFILE_PATH = /[A-Za-z]:(?:\\\\|\\|\/)Users(?:\\\\|\\|\/)(?!Public(?:\\\\|\\|\/|"|$))[^\\/"\s]+/;

test('the pattern catches a profile path in every spelling and allows Public', () => {
  for (const sample of ['C:\\Users\\someone\\Downloads', '"C:\\\\Users\\\\someone\\\\Downloads"', 'C:/Users/someone/AppData']) {
    assert.match(sample, USER_PROFILE_PATH, sample);
  }
  for (const sample of ['C:\\Users\\Public\\bbsdd\\en', '"C:\\\\Users\\\\Public\\\\Downloads"', 'C:/Users/Public/bbsdd', '<redacted: local user folder>']) {
    assert.doesNotMatch(sample, USER_PROFILE_PATH, sample);
  }
});

test('no text evidence under docs/screenshots names a folder under a user profile', () => {
  const files = execFileSync('git', ['ls-files', '-z', 'docs/screenshots'], { cwd: repoDir })
    .toString('utf8').split('\0').filter((f) => /\.(jsonl|json|md|txt|csv)$/.test(f));
  assert.ok(files.length > 0);
  const leaks = [];
  for (const file of files) {
    const text = readFileSync(path.join(repoDir, file), 'utf8');
    const lines = text.split('\n');
    lines.forEach((line, index) => {
      if (USER_PROFILE_PATH.test(line)) leaks.push(`${file}:${index + 1}`);
    });
  }
  assert.deepEqual(leaks, [], 'user-profile paths in public evidence');
});
