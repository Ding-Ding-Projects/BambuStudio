import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Graphite shell command palette: one SurfaceContainerHigh surface with the
// selected row in the secondary container (design/graphite-shell/Dialogs.dc.html).
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const palette = fs.readFileSync(path.join(root, 'src/slic3r/GUI/CommandPalette.cpp'), 'utf8').replaceAll('\r\n', '\n');

test('the palette, its list and its rows share the SurfaceContainerHigh surface', () => {
    assert(palette.includes('SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHigh));\n    auto *root'));
    assert(palette.includes('m_list->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHigh));'));
    assert.doesNotMatch(palette, /SurfaceContainerLowest|SurfaceContainerLow\)/);
});

test('the selected row uses the secondary container roles', () => {
    assert(palette.includes('const wxColour sel  = StateColor::semantic(MD3::Role::SecondaryContainer);'));
    assert.doesNotMatch(palette, /OnPrimaryContainer|Role::PrimaryContainer/);
    assert.match(palette, /m_selected == index \? MD3::Role::OnSecondaryContainer : MD3::Role::OnSurfaceVariant/);
});
