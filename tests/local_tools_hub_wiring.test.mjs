// Source checks for the Local tools page in the running app:
//
// - The page is the first page of the Local tools tab, whichever tool opened
//   the tab, and File > Local tools opens it.
// - The tools write Recent activity when they finish something: Base64 (the
//   name, never the payload), Virtual disks, Minecraft servers, Extensions
//   (real runs only, never a sample), the Drawing board export and the end of
//   setup.
// - Nothing is written while Keep recent activity is off.
// - Clear activity deletes nothing before the two-key confirmation.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const frame = read('src/slic3r/GUI/MainFrame.cpp');
const activity = read('src/slic3r/GUI/FeatureFinder/LocalToolsActivity.cpp');
const panel = read('src/slic3r/GUI/FeatureFinder/LocalToolsHubPanel.cpp');
const tools = {
    base64: read('src/slic3r/GUI/Base64Toolkit/Base64ToolkitPanel.cpp'),
    disks: read('src/slic3r/GUI/VirtualDisks/VirtualDisksPanel.cpp'),
    servers: read('src/slic3r/GUI/GameServers/ServersPanel.cpp'),
    extensions: read('src/slic3r/GUI/Extensions/ExtensionRuntime.cpp'),
    drawing: read('src/slic3r/GUI/Drawing/DrawingPanel.cpp'),
    setup: read('src/slic3r/GUI/SetupWizard/SetupWizardDialog.cpp'),
};

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

function checkFrame(source) {
    const open = code(body(source, 'wxPanel* MainFrame::open_service('));
    const created = open.indexOf('m_tabpanel->AddPage(m_service_workspace');
    const hub = open.indexOf('m_service_workspace->open("local-tools"');
    const requested = open.indexOf('m_service_workspace->open(id, title, create)');
    assert(created >= 0 && hub > created && requested > hub, 'the page opens first, then the tool that was asked for');
    assert.match(source, /feature_command\("local-tools", \[\]\(\) \{ LocalToolsHubPanel::open\(\); \}\)/, 'File > Local tools opens the page');
}

function checkTools(t) {
    const append = code(body(t.base64, 'void Base64ToolkitPanel::append_history('));
    assert.match(append, /LocalTools::record_activity\("base64", said, entry\.name\)/, 'Base64 records the name');
    assert.doesNotMatch(append, /record_activity\([^;]*payload/, 'never the payload');
    assert(append.indexOf('record_activity(') < append.indexOf('m_history.append(std::move(entry)'), 'recorded before the entry is moved');

    const run = code(body(t.disks, 'void VirtualDisksPanel::run('));
    assert(run.indexOf('m_library.record(h);') < run.indexOf('LocalTools::record_activity("virtual-disks"'), 'a disk operation is recorded when it ends');

    const record = code(body(t.servers, 'void record(const std::string &server'));
    assert.match(record, /if \(const char \*words = activity_words\(action\)\) LocalTools::record_activity\("minecraft", words, server, outcome\);/);

    const apply = code(body(t.extensions, 'void Runtime::apply('));
    const guard = apply.indexOf('if (!sample && result.outcome != Outcome::Stopped) {');
    const recorded = apply.indexOf('LocalTools::record_activity("extensions"');
    assert(guard >= 0 && recorded > guard, 'a sample run is never recorded');

    const exported = code(body(t.drawing, 'void DrawingPanel::export_drawing('));
    assert.match(exported, /LocalTools::record_activity\("drawing", L\("Exported"\)/);

    const save = code(body(t.setup, 'void SetupWizardDialog::save('));
    assert.match(save, /if \(m_progress\.finished && !was_finished\) LocalTools::record_activity\("onboarding"/, 'the end of setup is recorded once');
}

function checkActivity(source) {
    const record = code(body(source, 'void record_activity('));
    const off = record.indexOf('if (!activity_on()) return;');
    const append = record.indexOf('log().append(');
    assert(off >= 0 && append > off, 'nothing is written while Keep recent activity is off');
}

function checkClear(source) {
    const history = code(body(source, 'void LocalToolsHubPanel::show_history('));
    const gate = history.indexOf('if (!SuperConfirmGate::Run(clear, spec)) return;');
    const clear = history.indexOf('LocalTools::clear_activity();');
    assert(gate >= 0 && clear > gate, 'Clear activity waits for both keys and the slide');
}

function broken(source, from, to) {
    assert(source.includes(from), `fixture anchor: ${from}`);
    return source.replace(from, to);
}

test('the page is the first page of the tab and File > Local tools opens it', () => checkFrame(frame));
test('the tools write what they finished', () => checkTools(tools));
test('nothing is written while recording is off', () => checkActivity(activity));
test('Clear activity waits for the two-key confirmation', () => checkClear(panel));
test('each checker fails on a broken copy', () => {
    assert.throws(() => checkFrame(broken(frame, 'm_service_workspace->open("local-tools"', 'm_service_workspace->open("other"')));
    assert.throws(() => checkTools({...tools, extensions: broken(tools.extensions, 'if (!sample && result.outcome != Outcome::Stopped) {', 'if (result.outcome != Outcome::Stopped) {')}));
    assert.throws(() => checkTools({...tools, base64: broken(tools.base64, 'LocalTools::record_activity("base64", said, entry.name)', 'LocalTools::record_activity("base64", said, entry.payload)')}));
    assert.throws(() => checkTools({...tools, setup: broken(tools.setup, 'if (m_progress.finished && !was_finished) ', 'if (m_progress.finished) ')}));
    assert.throws(() => checkActivity(broken(activity, '    if (!activity_on()) return;\n', '')));
    assert.throws(() => checkClear(broken(panel, '        if (!SuperConfirmGate::Run(clear, spec)) return;\n', '')));
});
