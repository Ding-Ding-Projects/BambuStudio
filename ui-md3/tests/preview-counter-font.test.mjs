import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The Preview playback bar counts moves with "Move 292 / 292". The bar draws the numbers in the
// mono font, which has no CJK glyphs, and the word went with them: in Cantonese the counter read
// "?? 292 / 292". The word now stays in the regular font and only the numbers use the mono font.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const source = readFileSync(path.resolve(testDir, '..', '..', 'src', 'slic3r', 'GUI', 'IMSlider.cpp'), 'utf8')
  .replace(/\r\n/g, '\n')
  .replace(/\/\/.*$/gm, '');

// Every stretch of code between push_mono_font() and the matching pop_mono_font().
function monoSpans(text) {
  const spans = [];
  const push = /const bool\s+(\w+)\s*=\s*imgui\.push_mono_font\(\);/g;
  let m;
  while ((m = push.exec(text))) {
    const pop = text.indexOf(`if (${m[1]}) imgui.pop_mono_font();`, m.index);
    assert.notEqual(pop, -1, `no pop for ${m[1]}`);
    spans.push(text.slice(m.index, pop));
  }
  return spans;
}

test('no translated text is measured or drawn in the mono font of the playback bar', () => {
  const spans = monoSpans(source);
  assert.ok(spans.length >= 2, 'the playback bar no longer uses the mono font; read this test again');
  for (const span of spans)
    assert.doesNotMatch(span, /_u8L\(|_L\(|_CTX/, `translated text inside a mono font span:\n${span}`);
});

test('the move counter still shows the translated word', () => {
  assert.match(source, /counter_word\s*=\s*_u8L\("Move"\)/);
});
