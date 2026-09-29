import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Smart home media controls read "Previous · 上一首" and "Next · 下一步" in
// bilingual mode: a track back, then a wizard step forward. "Next" is shared
// with the calibration wizards, where 下一步 is right, and the bilingual layer
// looks labels up by their English, so a translation context could not keep the
// two apart. The media controls now say which kind of next they mean.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFile(path.join(repoDir, ...parts), 'utf8');
const dialog = await read('src', 'slic3r', 'GUI', 'SmartHomeDialog.cpp');

test('the media controls name the track, not a generic step', () => {
  assert.match(dialog, /\{_L\("Previous track"\), "media_previous_track"\}/);
  assert.match(dialog, /\{_L\("Next track"\), "media_next_track"\}/);
  assert.doesNotMatch(dialog, /\{_L\("Next"\), "media_next_track"\}/, 'the wizard "Next" is not reused for a track');
});

test('both labels have a Cantonese translation that says track', async () => {
  const po = await read('bbl', 'i18n', 'yue_HK', 'BambuStudio_yue_HK.po');
  assert.match(po, /msgid "Previous track"\r?\nmsgstr "上一首"/);
  assert.match(po, /msgid "Next track"\r?\nmsgstr "下一首"/);
});
