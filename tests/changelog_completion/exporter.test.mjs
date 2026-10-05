import { readFileSync } from 'node:fs';
import test from 'node:test';
import assert from 'node:assert/strict';
import { isoDate, resolveCommit, toAppRelease } from '../../scripts/changelog/export-app-changelog.mjs';
test('release date and unavailable commit are factual', () => {
  assert.equal(isoDate('2026-10-05T01:02:03Z'), '2026-10-05');
  assert.throws(() => isoDate('not a date'));
  assert.throws(() => resolveCommit('not-a-sha', 'fixture'));
  assert.throws(() => resolveCommit('0'.repeat(40), 'fixture'));
});
test('empty release retains its real version and records no invented change', () => {
  const result = toAppRelease({tag: 'v1', published: '2026-10-05T01:02:03Z', changes: []});
  assert.equal(result.version, 'v1'); assert.deepEqual(result.entries, []); assert.equal(result.commit, '');
});
test('freshness check never treats unavailable release service as success', () => {
  const source = readFileSync(new URL('../../scripts/changelog/export-app-changelog.mjs', import.meta.url), 'utf8');
  assert.equal(source.includes('process.exit(0)'), false);
  assert.equal(source.includes('Skipped the app changelog freshness check'), false);
});
test('viewer uses shared export and keeps its original filtered text serializer', () => {
  const source = readFileSync(new URL('../../src/slic3r/GUI/ChangelogDialog.cpp', import.meta.url), 'utf8');
  assert.ok(source.includes('ExportDialog::run(this, std::move(dataset))'));
  assert.ok(source.includes('dataset.prose = export_text(Changelog::ExportFormat::Markdown)'));
  assert.equal(source.includes('std::ios::trunc'), false);
});
