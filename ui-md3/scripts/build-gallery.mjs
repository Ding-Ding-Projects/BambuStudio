#!/usr/bin/env node
/*
 * Builds the public screenshot gallery page.
 *
 *   node ui-md3/scripts/build-gallery.mjs <outputDir>
 *
 * The gallery shows exactly the captures the README already publishes, in the
 * README's order and with the README's alt text as each caption. Only images
 * stored under docs/ are included; design artwork elsewhere stays on the
 * landing page. Every image is copied beside the page so the gallery has no
 * remote asset, and a missing file stops the build instead of shipping a
 * broken tile.
 */
import { copyFile, mkdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(scriptDir, '..', '..');
const outDir = path.resolve(process.argv[2] || path.join(repoRoot, '_site', 'gallery'));

const escapeHtml = (text) => text.replace(/[&<>"']/g, (c) => ({
  '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
}[c]));

const readme = await readFile(path.join(repoRoot, 'README.md'), 'utf8');
const seen = new Set();
const shots = [];
for (const match of readme.matchAll(/!\[([^\]]*)\]\((docs\/[^)\s]+\.(?:png|jpe?g|webp))\)/gi)) {
  const [, alt, source] = match;
  if (seen.has(source)) continue;
  seen.add(source);
  shots.push({ alt: alt.trim() || path.basename(source), source });
}
if (shots.length === 0) throw new Error('README.md lists no screenshots under docs/');

await mkdir(path.join(outDir, 'img'), { recursive: true });
const tiles = [];
for (const [index, shot] of shots.entries()) {
  const name = `${String(index + 1).padStart(3, '0')}-${path.basename(shot.source)}`;
  await copyFile(path.join(repoRoot, shot.source), path.join(outDir, 'img', name));
  tiles.push(`      <figure class="tile">
        <a href="img/${name}"><img src="img/${name}" alt="${escapeHtml(shot.alt)}" loading="lazy"></a>
        <figcaption>${escapeHtml(shot.alt)}<br><span class="src">${escapeHtml(shot.source)}</span></figcaption>
      </figure>`);
}

const page = `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bambu Studio MD3 screenshots</title>
  <meta name="description" content="Screenshots of the Bambu Studio Material Design 3 desktop app, captured from real builds.">
  <style>
    :root { --bg: #fbf8fd; --fg: #1b1b1f; --muted: #46464f; --card: #ffffff; --line: #c7c5d0; --accent: #3f5aa9; }
    @media (prefers-color-scheme: dark) {
      :root { --bg: #131316; --fg: #e4e1e6; --muted: #c7c5d0; --card: #1f1f23; --line: #46464f; --accent: #b4c4ff; }
    }
    * { box-sizing: border-box; }
    body { margin: 0; background: var(--bg); color: var(--fg); font: 16px/1.5 system-ui, "Segoe UI", sans-serif; }
    header, main { max-width: 1200px; margin: 0 auto; padding: 16px; }
    h1 { font-size: 1.75rem; margin: 8px 0; }
    a { color: var(--accent); }
    .grid { display: grid; gap: 16px; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); }
    .tile { margin: 0; background: var(--card); border: 1px solid var(--line); border-radius: 16px; overflow: hidden; }
    .tile img { display: block; width: 100%; height: auto; background: var(--bg); }
    .tile figcaption { padding: 12px; font-size: 0.9rem; overflow-wrap: anywhere; }
    .src { color: var(--muted); font-size: 0.8rem; }
  </style>
</head>
<body>
  <header>
    <h1>Bambu Studio MD3 screenshots</h1>
    <p>${shots.length} captures from real builds of the app, the same ones the
      <a href="https://github.com/Ding-Ding-Projects/BambuStudio#readme">README</a> shows. Each caption
      names the file it came from; the README's provenance table records when and how it was taken.
      <a href="../">Back to the site</a>.</p>
  </header>
  <main>
    <div class="grid">
${tiles.join('\n')}
    </div>
  </main>
</body>
</html>
`;
await writeFile(path.join(outDir, 'index.html'), page);
console.log(`Gallery: ${shots.length} screenshots in ${path.relative(process.cwd(), outDir)}`);
