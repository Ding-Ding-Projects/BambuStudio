// Source checks for the audio and video preview of a data: URL
// (docs/features/windows/base64-toolkit.md, "data: URL"):
//
// - Audio and Video join the preview choices, offered only when the bytes or
//   the declared type say so and the build has a player.
// - The player plays a temporary file named by the content's digest, written
//   to the temporary folder; the file is removed when the preview changes or
//   the panel closes, and nothing in the URL is opened or run.
// - Over 64 MB a size notice shows instead of a player, like images.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const panel = read('src/slic3r/GUI/Base64Toolkit/Base64ToolkitPanel.cpp');

function body(source, name) {
    const start = source.indexOf(name);
    assert(start >= 0, name);
    const begin = source.indexOf('{', start);
    const masked = source.slice(begin).replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, (t) => ' '.repeat(t.length));
    let depth = 0;
    for (let i = 0; i < masked.length; ++i) {
        if (masked[i] === '{') ++depth;
        if (masked[i] === '}' && --depth === 0) return source.slice(start, begin + i + 1);
    }
    assert.fail(`Unbalanced ${name}`);
}
const code = (text) => text.replace(/\/\/[^\n]*/g, '');

function checkChoices(source) {
    assert.match(source, /\{3, _L\("Audio"\)\}, \{4, _L\("Video"\)\}/, 'Audio and Video are preview choices');
    const readUrl = code(body(source, 'void Base64ToolkitPanel::url_read()'));
    assert.match(readUrl, /const bool player = m_url_media != nullptr;/);
    assert.match(readUrl, /const bool audio = m_url_parsed_ok && player && \(type\.preview == B64::DetectedType::Preview::Audio \|\| m_url_parsed\.mime\.rfind\("audio\/", 0\) == 0\);/);
    assert.match(readUrl, /\(v == 3 && audio\) \|\| \(v == 4 && video\)/, 'the choices are offered only when they apply');
    assert.match(readUrl, /const int kind = image \? 0 : textual \? 1 : audio \? 3 : video \? 4 : 2;/);
}

function checkPlayer(source) {
    const show = code(body(source, 'void Base64ToolkitPanel::url_show_preview('));
    assert.match(show, /media_unload\(\);\s*if \(kind == 3 \|\| kind == 4\) \{/, 'a new preview unloads the old file first');
    assert.match(show, /if \(bytes\.size\(\) > kImagePreview\) \{[\s\S]*?over the 64 MB preview limit\. Save it to play it\./, 'the 64 MB limit');
    const load = code(body(source, 'void Base64ToolkitPanel::media_load('));
    assert.match(load, /B64::sha256_hex\(bytes\.data\(\), bytes\.size\(\)\)/, 'the temporary file is named by its content');
    assert.match(load, /wxStandardPaths::Get\(\)\.GetTempDir\(\)/, 'in the temporary folder');
    assert.match(load, /if \(!mc->Load\(wxString::FromUTF8\(path\.u8string\(\)\)\)\) \{[\s\S]*?media_unload\(\);/, 'a format the player refuses removes the file');
    const unload = code(body(source, 'void Base64ToolkitPanel::media_unload()'));
    assert.match(unload, /fs::remove\(m_media_temp, ec\);\s*m_media_temp\.clear\(\);/, 'the file is removed');
    const dtor = code(body(source, 'Base64ToolkitPanel::~Base64ToolkitPanel()'));
    assert.match(dtor, /media_unload\(\);/, 'and removed when the panel closes');
    assert.match(source, /media->Hide\(\);\s*rs->Add\(media, 0, wxTOP, FromDIP\(4\)\);/, 'the player is hidden until used');
    assert.match(source, /media_row->Hide\(\);\s*rs->Add\(media_row/, 'its buttons too');
}

test('Audio and Video are preview choices, offered when they apply', () => checkChoices(panel));
test('the player plays a temporary file that is removed again', () => checkPlayer(panel));

test('each checker fails on a broken copy', () => {
    assert.throws(() => checkChoices(panel.replace('(v == 3 && audio) || (v == 4 && video)', 'v == 3 || v == 4')));
    assert.throws(() => checkPlayer(panel.replace('fs::remove(m_media_temp, ec);\n        m_media_temp.clear();', 'm_media_temp.clear();')));
});
