#!/usr/bin/env node
// Replace icon-sized raster bitmaps held in a wxStaticBitmap with Material
// Symbols glyphs rendered through MaterialIcon::bitmap(), for the sites whose
// holder is built once from create_scaled_bitmap("<name>", …, px) and never
// re-set. The holder stays a wxStaticBitmap (it is not a control); what changes
// is that the mark is the kit glyph in an MD3 role instead of a baked PNG/SVG.
//
//   node scripts/md3/convert-static-glyphs.mjs --check | --write
//
// Only names in GLYPH_MAP are touched. A site whose member later receives
// another SetBitmap() is left alone (its states need per-site work).

import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '..', '..');
const guiRoot = path.join(repoRoot, 'src', 'slic3r', 'GUI');
const write = process.argv.includes('--write');

// raster name -> [Material glyph, MD3 role]
const GLYPH_MAP = {
  warning: ['Warning', 'Error'],
  obj_warning: ['Warning', 'Error'],
  more_info: ['Info', 'OnSurfaceVariant'],
  open_in_browser: ['OpenInNew', 'OnSurfaceVariant'],
  print: ['Print', 'OnSurfaceVariant'],
  cost: ['Payments', 'OnSurfaceVariant'],
  monitor_item_cost: ['Payments', 'OnSurfaceVariant'],
  prediction: ['Schedule', 'OnSurfaceVariant'],
  monitor_item_prediction: ['Schedule', 'OnSurfaceVariant'],
  partskip_retry: ['Refresh', 'OnSurfaceVariant'],
  helio_copy: ['ContentCopy', 'OnSurfaceVariant'],
  helio_refesh: ['Refresh', 'OnSurfaceVariant'],
  create_success: ['TaskAlt', 'Primary'],
  score_star_dark: ['Star', 'OutlineVariant'],
};

async function walk(dir, out = []) {
  for (const e of await fs.readdir(dir, { withFileTypes: true })) {
    const f = path.join(dir, e.name);
    if (e.isDirectory()) await walk(f, out); else if (e.name.endsWith('.cpp')) out.push(f);
  }
  return out;
}

const includeFor = (file, src) => {
  const rel = path.relative(guiRoot, path.dirname(file)).split(path.sep);
  const inWidgets = rel[0] === 'Widgets';
  const usesFull = /^#include "slic3r\/GUI\//m.test(src);
  const pre = inWidgets ? '' : usesFull ? 'slic3r/GUI/Widgets/' : rel[0] && rel[0] !== '' ? `${'../'.repeat(rel.length)}Widgets/` : 'Widgets/';
  return { icon: `#include "${pre}MaterialIcon.hpp"`, color: `#include "${pre}StateColor.hpp"` };
};

let converted = 0, touched = 0;
const done = [];
for (const file of await walk(guiRoot)) {
  let src = await fs.readFile(file, 'utf8');
  const original = src;
  // The raster call is matched with exactly its own closing parenthesis (the
  // px argument is either a bare number or FromDIP(n)), so the holder's own
  // parentheses are never consumed.
  const re = /new wxStaticBitmap\(\s*([^,]+?),\s*wxID_ANY,\s*create_scaled_bitmap\(\s*"([A-Za-z0-9_./-]+)"\s*,\s*([^,()]+?),\s*(FromDIP\(\d+\)|\d+)\s*\)/g;
  src = src.replace(re, (whole, parent, name, holderParent, px, offset) => {
    const map = GLYPH_MAP[name];
    if (!map) return whole;
    // Skip when the assigned member is re-set later (states handled per site).
    const lineStart = original.lastIndexOf('\n', offset) + 1;
    const member = (original.slice(lineStart, offset).match(/([A-Za-z_]\w*)\s*=\s*$/) || [])[1];
    if (member && new RegExp(`\\b${member}\\s*->\\s*SetBitmap\\(`).test(original)) return whole;
    converted++;
    done.push(`${path.relative(guiRoot, file).replace(/\\/g, '/')}:${name}->${map[0]}`);
    return `new wxStaticBitmap(${parent}, wxID_ANY, MaterialIcon::bitmap(${parent.trim()}, MaterialIcon::${map[0]}, ${px}, StateColor::semantic(MD3::Role::${map[1]}))`;
  });
  if (src !== original) {
    const inc = includeFor(file, src);
    const lines = src.split('\n');
    let last = -1;
    for (let i = 0; i < lines.length; i++) if (/^#include\s+"/.test(lines[i])) last = i;
    const eol = src.includes('\r\n') ? '\r' : '';
    const add = [];
    if (!/MaterialIcon\.hpp"/.test(src)) add.push(inc.icon + eol);
    if (!/StateColor\.hpp"/.test(src)) add.push(inc.color + eol);
    if (add.length) lines.splice(last + 1, 0, ...add);
    src = lines.join('\n');
    touched++;
    if (write) await fs.writeFile(file, src, 'utf8');
  }
}
for (const d of done) console.log(d);
console.log(`\n${write ? 'converted' : 'convertible'}=${converted} files=${touched}`);
