import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// A narrow string literal becomes a wxString through the Windows code page, so
// its UTF-8 bytes turn into mojibake: Temperature calibration on md3-v154 showed
// "Â°C" once its centred units were drawn where they can be seen, and the What's
// new cards read "Â·" between date and tag. Every non-ASCII narrow literal in the
// GUI must go through a converting call (wxString::FromUTF8, _L, _u8L, ...).

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');

// Calls that take a narrow literal as UTF-8 (or keep it in a std::string).
const CONVERTING = /(FromUTF8|_L|_u8L|_utf8|\bL|_CTX|from_u8|translate\w*|L_str|_devL|funny_row_label\w*|localized_stacked\w*|format|std::string\s+\w+\s*=.*)\s*\(?\s*$/;
const CONVERTING_CALL = /(FromUTF8|_L|_u8L|_utf8|\bL|_CTX|from_u8|translate\w*|L_str|_devL|funny_row_label\w*|localized_stacked\w*|format)\s*\(/g;

// True when the literal sits inside a converting call that is still open,
// as in wxString::FromUTF8(name + "°C").
function insideConvertingCall(before) {
  let last = -1;
  for (const match of before.matchAll(CONVERTING_CALL)) last = match.index + match[0].length;
  if (last < 0) return false;
  const rest = before.slice(last);
  return (rest.match(/\(/g) || []).length >= (rest.match(/\)/g) || []).length;
}

// Files whose narrow literals are converted where they are used, with the reason.
const ALLOWED_FILES = new Map([
  ['LanguageMode.cpp', 'funny-level copy tables, converted with wxString::FromUTF8 where they are read'],
  ['DimSumSurpriseModel.hpp', 'Cantonese copy returned as const char*, converted with from_u8 by DimSumSurprise.cpp'],
  ['GLGizmoMeasure.cpp', 'std::string text drawn by ImGui, which reads UTF-8'],
  ['GLGizmoMeasure.hpp', 'macOS key symbol in a std::string'],
  ['GUI.cpp', 'macOS key names in a std::string'],
  ['wgtFilaManagerCloudDispatcher.cpp', 'debug-log text, not shown in the UI'],
]);

function* sources(dir) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) yield* sources(full);
    else if (/\.(cpp|hpp)$/.test(entry.name)) yield full;
  }
}

export function unsafeLiterals(text) {
  const found = [];
  const literal = /(?<![A-Za-z0-9_])(u8|L|u|U)?"((?:[^"\\\n]|\\.)*)"/g;
  text.split('\n').forEach((line, index) => {
    const trimmed = line.trim();
    if (trimmed.startsWith('//') || trimmed.startsWith('*') || trimmed.startsWith('/*') || trimmed.startsWith('#')) return;
    // A line that only continues a multi-line literal belongs to the call on the line above.
    if (trimmed.startsWith('"')) return;
    for (const match of line.matchAll(literal)) {
      if (match[1]) continue; // u8 / L / u / U literals carry their own encoding
      if (![...match[2]].some((ch) => ch.codePointAt(0) > 127)) continue;
      const before = line.slice(0, match.index);
      if (before.includes('//')) continue;
      if (CONVERTING.test(before) || insideConvertingCall(before)) continue;
      // Streamed into a narrow log or std::ostream: the bytes stay UTF-8.
      if (/<<\s*$/.test(before)) continue;
      found.push({ line: index + 1, literal: match[2] });
    }
  });
  return found;
}

test('the scan flags a bare degree sign and accepts a converted one', () => {
  assert.equal(unsafeLiterals('auto *t = new TextInput(this, v, "°C", "");').length, 1);
  assert.equal(unsafeLiterals('meta += " · " + _L("pre-release");').length, 1);
  assert.equal(unsafeLiterals('auto *t = new TextInput(this, v, wxString::FromUTF8("°C"), "");').length, 0);
  assert.equal(unsafeLiterals('label->SetLabel(_L("Used Filament (mm³)"));').length, 0);
  assert.equal(unsafeLiterals('auto s = L"°C";').length, 0);
  assert.equal(unsafeLiterals('m_bed->SetLabel(wxString::FromUTF8(text + "°C"));').length, 0);
  assert.equal(unsafeLiterals('m_bed->SetLabel(wxString::FromUTF8(text) + "°C");').length, 1, 'the call closed before the literal');
  assert.equal(unsafeLiterals('BOOST_LOG_TRIVIAL(info) << "[x] → sent";').length, 0);
});

test('no GUI source hands a non-ASCII narrow literal straight to wx', () => {
  const leaks = [];
  for (const file of sources(guiDir)) {
    if (ALLOWED_FILES.has(path.basename(file))) continue;
    for (const hit of unsafeLiterals(readFileSync(file, 'utf8'))) {
      leaks.push(`${path.relative(repoDir, file).replace(/\\/g, '/')}:${hit.line} "${hit.literal}"`);
    }
  }
  assert.deepEqual(leaks, []);
});
