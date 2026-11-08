// Source checks for the model.imported hook in the running app:
//
// - Plater::priv::load_files is a wrapper: the loading itself is
//   load_files_now, and the hook runs once it has returned, only when objects
//   arrived, and with the object indexes it returned.
// - Nothing else calls load_files_now, so no import skips the hook.
// - The runtime builds the request from those objects only, with the files'
//   names, and never runs when no extension is on for the hook.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const plater = read('src/slic3r/GUI/Plater.cpp');
const runtime = read('src/slic3r/GUI/Extensions/ExtensionRuntime.cpp');

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

function checkPlater(source) {
    const wrapper = code(body(source, 'std::vector<size_t> Plater::priv::load_files(const std::vector<fs::path>& input_files'));
    const load = wrapper.indexOf('std::vector<size_t> loaded = load_files_now(input_files, strategy, ask_multi, successful_3mf_loaded, prepared, context);');
    const hook = wrapper.indexOf('Extensions::Runtime::instance().models_imported(files, loaded);');
    assert(load >= 0 && hook > load, 'the hook runs after the load returns');
    assert.match(wrapper, /if \(!loaded\.empty\(\)\) \{/, 'only when objects arrived');
    assert.match(wrapper, /return loaded;\s*\}$/, 'the load result is returned unchanged');
    const calls = [...code(source).matchAll(/load_files_now\(/g)].length;
    assert.equal(calls, 3, 'load_files_now is declared, defined and called once, by the wrapper');
}

function checkRuntime(source) {
    const imported = code(body(source, 'void Runtime::models_imported('));
    const plan = imported.indexOf('host().plan(Hook::ModelImported)');
    const empty = imported.indexOf('if (runs.empty()) return;');
    assert(plan >= 0 && empty > plan, 'nothing is gathered while no extension is on for the hook');
    assert.match(imported, /for \(std::size_t i : objects\)/, 'only the objects that arrived');
    assert.match(imported, /imported_section\(arrived, plate, files\)/);
}

function broken(source, from, to) {
    assert(source.includes(from), `fixture anchor: ${from}`);
    return source.replace(from, to);
}

test('loading files runs the import hook once the load is done', () => checkPlater(plater));
test('the import hook describes only what arrived', () => checkRuntime(runtime));
test('each checker fails on a broken copy', () => {
    assert.throws(() => checkPlater(broken(plater, '        Extensions::Runtime::instance().models_imported(files, loaded);\n', '')));
    assert.throws(() => checkPlater(broken(plater, 'void Plater::priv::reset_gcode_toolpaths()', 'void Plater::priv::reset_gcode_toolpaths_x() { load_files_now({}, LoadStrategy::LoadModel, false, nullptr, nullptr, {}); }\nvoid Plater::priv::reset_gcode_toolpaths()')));
    assert.throws(() => checkRuntime(broken(runtime, '    if (runs.empty()) return;\n    Plater *plater = wxGetApp().plater();\n    if (!plater) return;\n    const Model                  &model',
                                            '    Plater *plater = wxGetApp().plater();\n    if (!plater) return;\n    const Model                  &model')));
});
