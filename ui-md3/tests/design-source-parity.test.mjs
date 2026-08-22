// Parity guard for the checked-in Claude Design source files.
//
// The files under design-source/ are the read-only source of truth the MD3
// implementation is measured against. These tests pin three facts: the files
// are present, the main shell carries its expected structural markers, and
// the proprietary DC runtime (support.js) is never imported by shipped code.

import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(testDir, '..');
const designSourceDir = path.join(uiDir, 'design-source');

const expectedFiles = [
  'Bambu Studio.dc.html',
  'SearchField.dc.html',
  'support.js',
  'CLAUDE.md',
];

const readDesignSource = (...p) => readFile(path.join(designSourceDir, ...p), 'utf8');

test('every expected design-source file exists and is non-empty', async () => {
  for (const name of expectedFiles) {
    const content = await readDesignSource(name);
    assert.ok(content.length > 0, `design-source/${name} must exist and be non-empty`);
  }
});

test('Bambu Studio.dc.html carries its key structural markers', async () => {
  const html = await readDesignSource('Bambu Studio.dc.html');
  for (const marker of ['dc-import', 'SearchField', 'data-theme']) {
    assert.ok(html.includes(marker),
      `the design shell must reference "${marker}"`);
  }
});

test('support.js stays a design-only asset and is not imported by any shipped file', async () => {
  // The DC runtime is proprietary and was authored against React; we reimplement
  // a vanilla runtime instead. Shipped code must therefore never import it —
  // neither through an ESM import nor a CommonJS require.
  const importPattern = /(?:from\s+['"][^'"]*support\.js|require\(\s*['"][^'"]*support\.js)/;
  for (const dir of ['site', 'app']) {
    const entries = await readdir(path.join(uiDir, dir), { withFileTypes: true });
    for (const entry of entries) {
      if (!entry.isFile() || !entry.name.endsWith('.js')) continue;
      const source = await readFile(path.join(uiDir, dir, entry.name), 'utf8');
      assert.doesNotMatch(source, importPattern,
        `shipped file ${dir}/${entry.name} must not import support.js`);
    }
  }
});

test('CLAUDE.md states the design sources are read-only source-of-truth files', async () => {
  const doc = await readDesignSource('CLAUDE.md');
  assert.ok(/do not edit/i.test(doc) && /source of truth/i.test(doc),
    'CLAUDE.md must declare the folder read-only and authoritative');
});
