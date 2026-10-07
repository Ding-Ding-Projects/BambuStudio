import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import test from 'node:test';
import vm from 'node:vm';

const read = file => readFileSync(fileURLToPath(new URL('../resources/web/model_new/' + file, import.meta.url)), 'utf8').replace(/\r\n/g, '\n');
const script = read('js/gallery.js');
const css = read('css/gallery.css');
const start = script.indexOf('function setActiveThumb(){');
assert.ok(start >= 0);
const reveal = script.slice(start, script.indexOf('\n    }', start) + 6);

test('the actual thumbnail reveal handler scrolls vertical bounds into view', () => {
    assert.match(reveal, /scrollTop\(/);
    assert.doesNotMatch(reveal, /scrollLeft\(/);
    for (const [relativeTop, expected] of [[200, 132], [-20, 20]]) {
        let scroll = 40;
        const item = {removeClass() {return this}, addClass() {return this}, attr() {return this}, eq() {return this}, position() {return {top: relativeTop}}, outerHeight() {return 72}};
        const ui = {$thumbs: {children() {return item}, height() {return 180}, scrollTop(value) {if (value !== undefined) scroll = value; return scroll}}};
        vm.runInNewContext(reveal + '; setActiveThumb();', {ui, index: 4}, {timeout: 100});
        assert.equal(scroll, expected);
    }
});

test('thumbnail layout keeps the vertical axis consumed by the reveal handler at every breakpoint', () => {
    const blocks = [...css.matchAll(/(^|})\s*\.bs-gallery-thumbs\s*\{([^}]+)}/gm)].map(match => match[2]);
    assert.ok(blocks.length >= 2, 'inspect base and responsive thumbnail declarations');
    // A row flex layout can overflow sideways while scrollTop still changes successfully.
    // Check every declaration block, including those nested in responsive media rules.
    for (const block of blocks) {
        assert.doesNotMatch(block, /overflow-x\s*:\s*(?:auto|scroll)/, 'a vertical-only reveal handler cannot expose horizontal overflow');
        if (/display\s*:\s*(?:inline-)?flex/.test(block))
            assert.match(block, /flex-direction\s*:\s*column\s*;/, 'flex thumbnails must remain a column');
        assert.doesNotMatch(block, /(?:grid-auto-flow\s*:\s*column|flex-direction\s*:\s*row)/);
    }
    assert.match(css, /\.bs-gallery-thumbs\s*\{[^}]*overflow-y:\s*auto/);
    assert.match(script, /\$thumbs\.css\(\{ maxHeight: settings\.mainHeight/);
});
