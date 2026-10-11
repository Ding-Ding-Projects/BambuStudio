import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The native personal vocabulary file contract: a 256 KiB UTF-8 file, nesting
// depth 8, keys up to 160 and values up to 1,000 Unicode code points, and no
// fixed entry count of any kind. The behaviour is proven by
// tests/personal_vocabulary/personal_vocabulary_schema_test.cpp; this source
// contract keeps a fixed entry count from coming back to the loader, its
// tests or its documentation, and keeps the stated limits in step.

const repoDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (file) => readFileSync(path.join(repoDir, file), 'utf8');

const LOADER = [
    'src/slic3r/GUI/PersonalVocabularySchema.hpp',
    'src/slic3r/GUI/PersonalVocabularySchema.cpp',
    'src/slic3r/GUI/PersonalVocabulary.hpp',
    'src/slic3r/GUI/PersonalVocabulary.cpp',
];
const TESTS = [
    'tests/personal_vocabulary/personal_vocabulary_schema_test.cpp',
    'tests/personal_vocabulary/personal_vocabulary_tests.cpp',
    'tests/personal_vocabulary/personal_vocabulary_probe.cpp',
];
const DOCS = [
    'docs/features/windows/personal-vocabulary.md',
    'docs/features/windows/personal-vocabulary.yue_HK.md',
];

// The only named limits the file contract has. Any other integer constant in
// the loader is a new bound and must be reviewed against the contract.
const CONTRACT = {
    schema_version: 1,
    max_file_bytes: 256 * 1024,
    max_depth: 8,
    max_key_code_points: 160,
    max_value_code_points: 1000,
};

// Size comparisons in the loader that bound something other than entries:
// the two root fields, the display string and output caps, and the session's
// registry of eligible interface copy.
const NON_ENTRY_SIZE_LITERALS = new Set([2, 32768, 65536, 50000]);

