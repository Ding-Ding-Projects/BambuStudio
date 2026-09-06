#!/usr/bin/env node
// Inventory every `new wxStaticBitmap(` under src/slic3r/GUI and classify it.
//
//   node scripts/md3/triage-static-bitmaps.mjs            print the table
//   node scripts/md3/triage-static-bitmaps.mjs --write    write docs/features/design-system/static-bitmap-triage.csv
//
// Verdicts (provisional; a human confirms them in the CSV, and the guard test
// treats the committed CSV as the allowlist):
//   md3-rendered   the holder already shows a Material Symbols glyph (MaterialIcon::bitmap,
//                  dialog_action_glyph, filament_loading_chevron). Acceptable rendering.
//   glyph          an icon-sized raster (<= 32 px) from a named resource: convert to a
//                  Material glyph, or record why the raster is data.
//   badge          a tiny status dot (<= 8 px): draw as an MD3 status dot.
//   data           a photo, thumbnail, diagram, QR, preview or other content image
//                  (>= 64 px, loaded from a file/network image, or a data-named member).
//   needs-control  any of the above that also has a click binding: a wxStaticBitmap cannot
//                  take focus or expose a role, so it must become a Button.
//   unclassified   the heuristics could not decide; a human must.

import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '..', '..');
const guiRoot = path.join(repoRoot, 'src', 'slic3r', 'GUI');
const csvPath = path.join(repoRoot, 'docs', 'features', 'design-system', 'static-bitmap-triage.csv');
const write = process.argv.includes('--write');

async function walk(dir, out = []) {
  for (const e of await fs.readdir(dir, { withFileTypes: true })) {
    const f = path.join(dir, e.name);
    if (e.isDirectory()) await walk(f, out);
    else if (e.name.endsWith('.cpp')) out.push(f);
  }
  return out;
}

function commentMask(src) {
  const mask = new Uint8Array(src.length);
  let i = 0;
  while (i < src.length) {
    const c = src[i], n = src[i + 1];
    if (c === '"' || c === "'") { const q = c; i++; while (i < src.length && src[i] !== q) { if (src[i] === '\\') i++; i++; } i++; continue; }
    if (c === '/' && n === '/') { while (i < src.length && src[i] !== '\n') mask[i++] = 1; continue; }
    if (c === '/' && n === '*') { const end = src.indexOf('*/', i + 2); const stop = end === -1 ? src.length : end + 2; while (i < stop) mask[i++] = 1; continue; }
    i++;
  }
  return mask;
}

function matchParen(src, open) {
  let depth = 0;
  for (let i = open; i < src.length; i++) {
    const c = src[i];
    if (c === '"' || c === "'") { const q = c; i++; while (i < src.length && src[i] !== q) { if (src[i] === '\\') i++; i++; } continue; }
    if (c === '(') depth++; else if (c === ')') { depth--; if (depth === 0) return i + 1; }
  }
  return -1;
}

function splitArgs(inner) {
  const parts = []; let depth = 0, start = 0;
  for (let i = 0; i < inner.length; i++) {
    const c = inner[i];
    if (c === '"' || c === "'") { const q = c; i++; while (i < inner.length && inner[i] !== q) { if (inner[i] === '\\') i++; i++; } continue; }
    if (c === '(' || c === '[' || c === '{') depth++; else if (c === ')' || c === ']' || c === '}') depth--;
    else if (c === ',' && depth === 0) { parts.push(inner.slice(start, i)); start = i + 1; }
  }
  parts.push(inner.slice(start));
  return parts.map((p) => p.replace(/\s+/g, ' ').trim());
}

function pxOf(sizeExpr) {
  if (!sizeExpr || sizeExpr === 'wxDefaultSize') return null;
  const nums = [...sizeExpr.matchAll(/FromDIP\((\d+)\)|(?<![\w.])(\d{1,4})(?![\w.])/g)].map((m) => Number(m[1] || m[2])).filter((n) => n > 0);
  return nums.length ? Math.max(...nums) : null;
}

