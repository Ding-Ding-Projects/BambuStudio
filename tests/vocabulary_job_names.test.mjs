// Source contract for "Use my wording in job names sent to printers".
// The personal vocabulary may reach a printer only through this switch: the
// default is declared off, Send print transforms the job name only behind the
// switch and cleans it like a rename afterwards, the transform itself refuses
// School mode and an unloaded vocabulary, and School mode hides the switch. A
// copy of SelectMachine.cpp without the switch check must be reported.
import {readFileSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (rel) => readFileSync(path.join(root, rel), 'utf8');
const sources = {
  appConfig: read('src/libslic3r/AppConfig.cpp'),
  selectMachine: read('src/slic3r/GUI/SelectMachine.cpp'),
  vocabulary: read('src/slic3r/GUI/PersonalVocabulary.cpp'),
  routes: read('src/slic3r/GUI/FeatureServices/PresentationRoutes.hpp'),
  preferences: read('src/slic3r/GUI/Preferences.cpp'),
};

function check(s) {
  const problems = [];
  if (!/if \(get\("vocabulary_job_names"\)\.empty\(\)\)\s*set_bool\("vocabulary_job_names", false\);/.test(s.appConfig))
    problems.push('the default is not declared off in AppConfig::set_defaults');
  const uses = [...s.selectMachine.matchAll(/apply_loaded\(/g)].length;
  if (uses !== 1) problems.push(`SelectMachine.cpp calls apply_loaded ${uses} times; exactly one guarded call is allowed`);
  const guard = s.selectMachine.indexOf('get_bool("vocabulary_job_names")) {');
  const transform = s.selectMachine.indexOf('PersonalVocabulary::apply_loaded(');
  const cleaning = s.selectMachine.indexOf('project_name = filter_characters(worded.utf8_string(), "<>[]:/\\\\|?*\\"");');
  const guarded = guard >= 0 && transform > guard && transform - guard < 200 && cleaning > transform && cleaning - transform < 200;
  if (!guarded) problems.push('the job name transform is not guarded by the switch and followed by the rename cleaning');
  const fn = s.vocabulary.slice(s.vocabulary.indexOf('wxString apply_loaded('));
  const body = fn.slice(0, fn.indexOf('\n}\n'));
  if (!body.includes('school_presentation_suppressed.load()) return source;')) problems.push('apply_loaded does not refuse School mode');
  if (!body.includes('if (!loaded()) return source;')) problems.push('apply_loaded does not refuse an unloaded vocabulary');
  if (!s.routes.includes('key != "vocabulary_job_names"')) problems.push('School mode does not hide the switch');
  if (!s.preferences.includes('"vocabulary_job_names"), flags);')) problems.push('the Preferences switch is missing');
  return problems;
}

const problems = check(sources);
const broken = check({...sources, selectMachine: sources.selectMachine.replace('get_bool("vocabulary_job_names")', 'true')});
if (!broken.some((p) => p.includes('guarded'))) problems.push('negative contract: an unguarded transform was not noticed');
if (problems.length) {
  for (const p of problems) console.error(p);
  process.exit(1);
}
console.log('Printer wording contract: off by default, guarded, cleaned, refused in School mode, hidden in School mode.');
console.log('Negative contract noticed an unguarded transform.');
