#!/usr/bin/env node
// Replace hand-styled legacy Button palettes with kit variants.
//
// A run of consecutive colour setters on one kit Button, such as
//
//     btn->SetBackgroundColor(btn_bg_green);
//     btn->SetBorderColor(ThemeColor::BrandGreen);
//     btn->SetTextColor(ThemeColor::White);
//
// becomes `btn->SetVariant(Button::Variant::Filled);` (green / white-text
// palettes) or `Button::Variant::Outlined` (white / grey palettes with a dark
// or grey border). The variant carries hover, pressed, focus and disabled
// states from MD3 roles, so the per-state StateColor literals are no longer
// needed. Only objects that the same file constructs with `new Button(` or
// declares as `Button *` (in the .cpp or its .hpp) are touched; TextInput,
// ComboBox and StaticBox setters of the same name are left alone.
//
//   node scripts/md3/convert-button-styling.mjs --check | --write
//
// Runs whose palette cannot be classified are listed and left in place.

import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '..', '..');
const guiRoot = path.join(repoRoot, 'src', 'slic3r', 'GUI');
const write = process.argv.includes('--write');

const SETTER = /^(\s*)([A-Za-z_]\w*(?:->|\.))Set(Background|Border|Text)Color(?:Normal)?\((.*)$/;
const GREEN = /green|BrandGreen|Primary\)|Primary,/i;
const WHITE_TEXT = /ThemeColor::White|\*wxWHITE|#FFFFFF|#FEFEFE|ok_btn_text|OnPrimary/i;
const LIGHT_BG = /btn_bg_white|ThemeColor::White|\*wxWHITE|Grey100|Grey200|Surface\b|SurfaceContainer/;
const DARK_BORDER = /TextPrimary|Grey400|Grey500|Grey600|Outline\)|Outline,|wxColour\(93, 93, 91\)/;

async function walk(dir, out = []) {
  for (const e of await fs.readdir(dir, { withFileTypes: true })) {
    const f = path.join(dir, e.name);
    if (e.isDirectory()) await walk(f, out); else if (e.name.endsWith('.cpp')) out.push(f);
  }
  return out;
}

function parenDepth(text, start = 0) {
  let d = start;
  let inStr = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (inStr) { if (c === '\\') i++; else if (c === '"') inStr = false; continue; }
    if (c === '"') inStr = true;
    else if (c === '(') d++;
    else if (c === ')') d--;
    else if (c === '/' && text[i + 1] === '/') break;
  }
  return d;
}

async function isButton(name, src, file) {
  const esc = name.replace(/[$()*+.?[\\\]^{|}]/g, '\\$&');
  const ctor = new RegExp(`\\b${esc}\\s*=\\s*new\\s+Button\\s*\\(`);
  const decl = new RegExp(`\\bButton\\s*\\*\\s*${esc}\\b`);
  if (ctor.test(src) || decl.test(src)) return true;
  const hpp = file.replace(/\.cpp$/, '.hpp');
  try {
    const h = await fs.readFile(hpp, 'utf8');
    return decl.test(h);
  } catch { return false; }
}

// A setter argument that is a bare identifier names a local or member
// StateColor; classify on its definition text (the first `Name(` or `Name =`
// that precedes the site, plus the balanced expression that follows).
function resolve(arg, src, before) {
  const id = arg.trim().replace(/\);?\s*$/, '');
  if (!/^[A-Za-z_]\w*$/.test(id)) return arg;
  const esc = id.replace(/[$()*+.?[\\\]^{|}]/g, '\\$&');
  const re = new RegExp(`\\b(?:StateColor|wxColour)\\s+${esc}\\s*[(=]|\\b${esc}\\s*=\\s*(?:StateColor|wxColour)\\s*\\(`, 'g');
  let last = -1, m;
  while ((m = re.exec(src)) && m.index < before) last = m.index;
  if (last < 0) return arg;
  const eq = src.indexOf('=', last), paren = src.indexOf('(', last);
  if (eq >= 0 && (paren < 0 || eq < paren)) {
    // `StateColor name = expr;` : the whole initialiser, helper name included.
    const semi = src.indexOf(';', eq);
    return semi < 0 ? arg : src.slice(eq + 1, semi).trim();
  }
  let depth = 0, i = paren, start = i;
  if (i < 0) return arg;
  for (; i < src.length; i++) {
    if (src[i] === '(') depth++;
    else if (src[i] === ')') { depth--; if (depth === 0) break; }
  }
  return src.slice(start, i + 1);
}

