import {readFileSync,mkdtempSync,writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync,execFileSync} from 'node:child_process';
import assert from 'node:assert/strict';
import {compileFixture} from './native_fixture_compiler.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const baseline=process.argv.find(arg=>arg.startsWith('--baseline='))?.slice(11);
const cpp=baseline ? execFileSync('git',['show',baseline+':src/slic3r/GUI/Preferences.cpp'],{cwd:root,encoding:'utf8',maxBuffer:4*1024*1024}) : readFileSync(path.join(root,'src/slic3r/GUI/Preferences.cpp'),'utf8');
function contract(source) {
 assert.match(source,/: DPIDialog\(parent, id, _L\("Preferences"\), pos, size, style \| wxRESIZE_BORDER\)/);
 const create=source.slice(source.indexOf('void PreferencesDialog::create()'),source.indexOf('// Preferences geometry'));
 const dpi=source.slice(source.indexOf('void PreferencesDialog::on_dpi_changed('),source.indexOf('void PreferencesDialog::Split('));
 assert.doesNotMatch(create,/\bFit\(\)|SetSizeHints/);
 assert.doesNotMatch(dpi,/\bFit\(\)|SetSize\(FromDIP\(780\)/);
 assert.match(dpi,/constrain_geometry\(suggested_rect.IsEmpty\(\) \? GetRect\(\) : suggested_rect\)/);
 assert.match(source,/wxDisplay\(display_index\)\.GetClientArea\(\)/);
 assert.match(source,/PreferencesDialog::~PreferencesDialog\(\)\s*\{\s*try \{ save_geometry\(\); \}/);
 assert.match(source,/const wxSize size = ToDIP\(GetSize\(\)\)/);
}
contract(cpp);
for(const [from,to] of [['style | wxRESIZE_BORDER','style'],['constrain_geometry(suggested_rect.IsEmpty() ? GetRect() : suggested_rect);','Fit();'],['save_geometry();','']])
 assert.throws(()=>contract(cpp.replace(from,to)),{name:'AssertionError'});
console.log('PASS production wiring and 3 negative regressions');
const out=mkdtempSync(path.join(tmpdir(),'preferences-geometry-'));
const source=path.join(out,'fixture.cpp'),exe=path.join(out,'fixture.exe');
writeFileSync(source,readFileSync(path.join(root,'tests/native_preferences_geometry_fixture.cpp')));
const compile=compileFixture({source,exe,cwd:out,include:root});
if(compile.error) throw compile.error;
if(compile.status!==0) process.exit(compile.status??1);
const run=spawnSync(exe,[],{cwd:out,stdio:'inherit'});
if(run.error) throw run.error;
process.exit(run.status??1);
