import {readFileSync, writeFileSync, mkdtempSync, mkdirSync, cpSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
import {compileFixture} from './native_fixture_compiler.mjs';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output=mkdtempSync(path.join(tmpdir(), 'bambu-preferences-geometry-'));
const read=p=>readFileSync(path.join(root,p),'utf8');
const gui='src/slic3r/GUI/';
let source=read(gui+'PreferencesGeometryProbe.cpp');
if(process.argv.includes('--privacy-mutant'))
  source=source.replace('const auto bounds = window->GetScreenRect();','window->GetChildren(); const auto bounds = window->GetScreenRect();');
if(process.argv.includes('--geometry-mutant'))
  source=source.replace('parent_client.Contains(bounds)', 'true');
writeFileSync(path.join(output,'PreferencesGeometryProbe.cpp'),source);
writeFileSync(path.join(output,'PreferencesGeometryProbe.hpp'),read(gui+'PreferencesGeometryProbe.hpp'));
writeFileSync(path.join(output,'fixture.hpp'),read('tests/preferences_geometry_probe_fixture.hpp'));
mkdirSync(path.join(output,'wx'));
for(const name of ['simplebook.h','thread.h','weakref.h'])
  writeFileSync(path.join(output,'wx',name),'#include "../fixture.hpp"\n');
cpSync(path.join(root,'src/nlohmann'),path.join(output,'nlohmann'),{recursive:true});

// Integration assertions supplement the executable production-module checks.
const bridge=read(gui+'AutomationBridge.cpp');
const permission=bridge.indexOf('if (local_capabilities_only) throw');
const operation=bridge.indexOf('if (op == "preferences_geometry")');
assert(permission>=0 && operation>permission);
const end=bridge.indexOf('\n        }',operation)+10;
assert(end>operation);
const dispatch=bridge.slice(permission,end);
writeFileSync(path.join(output,'fixture.cpp'),read('tests/preferences_geometry_probe_fixture.cpp').replace('// PRODUCTION_DISPATCH',dispatch));
assert.match(read(gui+'Preferences.cpp'),/PreferencesGeometryProbe::bind\(this, content_card, m_book, m_search, m_tabbar\)/);
const exe=path.join(output,'fixture.exe');
const compile=compileFixture({source:path.join(output,'fixture.cpp'),exe,cwd:output,include:output});
if(compile.error) throw compile.error;
if(compile.status!==0) process.exit(compile.status??1);
const run=spawnSync(exe,[],{cwd:output,stdio:'inherit'});
if(run.error) throw run.error;
process.exit(run.status??1);
