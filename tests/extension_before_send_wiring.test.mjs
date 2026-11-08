// Source checks for the print.before_send hook in the running app:
//
// - The print dialog asks the extensions first, when Send is pressed for a
//   plate sliced in this project, a file already on the printer or a print
//   queue job, before the app's own pre-print check, and honours what the
//   check window decided: Back stays, Open Extensions closes the print dialog
//   and opens the manager, Send carries on. A file on the printer and a queue
//   job say so and carry their name only; Send to the printer's storage and
//   sending to several printers ask too, before anything is prepared, and
//   never name the printers.
// - Only a send sliced in this project carries the project's slice.
// - print.finished reports the slice's plan only for a print sent from here,
//   printed now (not queued), and seen starting on that printer within 30
//   minutes.
// - What the extensions are told about the send never names the printer: no
//   serial number (dev_id), name, address or access code.
// - The runtime runs the check ahead of anything waiting, reports every
//   result to the window instead of a notification, and always says when it
//   is done, also when the job is dropped.
// - The window decides with summarise_send_check and stops the runs when the
//   person leaves early.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const dialog = read('src/slic3r/GUI/SelectMachine.cpp');
const runtime = read('src/slic3r/GUI/Extensions/ExtensionRuntime.cpp');
const window = read('src/slic3r/GUI/Extensions/BeforeSendCheck.cpp');
const core = read('src/slic3r/GUI/Extensions/ExtensionCore.cpp');
const storage = read('src/slic3r/GUI/SendToPrinter.cpp');
const several = read('src/slic3r/GUI/SendMultiMachinePage.cpp');

