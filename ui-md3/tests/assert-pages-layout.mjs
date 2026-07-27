import assert from 'node:assert/strict';
import { access, readFile, readdir } from 'node:fs/promises';
import path from 'node:path';

const siteDir = path.resolve(process.argv[2] || '_site');
const indexPath = path.join(siteDir, 'index.html');
const index = await readFile(indexPath, 'utf8');
const baseUrl = new URL('https://pages-layout.invalid/');
const localScripts = [...index.matchAll(/<script\b[^>]*\bsrc=(['"])(.*?)\1/gi)]
  .map((match) => match[2])
  .filter((source) => new URL(source, baseUrl).origin === baseUrl.origin);
const localImages = [
  ...[...index.matchAll(/<img\b[^>]*\bsrc=(['"])(.*?)\1/gi)].map((match) => match[2]),
  ...[...index.matchAll(/<meta\b[^>]*(?:property|name)=(['"])(?:og:image|twitter:image)\1[^>]*\bcontent=(['"])(.*?)\2/gi)]
    .map((match) => match[3])
].filter((source) => !source.includes('+') && new URL(source, baseUrl).origin === baseUrl.origin);

assert.ok(localScripts.length > 0, 'The composed landing page must load at least one local script.');
assert.ok(localImages.length >= 1, 'The composed landing page must publish its hero image.');
await access(path.join(siteDir, 'app', 'index.html'));
const showcaseDir = path.join(siteDir, 'assets', 'showcase');
const showcaseFiles = (await readdir(showcaseDir)).sort();
assert.deepEqual(showcaseFiles, [
  'calibration.webp',
  'device.webp',
  'filament.webp',
  'hero-studio.webp',
  'home.webp',
  'multi-device.webp',
  'og-social.webp',
  'prepare.webp',
  'preview.webp',
  'project.webp',
  'settings.webp'
], 'The composed Pages site must publish the complete optimized showcase.');

for (const source of [...localScripts, ...localImages]) {
  const pathname = decodeURIComponent(new URL(source, baseUrl).pathname).replace(/^\/+/, '');
  const resolved = path.resolve(siteDir, pathname);
  assert.ok(
    resolved.startsWith(`${siteDir}${path.sep}`),
    `Landing script escapes the composed Pages root: ${source}`
  );
  await assert.doesNotReject(
    access(resolved),
    undefined,
    `Landing script does not resolve in the composed Pages layout: ${source}`
  );
}

console.log(`Pages layout OK: ${localScripts.length} scripts, ${localImages.length} showcase image references`);
