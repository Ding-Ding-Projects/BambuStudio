// Source checks for pausing and cancelling one Base64 file alone
// (docs/features/windows/base64-toolkit.md, "Files, beside the originals"):
//
// - The worker never starts a queued file that is paused by itself, and a
//   running file that is paused waits inside its progress callback until it
//   is resumed, cancelled or the queue stops.
// - A cancel clears the file's own pause, so a paused file wakes to notice it.
// - The buttons and the row's menu act on the selected row only; the row says
//   it is paused by itself, apart from a pause of everything.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const panel = read('src/slic3r/GUI/Base64Toolkit/Base64ToolkitPanel.cpp');

function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, name);
    const begin = source.indexOf('{', start);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, (t) => ' '.repeat(t.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const code = (text) => text.replace(/\/\/[^\n]*/g, '');

function checkWorker(source) {
    const worker = code(body(source, 'void Base64ToolkitPanel::worker_loop('));
    assert.match(worker, /return j\.status == FileJob::Status::Queued && !j\.paused;/, 'a paused queued file does not wake the worker');
    assert.match(worker, /if \(job\.status != FileJob::Status::Queued \|\| job\.paused\) continue;/, 'a paused queued file is skipped');
    assert.match(worker, /return q->stop \|\| job == nullptr \|\| job->cancel \|\| \(!q->paused && !job->paused\);/, 'a paused running file waits in its callback');
}

function checkPauseAndCancel(source) {
    const pause = code(body(source, 'void Base64ToolkitPanel::pause_selected('));
    assert.match(pause, /const int row = m_jobs->GetSelectedRow\(\);/);
    assert.match(pause, /if \(job\.status != FileJob::Status::Queued && job\.status != FileJob::Status::Running\) return;/);
    assert.match(pause, /else if \(job\.status == FileJob::Status::Running && !m_queue->paused\)\s*job\.paused_seconds \+= /, 'time left leaves the pause out');
    assert.match(pause, /job\.paused = pause;\s*\}\s*m_queue->wake\.notify_all\(\);/);
    const cancel = code(body(source, 'void Base64ToolkitPanel::cancel_selected('));
    assert.match(cancel, /job\.cancel = true;\s*job\.paused = false;/, 'a cancel wakes a paused file');
    assert.match(cancel, /job\.result\.cancelled = true;/);
}

function checkSurface(source) {
    const poll = code(body(source, 'void Base64ToolkitPanel::poll_jobs()'));
    assert.match(poll, /job\.paused \? _L\("Paused, this file"\) : _L\("Waiting"\)/);
    assert.match(poll, /state\s*= paused \? _L\("Paused"\) : _L\("Paused, this file"\);/);
    assert.match(poll, /refresh_selection_buttons\(\);/);
    const menu = code(body(source, 'void Base64ToolkitPanel::show_jobs_menu('));
    assert.match(menu, /menu\.Append\(pause_id, paused \? _L\("Resume this file"\) : _L\("Pause this file"\)\)->Enable\(live\);/);
    assert.match(menu, /menu\.Append\(cancel_id, _L\("Cancel this file"\)\)->Enable\(live\);/);
    assert.match(menu, /if \(chosen == pause_id\) pause_selected\(!paused\);\s*else if \(chosen == cancel_id\) cancel_selected\(\);/);
    assert.match(source, /m_jobs->Bind\(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, \[this\]\(wxDataViewEvent &\) \{ show_jobs_menu\(wxGetMousePosition\(\)\); \}\);/);
    assert.match(source, /m_cancel_one->Bind\(wxEVT_BUTTON, \[this\]\(wxCommandEvent &\) \{ cancel_selected\(\); \}\);/);
}

test('the worker honours a file paused by itself', () => checkWorker(panel));
test('pause and cancel act on the selected file', () => checkPauseAndCancel(panel));
test('the row, the buttons and the menu say and do it', () => checkSurface(panel));

test('each checker fails on a broken copy', () => {
    assert.throws(() => checkWorker(panel.replace('(!q->paused && !job->paused)', '!q->paused')));
    assert.throws(() => checkPauseAndCancel(panel.replace('job.cancel = true;\n            job.paused = false;', 'job.cancel = true;')));
    assert.throws(() => checkSurface(panel.replace('_L("Paused, this file") : _L("Waiting")', '_L("Waiting") : _L("Waiting")')));
});