function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, name);
    const begin = source.indexOf('{', start);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g,
        text => ' '.repeat(text.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const code = text => text.replace(/\/\/[^\n]*/g, '');

function checkPrintDialog(source) {
    const ok = code(body(source, 'void SelectMachineDialog::on_ok_btn('));
    const machine = ok.indexOf('if (!obj_) return;');
    const guard = ok.indexOf('if (m_print_type == PrintFromType::FROM_NORMAL || m_print_type == PrintFromType::FROM_SDCARD_VIEW ||');
    const call = ok.indexOf('Extensions::check_before_send(');
    const own = ok.indexOf('std::vector<ConfirmBeforeSendInfo> confirm_text;');
    assert(machine >= 0 && guard > machine && call > guard && own > call, 'the extensions answer after the printer is known and before the app check');
    const branch = ok.slice(call, own);
    assert.match(branch, /check\.decision == Extensions::SendDecision::OpenExtensions/);
    assert.match(branch, /CallAfter\(\[id\]\(\) \{ Extensions::Runtime::open_manager\(id\); \}\);\s*EndModal\(wxID_CANCEL\);\s*return;/,
        'Open Extensions closes the print dialog before the manager opens');
    assert.match(branch, /if \(check\.decision == Extensions::SendDecision::Back\) return;/);
    const facts_for_send = ok.slice(guard, call);
    assert.match(facts_for_send, /m_print_type == PrintFromType::FROM_PRINT_QUEUE\) \{/, 'a print queue job is checked too');
    assert.match(facts_for_send, /facts\.source = m_print_type == PrintFromType::FROM_SDCARD_VIEW \? "printer-file" : "queue";/);
    assert.match(facts_for_send, /facts\.file = slash == std::string::npos \? m_required_data_file_name : m_required_data_file_name\.substr\(slash \+ 1\);/,
        'the file name only, never its folder');
    assert.match(facts_for_send, /facts\.to_queue = m_add_to_queue;/);

    const facts = code(body(source, 'static Extensions::SendFacts extension_send_facts('));
    for (const secret of [/dev_id/, /get_dev_id/, /dev_ip/, /access_code/, /get_access_code/, /dev_name/, /get_dev_name/, /serial/i])
        assert.doesNotMatch(facts, secret, `the send facts never read ${secret}`);
    assert.match(facts, /f\.connection\s*= obj && obj->is_lan_mode_printer\(\) \? "lan" : "cloud";/);
    assert.match(facts, /if \(option && option->IsShown\(\)\)/, 'only the options the dialog shows');
}

function checkOtherSends(storageSource, severalSource, dialogSource, runtimeSource) {
    const ok = code(body(storageSource, 'void SendToPrinterDialog::on_ok('));
    const call = ok.indexOf('Extensions::check_before_send(this, facts)');
    const sending = ok.indexOf('show_status(PrintDialogStatus::PrintStatusSending);');
    assert(call >= 0 && sending > call, 'Send to storage asks before anything is sent');
    assert.match(ok.slice(0, call), /facts\.source\s*= "storage";/);
    assert.match(ok, /if \(check\.decision == Extensions::SendDecision::Back\) \{\s*Enable_Send_Button\(true\);\s*return;/, 'Back keeps the dialog usable');
    const send = code(body(severalSource, 'void SendMultiMachinePage::on_send('));
    const asked = send.indexOf('Extensions::check_before_send(this, facts)');
    const prepared = send.indexOf('m_plater->send_gcode(');
    assert(asked >= 0 && prepared > asked, 'several printers are asked about before the file is prepared');
    const block = send.slice(send.indexOf('facts.source     = "several";'), asked);
    assert(block.length > 0);
    for (const secret of [/dev_id/, /get_dev_id/, /dev_ip/, /access_code/, /get_dev_name/, /serial/i])
        assert.doesNotMatch(block, secret, `the several-printer facts never read ${secret}`);
    const context = code(body(runtimeSource, 'json Runtime::before_send_context('));
    assert.match(context, /Plater \*plater = sliced_here\(send\) \? wxGetApp\(\)\.plater\(\) : nullptr;/, 'only a send sliced here carries the slice');
    const start = code(body(dialogSource, 'void SelectMachineDialog::on_send_print('));
    const started = start.indexOf('m_print_job->start();');
    const remembered = start.indexOf('Extensions::Runtime::instance().remember_send(');
    assert(started >= 0 && remembered > started, 'the plan is remembered once the job starts');
    assert.match(start, /if \(m_print_type == PrintFromType::FROM_NORMAL && !m_add_to_queue\)\s*Extensions::Runtime::instance\(\)\.remember_send/);
    const tick = code(body(runtimeSource, 'void Runtime::print_tick('));
    assert.match(tick, /m_sent_plan->printer == now\.machine && clock - m_sent_plan->sent <= std::chrono::minutes\(30\)/, 'the same printer, within 30 minutes');
}

function checkRuntime(source) {
    const run = code(body(source, 'std::shared_ptr<std::atomic<bool>> Runtime::run_before_send('));
    assert.match(run, /job\.hook\s*= Hook::PrintBeforeSend;/);
    assert.match(run, /job\.first\s*= true;/, 'the check goes ahead of jobs waiting');
    const submit = code(body(source, 'void Runtime::submit('));
    assert.match(submit, /if \(job\.first\) m_queue\.push_front\(std::move\(job\)\);/);
    const apply = code(body(source, 'void Runtime::apply('));
    const record = apply.indexOf('m_host->record(result);');
    const each = apply.indexOf('if (each) {');
    const notify = apply.indexOf('push_notification');
    assert(record >= 0 && each > record && notify > each, 'recorded first, then the window, never a notification');
    assert.match(apply.slice(each, notify), /each\(result\);\s*notify_changed\(\);\s*return;/);
    const loop = code(body(source, 'void Runtime::worker_loop('));
    assert.match(loop, /if \(job\.done\) post\(std::move\(job\.done\)\);/, 'the window hears when the job is over');
    const drop = code(body(source, 'Runtime::drop_queue('));
    assert.match(drop, /if \(it->done\) dropped\.push_back\(std::move\(it->done\)\);/, 'a dropped job still says it is done');
    for (const name of ['void Runtime::stop_all(', 'void Runtime::stop(']) {
        const fn = code(body(source, name));
        assert.match(fn, /for \(auto &fn : dropped\) post\(std::move\(fn\)\);/, `${name} reports dropped jobs`);
    }
}

function checkWindow(source, coreSource) {
    const done = code(body(source, 'void on_done('));
    assert.match(done, /const SendSummary summary = summarise_send_check\(runs\);/);
    assert.match(done, /if \(summary\.verdict == SendVerdict::Continue\)/);
    const finish = code(body(source, 'void finish('));
    assert.match(finish, /if \(!m_runs_done\) Runtime::instance\(\)\.stop\(m_cancel\);/, 'leaving early stops the runs');
    const results = code(body(source, 'void show_results('));
    const stop = results.slice(results.indexOf('if (stop)\n            set_buttons'));
    assert(stop.length > 0);
    assert.doesNotMatch(stop.slice(0, stop.indexOf('else')), /SendDecision::Send/, 'a stopped print offers no Send');
    const check = code(body(source, 'SendCheckOutcome check_before_send('));
    assert.match(check, /if \(runs\.empty\(\)\) return SendCheckOutcome\{SendDecision::Send, \{\}\};/, 'no extension on: no window');
    const summary = code(body(coreSource, 'SendSummary summarise_send_check('));
    const answeredOnly = summary.indexOf('if (r.outcome != Outcome::Answered)');
    const blockers = summary.indexOf('s.blockers.push_back(i)');
    assert(answeredOnly >= 0 && blockers > answeredOnly, 'only a valid answer can block');
}

function broken(source, from, to) {
    assert(source.includes(from), `fixture anchor: ${from}`);
    return source.replace(from, to);
}

test('the print dialog asks the extensions before its own check', () => checkPrintDialog(dialog));
test('the runtime runs the check first and reports to its window', () => checkRuntime(runtime));
test('Send to storage and several printers ask too; plans follow the print they sent', () => checkOtherSends(storage, several, dialog, runtime));
test('the window decides with the shared verdict and stops runs it leaves', () => checkWindow(window, core));

test('each checker fails on a broken copy', () => {
    assert.throws(() => checkPrintDialog(broken(dialog, 'if (m_print_type == PrintFromType::FROM_NORMAL || m_print_type == PrintFromType::FROM_SDCARD_VIEW ||',
                                                   'if (true ||')));
    assert.throws(() => checkPrintDialog(broken(dialog, '            facts.file = slash == std::string::npos ? m_required_data_file_name : m_required_data_file_name.substr(slash + 1);',
                                                   '            facts.file = m_required_data_file_name;\n            (void) slash;')));
    assert.throws(() => checkOtherSends(broken(storage, '        if (check.decision == Extensions::SendDecision::Back) {\n            Enable_Send_Button(true);',
                                                       '        if (check.decision == Extensions::SendDecision::Back) {'), several, dialog, runtime));
    assert.throws(() => checkOtherSends(storage, broken(several, '            connections.insert(obj->is_lan_mode_printer() ? "lan" : "cloud");',
                                                        '            connections.insert(obj->is_lan_mode_printer() ? "lan" : "cloud");\n            facts.file += obj->get_dev_id();'), dialog, runtime));
    assert.throws(() => checkOtherSends(storage, several, dialog, broken(runtime, 'Plater *plater = sliced_here(send) ? wxGetApp().plater() : nullptr;',
                                                                         'Plater *plater = wxGetApp().plater();')));
    assert.throws(() => checkOtherSends(storage, several, broken(dialog, 'if (m_print_type == PrintFromType::FROM_NORMAL && !m_add_to_queue)\n        Extensions',
                                                                 'if (m_print_type == PrintFromType::FROM_NORMAL)\n        Extensions'), runtime));
    assert.throws(() => checkPrintDialog(broken(dialog, 'f.connection            = obj && obj->is_lan_mode_printer() ? "lan" : "cloud";',
                                                   'f.connection            = obj && obj->is_lan_mode_printer() ? "lan" : "cloud";\n    f.printer_model += obj->get_dev_id();')));
    assert.throws(() => checkRuntime(broken(runtime, 'job.first   = true;', 'job.first   = false;')));
    assert.throws(() => checkRuntime(broken(runtime, '        each(result);\n        notify_changed();\n        return;', '        each(result);\n        notify_changed();')));
    assert.throws(() => checkRuntime(broken(runtime, '        if (job.done) post(std::move(job.done));\n', '')));
    assert.throws(() => checkWindow(broken(window, '        if (!m_runs_done) Runtime::instance().stop(m_cancel);\n        m_outcome',
                                           '        m_outcome'), core));
    assert.throws(() => checkWindow(window, broken(core, '        if (r.outcome != Outcome::Answered) {', '        if (false) {')));
});
