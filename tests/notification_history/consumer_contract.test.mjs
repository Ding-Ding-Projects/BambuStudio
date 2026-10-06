import { readFileSync } from 'node:fs';
import assert from 'node:assert/strict';
import test from 'node:test';
const source = readFileSync(new URL('../../src/slic3r/GUI/NotificationCenterPanel.cpp', import.meta.url), 'utf8');
function confirmsReviewed(text) {
  const body = text.slice(text.indexOf('void NotificationCenterPanel::on_delete_confirmed'));
  const request = text.slice(text.indexOf('void NotificationCenterPanel::on_delete_requested'), text.indexOf('void NotificationCenterPanel::on_delete_confirmed'));
  const snapshot = request.indexOf('const auto reviewed_ids = m_manager->history().reviewed_selection(m_selection, m_matches);');
  const preview = request.indexOf('Bulk::BulkActionPreviewDialog::Run(this, plan)');
  return snapshot >= 0 && preview > snapshot &&
    /if \(Bulk::BulkActionPreviewDialog::Run\(this, plan\)\)\s*on_delete_confirmed\(reviewed_ids\);/.test(request) &&
    body.includes('history().erase(reviewed_ids)') && !body.includes('m_matches') && !body.includes('m_selection.contains');
}
test('confirmation uses reviewed IDs without consulting a later selection', () => assert.ok(confirmsReviewed(source)));
test('negative regression rejects later-selection substitution', () => {
  assert.equal(confirmsReviewed(source.replace('history().erase(reviewed_ids)', 'history().erase(m_selection.ids())')), false);
});
test('notification export uses shared snapshot dialog', () => {
  assert.ok(source.includes('ExportDialog::run(this, m_manager->history().export_dataset(ids, current_filter()))'));
  assert.equal(source.includes('std::ios::trunc'), false);
});

test('negative regression rejects a live selection passed after preview', () => {
  assert.equal(confirmsReviewed(source.replace('on_delete_confirmed(reviewed_ids);', 'on_delete_confirmed(m_selection.ids());')), false);
});
