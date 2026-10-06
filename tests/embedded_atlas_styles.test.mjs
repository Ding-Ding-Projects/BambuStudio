import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
const baseline = '795788e7c4790250bcd26f58e4700b78d4a66b7c';
const files = [
  "resources/web/device/css/dark.css",
  "resources/web/device/css/home.css",
  "resources/web/fila_manager/index.css",
  "resources/web/filament_create/dark.css",
  "resources/web/filament_create/edit_filament.css",
  "resources/web/filament_create/step1.css",
  "resources/web/filament_create/step2_copy.css",
  "resources/web/filament_create/step2_type.css",
  "resources/web/filament_create/step3.css",
  "resources/web/filament_create/style.css",
  "resources/web/guide/6/6.css",
  "resources/web/guide/css/common.css",
  "resources/web/guide/css/dark.css",
  "resources/web/homepage3/css/common.css",
  "resources/web/homepage3/css/dark.css",
  "resources/web/login/css/login.css",
  "resources/web/model/css/dark.css",
  "resources/web/model/model.css",
  "resources/web/model_new/css/black.css",
  "resources/web/model_new/css/dark.css",
  "resources/web/model_new/css/editor.css",
  "resources/web/model_new/index.css",
  "src/slic3r/GUI/DeviceWeb/device_page/src/styles.css"
];
const read = p => readFileSync(p, 'utf8');
function structure(css) {
  return css.split('/* Studio Atlas:')[0]
    .replace(/:root\s*\{\s*--atlas-focus:[^}]*\}/g, '')
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/#[\da-f]{3,8}\b/gi, '#COLOR')
    .replace(/(?<![-\w])color\s*:[^;}]+;?/g, '')
    .replace(/\s+/g, '');
}
test('visual refresh retains every original selector and non-color declaration', () => {
  for (const p of files) {
    const old = execFileSync('git', ['show', baseline + ':' + p], {encoding:'utf8',maxBuffer:4*1024*1024});
    assert.equal(structure(read(p)), structure(old), p);
  }
});
test('preservation check rejects changed geometry', () => {
  assert.notEqual(structure('.card { height: 40px; color: #ffffff; }'), structure('.card { height: 20px; color: #e8eff8; }'));
});
test('accent and status variables retain their exact values', () => {
  const values = css => [...css.matchAll(/(--(?:md-(?:primary|on-primary|secondary|on-secondary|error|on-error|inverse|accent|warning|info)|color-fm-(?:brand|selected|danger|warning)|color-brand)[\w-]*)\s*:\s*([^;]+);/g)].map(m => [m[1], m[2].trim()]);
  for (const p of files) assert.deepEqual(values(read(p)), values(execFileSync('git',['show',baseline+':'+p],{encoding:'utf8'})),p);
});
const interactionMarker = '/* Studio Atlas:';
const interactionEntries = new Map([
  ['resources/web/device/css/home.css', ['body', 'var(--md-on-surface)']],
  ['resources/web/fila_manager/index.css', ['body', 'var(--md-primary)']],
  ['resources/web/filament_create/style.css', ['body', 'var(--color-brand)']],
  ['resources/web/guide/css/common.css', ['body', 'var(--md-primary)']],
  ['resources/web/homepage3/css/common.css', ['body', 'var(--md-primary)']],
  ['resources/web/login/css/login.css', ['body', 'var(--atlas-focus, #146c2e)']],
  ['resources/web/model/model.css', ['body', 'var(--atlas-focus, #146c2e)']],
  ['resources/web/model_new/index.css', ['body', 'var(--md-primary)']],
  ['src/slic3r/GUI/DeviceWeb/device_page/src/styles.css', ['#root', 'var(--color-fm-brand)']],
]);
// Collapse formatting whitespace only. Do not erase selector combinators,
// comments, punctuation or unknown trailing source to make a comparison pass.
const normalizeInteraction = css => css.replace(/\s+/g, ' ').trim();
function expectedInteraction([scope, focus]) {
  const controls = scope + ' :is(button, input, select, textarea, a[href], [role="button"], [tabindex])';
  return interactionMarker + ' paint-only interaction states. Existing geometry and actions remain owned by the view. */' +
    '\n' + controls + ':focus-visible {\n  outline: 2px solid ' + focus + ';\n  outline-offset: 2px;\n}\n' +
    '@media (prefers-reduced-motion: no-preference) {\n  ' + scope + ' :is(button, input, select, textarea, [role="button"]) {\n' +
    '    transition: background-color 100ms ease-out, border-color 100ms ease-out;\n  }\n}\n' +
    '@media (prefers-reduced-motion: reduce) {\n  ' + controls + ' {\n    transition: none;\n  }\n}';
}
function validateInteraction(path, css) {
  const count = css.split(interactionMarker).length - 1;
  const entry = interactionEntries.get(path);
  assert.equal(count, entry ? 1 : 0, path + ': exact interaction marker count');
  if (!entry) return;
  const suffix = css.slice(css.indexOf(interactionMarker));
  assert.equal(normalizeInteraction(suffix), normalizeInteraction(expectedInteraction(entry)), path + ': complete interaction suffix');
}
test('exactly nine declared entrypoints carry the complete interaction rules', () => {
  assert.equal(interactionEntries.size, 9);
  assert.equal(files.filter(p => !interactionEntries.has(p)).length, 14);
  for (const p of interactionEntries.keys()) assert.ok(files.includes(p), p);
  for (const p of files) validateInteraction(p, read(p));
});
test('removing each actual required interaction block is rejected', () => {
  for (const p of interactionEntries.keys()) {
    const css = read(p);
    assert.throws(() => validateInteraction(p, css.slice(0, css.indexOf(interactionMarker))), {name:'AssertionError'}, p);
  }
});
test('duplicate markers and geometry after a second marker are rejected', () => {
  for (const p of interactionEntries.keys()) {
    const css = read(p);
    for (const extra of [interactionMarker + ' duplicate */', interactionMarker + ' duplicate */\nbody { height : 1px; }'])
      assert.throws(() => validateInteraction(p, css + '\n' + extra), {name:'AssertionError'}, p);
  }
});
test('unknown suffix declarations, selectors, at-rules and trailing text are rejected', () => {
  for (const p of interactionEntries.keys()) {
    const original = read(p);
    const offset = original.indexOf(interactionMarker);
    const prefix = original.slice(0, offset);
    const css = original.slice(offset);
    const mutations = [
      css + '\nbody { padding: 1px; }',
      css.replace('outline-offset: 2px;', 'outline-offset: 2px;\n  height \t : 1px;'),
      css.replace('outline-offset: 2px;', 'outline-offset: 2px; padding: 1px;'),
      css.replace(':focus-visible', ':hover'),
      css + '\n@media print { body { padding: 1px; } }',
      css + '\ntrailing-text',
      css.replace('100ms ease-out', '250ms ease-out'),
    ];
    for (const mutation of mutations) assert.throws(() => validateInteraction(p, prefix + mutation), {name:'AssertionError'}, p);
  }
});
test('interaction blocks are rejected on each undeclared sheet', () => {
  const extra = expectedInteraction(['body', 'var(--md-primary)']);
  for (const p of files.filter(p => !interactionEntries.has(p)))
    assert.throws(() => validateInteraction(p, read(p) + '\n' + extra), {name:'AssertionError'}, p);
});
function luminance(hex) { const c=hex.match(/../g).map(x=>parseInt(x,16)/255).map(v=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4);return c[0]*.2126+c[1]*.7152+c[2]*.0722; }
function contrast(a,b){const x=luminance(a),y=luminance(b);return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);}
test('default body text and field outlines meet static contrast thresholds', () => {
  for(const background of ['f7f9fc','eef2f7','e7edf5','dfe7f1','d7e1ed']) {assert.ok(contrast('172434',background)>=4.5);assert.ok(contrast('46576a',background)>=4.5);assert.ok(contrast('6d7e94',background)>=3);}
  for(const background of ['151c25','1a2430','202d3b','293849','344557']) {assert.ok(contrast('e8eff8',background)>=4.5);assert.ok(contrast('b9c8da',background)>=4.5);assert.ok(contrast('899caf',background)>=3);}
});