// Light-mode values of the legacy ThemeColor tokens (Widgets/StateColor.hpp).
const THEME = {
  BrandGreen: '#146c2e', BrandGreenHovered: '#1a7d38', BrandGreenPressed: '#0d5322', Warning: '#FF6F00',
  Danger: '#ba1a1a', Link: '#0078D4', TextPrimary: '#1a1b1f', TextSecondary: '#44464e', TextMuted: '#5c5f66',
  TextDisabled: '#9a9ba3', White: '#ffffff', Grey200: '#f4f2f9', Grey250: '#eeedf3', Grey300: '#e8e7ee',
  Grey350: '#e2e1e9', Grey400: '#c5c6d0', Grey450: '#75777f', Grey500: '#75777f', Grey700: '#5c5f66',
};
const CANCEL_FAMILY = /cancel|skip|close|back|dont_show|abort|later|ignore|no_btn|del(?:ete)?\b|remove|refresh|reset|retry/i;

// Mean channel value of a colour expression, or null when it cannot be read.
function luminance(expr, ctx) {
  let e = expr.trim().replace(/\)*;?\s*$/, '');
  for (let hop = 0; hop < 3; hop++) {
    let m;
    if ((m = e.match(/wxColou?r\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})/))) return (+m[1] + +m[2] + +m[3]) / 3;
    if ((m = e.match(/#([0-9a-fA-F]{6})\b/))) { const h = m[1]; return (parseInt(h.slice(0, 2), 16) + parseInt(h.slice(2, 4), 16) + parseInt(h.slice(4, 6), 16)) / 3; }
    if (/\*wxWHITE\b/.test(e)) return 255;
    if (/\*wxBLACK\b/.test(e)) return 0;
    if ((m = e.match(/ThemeColor::(\w+)/))) { const hex = THEME[m[1]]; if (hex) { e = hex; continue; } return null; }
    if (/StateColor::createButtonStyleGray\(\)/.test(e)) return 255;
    if ((m = e.match(/MD3::Role::(\w+)/))) {
      const role = m[1];
      // Light roles: the text-on-accent roles, every container, and surfaces.
      if (/^On(Primary|Secondary|Tertiary|Error)$/.test(role) || /Container/.test(role) || /^(Surface|Background|InverseOnSurface)/.test(role)) return 240;
      return 40;
    }
    if ((m = e.match(/^([A-Za-z_]\w*)$/)) && ctx) {
      const def = ctx.defines.get(m[1]);
      if (def) { e = def; continue; }
    }
    return null;
  }
  return null;
}

// The Normal-state colour of a resolved setter argument.
function normalOf(resolved) {
  // One argument (no bare commas; one level of parentheses allowed) directly
  // followed by the Normal state, so a multi-state list yields its Normal entry
  // and never the first entry.
  const m = resolved.match(/std::(?:pair<wxColou?r,\s*int>|make_pair)\(\s*((?:[^,()]|\([^()]*\))+?)\s*,\s*(?:\(int\)\s*)?StateColor::(?:Normal|Enabled)\s*\)/);
  if (m) return m[1];
  if (/StateColor::(Hovered|Pressed|Disabled|Focused|Checked)/.test(resolved)) return null; // states only, no rest
  return resolved;
}

function classify(setters, name, ctx) {
  const pick = (kind) => setters.filter((s) => s.kind === kind).map((s) => s.resolved).join(' ');
  const bg = pick('Background'), border = pick('Border'), text = pick('Text');
  const all = bg + border + text;
  // Already on MD3 roles: leave the caller's semantic styling alone.
  if (/MD3::Role::/.test(all) && !/ThemeColor::|wxColou?r\(|\*wx(WHITE|BLACK)/.test(all)) return 'semantic';
  // Tone helpers that already return MD3 roles (device panel, dialog helpers,
  // the AMS macros annotated to MD3 tokens): the caller is semantic already.
  if (/\bdevice_\w*(?:color|text|border|background)\(\)|(?:outlined|filled)_button_background\(\)|AMS_CONTROL_\w+/.test(all) && !/ThemeColor::|wxColou?r\(\s*\d/.test(all)) return 'semantic';
  // Data-coloured chips (filament type tiles painted with the filament colour)
  // are data, not chrome: keep them.
  if (/^f_type$/.test(name) || /decode_color\(|\bf\.color\b|/.test(all)) return 'semantic';
  if (CANCEL_FAMILY.test(name)) return 'Outlined';
  const bgN = bg ? normalOf(bg) : null;
  const lBg = bgN ? luminance(bgN, ctx) : null;
  if (lBg !== null) return lBg >= 190 ? 'Outlined' : 'Filled';
  const textN = text ? normalOf(text) : null;
  const lText = textN ? luminance(textN, ctx) : null;
  if (lText !== null) return lText >= 190 ? 'Filled' : 'Outlined';
  const borderN = border ? normalOf(border) : null;
  const lBorder = borderN ? luminance(borderN, ctx) : null;
  if (lBorder !== null && !bg) return 'Outlined';
  return null;
}

// #define NAME wxColour(...) / StateColor NAME(...) tables in the file and its header.
async function definesOf(src, file) {
  const map = new Map();
  let h = '';
  try { h = await fs.readFile(file.replace(/\.cpp$/, '.hpp'), 'utf8'); } catch {}
  try { h += await fs.readFile(file.replace(/\.cpp$/, '.h'), 'utf8'); } catch {}
  for (const text of [src, h]) {
    for (const m of text.matchAll(/^\s*#define\s+([A-Z_][A-Z0-9_]*)\s+(.+?)\s*$/gm)) map.set(m[1], m[2]);
    for (const m of text.matchAll(/^\s*(?:static\s+)?(?:const\s+)?wxColou?r\s+([A-Za-z_]\w*)\s*[=({]\s*([^;]+);/gm)) map.set(m[1], m[2]);
  }
  return map;
}

let converted = 0, skipped = 0, filesTouched = 0;
const report = [];
for (const file of await walk(guiRoot)) {
  const rel = path.relative(guiRoot, file).replace(/\\/g, '/');
  if (rel.startsWith('Widgets/')) continue; // the kit styles itself
  const src = await fs.readFile(file, 'utf8');
  const nl = src.includes('\r\n') ? '\r\n' : '\n';
  const lines = src.split(/\r?\n/);
  const out = [];
  const ctx = { defines: await definesOf(src, file) };
  let changed = false;
  for (let i = 0; i < lines.length;) {
    const m = lines[i].match(SETTER);
    if (!m) { out.push(lines[i]); i++; continue; }
    // collect a run of setters on the same object
    const indent = m[1], obj = m[2];
    const name = obj.slice(0, -(obj.endsWith('->') ? 2 : 1));
    const setters = [];
    let j = i;
    while (j < lines.length) {
      const mm = lines[j].match(SETTER);
      if (!mm || mm[2] !== obj) break;
      let arg = mm[4];
      let depth = parenDepth('(' + arg);
      let k = j;
      while (depth > 0 && k + 1 < lines.length) { k++; arg += ' ' + lines[k].trim(); depth = parenDepth('(' + arg); }
      const before = lines.slice(0, j).reduce((n, l) => n + l.length + nl.length, 0);
      setters.push({ kind: mm[3], arg, resolved: resolve(arg, src, before), from: j, to: k });
      j = k + 1;
    }
    const variant = classify(setters, name, ctx);
    const button = await isButton(name, src, file);
    const site = `${rel}:${i + 1} ${name}`;
    if (!button) {
      report.push(`  skip (not a Button)  ${site}`);
      skipped++;
      for (let q = i; q < j; q++) out.push(lines[q]);
      i = j; continue;
    }
    if (variant === 'semantic') {
      report.push(`  keep (roles / data)  ${site}`);
      for (let q = i; q < j; q++) out.push(lines[q]);
      i = j; continue;
    }
    if (!variant) {
      report.push(`  skip (unclassified)  ${site}  [${setters.map((s) => s.kind + '=' + s.resolved.replace(/\s+/g, ' ').slice(0, 70)).join(' | ')}]`);
      skipped++;
      for (let q = i; q < j; q++) out.push(lines[q]);
      i = j; continue;
    }
    const line = `${indent}${obj}SetVariant(Button::Variant::${variant});`;
    // Two runs on one object a few lines apart collapse into one call.
    const recent = out.slice(-4).some((l) => l.trim() === line.trim());
    if (!recent) out.push(line);
    report.push(`  ${variant.padEnd(8)} ${site}${recent ? ' (merged)' : ''}`);
    converted++; changed = true;
    i = j;
  }
  if (changed) {
    filesTouched++;
    if (write) await fs.writeFile(file, out.join(nl), 'utf8');
  }
}
console.log(report.join('\n'));
console.log(`\n${converted} run(s) converted in ${filesTouched} file(s), ${skipped} skipped${write ? '' : ' (check only)'}`);
