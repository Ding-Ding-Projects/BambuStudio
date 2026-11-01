import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Graphite shell: the Prepare sidebar sections float as rounded cards on the
// window ground instead of filling the sidebar edge to edge.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const plater = fs.readFileSync(path.join(root, 'src/slic3r/GUI/Plater.cpp'), 'utf8').replaceAll('\r\n', '\n');

const body = name => {
    const start = plater.indexOf(`${name}\n{`);
    assert.notEqual(start, -1, name);
    const end = plater.indexOf('\n}\n', start);
    return plater.slice(start, end);
};

test('the card padding stays larger than the corner cut of the panel radius', () => {
    const padding = Number(/static constexpr int SECTION_CARD_PADDING = (\d+);/.exec(plater)?.[1]);
    const tokens = fs.readFileSync(path.join(root, 'src/slic3r/GUI/Widgets/MD3Tokens.hpp'), 'utf8');
    const radius = Number(/radius_panel\s*=\s*(\d+)/.exec(tokens)?.[1]);
    assert(padding > radius * (1 - Math.SQRT1_2), `${padding} > corner cut of ${radius}`);
});

test('the sidebar sets up the cards once, after every section is in the scrolled sizer', () => {
    const setup = plater.indexOf('    p->setup_section_cards();');
    assert.notEqual(setup, -1);
    assert(plater.indexOf('scrolled_sizer->Add(p->m_manip_panel') < setup);
    assert.match(body('void Sidebar::priv::setup_section_cards()'), /if \(sizer == nullptr \|\| !card_spacers\.empty\(\)\)\n        return;/);
});

test('the cards follow the active section and every scroll body update', () => {
    assert.match(body('void Sidebar::update_scroll_body() const'), /p->layout_section_cards\(\);\n    update_sidebar_scroll_body\(p->scrolled\);/);
    assert(body('void Sidebar::apply_prepare_section(const std::string &section) const').includes('update_scroll_body();'));
});

test('the ground and the cards use the Graphite roles', () => {
    const paint = body('void Sidebar::priv::paint_section_cards(wxDC &dc)');
    assert(paint.includes('MD3::Role::Surface)'));
    assert(paint.includes('MD3::Role::SurfaceContainerLow'));
    assert(paint.includes('MD3::Role::OutlineVariant'));
    assert(paint.includes('MD3::Metrics::radius_panel'));
});

test('no divider is drawn between the printer and filament cards', () => {
    const at = plater.indexOf('scrolled_sizer->Add(p->m_panel_filament_title');
    assert.notEqual(at, -1);
    assert.doesNotMatch(plater.slice(at - 300, at), /new ::StaticLine\(p->scrolled\)/);
});
