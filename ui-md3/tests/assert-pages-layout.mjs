import assert from 'node:assert/strict';
import { access, readFile, readdir } from 'node:fs/promises';
import path from 'node:path';

const siteRoot = path.resolve(process.argv[2] || '_site');
const indexPath = path.join(siteRoot, 'index.html');
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

// The landing page builds its panels at runtime, so most showcase artwork is
// referenced from the site modules rather than from the HTML. Those references
// are resolved here too — a renamed image must fail the deploy, not 404 live.
const moduleDir = path.join(siteRoot, 'site');
const siteModules = (await readdir(moduleDir)).filter((name) => name.endsWith('.js'));
const moduleImages = [];
for (const name of siteModules) {
  const source = await readFile(path.join(moduleDir, name), 'utf8');
  for (const match of source.matchAll(/['"](\.\/assets\/[A-Za-z0-9._/-]+\.[a-z0-9]+)['"]/g)) {
    moduleImages.push(match[1].replace(/^\.\//, ''));
  }
  // Card artwork is concatenated onto the showcase directory at render time.
  for (const match of source.matchAll(/\bart:\s*'([A-Za-z0-9._-]+\.webp)'/g)) {
    moduleImages.push(`assets/showcase/${match[1]}`);
  }
}

assert.ok(localScripts.length > 0, 'The composed landing page must load at least one local script.');
assert.ok(
  localScripts.some((source) => source.includes('site/core.js')),
  'The composed landing page must load the site runtime.'
);
assert.ok(
  localStylesheets.includes('site/site.css'),
  'The composed landing page must load the site stylesheet.'
);
assert.ok(
  moduleImages.length >= 9,
  `The site modules must reference the showcase artwork; found ${moduleImages.length}`
);
assert.ok(
  localStylesheets.includes('assets/fonts.css'),
  'The composed landing page must load its bundled typography.'
);
assert.doesNotMatch(
  index,
  /fonts\.(?:googleapis|gstatic)\.com/i,
  'The composed landing page must not depend on third-party font requests.'
);
assert.doesNotMatch(
  index,
  /<script\b[^>]*\bsrc=(['"])https?:/i,
  'The composed landing page must not load third-party script.'
);
assert.ok(localImages.length >= 1, 'The composed landing page must publish its hero image.');
await access(path.join(siteRoot, 'app', 'index.html'));
// The landing page loads the shared localisation runtime from the site root.
await access(path.join(siteRoot, 'app', 'i18n.js'));
await access(path.join(siteRoot, 'app', 'i18n.resources.js'));

// Prose does not belong in a deployed site, at any depth. The design system
// keeps a .prompt.md beside most components, and the previous rsync-based
// compose step excluded them; a composer that ships them is a regression.
const prose = [];
async function findProse(directory, prefix) {
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const next = path.join(directory, entry.name);
    if (entry.isDirectory()) await findProse(next, `${prefix}${entry.name}/`);
    else if (entry.name.toLowerCase().endsWith('.md')) prose.push(`${prefix}${entry.name}`);
  }
}
await findProse(siteRoot, '');
assert.deepEqual(prose, [], 'The composed Pages site must not publish markdown.');

const expectedModules = [
  'boot.js', 'changelog.data.js', 'changelog.js', 'copy.js', 'core.js',
  'dimsum.data.js', 'regex.js', 'settings.js', 'tabs.js', 'views.js'
];
assert.deepEqual(
  siteModules.sort(),
  expectedModules,
  'The composed Pages site must publish every site module.'
);

const showcaseDir = path.join(siteRoot, 'assets', 'showcase');
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
const fontDir = path.join(siteRoot, 'assets', 'fonts');
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

for (const source of [...localScripts, ...localStylesheets, ...localImages, ...moduleImages]) {
  const pathname = decodeURIComponent(new URL(source, baseUrl).pathname).replace(/^\/+/, '');
  const resolved = path.resolve(siteRoot, pathname);
  assert.ok(
    resolved.startsWith(`${siteRoot}${path.sep}`),
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
  `${localImages.length + moduleImages.length} showcase image references, ` +
  `${siteModules.length} site modules, ${fontFiles.length} font assets`
);