function stripComments(source) {
    return source.replace(/\/\*[\s\S]*?\*\//g, ' ').replace(/\/\/[^\n]*/g, ' ');
}

function constants(source) {
    const found = {};
    for (const [, name, expression] of stripComments(source)
        .matchAll(/\bconstexpr\s+(?:const\s+)?(?:std::)?(?:size_t|int|unsigned|long|uint\d+_t|int\d+_t)\s+(\w+)\s*=\s*([^;]+);/g)) {
        assert.match(expression, /^[\d\s*+()]+$/, `${name} is not a plain integer expression`);
        found[name] = Function(`"use strict"; return (${expression});`)();
    }
    return found;
}

// Returns every identifier that names an entry maximum.
function entryCountIdentifiers(source) {
    return [...stripComments(source).matchAll(/\b\w*(?:max_?entr|entr(?:y|ies)_?(?:limit|cap|max|count))\w*\b/gi)]
        .map(([name]) => `entry-count identifier ${name}`);
}

// Returns a description of every fixed entry count found in loader source:
// an entry-count identifier, a comparison of an entries collection's size, a
// size compared with an unexplained literal, or a constant outside the contract.
function fixedEntryCountsInCode(source) {
    const code = stripComments(source);
    const findings = entryCountIdentifiers(source);
    const relation = String.raw`(?:<=|>=|==|!=|<(?!<)|>(?!>))`;
    const entriesSize = String.raw`\w*entr\w*(?:"\])?\s*(?:\.|->)\s*(?:size|count)\s*\(\s*\)`;
    for (const [match] of code.matchAll(new RegExp(`${entriesSize}\\s*${relation}|${relation}\\s*[\\w.\\[\\]">-]*?${entriesSize}`, 'g')))
        findings.push(`entries size comparison ${match.trim()}`);
    const sizeCall = String.raw`[\w.\]"\[]+\s*(?:\.|->)\s*(?:size|count)\s*\(\s*\)`;
    for (const [match, literal] of code.matchAll(new RegExp(`${sizeCall}\\s*${relation}=?\\s*(\\d[\\d']*)`, 'g'))) {
        if (!NON_ENTRY_SIZE_LITERALS.has(Number(literal.replace(/'/g, '')))) findings.push(`size bound ${match.trim()}`);
    }
    for (const [match, literal] of code.matchAll(new RegExp(`(\\d[\\d']*)\\s*${relation}=?\\s*${sizeCall}`, 'g'))) {
        if (!NON_ENTRY_SIZE_LITERALS.has(Number(literal.replace(/'/g, '')))) findings.push(`size bound ${match.trim()}`);
    }
    for (const name of Object.keys(constants(source)))
        if (!(name in CONTRACT)) findings.push(`constant outside the file contract ${name}`);
    return findings;
}

// Returns every sentence fragment that states an entry maximum.
function fixedEntryCountsInDocs(text) {
    const patterns = [
        /\b(?:at most|up to|maximum(?: of)?|no more than|limited to|limit of|cap of)\s+[\d,]+\s+entries/gi,
        /\b[\d,]+\s+entries\s+(?:at most|maximum|or fewer|or less)\b/gi,
        /\bentr(?:y|ies)\s+(?:limit|cap|maximum)\s+(?:of|is)\s+[\d,]+/gi,
        /最多\s*[\d,]+\s*個項目/g,
        /項目(?:數量)?上限/g,
    ];
    return patterns.flatMap((pattern) => [...text.matchAll(pattern)].map(([match]) => match));
}

test('the loader constants are the contract limits and nothing else', () => {
    assert.deepEqual(constants(read('src/slic3r/GUI/PersonalVocabularySchema.hpp')), CONTRACT);
    for (const file of LOADER.slice(1)) assert.deepEqual(constants(read(file)), {}, file);
});

test('no fixed entry count appears in the loader or its tests', () => {
    for (const file of LOADER) assert.deepEqual(fixedEntryCountsInCode(read(file)), [], file);
    // Tests build and count documents, so only a named entry maximum is a finding there.
    for (const file of TESTS) assert.deepEqual(entryCountIdentifiers(read(file)), [], file);
    // The tests must keep proving that far more than any former fixed count loads.
    assert.match(read(TESTS[0]), /for \(std::size_t count : \{2049u, 5000u, 20000u\}\)/);
});

test('the documentation states every limit and no entry maximum', () => {
    const english = read(DOCS[0]);
    for (const phrase of ['256 KiB', '8 levels', '160 Unicode code points', '1,000 Unicode code points',
        'No fixed count', 'Every valid entry is accepted'])
        assert.ok(english.includes(phrase), phrase);
    const cantonese = read(DOCS[1]);
    for (const phrase of ['256 KiB', '最多 8 層', '160 個 Unicode 碼位', '1,000 個 Unicode 碼位', '冇固定數目'])
        assert.ok(cantonese.includes(phrase), phrase);
    for (const file of DOCS) assert.deepEqual(fixedEntryCountsInDocs(read(file)), [], file);
});

test('the settings control names the limits from the parser constants', () => {
    const preferences = read('src/slic3r/GUI/Preferences.cpp');
    for (const [message, constant] of [
        ['The file is larger than %d KiB.', 'max_file_bytes / 1024'],
        ['The file nests objects or lists more than %d levels deep.', 'max_depth'],
        ['An entry key is longer than %d characters.', 'max_key_code_points'],
        ['An entry value is longer than %d characters.', 'max_value_code_points'],
    ]) {
        const at = preferences.indexOf(`_L("${message}")`);
        assert.ok(at > 0, message);
        assert.ok(preferences.slice(at, at + 200).includes(`PersonalVocabulary::${constant}`), message);
    }
    assert.ok(!/_L\("[^"]*(?:2,?048|1 MiB|512)[^"]*"\)/.test(preferences), 'an old limit is named in the control');
    assert.match(preferences, /PersonalVocabulary::load\([^;]*&reason\)/);
    assert.match(preferences, /PersonalVocabulary::cache_rejection\(\)/);
});

// tests/native_preferences_atlas.test.mjs sets this block aside from its
// baseline fingerprint; this contract owns it.
test('the settings control keeps its load, replace, clear and status states', () => {
    const preferences = read('src/slic3r/GUI/Preferences.cpp');
    const start = preferences.indexOf('auto *wording_description = new Label(wording,');
    const end = preferences.indexOf('register_option_row("personal_vocabulary", nullptr, wording);', start);
    assert.ok(start > 0 && end > start, 'personal vocabulary control block');
    const block = preferences.slice(start, end);
    for (const needle of [
        '_L("Load a version 1 JSON file to change display wording on this device. Original data, logs and exports keep their original wording.")',
        'PersonalVocabulary::loaded()',
        '_L("Replace JSON")',
        '_L("Load JSON")',
        '_L("Personal vocabulary is active on this device.")',
        '_L("Original wording is active.")',
        '_L("The vocabulary saved on this device did not pass validation, so original wording is shown.")',
        'MD3::Role::Error',
        'wording_load->Bind(wxEVT_BUTTON',
        '_L("Load personal vocabulary")',
        '_L("JSON files (*.json)|*.json")',
        'wxFD_OPEN | wxFD_FILE_MUST_EXIST',
        '_L("The vocabulary file was not applied, and the wording on screen has not changed.")',
        'vocabulary_rejection_text(reason)',
        'wording_clear->Bind(wxEVT_BUTTON',
        'PersonalVocabulary::clear()',
        '_L("The saved vocabulary could not be removed from this device, so nothing changed.")',
    ]) assert.ok(block.includes(needle), needle);
    // Both handlers stay inert under School mode.
    assert.equal([...block.matchAll(/if \(PersonalModes::school_presentation_suppressed\.load\(\)\) return;/g)].length, 2);

    // Every rejection reason has its own localized sentence.
    const header = stripComments(read('src/slic3r/GUI/PersonalVocabularySchema.hpp'));
    const reasons = /enum class Rejection \{([^}]*)\}/.exec(header)[1].split(',').map((name) => name.trim()).filter(Boolean);
    assert.ok(reasons.length >= 12, reasons.join());
    const helperStart = preferences.indexOf('static wxString vocabulary_rejection_text(PersonalVocabulary::Rejection reason)');
    const helper = preferences.slice(helperStart, preferences.indexOf('\n}\n', helperStart));
    for (const reason of reasons.filter((name) => name !== 'None'))
        assert.match(helper, new RegExp(String.raw`case Rejection::${reason}:\s*\n?\s*return (?:_L\(|wxString::Format\(_L\()`), reason);
});

test('a selected file and the saved copy pass through the same validator', () => {
    const source = stripComments(read('src/slic3r/GUI/PersonalVocabulary.cpp'));
    assert.equal([...source.matchAll(/\bvalidate\(/g)].length, 1, 'one validator call, inside parse');
    const body = (name) => {
        const start = source.search(new RegExp(String.raw`\n(?:bool|void)\s+${name}\(`));
        assert.ok(start >= 0, name);
        return source.slice(start, source.indexOf('\n}\n', start));
    };
    assert.match(body('parse'), /validate\(bytes, text\)/);
    for (const name of ['initialize', 'load']) {
        assert.match(body(name), /read_bounded\(/, name);
        assert.match(body(name), /\bparse\(bytes, \w+, &\w+\)/, name);
    }
});

test('the guards reject a fixed entry count when one is reintroduced', () => {
    const header = read('src/slic3r/GUI/PersonalVocabularySchema.hpp');
    const schema = read('src/slic3r/GUI/PersonalVocabularySchema.cpp');
    const loader = read('src/slic3r/GUI/PersonalVocabulary.cpp');
    const mutate = (source, from, to) => {
        assert.ok(source.includes(from), from);
        return source.replace(from, to);
    };
    const anchor = 'constexpr std::size_t max_depth';
    const mutations = [
        mutate(header, anchor, `constexpr std::size_t max_entries = 2048;\n${anchor}`),
        mutate(header, anchor, `constexpr std::size_t max_mappings = 4096;\n${anchor}`),
        mutate(header, anchor, `constexpr std::size_t kMaxEntryCount = 4096;\n${anchor}`),
        mutate(schema, 'next.reserve(entries->size());',
            'if (entries->size() > 2048) return Rejection::UnexpectedField;\n    next.reserve(entries->size());'),
        mutate(schema, 'next.reserve(entries->size());',
            'if (root["entries"].size() > limit) return Rejection::UnexpectedField;\n    next.reserve(entries->size());'),
        mutate(schema, 'next.emplace_back(key, value);', 'if (next.size() >= 4096) break;\n        next.emplace_back(key, value);'),
        mutate(loader, 'next.reserve(text.size());', 'if (4096 < text.size()) return false;\n    next.reserve(text.size());'),
        mutate(loader, 's.entries = std::move(cached);', 'if (cached.size() > 2048) cached.resize(2048);\n    s.entries = std::move(cached);'),
    ];
    for (const source of mutations) assert.notDeepEqual(fixedEntryCountsInCode(source), [], source.slice(0, 80));
    const english = read(DOCS[0]);
    for (const regression of ['at most 2,048 entries', 'up to 4096 entries', '2,048 entries at most', 'an entry limit of 2048'])
        assert.notDeepEqual(fixedEntryCountsInDocs(mutate(english, 'No fixed count.', `${regression}.`)), [], regression);
    const cantonese = read(DOCS[1]);
    assert.notDeepEqual(fixedEntryCountsInDocs(mutate(cantonese, '冇固定數目。', '最多 2,048 個項目。')), []);
    // Comparisons that bound something else stay accepted.
    assert.deepEqual(fixedEntryCountsInCode('if (value.size() > 32768) return source;\nif (root.size() != 2) return x;'), []);
});
