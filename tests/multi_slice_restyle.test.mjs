// Source checks for the restyled Multi-slice dialog and the Tag widget
// (docs/features/prepare/multi-slice.md, design/local-tools-2026-11/surface-restyle-2026-11-09.md
// section 6):
//
// - The dialog is built on the shared MD3Dialog shell, resizable, with the
//   stack glyph in the header; the header close and Escape take the same path as
//   the window's close, so a run in progress is stopped first.
// - Ink rows are rows of a rounded fill with hover forwarded from their children,
//   a checked fill, the ink type as a tag and the status as a tag with a glyph.
// - The footer uses Size::Action buttons: Show files (Text) at index 0 of the
//   footer, Close (Outlined) that becomes Stop (Danger) during a run, and Slice and
//   save (Filled); Change folder is Outlined in the destination well.
// - The two new strings are in the catalogues with a Cantonese translation, and
//   the upper-casing happens after the lookup.
// - The Tag widget has the board geometry and is in the build.
//
// Each checker also runs against a broken copy of the source and must fail.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (file) => fs.readFileSync(path.join(root, file), 'utf8').replaceAll('\r\n', '\n');
const dialog = read('src/slic3r/GUI/MultiSlice/MultiSliceDialog.cpp');
const header = read('src/slic3r/GUI/MultiSlice/MultiSliceDialog.hpp');
const tagSource = read('src/slic3r/GUI/Widgets/Tag.cpp');
const tagHeader = read('src/slic3r/GUI/Widgets/Tag.hpp');
const cmake = read('src/slic3r/CMakeLists.txt');

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

