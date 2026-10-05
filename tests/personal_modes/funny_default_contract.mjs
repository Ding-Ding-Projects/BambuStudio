import { readFileSync, writeFileSync, mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert/strict';

const header = readFileSync(new URL('../../src/slic3r/GUI/LanguageMode.hpp', import.meta.url), 'utf8');
const source = readFileSync(new URL('../../src/slic3r/GUI/LanguageMode.cpp', import.meta.url), 'utf8');
const declarations = [...header.matchAll(/inline constexpr int FUNNY_LEVEL_(?:MIN|MAX|DEFAULT)\s*=\s*\d+;/g)].map(m => m[0]);
assert.equal(declarations.length, 3, 'all three actual production constants must be extracted');
assert.match(header, /parse_funny_level\(std::string_view stored, int fallback = FUNNY_LEVEL_DEFAULT\)/);
assert.match(header, /m_funny_level_english\s*\{ FUNNY_LEVEL_DEFAULT \}/);
assert.match(header, /m_funny_level_cantonese\s*\{ FUNNY_LEVEL_DEFAULT \}/);
const begin = source.indexOf('int clamp_funny_level(int level)');
const end = source.indexOf('bool parse_dialog_emojis(', begin);
assert(begin >= 0 && end > begin, 'actual production parsing functions must exist');
// Compile the production bodies directly, without restating the parser in a test.
const functions = source.slice(begin, end);
const driver = `
int main() {
  if (FUNNY_LEVEL_DEFAULT != 5) return 1;
  if (parse_funny_level("", FUNNY_LEVEL_DEFAULT) != 5) return 2;
  if (parse_funny_level("  ", FUNNY_LEVEL_DEFAULT) != 5) return 3;
  if (parse_funny_level("invalid", FUNNY_LEVEL_DEFAULT) != 5) return 4;
  for(int level = 1; level <= 5; ++level)
    if(parse_funny_level(std::to_string(level), FUNNY_LEVEL_DEFAULT) != level) return 5;
  if(parse_funny_level(" 2 ", FUNNY_LEVEL_DEFAULT) != 2) return 6;
  if(parse_funny_level("0", FUNNY_LEVEL_DEFAULT) != 1) return 7;
  if(parse_funny_level("6", FUNNY_LEVEL_DEFAULT) != 5) return 8;
  if(parse_funny_level("", 3) != 3) return 9;
  return 0;
}
`;
const directory = mkdtempSync(join(tmpdir(), 'funny-default-check-'));
try {
    const cpp = join(directory, 'default.cpp');
    const exe = join(directory, 'default.exe');
    const prelude = '#include <algorithm>\n#include <cctype>\n#include <cstdlib>\n#include <string>\n#include <string_view>\n';
    for (const mutant of [false, true, false]) {
        const constants = declarations.join('\n').replace(/FUNNY_LEVEL_DEFAULT\s*=\s*\d+;/,
            `FUNNY_LEVEL_DEFAULT = ${mutant ? 2 : 5};`);
        // Baseline must use precisely the source declaration, not an imposed value.
        if (!mutant) assert.equal(constants, declarations.join('\n'));
        writeFileSync(cpp, prelude + constants + '\n' + functions + driver);
        const compile = spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', cpp, `/Fe${exe}`, `/Fo${join(directory, 'default.obj')}`], { encoding: 'utf8' });
        assert.equal(compile.status, 0, compile.stderr || compile.stdout || String(compile.error));
        const result = spawnSync(exe, [], { encoding: 'utf8' });
        assert.equal(result.status, mutant ? 1 : 0, 'actual defaults and explicit choices must have the expected behavior');
    }
    console.log('PASS actual funny-level parser: default 5, explicit 1..5 preserved, 1 negative mutation restored green');
} finally { rmSync(directory, { recursive: true, force: true }); }
