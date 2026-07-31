import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { splitStructuredTranslation } from '../src/i18nResources.ts';

const featureDirectory = fileURLToPath(
  new URL('../src/features/filament-manager/', import.meta.url),
);
const readFeature = (name: string) => readFileSync(`${featureDirectory}${name}`, 'utf8');

const page = readFeature('FilamentManagerPage.tsx');
const table = readFeature('SpoolTable.tsx');
const addEdit = readFeature('AddEditDialog.tsx');
const confirm = readFeature('ConfirmDialog.tsx');
const detail = readFeature('DetailDialog.tsx');
const history = readFeature('CloudHistoryPopover.tsx');
const toast = readFeature('ToastStack.tsx');
const dialog = readFeature('AccessibleDialog.tsx');
const css = readFeature('filament-manager.css');

assert.deepEqual(splitStructuredTranslation('Ink'), { primary: 'Ink' });
assert.deepEqual(splitStructuredTranslation('Ink\n粵語：墨水'), {
  primary: 'Ink',
  secondary: '粵語：墨水',
});

assert.match(page, /role="tablist"/);
assert.match(page, /role="tab"/);
assert.match(page, /aria-selected=\{tab === tabItem\}/);
assert.match(page, /const FILAMENT_TABS: readonly TabMode\[\]/);
assert.match(page, /index - 1 \+ FILAMENT_TABS\.length/);
assert.match(page, /aria-haspopup="listbox"/);
assert.match(page, /aria-label=\{`\$\{t\(FILTER_LABEL_KEYS\[fk\]\)\}: \$\{filters\[fk\] \|\| t\('All'\)\}`\}/);
assert.match(page, /event\.key === 'Escape'/);
assert.match(page, /tabIndex=\{index === activeIndex \? 0 : -1\}/);
assert.match(page, /filterTriggerRefs\.current\[fk\]\?\.focus\(\)/);
assert.match(page, /role="option"/);
assert.match(page, /className=\{`fm-search/);
assert.match(page, /min-w-\[8rem\].*flex-1/);

assert.match(table, /aria-sort=\{isCurrent \?/);
assert.match(table, /fm-sort-button/);
assert.match(table, /aria-expanded=\{!isCollapsed\}/);
assert.match(table, /fm-row-action/);
assert.match(table, /fm-pagination-target/);
assert.match(table, /aria-current=\{p === safePage \? 'page'/);

for (const source of [addEdit, confirm, detail, history]) {
  assert.match(source, /<AccessibleDialog/);
}
assert.match(dialog, /role="dialog"/);
assert.match(dialog, /aria-modal="true"/);
assert.match(dialog, /event\.key === 'Escape'/);
assert.match(dialog, /event\.key !== 'Tab'/);
assert.match(dialog, /restoreTarget\.focus/);
assert.match(dialog, /onCloseRef\.current\(\)/);
assert.match(dialog, /initialFocusRefRef\.current\?\.current/);
assert.match(dialog, /\}, \[open\]\);/);
assert.doesNotMatch(dialog, /\[initialFocusRef, onClose, open\]/);
assert.doesNotMatch(confirm, /e\.key === 'Enter'/);

assert.match(toast, /toast\.level === 'info'/);
assert.match(toast, /role="status" aria-live="polite"/);
assert.match(toast, /role="alert" aria-live="assertive"/);
assert.match(toast, /setPoliteAnnouncement/);
assert.match(toast, /setAssertiveAnnouncement/);
assert.doesNotMatch(toast, /role=\{toast\.level ===/);
assert.match(toast, /fm-toast-dismiss/);

for (const width of ['640px', '360px']) {
  assert.match(css, new RegExp(`max-width: ${width.replace('.', '\\.')}`));
}
assert.match(css, /\.fm-search input\s*\{\s*min-width: 0;/s);
assert.match(css, /@media \(prefers-reduced-motion: reduce\)/);
assert.match(css, /:focus-visible/);
assert.match(css, /\.fm-label-secondary/);
