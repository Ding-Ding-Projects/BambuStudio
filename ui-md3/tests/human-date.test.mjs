import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

const source = fileURLToPath(new URL('../../resources/web/include/human-date.js', import.meta.url));
await import('../site/human-date.js');

test('web and documentation use the identical date formatter', () => {
  assert.equal(readFileSync(source, 'utf8'), readFileSync(new URL('../site/human-date.js', import.meta.url), 'utf8'));
});

test('full month dates support English, Cantonese and bilingual modes', () => {
  assert.equal(BambuHumanDate.format('2026-09-29', 'en'), '29 September 2026');
  assert.equal(BambuHumanDate.format('2026-09-29', 'yue_HK'), '2026年9月29日');
  assert.equal(BambuHumanDate.format('2026-09-29', 'bilingual_en_yue_HK'), '29 September 2026 / 2026年9月29日');
  assert.equal(BambuHumanDate.format('2024-02-29', 'en'), '29 February 2024');
  assert.equal(BambuHumanDate.format('2026-02-29', 'en'), '');
  assert.equal(BambuHumanDate.format('2026-02-29T12:30:00Z', 'en'), '');
  assert.equal(BambuHumanDate.format('invalid', 'en'), '');
  assert.equal(BambuHumanDate.format('2026-09-29T00:00:00', 'en'), '');
  assert.equal(BambuHumanDate.formatLocal('2026-09-29 12:30:00', 'en', true), '29 September 2026 12:30');
  assert.equal(BambuHumanDate.formatLocal('2026-02-29 12:30:00', 'en', true), '');
});

test('calendar days never shift while UTC instants become viewer-local once', () => {
  const program = `require(${JSON.stringify(source)}); process.stdout.write(JSON.stringify([
    BambuHumanDate.format('2026-09-29', 'en'),
    BambuHumanDate.format('2026-09-29T00:30:00Z', 'en', true),
    BambuHumanDate.format('2026-03-08T07:30:00Z', 'en', true)
  ]));`;
  const result = spawnSync(process.execPath, ['-e', program], { env: { ...process.env, TZ: 'America/Toronto' }, encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr);
  assert.deepEqual(JSON.parse(result.stdout), ['29 September 2026', '28 September 2026 20:30', '8 March 2026 03:30']);
});
