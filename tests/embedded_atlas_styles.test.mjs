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
test('new interactions use only paint transitions and honor reduced motion', () => {
  for (const p of files) {
    const added = read(p).split('/* Studio Atlas:')[1];
    if (!added) continue;
    assert.match(added, /:focus-visible/);
    assert.match(added, /prefers-reduced-motion: reduce/);
    assert.match(added, /transition: none/);
    assert.doesNotMatch(added, /!important|transition: all|transform:|opacity:|display:|height:|width:/);
  }
});
function luminance(hex) { const c=hex.match(/../g).map(x=>parseInt(x,16)/255).map(v=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4);return c[0]*.2126+c[1]*.7152+c[2]*.0722; }
function contrast(a,b){const x=luminance(a),y=luminance(b);return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);}
test('default body text and field outlines meet static contrast thresholds', () => {
  for(const background of ['f7f9fc','eef2f7','e7edf5','dfe7f1','d7e1ed']) {assert.ok(contrast('172434',background)>=4.5);assert.ok(contrast('46576a',background)>=4.5);assert.ok(contrast('6d7e94',background)>=3);}
  for(const background of ['151c25','1a2430','202d3b','293849','344557']) {assert.ok(contrast('e8eff8',background)>=4.5);assert.ok(contrast('b9c8da',background)>=4.5);assert.ok(contrast('899caf',background)>=3);}
});