const DATA_NAMES = /img|photo|thumb|preview|qr|logo|printer|ams(?!_icon)|extruder|nozzle|ext_|diagram|picture|image|banner|cover|snapshot|camera|render/i;
const GLYPH_SOURCES = /MaterialIcon::|dialog_action_glyph|filament_loading_chevron|_glyph\(/;

function classify(site) {
  const src = site.source;
  const member = site.member || '';
  if (GLYPH_SOURCES.test(src) || GLYPH_SOURCES.test(site.laterSets.join(' '))) return site.hasBind ? 'needs-control' : 'md3-rendered';
  const px = site.px;
  if (/wxImage|wxBITMAP_TYPE|Rescale\(|local_path|GetBitmap\(\)|LoadFile|thumbnail/i.test(src) || /wxImage|LoadFile|thumbnail|Rescale/i.test(site.laterSets.join(' '))) return site.hasBind ? 'needs-control' : 'data';
  if (px !== null && px <= 8) return 'badge';
  if (px !== null && px >= 64) return site.hasBind ? 'needs-control' : 'data';
  if (DATA_NAMES.test(member) && !/icon|arrow|status|badge|dot/i.test(member)) return site.hasBind ? 'needs-control' : 'data';
  if (/create_scaled_bitmap\(|ScalableBitmap\(|\.bmp\(\)|wxNullBitmap/.test(src) || site.laterSets.length) {
    if (px === null || px <= 32) return site.hasBind ? 'needs-control' : 'glyph';
  }
  return 'unclassified';
}

const sites = [];
for (const file of await walk(guiRoot)) {
  const src = await fs.readFile(file, 'utf8');
  const mask = commentMask(src);
  const needle = 'new wxStaticBitmap(';
  let idx = 0;
  while ((idx = src.indexOf(needle, idx)) !== -1) {
    const open = idx + needle.length - 1;
    const close = matchParen(src, open);
    if (close === -1 || mask[idx]) { idx = open + 1; continue; }
    const args = splitArgs(src.slice(open + 1, close - 1));
    const lineNo = src.slice(0, idx).split('\n').length;
    const lineStart = src.lastIndexOf('\n', idx) + 1;
    const lhs = src.slice(lineStart, idx);
    const member = (lhs.match(/([A-Za-z_][\w]*)\s*=\s*$/) || [])[1] || '';
    const later = [];
    let hasBind = false;
    if (member) {
      const re = new RegExp(`\\b${member.replace(/[$()*+.?[\\\]^{|}]/g, '\\$&')}\\s*->\\s*(SetBitmap|Bind)\\(([^;]{0,160})`, 'g');
      let m;
      while ((m = re.exec(src))) {
        if (mask[m.index]) continue;
        if (m[1] === 'Bind') { if (/wxEVT_LEFT_(DOWN|UP)|wxEVT_LEFT_DCLICK|wxEVT_BUTTON/.test(m[2])) hasBind = true; }
        else later.push(m[2].slice(0, 100));
      }
    }
    const site = {
      file: path.relative(guiRoot, file).replace(/\\/g, '/'), line: lineNo, member,
      source: (args[2] || '').slice(0, 120), size: args[4] || '', px: pxOf(args[4] || ''),
      laterSets: later, hasBind,
    };
    site.verdict = classify(site);
    sites.push(site);
    idx = close;
  }
}

sites.sort((a, b) => a.file.localeCompare(b.file) || a.line - b.line);
const csvEscape = (v) => `"${String(v).replace(/"/g, '""')}"`;
const header = 'file,line,member,px,has_bind,verdict,source,reason';
const rows = sites.map((s) => [s.file, s.line, s.member, s.px ?? '', s.hasBind ? 'yes' : 'no', s.verdict, s.source, ''].map(csvEscape).join(','));
const csv = [header, ...rows].join('\n') + '\n';
if (write) {
  await fs.mkdir(path.dirname(csvPath), { recursive: true });
  await fs.writeFile(csvPath, csv, 'utf8');
  console.log(`wrote ${path.relative(repoRoot, csvPath)} (${sites.length} sites)`);
} else {
  process.stdout.write(csv);
}
const tally = {};
for (const s of sites) tally[s.verdict] = (tally[s.verdict] || 0) + 1;
console.error(JSON.stringify(tally));
