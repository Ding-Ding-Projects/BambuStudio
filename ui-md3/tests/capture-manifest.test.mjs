import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { basename, join } from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const repositoryRoot = fileURLToPath(new URL('../../', import.meta.url));
const captureScriptPath = join(repositoryRoot, 'ui-md3', 'scripts', 'capture-site.mjs');
const manifestPath = join(repositoryRoot, 'docs', 'screenshots', 'pages', 'capture-manifest.json');
const captureDirectory = join(repositoryRoot, 'docs', 'screenshots', 'pages');
const pngSignature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

async function fixture() {
  const [script, rawManifest] = await Promise.all([
    readFile(captureScriptPath, 'utf8'),
    readFile(manifestPath, 'utf8'),
  ]);
  return { script, manifest: JSON.parse(rawManifest) };
}

function literalCaptureNames(script) {
  const names = Array.from(
    script.matchAll(/\bshoot\(\s*['"]([a-z0-9-]+)['"]/g),
    match => match[1]
  );
  const tabLoop = script.match(
    /for\s*\(const tab of\s*\[([^\]]+)\]\)\s*\{[\s\S]*?shoot\(`tab-\$\{tab\}`\);[\s\S]*?\}/
  );
  assert.ok(tabLoop, 'capture script tab loop is not statically enumerable');
  const tabs = Array.from(tabLoop[1].matchAll(/['"]([a-z0-9]+)['"]/g), match => match[1]);
  assert.ok(tabs.length > 0, 'capture script tab loop has no tab names');
  return names.concat(tabs.map(tab => `tab-${tab}`));
}

test('capture manifest owns the complete Pages capture inventory', async () => {
  const { script, manifest } = await fixture();
  assert.equal(manifest.schemaVersion, 1);
  assert.equal(manifest.generator, 'ui-md3/scripts/capture-site.mjs');
  assert.deepEqual(manifest.provenance, {
    source: 'runtime Pages site from the same revision as this manifest',
    method: 'Chrome DevTools Protocol Page.captureScreenshot',
  });
  assert.ok(Array.isArray(manifest.captures));

  const scriptNames = literalCaptureNames(script);
  const manifestNames = manifest.captures.map(capture => capture.name);
  assert.equal(new Set(scriptNames).size, scriptNames.length, 'capture script has duplicate names');
  assert.equal(new Set(manifestNames).size, manifestNames.length, 'manifest has duplicate names');
  assert.deepEqual([...manifestNames].sort(), [...scriptNames].sort());
});

test('tracked Pages captures match their declared dimensions and SHA-256', async () => {
  const { manifest } = await fixture();
  for (const capture of manifest.captures) {
    assert.match(capture.name, /^[a-z0-9]+(?:-[a-z0-9]+)*$/);
    assert.equal(capture.file, `${capture.name}.png`);
    assert.equal(basename(capture.file), capture.file, `${capture.file} must be a plain filename`);
    assert.ok(Number.isInteger(capture.width) && capture.width > 0);
    assert.ok(Number.isInteger(capture.height) && capture.height > 0);
    assert.match(capture.sha256, /^[0-9a-f]{64}$/);

    const png = await readFile(join(captureDirectory, capture.file));
    assert.equal(png.subarray(0, 8).equals(pngSignature), true, `${capture.file} is not a PNG`);
    assert.equal(png.readUInt32BE(16), capture.width, `${capture.file} width drifted`);
    assert.equal(png.readUInt32BE(20), capture.height, `${capture.file} height drifted`);
    assert.equal(createHash('sha256').update(png).digest('hex'), capture.sha256,
      `${capture.file} bytes drifted`);
  }
});

test('dim-sum capture suppresses startup randomness and invokes the production renderer once', async () => {
  const { script, manifest } = await fixture();
  const suppression = script.indexOf('await suppressAutomaticDimSum(session)');
  const firstNavigation = script.indexOf("await open({ tab: 'overview' });");
  assert.ok(suppression >= 0 && suppression < firstNavigation,
    'automatic startup suppression must be installed before the first navigation');
  assert.match(script, /BAMBU_CAPTURE_ONLY/);
  assert.match(script, /window\.BambuSite\.renderDimSumSurprise\(dish\)/);
  assert.match(script, /await shoot\('dim-sum-card', \{ selector: '\.dimsum', pad: 0 \}\);/);

  const capture = manifest.captures.find(item => item.name === 'dim-sum-card');
  assert.ok(capture, 'dim-sum-card is absent from the capture manifest');
  assert.deepEqual(capture.viewport, { width: 1280, height: 980, scale: 1 });
  assert.equal(capture.selector, '.dimsum');
  assert.equal(capture.padding, 0);
});
