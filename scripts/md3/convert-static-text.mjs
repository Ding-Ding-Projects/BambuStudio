#!/usr/bin/env node
// Rewrite `new wxStaticText(...)` constructions under src/slic3r/GUI to the kit
// `Label` widget, which subclasses wxStaticText and applies the Material Design 3
// body type and OnSurface tone by default.
//
//   node scripts/md3/convert-static-text.mjs --check            report only
//   node scripts/md3/convert-static-text.mjs --write [files…]   rewrite (all, or the given files)
//
// Only the mechanically safe shapes are rewritten; everything else is listed for a
// human. Arguments are split with a balanced-parenthesis tokenizer, never a regex,
// because the text argument routinely contains commas inside _L("…") or
// wxString::Format(…). Rewritten shapes:
//
//   wxStaticText(P, wxID_ANY, T)                                     -> Label(P, T)
//   wxStaticText(P, wxID_ANY, T, wxDefaultPosition)                  -> Label(P, T)
//   wxStaticText(P, wxID_ANY, T, wxDefaultPosition, wxDefaultSize)   -> Label(P, T)
//   wxStaticText(P, wxID_ANY, T, wxDefaultPosition, S)               -> Label(P, T, 0, S)
//   wxStaticText(P, wxID_ANY, T, wxDefaultPosition, wxDefaultSize, F)-> Label(P, T, F)
//   wxStaticText(P, wxID_ANY, T, wxDefaultPosition, S, F)            -> Label(P, T, F, S)
//   … any of the above with a trailing wxStaticTextNameStr           -> the name is dropped
//
// Refused (reported, untouched): a window id other than wxID_ANY, an explicit
// position, a custom name, or a call inside a comment.

import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '..', '..');
const guiRoot = path.join(repoRoot, 'src', 'slic3r', 'GUI');
const KIT_SELF = new Set([path.join(guiRoot, 'Widgets', 'Label.cpp')]);

const args = process.argv.slice(2);
const write = args.includes('--write');
const check = args.includes('--check') || !write;
const explicitFiles = args.filter((a) => !a.startsWith('--')).map((f) => path.resolve(f));

async function walk(dir, out = []) {
  for (const entry of await fs.readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) await walk(full, out);
    else if (/\.(cpp|hpp|h)$/.test(entry.name)) out.push(full);
  }
  return out;
}

// Return the index just past the matching ')' for the '(' at `open`, honouring
// string and char literals. Returns -1 when unbalanced.
function matchParen(src, open) {
  let depth = 0;
  for (let i = open; i < src.length; i++) {
    const c = src[i];
    if (c === '"' || c === "'") {
      const q = c;
      i++;
      while (i < src.length && src[i] !== q) { if (src[i] === '\\') i++; i++; }
      continue;
    }
    if (c === '(') depth++;
    else if (c === ')') { depth--; if (depth === 0) return i + 1; }
  }
  return -1;
}

function splitArgs(inner) {
  const parts = [];
  let depth = 0, start = 0;
  for (let i = 0; i < inner.length; i++) {
    const c = inner[i];
    if (c === '"' || c === "'") {
      const q = c;
      i++;
      while (i < inner.length && inner[i] !== q) { if (inner[i] === '\\') i++; i++; }
      continue;
    }
    if (c === '(' || c === '[' || c === '{') depth++;
    else if (c === ')' || c === ']' || c === '}') depth--;
    else if (c === ',' && depth === 0) { parts.push(inner.slice(start, i)); start = i + 1; }
  }
  parts.push(inner.slice(start));
  return parts.map((p) => p.trim()).filter((p, i, a) => !(p === '' && i === a.length - 1));
}

// One forward scan that knows about string/char literals, line comments and
// block comments, so a "/*" inside a string can never make the rest of the
// file look commented out (that heuristic silently skipped five live sites).
function commentMask(src) {
  const mask = new Uint8Array(src.length);
  let i = 0;
  while (i < src.length) {
    const c = src[i], n = src[i + 1];
    if (c === '"' || c === "'") {
      const q = c; i++;
      while (i < src.length && src[i] !== q) { if (src[i] === '\\') i++; i++; }
      i++;
      continue;
    }
    if (c === '/' && n === '/') { while (i < src.length && src[i] !== '\n') mask[i++] = 1; continue; }
    if (c === '/' && n === '*') {
      const end = src.indexOf('*/', i + 2);
      const stop = end === -1 ? src.length : end + 2;
      while (i < stop) mask[i++] = 1;
      continue;
    }
    i++;
  }
  return mask;
}

