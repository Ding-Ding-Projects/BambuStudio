import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// GetModuleFileNameW takes the size of its buffer in characters. Passing
// sizeof(buffer) for a wchar_t array tells Windows the buffer holds twice as
// many characters as it does, so a long executable path can be written past its
// end (GUI_App.cpp did this in its file association code until that code moved
// to current_executable_path()). These contracts read every C and C++ source
// under src/slic3r/GUI as text and refuse a byte count as the size of a module
// file name buffer, so they run without a build. The build defines UNICODE for
// every target (CMakeLists.txt), which makes the plain GetModuleFileName and
// GetModuleFileNameEx macros the wide functions as well. The narrow A functions
// are not checked: for a char buffer, sizeof is the number of characters.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');

const SOURCE_FILE = /\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inl|ipp|mm)$/;

function* sources(dir) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (entry.name === 'node_modules' || entry.name.startsWith('.')) continue;
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) yield* sources(full);
    else if (SOURCE_FILE.test(entry.name)) yield full;
  }
}

const isIdentifierStart = (c) => (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c === '_';
const isIdentifierPart = (c) => isIdentifierStart(c) || (c >= '0' && c <= '9');
const isDigit = (c) => c >= '0' && c <= '9';
const RAW_STRING_PREFIX = /^(?:u8|u|U|L)?R$/;

// Replaces comments and the contents of string and character literals with
// spaces and keeps every newline, so a call is only ever found in code and its
// line number still matches the file. `complete` is false when the text ends
// inside a comment or a literal: the scan misread the file there, and a call
// after that point would have been hidden.
export function maskNonCode(source) {
  const text = source.replace(/\r\n?/g, '\n');
  const parts = [];
  let copied = 0;
  const blank = (from, to) => {
    parts.push(text.slice(copied, from), text.slice(from, to).replace(/[^\n]/g, ' '));
    copied = to;
  };
  const done = (complete) => {
    parts.push(text.slice(copied));
    return { code: parts.join(''), complete };
  };

  let i = 0;
  while (i < text.length) {
    const c = text[i];
    if (c === '/' && text[i + 1] === '/') {
      // A backslash at the end of the line carries the comment on to the next one.
      let end = text.indexOf('\n', i);
      while (end > 0 && text[end - 1] === '\\') end = text.indexOf('\n', end + 1);
      if (end < 0) end = text.length;
      blank(i, end);
      i = end;
    } else if (c === '/' && text[i + 1] === '*') {
      const end = text.indexOf('*/', i + 2);
      if (end < 0) {
        blank(i, text.length);
        return done(false);
      }
      blank(i, end + 2);
      i = end + 2;
    } else if (isIdentifierStart(c)) {
      let end = i + 1;
      while (end < text.length && isIdentifierPart(text[end])) end++;
      const prefix = text.slice(i, end);
      i = end;
      if (text[i] === '"' && RAW_STRING_PREFIX.test(prefix)) {
        // R"delimiter( ... )delimiter"
        const open = text.indexOf('(', i);
        const delimiter = open < 0 ? '' : text.slice(i + 1, open);
        const valid = open >= 0 && delimiter.length <= 16 && !/[\s\\)"]/.test(delimiter);
        const close = valid ? text.indexOf(`)${delimiter}"`, open) : -1;
        if (close < 0) {
          blank(i + 1, text.length);
          return done(false);
        }
        const quote = close + delimiter.length + 1;
        blank(i + 1, quote);
        i = quote + 1;
      }
    } else if (isDigit(c) || (c === '.' && isDigit(text[i + 1]))) {
      // A number, with exponents and digit separators such as 1'000.
      let end = i + 1;
      while (end < text.length) {
        const d = text[end];
        if (isIdentifierPart(d) || d === '.') end++;
        else if ((d === '+' || d === '-') && 'eEpP'.includes(text[end - 1])) end++;
        else if (d === "'" && isIdentifierPart(text[end + 1])) end += 2;
        else break;
      }
      i = end;
    } else if (c === '"' || c === "'") {
      let end = i + 1;
      while (end < text.length && text[end] !== c && text[end] !== '\n') end += text[end] === '\\' ? 2 : 1;
      if (text[end] !== c) {
        blank(i + 1, text.length);
        return done(false);
      }
      blank(i + 1, end);
      i = end + 1;
    } else {
      i++;
    }
  }
  return done(true);
}

const MODULE_FILE_NAME_CALL = /(?<![A-Za-z0-9_])(GetModuleFileName(?:Ex)?W?)(?=\s*\()/g;
const squeeze = (text) => text.replace(/\s+/g, ' ').trim();

// Every call of GetModuleFileName, GetModuleFileNameW, GetModuleFileNameEx and
// GetModuleFileNameExW in `source`, with its line and its last argument, which
// is the size of the buffer. `size` is null when the argument list never closes.
export function moduleFileNameCalls(source) {
  const { code, complete } = maskNonCode(source);
  const calls = [];
  for (const match of code.matchAll(MODULE_FILE_NAME_CALL)) {
    const open = code.indexOf('(', match.index + match[1].length);
    const args = [];
    let start = open + 1;
    let depth = 0;
    let close = -1;
    for (let i = open; i < code.length && close < 0; i++) {
      const c = code[i];
      if (c === '(' || c === '[' || c === '{') {
        depth++;
      } else if (c === ')' || c === ']' || c === '}') {
        if (--depth === 0) {
          args.push(code.slice(start, i));
          close = i;
        }
      } else if (c === ',' && depth === 1) {
        args.push(code.slice(start, i));
        start = i + 1;
      }
    }
    calls.push({
      name: match[1],
      line: code.slice(0, match.index).split('\n').length,
      size: close < 0 ? null : squeeze(args[args.length - 1]),
      text: squeeze(code.slice(match.index, close < 0 ? match.index + 120 : close + 1)),
    });
  }
  return { complete, calls };
}

// True when a size argument counts bytes: it takes sizeof of something and does
// not divide that down to a number of elements, as sizeof(a) / sizeof(a[0]) does.
export function isByteCount(size) {
  return size !== null && /(?<![A-Za-z0-9_])sizeof(?![A-Za-z0-9_])/.test(size) && !size.includes('/');
}

const byteCountCalls = (source) => moduleFileNameCalls(source).calls.filter((call) => isByteCount(call.size));

// A function that starts at column 0 ends at the first closing brace at column 0.
const bodyOf = (code, signature) => {
  const start = code.indexOf(signature);
  assert.ok(start >= 0, `${signature} must exist`);
  const end = code.indexOf('\n}', start);
  assert.ok(end > start, `${signature} must end with a closing brace at column 0`);
  return code.slice(start, end + 2);
};

test('the scan flags a byte count in every wide spelling and accepts a character count', () => {
  for (const source of [
    '::GetModuleFileNameW(nullptr, app_path, sizeof(app_path));',
    'GetModuleFileNameW(\n    nullptr,\n    app_path,\n    sizeof(app_path));',
    'GetModuleFileNameW(nullptr, app_path, sizeof app_path);',
    'GetModuleFileNameW(nullptr, app_path, static_cast<DWORD>(sizeof(app_path)));',
    'GetModuleFileName(nullptr, exe_path, sizeof(exe_path));',
    'GetModuleFileNameExW(process, nullptr, name, sizeof(name));',
    'GetModuleFileNameEx /* the wide one */ (process, nullptr, name, sizeof(name));',
  ]) {
    assert.equal(byteCountCalls(source).length, 1, `must be flagged: ${source}`);
  }

  for (const source of [
    'GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));',
    'GetModuleFileNameW(nullptr, app_path, MAX_PATH);',
    'GetModuleFileNameW(nullptr, app_path, static_cast<DWORD>(std::size(app_path)));',
    'GetModuleFileNameW(nullptr, app_path, sizeof(app_path) / sizeof(app_path[0]));',
  ]) {
    const { calls } = moduleFileNameCalls(source);
    assert.equal(calls.length, 1, `must be seen: ${source}`);
    assert.equal(isByteCount(calls[0].size), false, `must be accepted: ${source}`);
  }

  for (const source of [
    'GetModuleFileNameA(nullptr, name, sizeof(name));',
    'GetModuleFileNameExA(process, nullptr, name, sizeof(name));',
    'pfnGetModuleFileNameEx(process, nullptr, name, sizeof(name));',
    'GetModuleFileNameWrapper(nullptr, name, sizeof(name));',
  ]) {
    assert.equal(moduleFileNameCalls(source).calls.length, 0, `not a wide GetModuleFileName call: ${source}`);
  }
});

test('comments and literals can neither hide a call nor fake one', () => {
  const call = '::GetModuleFileNameW(nullptr, app_path, sizeof(app_path));';
  for (const source of [
    `// ${call}`,
    `/* ${call} */`,
    `/*\n * ${call}\n */`,
    `const char *text = "${call}";`,
    `const wchar_t *text = LR"(${call})";`,
    `// C:\\temp\\\n${call}`,
  ]) {
    assert.equal(moduleFileNameCalls(source).calls.length, 0, `not code: ${JSON.stringify(source)}`);
  }

  for (const source of [
    `const wchar_t *pattern = L"*.3mf;dir/*"; ${call}`,
    `const char *url = "https://github.com"; ${call}`,
    `const int count = 1'000'000; ${call}`,
    `const char quote = '"'; ${call}`,
    `const char apostrophe = '\\''; ${call}`,
    `const char *json = R"json({"a": ")"})json"; ${call}`,
  ]) {
    assert.equal(byteCountCalls(source).length, 1, `still code after the literal: ${JSON.stringify(source)}`);
  }

  const { calls } = moduleFileNameCalls(`/* one\r\n two */\r\nint x;\r\n${call}`);
  assert.equal(calls[0].line, 4, 'line numbers survive CRLF line ends and blanked comments');
});

test('a scan that ends inside a comment or a literal reports that it misread the file', () => {
  assert.equal(maskNonCode('int x; /* never closed').complete, false);
  assert.equal(maskNonCode('auto s = "never closed;\nint x;').complete, false);
  assert.equal(maskNonCode("auto c = 'x;\nint y;").complete, false);
  assert.equal(maskNonCode('auto s = R"x(never closed)";').complete, false);
  assert.equal(maskNonCode('auto s = R"x(closed)x"; // done').complete, true);
});

test('no call under src/slic3r/GUI passes a byte count as the size of its buffer', () => {
  const files = [...sources(guiDir)].map((file) => ({ file, relative: path.relative(repoDir, file).split(path.sep).join('/') }));
  const relatives = new Set(files.map(({ relative }) => relative));
  for (const known of ['src/slic3r/GUI/GUI_App.cpp', 'src/slic3r/GUI/Widgets/BoundedRegex.cpp']) {
    assert.ok(relatives.has(known), `the scan must reach ${known}`);
  }

  const misread = [];
  const calls = [];
  for (const { file, relative } of files) {
    const scan = moduleFileNameCalls(readFileSync(file, 'utf8'));
    if (!scan.complete) misread.push(relative);
    for (const call of scan.calls) calls.push({ file: relative, ...call });
  }
  assert.deepEqual(misread, [], 'every source must scan to its end outside comments and literals, or a call after the misread part would be missed');
  assert.deepEqual(
    calls.filter((call) => call.size === null).map((call) => `${call.file}:${call.line}`),
    [],
    'every call must have a closed argument list'
  );
  assert.ok(
    calls.some((call) => call.file === 'src/slic3r/GUI/GUI_App.cpp' && call.name === 'GetModuleFileNameW'),
    'the scan must see the call in current_executable_path()'
  );

  const byteCounts = calls.filter((call) => isByteCount(call.size)).map((call) => `${call.file}:${call.line}: ${call.text}`);
  assert.deepEqual(
    byteCounts,
    [],
    'GetModuleFileName takes the buffer size in characters and sizeof of a wchar_t buffer is twice that; use current_executable_path() or static_cast<DWORD>(std::size(buffer))'
  );
});

test('the file association code takes the executable path from the helper that grows its buffer', () => {
  const { code } = maskNonCode(readFileSync(path.join(guiDir, 'GUI_App.cpp'), 'utf8'));
  const helper = bodyOf(code, 'static boost::filesystem::path current_executable_path()');
  assert.match(
    helper,
    /GetModuleFileNameW\(nullptr, buffer\.data\(\), static_cast<DWORD>\(buffer\.size\(\)\)\)/,
    'the helper passes the length of its buffer in characters'
  );
  assert.match(helper, /buffer\.resize\(buffer\.size\(\) \* 2\);/, 'a path that does not fit gets a buffer twice as long, so there is no MAX_PATH ceiling');

  for (const signature of ['void GUI_App::associate_files(std::wstring extend)', 'void GUI_App::disassociate_files(std::wstring extend)']) {
    assert.match(bodyOf(code, signature), /current_executable_path\(\)\.wstring\(\)/, `${signature} must take the path from current_executable_path()`);
  }
  for (const signature of [
    'bool is_associate_files(std::wstring extend)',
    'void GUI_App::associate_files(std::wstring extend)',
    'void GUI_App::disassociate_files(std::wstring extend)',
  ]) {
    assert.doesNotMatch(bodyOf(code, signature), /\[MAX_PATH\]/, `${signature} must not keep a fixed MAX_PATH buffer for the path`);
  }
});
