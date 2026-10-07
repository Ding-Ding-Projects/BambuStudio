import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arg = name => { const i = process.argv.indexOf(name); return i < 0 ? null : process.argv[i + 1]; };
const revision = arg('--source-revision');
const file = 'src/slic3r/GUI/Widgets/ListBox.cpp';
const source = (revision ? execFileSync('git', ['show', revision + ':' + file], {cwd: root, encoding: 'utf8'})
    : fs.readFileSync(path.join(root, file), 'utf8')).replaceAll('\r\n', '\n');
function body(name) {
    const start = source.indexOf(name), begin = source.indexOf('{', start);
    assert(start >= 0, name);
    let depth = 0;
    for (let i = begin; i < source.length; ++i) {
        if (source[i] === '{') ++depth;
        if (source[i] === '}' && --depth === 0) return source.slice(start, i + 1);
    }
    assert.fail(name);
}
const output = arg('--extract');
assert(output, 'Pass --extract with a temporary output directory');
fs.mkdirSync(output, {recursive: true});
const transitions = ['void ListBox::onMotion(', 'void ListBox::onLeave(', 'void ListBox::animateHover('].map(body).join('\n');
const paint = body('void ListBox::OnDrawBackground(');
const progress = paint.match(/const double progress = [^;]+;/)?.[0];
const hover = paint.match(/const double hover = [^;]+;/)?.[0];
assert(progress && hover);
fs.writeFileSync(path.join(output, 'atlas_listbox_hover.inc'), transitions + '\ndouble ListBox::hoverPaint(size_t n) const {\n'
    + progress + '\n' + hover + '\nreturn hover;\n}\n');
console.log('Extracted production motion/leave/transition bodies and paint expression' + (revision ? ' from ' + revision : ''));