function rewriteCall(callArgs) {
  const [parent, id, text, pos, size, style, name, ...rest] = callArgs;
  if (rest.length) return { refuse: 'too many arguments' };
  if (callArgs.length < 3) return { refuse: 'fewer than three arguments' };
  if (id !== 'wxID_ANY') return { refuse: `window id ${id}` };
  if (pos !== undefined && pos !== 'wxDefaultPosition') return { refuse: `explicit position ${pos}` };
  if (name !== undefined && name !== 'wxStaticTextNameStr') return { refuse: `custom name ${name}` };
  const hasSize = size !== undefined && size !== 'wxDefaultSize';
  const hasStyle = style !== undefined && style !== '0';
  let out = `new Label(${parent}, ${text}`;
  if (hasStyle && hasSize) out += `, ${style}, ${size}`;
  else if (hasStyle) out += `, ${style}`;
  else if (hasSize) out += `, 0, ${size}`;
  out += ')';
  return { out };
}

function includeSpelling(file, src) {
  if (/^#include "(\.\.\/)?Widgets\/Label\.hpp"|^#include "Label\.hpp"|^#include "slic3r\/GUI\/Widgets\/Label\.hpp"/m.test(src)) return null;
  const rel = path.relative(guiRoot, path.dirname(file)).split(path.sep);
  if (rel[0] === 'Widgets') return '#include "Label.hpp"';
  if (/^#include "slic3r\/GUI\//m.test(src)) return '#include "slic3r/GUI/Widgets/Label.hpp"';
  if (rel.length && rel[0] !== '') return `#include "${'../'.repeat(rel.length)}Widgets/Label.hpp"`;
  return '#include "Widgets/Label.hpp"';
}

function addInclude(src, line) {
  const lines = src.split('\n');
  let last = -1;
  for (let i = 0; i < lines.length; i++) if (/^#include\s+"/.test(lines[i])) last = i;
  if (last === -1) for (let i = 0; i < lines.length; i++) if (/^#include\s+</.test(lines[i])) last = i;
  const eol = src.includes('\r\n') ? '\r' : '';
  lines.splice(last + 1, 0, line + eol);
  return lines.join('\n');
}

async function processFile(file) {
  const src = await fs.readFile(file, 'utf8');
  const needle = 'new wxStaticText(';
  let idx = 0, out = '', cursor = 0;
  const converted = [], refused = [];
  const mask = commentMask(src);
  while ((idx = src.indexOf(needle, idx)) !== -1) {
    const lineNo = src.slice(0, idx).split('\n').length;
    const open = idx + needle.length - 1;
    const close = matchParen(src, open);
    if (close === -1) { refused.push({ lineNo, why: 'unbalanced parentheses' }); idx = open + 1; continue; }
    if (mask[idx]) { idx = close; continue; }
    const inner = src.slice(open + 1, close - 1);
    const r = rewriteCall(splitArgs(inner));
    if (r.refuse) { refused.push({ lineNo, why: r.refuse }); idx = close; continue; }
    out += src.slice(cursor, idx) + r.out;
    cursor = close;
    converted.push(lineNo);
    idx = close;
  }
  out += src.slice(cursor);
  let includeAdded = null;
  if (converted.length) {
    const inc = includeSpelling(file, out);
    if (inc) { out = addInclude(out, inc); includeAdded = inc; }
  }
  return { file, converted, refused, out, changed: out !== src, includeAdded };
}

const files = (explicitFiles.length ? explicitFiles : await walk(guiRoot)).filter((f) => !KIT_SELF.has(f));
let totalConverted = 0, totalRefused = 0, touched = 0;
const refusals = [];
for (const file of files) {
  const r = await processFile(file);
  if (!r.converted.length && !r.refused.length) continue;
  totalConverted += r.converted.length;
  totalRefused += r.refused.length;
  for (const x of r.refused) refusals.push(`${path.relative(repoRoot, file)}:${x.lineNo}  ${x.why}`);
  if (write && r.changed) { await fs.writeFile(file, r.out, 'utf8'); touched++; }
  if (check) console.log(`${path.relative(repoRoot, file)}  convert=${r.converted.length} refuse=${r.refused.length}${r.includeAdded ? '  +' + r.includeAdded : ''}`);
}
console.log(`\nconvertible=${totalConverted} refused=${totalRefused}${write ? ` written=${touched}` : ''}`);
if (refusals.length) { console.log('\nManual sites:'); for (const r of refusals) console.log('  ' + r); }
