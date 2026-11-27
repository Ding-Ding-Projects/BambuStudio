import {readFileSync,writeFileSync,mkdtempSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const source=readFileSync(path.join(root,'src/slic3r/GUI/Plater.cpp'),'utf8');
const start=source.indexOf('                    if (context.automatic()) {',source.indexOf('model = archive_future.get();'));
const end=source.indexOf('                    if (is_user_cancel)',start);
assert(start>0&&end>start);
let admission=source.slice(start,end);
if(process.argv.includes('--admission-mutant'))admission=admission.replace('if (!safe)', 'if (false)');
if(process.argv.includes('--classification-mutant'))admission=admission.replace('is_project_file = true;', 'is_project_file = false;');
const take=pattern=>{const match=source.match(pattern);assert(match,'Missing downstream production statement');return match[0];};
const manualStart=source.indexOf('if (load_config && !context.automatic())',end);
assert(manualStart>end);
const manualClassification=source.slice(manualStart).match(/is_project_file = true;/);
assert(manualClassification,'Missing interactive project classification');
const downstream=[
  take(/if \(\(!is_project_file\) && \(!load_old_project\)\)/)+' { ++import_heuristics; '+take(/deleted_objects = model\.removed_objects_with_zero_volume\(\);/)+' if(deleted_objects) ++modal_notices; }',
  'for(auto* model_object:model.objects) '+take(/model_object->ensure_on_bed\(is_project_file\);/),
  take(/auto loaded_idxs = load_model_objects\(model\.objects, is_project_file, false,\s+load_progress_callback, prepared \? &model : nullptr\);/),
  '(void)loaded_idxs;'
].join('\n');
// Compile the actual archive routing. Crash recovery reads split components
// from a backup directory; both history recovery paths require embedded parts.
const loadProject = source.slice(source.indexOf('int Plater::load_project('));
const strategyStart = loadProject.indexOf('    auto strategy = LoadStrategy::LoadModel | LoadStrategy::LoadConfig;');
const strategyEnd = loadProject.indexOf('    // A non-Bambu 3MF', strategyStart);
assert(strategyStart >= 0 && strategyEnd > strategyStart, 'Missing project archive routing');
const archiveRouting = loadProject.slice(strategyStart, strategyEnd);
const restore = source.slice(source.indexOf('bool Plater::restore_project_history_snapshot('));
assert.match(restore, /history_context\.standalone_history_snapshot\s*=\s*true/);
assert.match(restore, /load_project\(restored_path, original_saved_project, &selected_load_succeeded, true, history_context\)/);
assert.match(restore, /load_project\(from_path\(boost::filesystem::path\(rollback_path.native\(\)\)\), original_saved_project,\s*&rollback_load_succeeded, true, history_context\)/);
const fixture=readFileSync(path.join(root,'tests/startup_nonmodal_fixture.cpp'),'utf8')
  .replace('// ACTUAL_ARCHIVE_ROUTING',archiveRouting)
  .replace('// ACTUAL_DETACHED_ADMISSION',admission)
  .replace('// ACTUAL_MANUAL_CLASSIFICATION',take(/if \(load_config && !context\.automatic\(\)\)/)+' { '+manualClassification[0]+' }')
  .replace('// ACTUAL_DOWNSTREAM',downstream)
  .replace('// ACTUAL_OBJECT_PLACEMENT',take(/object->ensure_on_bed\(allow_negative_z\);/));
const output=mkdtempSync(path.join(tmpdir(),'bambu-startup-nonmodal-'));
const file=path.join(output,'fixture.cpp'),exe=path.join(output,'fixture.exe');writeFileSync(file,fixture);
const compile=spawnSync(process.env.CXX||'g++',['-std=c++17','-Wall','-Wextra','-Werror','-I'+path.join(root,'src'),file,'-o',exe],{stdio:'inherit'});
if(compile.error)throw compile.error;
if(compile.status!==0)process.exit(compile.status??1);
const run=spawnSync(exe,[],{stdio:'inherit'});if(run.error)throw run.error;process.exit(run.status??1);