function checkShell(source, headerSource) {
    assert.match(headerSource, /class MultiSliceDialog : public MD3Dialog/);
    assert.match(source, /: MD3Dialog\(parent, _L\("Multi-slice"\), wxEmptyString, MaterialIcon::Stack,\s*MD3Dialog::Options\{\/\*resizable\*\/ true,/);
    // The header close must not end the modal loop under a run in progress.
    const close = code(body(source, 'void MultiSliceDialog::OnHeaderClose()'));
    assert.match(close, /\bClose\(\);/);
    assert.doesNotMatch(close, /EndModal/);
    assert.match(headerSource, /void OnHeaderClose\(\) override;/);
    // Escape takes the same path.
    assert.match(source, /wxEVT_CHAR_HOOK[\s\S]{0,200}WXK_ESCAPE[\s\S]{0,60}Close\(\);/);
    // No native caption title any more.
    assert.doesNotMatch(source, /Label::Head_20, _L\("Multi-slice"\)/);
}

function checkRows(source, headerSource) {
    assert.match(source, /class InkRow : public TonalSurface/);
    assert.match(source, /void hover_through\(wxWindow \*child\) \{ state_handler\.attach_child\(child\); \}/);
    assert.match(source, /SecondaryContainer/);
    assert.match(source, /SurfaceContainerHigh/);
    assert.match(source, /blend\(fill, role\(MD3::Role::OnSecondaryContainer\), 0\.08\)/);
    const row = code(body(source, 'void MultiSliceDialog::build_ink_row(Ink &ink)'));
    assert.match(row, /new InkRow\(m_list\)/);
    assert.match(row, /ink\.type_tag = new Tag\(row\)/);
    assert.match(row, /row->hover_through\(child\)/);
    assert.match(row, /row->Bind\(wxEVT_LEFT_UP, toggle\)/);
    assert.match(row, /ink\.type_tag->Bind\(wxEVT_LEFT_UP, toggle\)/);
    assert.match(row, /kRowPadY/);
    assert.doesNotMatch(row, /new wxPanel/);
    assert.match(source, /constexpr int kRowRadius\s*=\s*12;/);
    assert.match(source, /constexpr int kRowHeight\s*=\s*44;/);
    assert.match(headerSource, /Tag\s+\*status = nullptr;/);
}

function checkStatus(source) {
    const status = code(body(source, 'void MultiSliceDialog::show_status('));
    assert.match(status, /case Status::Waiting: text = _L\("Waiting"\);\s+kind = Tag::Kind::Neutral; glyph = MaterialIcon::Schedule;/);
    assert.match(status, /case Status::Slicing: text = _L\("Slicing"\);\s+kind = Tag::Kind::Tonal;\s+glyph = MaterialIcon::Sync;/);
    assert.match(status, /case Status::Saved:\s+text = _L\("File saved"\); kind = Tag::Kind::Green;\s+glyph = MaterialIcon::CheckCircle;/);
    assert.match(status, /case Status::Failed:\s+text = _L\("Not saved"\);\s+kind = Tag::Kind::Red;\s+glyph = MaterialIcon::Error;/);
    assert.match(status, /case Status::Skipped: text = _L\("Stopped"\);\s+kind = Tag::Kind::Neutral; glyph = MaterialIcon::Stop;/);
    // The failure detail is its own line; the tag shows only "Not saved".
    assert.match(status, /ink\.detail->SetLabel\(detail\)/);
    assert.doesNotMatch(status, /Not saved: %s/);
}

function checkFooter(source) {
    const footer = code(body(source, 'void MultiSliceDialog::build_footer()'));
    assert.match(footer, /m_open->SetVariant\(Button::Variant::Text\);/);
    assert.match(footer, /GetFooterSizer\(\)->Insert\(0, m_open, 0, wxALIGN_CENTER_VERTICAL\);/);
    assert.match(footer, /m_close->SetVariant\(Button::Variant::Outlined\);/);
    assert.match(footer, /AddFooterButton\(m_close\);/);
    assert.match(footer, /m_start->SetVariant\(Button::Variant::Filled\);/);
    assert.match(footer, /AddFooterButton\(m_start\);/);
    assert.equal((footer.match(/SetButtonSize\(Button::Size::Action\)/g) ?? []).length, 3);
    const running = code(body(source, 'void MultiSliceDialog::set_running(bool running)'));
    assert.match(running, /running \? Button::Variant::Danger : Button::Variant::Outlined/);
    const destination = code(body(source, 'void MultiSliceDialog::build_destination_well('));
    assert.match(destination, /m_change->SetVariant\(Button::Variant::Outlined\);/);
    assert.match(destination, /m_change->SetButtonSize\(Button::Size::Action\);/);
    assert.match(destination, /Label::Mono_12/);
    assert.match(destination, /wxST_ELLIPSIZE_MIDDLE/);
    assert.match(destination, /Tag::Kind::Green/);
    // The path is printed once: the old "Saving to %s" sentence is gone.
    assert.doesNotMatch(source, /Saving to/);
}

function checkStrings(source) {
    // Look up the mixed-case msgid first, upper-case afterwards.
    assert.match(source, /looked_up\.Upper\(\)/);
    assert.doesNotMatch(source, /_L\("[^"]*"\)\.Upper\(\)/);
    assert.match(source, /header_text\(_L\("Ink"\)\)/);
    assert.match(source, /header_text\(_L\("Status"\)\)/);
    assert.match(source, /header_text\(_L\("Save to"\)\)/);
    assert.match(source, /with_cantonese\(_L\("USB drive"\)\)/);
    assert.match(source, /_L\("No matches\."\)/);
}

function checkTag(sourceCpp, sourceHpp, cmakeSource) {
    assert.match(sourceHpp, /enum class Kind \{\s*Neutral,[\s\S]*Green,[\s\S]*Tonal,[\s\S]*Red,/);
    assert.match(sourceHpp, /bool AcceptsFocus\(\) const override \{ return false; \}/);
    assert.match(sourceCpp, /constexpr int kHeight\s*=\s*22;/);
    assert.match(sourceCpp, /constexpr int kPadX\s*=\s*8;/);
    assert.match(sourceCpp, /constexpr int kGlyphPx\s*=\s*16;/);
    assert.match(sourceCpp, /MD3::Metrics::radius_tag/);
    assert.match(sourceCpp, /SetNumericWeight\(500\)/);
    assert.match(sourceCpp, /PrimaryContainer[\s\S]*SecondaryContainer[\s\S]*ErrorContainer/);
    assert.match(sourceCpp, /PersonalVocabulary::display\(m_text\)/);
    assert.match(cmakeSource, /GUI\/Widgets\/Tag\.cpp/);
    assert.match(cmakeSource, /GUI\/Widgets\/Tag\.hpp/);
}

function checkCatalogues() {
    for (const [file, translated] of [
        ['bbl/i18n/BambuStudio.pot', false],
        ['bbl/i18n/en/BambuStudio_en.po', false],
        ['bbl/i18n/yue_HK/BambuStudio_yue_HK.po', true],
    ]) {
        const text = read(file);
        for (const [id, cantonese] of [['Save to', '存去'], ['USB drive', 'USB 手指']]) {
            assert.equal(text.split(`\nmsgid "${id}"\n`).length - 1, 1, `${file} has ${id} once`);
            if (translated) assert.match(text, new RegExp(`msgid "${id}"\\nmsgstr "${cantonese}"`));
        }
    }
}

test('the dialog is built on the resizable shell and closes through one path', () => {
    checkShell(dialog, header);
    assert.throws(() => checkShell(dialog.replace(/(OnHeaderClose\(\)\s*\{[^}]*?)\bClose\(\);/, '$1EndModal(wxID_CANCEL);'), header));
    assert.throws(() => checkShell(dialog.replace('/*resizable*/ true', '/*resizable*/ false'), header));
    assert.throws(() => checkShell(dialog.replace('WXK_ESCAPE', 'WXK_F1'), header));
});

test('ink rows are rounded fills with hover forwarding, a type tag and a status tag', () => {
    checkRows(dialog, header);
    assert.throws(() => checkRows(dialog.replace('new InkRow(m_list)', 'new wxPanel(m_list)'), header));
    assert.throws(() => checkRows(dialog.replace('row->hover_through(child)', 'void()'), header));
    assert.throws(() => checkRows(dialog.replace('ink.type_tag = new Tag(row)', 'ink.type_tag = nullptr'), header));
    checkStatus(dialog);
    assert.throws(() => checkStatus(dialog.replace('Tag::Kind::Green;   glyph = MaterialIcon::CheckCircle', 'Tag::Kind::Neutral; glyph = MaterialIcon::CheckCircle')));
    assert.throws(() => checkStatus(dialog.replace('ink.detail->SetLabel(detail)', 'void()')));
});

test('the footer and the destination well use the Action size and the right variants', () => {
    checkFooter(dialog);
    assert.throws(() => checkFooter(dialog.replace('Button::Variant::Danger : Button::Variant::Outlined', 'Button::Variant::Outlined : Button::Variant::Outlined')));
    assert.throws(() => checkFooter(dialog.replace('GetFooterSizer()->Insert(0, m_open', 'GetFooterSizer()->Add(m_open')));
    assert.throws(() => checkFooter(dialog.replace('m_start->SetVariant(Button::Variant::Filled)', 'm_start->SetVariant(Button::Variant::Tonal)')));
    assert.throws(() => checkFooter(dialog.replace('m_change->SetVariant(Button::Variant::Outlined)', 'm_change->SetVariant(Button::Variant::Text)')));
});

test('new strings go through the lookup before the upper-casing and have Cantonese', () => {
    checkStrings(dialog);
    assert.throws(() => checkStrings(dialog.replace('header_text(_L("Ink"))', '_L("Ink").Upper()')));
    assert.throws(() => checkStrings(dialog.replace('header_text(_L("Save to"))', '_L("Save to")')));
    checkCatalogues();
});

test('the Tag widget has the board geometry and is part of the build', () => {
    checkTag(tagSource, tagHeader, cmake);
    assert.throws(() => checkTag(tagSource.replace('MD3::Metrics::radius_tag', 'MD3::Metrics::pill_radius(22)'), tagHeader, cmake));
    assert.throws(() => checkTag(tagSource.replace('kHeight   = 22', 'kHeight   = 28'), tagHeader, cmake));
    assert.throws(() => checkTag(tagSource, tagHeader.replace('return false;', 'return true;'), cmake));
    assert.throws(() => checkTag(tagSource, tagHeader, cmake.replace('GUI/Widgets/Tag.cpp', '')));
});
