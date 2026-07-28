#!/usr/bin/env node
/*
 * Composes the published Pages tree.
 *
 *   node ui-md3/scripts/compose-site.mjs [outputDir]
 *
 * Layout produced:
 *   <out>/index.html      the tabbed landing site (ui-md3/landing.html)
 *   <out>/site/           the landing site's modules and stylesheet
 *   <out>/assets/         fonts and showcase artwork, shared by both surfaces
 *   <out>/app/            the interactive prototype, served at /app/
 *   <out>/app/i18n*.js    the shared localisation runtime the landing loads
 *
 * Running it locally produces byte-for-byte what CI publishes, so the layout
 * and clipping checks can be reproduced on a development machine.
 */
import { cp, mkdir, readdir, readFile, rm, copyFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(scriptDir, '..');
const outDir = path.resolve(process.argv[2] || path.join(uiDir, '..', '_site'));

// Read-only design source, the desktop shell notes, tests, build scripts and
// prose never ship; the landing page is lifted to the site root instead.
const APP_EXCLUDES = new Set(['design-source', 'desktop', 'tests', 'scripts', 'landing.html']);

await rm(outDir, { recursive: true, force: true });
await mkdir(path.join(outDir, 'app'), { recursive: true });

for (const entry of await readdir(uiDir, { withFileTypes: true })) {
  if (APP_EXCLUDES.has(entry.name)) continue;
  if (entry.isFile() && entry.name.endsWith('.md')) continue;
  await cp(path.join(uiDir, entry.name), path.join(outDir, 'app', entry.name), { recursive: true });
}

// In the composed tree the landing page is the root and the prototype moves to
// /app/, so the app-base marker is rewritten to match. A stamped value beats a
// hostname sniff: it is correct on Pages, on a custom domain and on localhost.
const landing = await readFile(path.join(uiDir, 'landing.html'), 'utf8');
const stamped = landing.replace(
  /(<meta name="bambu-app-base" content=")[^"]*(">)/,
  '$1app/$2'
);
if (stamped === landing) {
  throw new Error('landing.html no longer carries the bambu-app-base marker to rewrite');
}
await writeFile(path.join(outDir, 'index.html'), stamped);
await cp(path.join(uiDir, 'assets'), path.join(outDir, 'assets'), { recursive: true });
await cp(path.join(uiDir, 'site'), path.join(outDir, 'site'), { recursive: true });

// The landing page loads the shared localisation runtime from ./app/, which at
// the site root is <out>/app/ rather than the prototype's own app/ folder.
for (const name of ['i18n.resources.js', 'i18n.js']) {
  await copyFile(path.join(uiDir, 'app', name), path.join(outDir, 'app', name));
}

const files = [];
async function walk(directory, prefix) {
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const next = path.join(directory, entry.name);
    if (entry.isDirectory()) await walk(next, `${prefix}${entry.name}/`);
    else files.push(`${prefix}${entry.name}`);
  }
}
await walk(outDir, '');
console.log(`Composed ${path.relative(process.cwd(), outDir)}: ${files.length} files`);
