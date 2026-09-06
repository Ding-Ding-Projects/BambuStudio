#!/usr/bin/env node
// Turn one or more layout-probe NDJSON dumps (written by the app when
// BAMBU_LAYOUT_PROBE is set; see src/slic3r/GUI/LayoutProbe.hpp) into a findings
// table. Exit code 1 when any finding is present, so a capture run can gate on it.
//
//   node ui-md3/tests/layout-probe-report.mjs <dump.jsonl> [more.jsonl…] [--json]
//
// Findings, in severity order:
//   zero_sized       a shown window with zero width or height (a control that vanished)
//   starved          a shown sizer child allocated less than its own minimum
//   oversubscribed   a wxBoxSizer whose children's minimums exceed its size
//   text_clipped     a label whose text is wider than its client area and not ellipsized
//   clipped_by_parent a shown window whose rect leaves its parent's client area
// Hidden windows and hidden top-levels never count; a wxStaticText that is
// ellipsized is reported under `ellipsized` for review but is not a finding.

import { readFileSync } from 'node:fs';

const args = process.argv.slice(2);
const asJson = args.includes('--json');
const files = args.filter((a) => !a.startsWith('--'));
if (!files.length) {
  console.error('usage: layout-probe-report.mjs <dump.jsonl> [more…] [--json]');
  process.exit(2);
}

const findings = [];
const headers = [];
for (const file of files) {
  const lines = readFileSync(file, 'utf8').split(/\r?\n/).filter(Boolean);
  const records = lines.map((l) => JSON.parse(l));
  const header = records.find((r) => r.kind === 'header') || {};
  headers.push({ file, ...header });
  const tops = new Map(records.filter((r) => r.kind === 'toplevel').map((r) => [r.hwnd, r]));
  const byHwnd = new Map(records.filter((r) => r.kind === 'window').map((r) => [r.hwnd, r]));
  const shownChain = (w) => {
    // Dumps from builds after 2026-09-06 carry IsShownOnScreen; older dumps
    // fall back to walking the shown flags up the parent chain.
    // A hidden top-level still answers IsShownOnScreen for its children on
    // MSW (the hidden Compare Presets dialog's buttons kept coming back), so
    // the top-level's own flag always wins.
    const top0 = tops.get(w.top);
    if (top0 && !top0.shown) return false;
    if (typeof w.on_screen === 'boolean') return w.on_screen;
    for (let cur = w; cur; cur = byHwnd.get(cur.parent)) if (!cur.shown) return false;
    const top = tops.get(w.top);
    return !top || top.shown;
  };
  const labelOf = (w) => w.label || w.name || '';
  const where = (w) => `${(tops.get(w.top) || {}).title || ''} > ${w.class}${labelOf(w) ? ` "${labelOf(w).slice(0, 48)}"` : ''}`;
  const seenRows = new Set();
  // A window whose ancestor has collapsed to zero size is not on screen even
  // when every shown flag says yes (the rating stars live inside a 0x0 scroller
  // until a file is rated). IsShownOnScreen does not see that either.
  const underCollapsed = (w) => {
    for (let cur = byHwnd.get(w.parent); cur; cur = byHwnd.get(cur.parent)) if (cur.rect && (cur.rect.w === 0 || cur.rect.h === 0)) return true;
    return false;
  };
  for (const w of byHwnd.values()) {
    if (!shownChain(w) || underCollapsed(w)) continue;
    const base = { file, hwnd: w.hwnd, where: where(w), rect: w.rect, min: w.min };
    // An empty label legitimately measures zero wide (the estimate detail is
    // blank until a plate is sliced); only a labelled or non-text control
    // that vanished is a finding.
    const emptyLabel = /StaticText|Label/.test(w.class) && !(w.label || '').trim();
    if (w.zero_sized && !emptyLabel) findings.push({ ...base, finding: 'zero_sized' });
    if (w.starved) findings.push({ ...base, finding: 'starved', alloc: w.sizer?.alloc, need: w.sizer?.min });
    if (w.sizer?.row?.oversubscribed) {
      const key = `${w.parent}:${w.sizer.row.orient}:${w.sizer.row.available}:${w.sizer.row.required}`;
      if (!seenRows.has(key)) {
        seenRows.add(key);
        findings.push({ file, hwnd: w.parent, where: `${where(w)} (row)`, finding: 'oversubscribed', available: w.sizer.row.available, required: w.sizer.row.required, deficit: w.sizer.row.required - w.sizer.row.available });
      }
    }
    if (w.text_clipped) findings.push({ ...base, finding: 'text_clipped', text_width: w.text_width, client: w.client });
    // A top-level window is positioned by the user, not clipped by its owner;
    // a row further down a scrolled page is scrolled out of view, not clipped.
    // Both would otherwise flood the report (measured on Preferences: 30 of 44
    // findings were the Developer Mode rows below the fold).
    const underScroller = (() => {
      for (let cur = byHwnd.get(w.parent); cur; cur = byHwnd.get(cur.parent)) if (/Scrolled/.test(cur.class)) return true;
      return false;
    })();
    if (w.clipped_by_parent && w.depth > 0 && !underScroller) findings.push({ ...base, finding: 'clipped_by_parent' });
  }
}

const order = { zero_sized: 0, starved: 1, oversubscribed: 2, text_clipped: 3, clipped_by_parent: 4 };
findings.sort((a, b) => order[a.finding] - order[b.finding] || a.where.localeCompare(b.where));

if (asJson) {
  console.log(JSON.stringify({ headers, findings }, null, 2));
} else {
  for (const h of headers) console.log(`# ${h.file}: reason=${h.reason} tag=${h.tag || ''} dpi=${h.dpi_scale} lang=${h.language} dark=${h.dark} density=${h.density} toplevels=${h.top_levels}`);
  if (!findings.length) console.log('No layout findings.');
  for (const f of findings) {
    const extra = f.finding === 'oversubscribed' ? ` available=${f.available} required=${f.required} deficit=${f.deficit}`
      : f.finding === 'starved' ? ` alloc=${f.alloc?.w}x${f.alloc?.h} need=${f.need?.w}x${f.need?.h}`
      : f.finding === 'text_clipped' ? ` text=${f.text_width}px client=${f.client?.w}px`
      : ` rect=${f.rect?.x},${f.rect?.y} ${f.rect?.w}x${f.rect?.h}`;
    console.log(`${f.finding.padEnd(18)} ${f.where}${extra}`);
  }
  console.log(`\n${findings.length} finding(s) across ${files.length} dump(s).`);
}
process.exit(findings.length ? 1 : 0);
