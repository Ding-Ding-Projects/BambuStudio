import assert from 'node:assert/strict';
import { access, readFile, readdir } from 'node:fs/promises';
import path from 'node:path';

const siteDir = path.resolve(process.argv[2] || '_site');
const indexPath = path.join(siteDir, 'index.html');
const index = await readFile(indexPath, 'utf8');
const baseUrl = new URL('https://pages-layout.invalid/');
const attribute = (tag, name) => {
  const match = tag.match(new RegExp(`\\b${name}\\s*=\\s*(['"])(.*?)\\1`, 'i'));
  return match?.[2];
};
const localScripts = [...index.matchAll(/<script\b[^>]*\bsrc=(['"])(.*?)\1/gi)]
  .map((match) => match[2])
  .filter((source) => new URL(source, baseUrl).origin === baseUrl.origin);
const localStylesheets = [...index.matchAll(/<link\b[^>]*>/gi)]
  .map((match) => match[0])
  .filter((tag) => attribute(tag, 'rel')?.split(/\s+/).includes('stylesheet'))
  .map((tag) => attribute(tag, 'href'))
  .filter(Boolean)
  .filter((source) => new URL(source, baseUrl).origin === baseUrl.origin);
const localImages = [
  ...[...index.matchAll(/<img\b[^>]*\bsrc=(['"])(.*?)\1/gi)].map((match) => match[2]),
  ...[...index.matchAll(/<meta\b[^>]*(?:property|name)=(['"])(?:og:image|twitter:image)\1[^>]*\bcontent=(['"])(.*?)\2/gi)]
    .map((match) => match[3])
].filter((source) => !source.includes('+') && new URL(source, baseUrl).origin === baseUrl.origin);

assert.ok(localScripts.length > 0, 'The composed landing page must load at least one local script.');
assert.ok(
  localStylesheets.includes('assets/fonts.css'),
  'The composed landing page must load its bundled typography.'
);
assert.doesNotMatch(
  index,
  /fonts\.(?:googleapis|gstatic)\.com/i,
  'The composed landing page must not depend on third-party font requests.'
);
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
const fontDir = path.join(siteDir, 'assets', 'fonts');
const fontFiles = (await readdir(fontDir)).sort();
assert.deepEqual(fontFiles, [
  'Apache-2.0-LICENSE.txt',
  'MaterialSymbolsOutlined.woff2',
  'Roboto-Bold.woff2',
  'Roboto-Medium.woff2',
  'Roboto-Regular.woff2',
  'RobotoMono-Bold.woff2',
  'RobotoMono-Medium.woff2',
  'RobotoMono-Regular.woff2',
], 'The composed Pages site must publish every bundled font and its license.');

for (const source of [...localScripts, ...localStylesheets, ...localImages]) {
  const pathname = decodeURIComponent(new URL(source, baseUrl).pathname).replace(/^\/+/, '');
  const resolved = path.resolve(siteDir, pathname);
  assert.ok(
    resolved.startsWith(`${siteDir}${path.sep}`),
    `Landing asset escapes the composed Pages root: ${source}`
  );
  await assert.doesNotReject(
    access(resolved),
    undefined,
    `Landing asset does not resolve in the composed Pages layout: ${source}`
  );
}

console.log(
  `Pages layout OK: ${localScripts.length} scripts, ${localStylesheets.length} stylesheets, ` +
  `${localImages.length} showcase image references, ${fontFiles.length} font assets`
);
