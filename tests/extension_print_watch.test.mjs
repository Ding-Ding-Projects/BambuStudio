// Source checks for the print.finished hook in the running app:
//
// - The app starts the watcher after startup and the runtime stops its timer
//   at shutdown.
// - A tick reads no extension before anyone has answered a consent prompt
//   (the built-ins are installed on first use, not at startup), looks at
//   nothing while no extension is on for the hook, and forgets what it saw,
//   so turning one on later starts from what the printer does then.
// - Endings come from the narrator's tested transition detector, so a print
//   that was already over when watching began is never reported, and one end
//   is reported once.
// - The request holds how the print ended and the printer's model; the
//   printer's identity is only compared, never sent.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const runtime = read('src/slic3r/GUI/Extensions/ExtensionRuntime.cpp');
const app = read('src/slic3r/GUI/GUI_App.cpp');

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

function checkLifetime(appSource, runtimeSource) {
    assert.match(appSource, /TtsNarrator::install\(\);[\s\S]{0,400}Extensions::Runtime::instance\(\)\.start_watching_prints\(\);/,
        'the watcher starts with the other printer watchers after startup');
    const shutdown = code(body(runtimeSource, 'void Runtime::shutdown('));
    assert.match(shutdown, /print_watch_timer\(\)->Stop\(\);/, 'the timer stops at shutdown');
}

function checkTick(source) {
    const tick = code(body(source, 'void Runtime::print_tick('));
    const consent = tick.indexOf('"consent.json"');
    const plan = tick.indexOf('if (host().plan(Hook::PrintFinished).empty()) {');
    assert(consent >= 0 && plan > consent, 'extensions are not read before anyone answered a consent prompt');
    const forget = tick.indexOf('m_print_watch.reset();');
    const printer = tick.indexOf('get_selected_machine()');
    assert(plan >= 0 && forget > plan && printer > forget, 'no printer is looked at while no extension is on');
    assert.match(tick, /w\.detector\.observe\(now\)/, 'endings come from the tested transition detector');
    assert.match(tick, /if \(ended\.empty\(\) \|\| !w\.running\) return;\s*w\.running = false;/, 'one end is reported once');
    const context = tick.slice(tick.indexOf('PrintEndFacts facts;'));
    assert.doesNotMatch(context, /dev_id|get_dev_name|dev_ip|access_code|serial/i, 'the printer identity is never sent');
    assert.match(context, /job\.context = json\{\{"print", print_section\(facts\)\}, \{"printer", printer_section\(printer_facts\(\)\)\}\};/);
}

function broken(source, from, to) {
    assert(source.includes(from), `fixture anchor: ${from}`);
    return source.replace(from, to);
}

test('the watcher starts after startup and stops at shutdown', () => checkLifetime(app, runtime));
test('a tick looks only while an extension is on and reports one end once', () => checkTick(runtime));
test('each checker fails on a broken copy', () => {
    assert.throws(() => checkLifetime(broken(app, '    Extensions::Runtime::instance().start_watching_prints();\n', ''), runtime));
    assert.throws(() => checkLifetime(app, broken(runtime, '    print_watch_timer()->Stop();\n', '')));
    assert.throws(() => checkTick(broken(runtime, '        m_print_watch.reset();\n        return;\n', '')));
    assert.throws(() => checkTick(broken(runtime, '/ "extension-state" / "consent.json", ec)) return;', '/ "extension-state" / "nothing", ec)) {}')));
    assert.throws(() => checkTick(broken(runtime, '    w.running = false;\n\n    std::vector<PlannedRun> runs', '\n    std::vector<PlannedRun> runs')));
    assert.throws(() => checkTick(broken(runtime, '    facts.result          = ended;\n', '    facts.result          = ended;\n    facts.printer_model += obj->get_dev_id();\n')));
});
