// A small C and C++ lexer shared by the source-contract tests in this folder.
// They read sources as text so they run without a build, and they blank
// comments and literals first, so a call is only ever found in code and a
// commented-out line can neither satisfy a contract nor break one.

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
