// A menu row with a live control must never read its value while the menu is
// being built: the 3D view's menus are built inside the Plater's constructor,
// before wxGetApp().plater() is set, and a switch that asked the Plater for
// its value there crashed the app on every start (md3-v285 to md3-v291).
//
// - append_live (MD3Menu.cpp) reads a switch's value only when its menu opens.
// - Every switch getter in the menus built at startup (GUI_Factories.cpp)
//   copes with no Plater: it never calls plater()-> without a guard.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const menu = read('src/slic3r/GUI/Widgets/MD3Menu.cpp');
const factories = read('src/slic3r/GUI/GUI_Factories.cpp');

// The text from `open` to its matching close, skipping strings and comments.
function balanced(source, from, open = '(', close = ')') {
    const start = source.indexOf(open, from);
    const masked = source.slice(start).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, (t) => ' '.repeat(t.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === open) ++depth;
        if (masked[i] === close && --depth === 0) return source.slice(start, start + i + 1);
    }
    assert.fail('unbalanced');
}

function checkAppendLive(source) {
    const at = source.indexOf('wxMenuItem *append_live(wxMenu *menu');
    assert(at >= 0, 'append_live is where it was');
    const body = balanced(source, at, '{', '}').replace(/\/\/[^\n]*/g, '');
    const reads = [...body.matchAll(/control\(\)\.value\(\)/g)].map((m) => m.index);
    const open = body.indexOf('wxEVT_MENU_OPEN');
    assert(open >= 0, 'a native menu takes the check mark when it opens');
    for (const index of reads) assert(index > open, 'the value is read only inside the menu-open handler');
}

// Every AppendSwitch getter (its first lambda) in the menus built at startup.
function switchGetters(source) {
    const getters = [];
    for (const m of source.matchAll(/MD3::AppendSwitch\(/g)) {
        const args = balanced(source, m.index + 'MD3::AppendSwitch'.length);
        const lambda = args.search(/\[[^\]]*\]\s*\(\s*\)\s*(?:->\s*\w+\s*)?\{/);
        if (lambda < 0) continue; // a named getter: checked where it is defined
        getters.push(balanced(args, lambda, '{', '}'));
    }
    return getters;
}

function checkGetters(source) {
    const getters = switchGetters(source);
    assert(getters.length >= 4, 'the startup menus still have their switches');
    for (const getter of getters) {
        if (!getter.includes('plater()->')) continue;
        assert.match(getter, /!plater\(\)|plater\(\) \?|plater\(\) &&/, `a switch getter reads the Plater without a guard: ${getter.slice(0, 80)}`);
    }
    // The helper the label switch uses is guarded too.
    assert.match(source, /PartPlate \*plate = plater\(\) \? plater\(\)->get_partplate_list\(\)\.get_curr_plate\(\) : nullptr;/);
}

test('a live row never reads its value while its menu is built', () => checkAppendLive(menu));
test('switch getters in the startup menus cope with no Plater', () => checkGetters(factories));

test('each checker fails on a broken copy', () => {
    const eager = menu.replace("    menu->Append(item);\n", "    menu->Append(item);\n    if (item->IsCheckable()) item->Check(item->control().value() >= 0.5);\n");
    assert.throws(() => checkAppendLive(eager));
    assert.throws(() => checkGetters(factories.replace('return plater() && plater()->is_view3D_overhang_shown();', 'return plater()->is_view3D_overhang_shown();')));
});
