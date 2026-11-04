// Source-contract checks for the narration wiring that a compile-free run can still prove:
// producers call the narrator, every spoken msgid has a Cantonese entry, new settings
// keys have defaults, descriptors and palette rows, and new sources are in CMake.
// `--negative` removes one wiring element at a time from an in-memory copy and requires a failure.
import {readFileSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const read = (p) => readFileSync(path.join(root, p), 'utf8');
const negative = process.argv.includes('--negative');

const files = {
  events: 'src/slic3r/GUI/Narration/NarrationEvents.hpp',
  plater: 'src/slic3r/GUI/Plater.cpp',
  notifications: 'src/slic3r/GUI/NotificationManager.cpp',
  app: 'src/slic3r/GUI/GUI_App.cpp',
  scanner: 'src/slic3r/GUI/FilamentScanner.cpp',
  cmake: 'src/slic3r/CMakeLists.txt',
  defaults: 'src/libslic3r/PresentationDefaults.hpp',
  descriptors: 'src/libslic3r/EffectivePreferences.hpp',
  palette: 'src/slic3r/GUI/CommandPaletteIndex.cpp',
  prefs: 'src/slic3r/GUI/Preferences.cpp',
  tts: 'src/slic3r/GUI/TtsNarrator.cpp',
  pot: 'bbl/i18n/BambuStudio.pot',
  yue: 'bbl/i18n/yue_HK/BambuStudio_yue_HK.po',
  en: 'bbl/i18n/en/BambuStudio_en.po',
};

function check(src) {
  const problems = [];
  const need = (cond, message) => { if (!cond) problems.push(message); };
  const unescape = (s) => s.replace(/\\"/g, '"').replace(/\\\\/g, '\\');

  // Every spoken msgid (all kinds, all tones) is in the POT, English catalogue and Cantonese catalogue with a non-empty translation.
  const rows = src.events.match(/\{"(?:[^"\\]|\\.)*"(?:,\s*"(?:[^"\\]|\\.)*")+\}/g) || [];
  const msgids = new Set();
  for (const row of rows) for (const m of row.matchAll(/"((?:[^"\\]|\\.)*)"/g)) msgids.add(unescape(m[1]));
  need(msgids.size >= 30, 'event table too small: ' + msgids.size);
  const catalogue = new Map();
  for (const m of src.yue.matchAll(/msgid "((?:[^"\\]|\\.)*)"\nmsgstr "((?:[^"\\]|\\.)*)"/g)) catalogue.set(unescape(m[1]), unescape(m[2]));
  for (const id of msgids) {
    const quoted = 'msgid "' + id.replace(/\\/g, '\\\\').replace(/"/g, '\\"') + '"';
    need(src.pot.includes(quoted), 'POT missing: ' + id);
    need(src.en.includes(quoted), 'English catalogue missing: ' + id);
    need((catalogue.get(id) || '').length > 0, 'Cantonese missing: ' + id);
    if (/%06X/.test(id)) need(/%06X/.test(catalogue.get(id) || ''), 'Cantonese placeholder lost: ' + id);
  }

  // Producers.
  need(/narrate\(\{Narration::EventKind::SliceFinished/.test(src.plater), 'slice finished not narrated');
  need(/narrate\(\{Narration::EventKind::SliceFailed/.test(src.plater), 'slice failure not narrated');
  need(/EventKind::ExportDone/.test(src.notifications), 'export not narrated');
  need(/EventKind::AppError/.test(src.notifications) && /ErrorNotificationLevel\)\s*\n?\s*TtsNarrator::narrate/.test(src.notifications), 'app error not narrated');
  need(/EventKind::UpdateReady/.test(src.app), 'update ready not narrated');
  need(/say_tracks\(/.test(src.scanner) && !/say_now\(/.test(src.scanner), 'scanner still on say_now');
  need(/s_detector\.observe\(/.test(src.tts), 'printer events bypass the transition detector');
  need(/s_deduper\.admit\(/.test(src.tts), 'event de-duplication missing');

  // Settings.
  const keys = ['narrator_track_printer', 'narrator_track_app', 'narrator_track_accessibility', 'narrator_quiet_hours_enabled',
    'narrator_quiet_hours_start', 'narrator_quiet_hours_end', 'narrator_respect_focus_assist'];
  for (const key of keys) {
    need(src.defaults.includes('"' + key + '"'), 'no default: ' + key);
    need(src.descriptors.includes('"' + key + '"'), 'no descriptor: ' + key);
    need(src.palette.includes('"' + key + '"'), 'no palette row: ' + key);
    need(src.prefs.includes('"' + key + '"'), 'no Preferences control: ' + key);
  }
  need(/TtsNarrator::preview\(cantonese\)/.test(src.prefs), 'no voice preview button');
  need(/TtsNarrator::catalog\(\)/.test(src.prefs), 'picker does not use the merged catalogue');

  // Build.
  for (const f of ['OneCoreVoices.cpp', 'NarrationEvents.hpp', 'NarrationTransitions.hpp', 'NarrationPolicy.hpp', 'VoiceCatalog.hpp'])
    need(src.cmake.includes('GUI/Narration/' + f), 'CMake missing ' + f);
  return problems;
}

const src = Object.fromEntries(Object.entries(files).map(([k, p]) => [k, read(p).replace(/\r\n/g, '\n')]));
const base = check(src);
if (base.length) { console.log(base.join('\n')); process.exit(1); }
console.log('narration wiring: all checks passed');

if (negative) {
  const cuts = [
    ['plater', 'narrate({Narration::EventKind::SliceFinished', 'noop({Narration::EventKind::SliceFinished'],
    ['notifications', 'EventKind::ExportDone', 'EventKind::Other'],
    ['app', 'EventKind::UpdateReady', 'EventKind::Other'],
    ['tts', 's_detector.observe(', 's_detector_unused('],
    ['tts', 's_deduper.admit(', 's_deduper_unused('],
    ['cmake', 'GUI/Narration/OneCoreVoices.cpp', 'GUI/Narration/Gone.cpp'],
    ['defaults', '"narrator_track_app"', '"narrator_track_x"'],
    ['yue', 'msgid "Slicing finished."\nmsgstr "切片完成。"', 'msgid "Slicing finished."\nmsgstr ""'],
    ['prefs', 'TtsNarrator::preview(cantonese)', 'TtsNarrator::nothing(cantonese)'],
  ];
  let missed = 0;
  for (const [key, from, to] of cuts) {
    if (!src[key].includes(from)) { console.log('ANCHOR MISSING: ' + from); missed++; continue; }
    const mutated = {...src, [key]: src[key].replace(from, to)};
    if (check(mutated).length === 0) { console.log('NOT DETECTED: ' + from); missed++; } else console.log('detected: ' + from);
  }
  process.exit(missed ? 1 : 0);
}
