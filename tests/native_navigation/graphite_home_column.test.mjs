import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

// Graphite shell Home column: the LAN model drop, the printers and the melody
// creator beside the recent projects panel.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const html = read('resources/web/homepage3/home.html');
const script = read('resources/web/homepage3/js/home.js');
const css = read('resources/web/homepage3/css/home.css');
const app = read('src/slic3r/GUI/GUI_App.cpp');
const web = read('src/slic3r/GUI/WebViewDialog.cpp');

const commands = ['homepage_side_status_get', 'homepage_lan_invite', 'homepage_lan_settings', 'homepage_open_device', 'homepage_melody'];

test('the column sits beside the recent projects panel', () => {
    const row = html.indexOf('<div id="HomeTopRow">');
    const recent = html.indexOf('<div id="MiniRecentFileArea">');
    const side = html.indexOf('<div id="HomeSideColumn"');
    const online = html.indexOf('<div id="HotModelArea">');
    assert(row !== -1 && row < recent && recent < side && side < online);
    for (const id of ['HomeLanCard', 'HomePrinterCard', 'HomeMelodyCard'])
        assert(html.includes(`id="${id}"`), id);
    assert.match(css, /#HomeTopRow\s*\{[^}]*grid-template-columns: minmax\(0, 2fr\) minmax\(260px, 1fr\);/);
});

test('every command the page sends has a native handler', () => {
    for (const command of commands) {
        assert(script.includes(`"${command}"`), `page sends ${command}`);
        assert(app.includes(`command_str.compare("${command}") == 0`), `native handles ${command}`);
    }
    assert(web.includes('req.put("command", "homepage_side_status");'));
    assert(script.includes("strCmd=='homepage_side_status'"));
});

test('printer names and states are inserted as text', () => {
    const show = script.slice(script.indexOf('function ShowHomeSideStatus'));
    assert(show.includes("name.textContent=String(p['name'] || '');"));
    assert(show.includes("st.textContent=String(p['state'] || '');"));
    assert.doesNotMatch(show, /innerHTML|\.html\(/);
});

test('the column strings exist in English and Hong Kong Cantonese', () => {
    const context = {};
    vm.createContext(context);
    vm.runInContext(`${read('resources/web/data/text.js')};this.L=LangText;`, context);
    for (let n = 314; n <= 325; n++) {
        const key = `t${n}`;
        assert(context.L.en[key], `en ${key}`);
        assert(context.L.yue_HK[key] && context.L.yue_HK[key] !== context.L.en[key], `yue_HK ${key}`);
    }
});
