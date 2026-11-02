import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Graphite shell: the Preview legend dock floats in from the canvas edges as a
// rounded panel instead of running edge to edge.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const base = fs.readFileSync(path.join(root, 'src/slic3r/GUI/GCodeRenderer/BaseRenderer.cpp'), 'utf8').replaceAll('\r\n', '\n');
const tokens = fs.readFileSync(path.join(root, 'src/slic3r/GUI/Widgets/MD3Tokens.hpp'), 'utf8');

test('the legend dock is inset from the top and right edges and rounded', () => {
    assert(base.includes('const float dock_inset = 0.5f * float(MD3::Metrics::active().gap) * m_scale;'));
    assert(base.includes('imgui.set_next_window_pos(float(canvas_width - right_margin * m_scale) - dock_inset, dock_inset, ImGuiCond_Always, 1.0f, 0.0f);'));
    assert(base.includes('ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, float(MD3::Metrics::active().small_radius) * m_scale);'));
    assert(base.includes('- float(MD3::Metrics::preview_timeline_height) * m_scale - dock_inset);'));
});

test('the inset counts toward the dock width that neighbours keep clear of', () => {
    assert(base.includes('m_legend_width = legend_width + dock_inset;'));
});

test('the dock corner cut stays inside the 4 px content inset at both densities', () => {
    for (const name of ['comfortable', 'compact']) {
        const values = new RegExp(`DensityMetrics ${name}\\{([^}]*)\\}`).exec(tokens)[1].split(',').map(Number);
        const smallRadius = values[7];
        assert(smallRadius * (1 - Math.SQRT1_2) < 4, `${name} small radius ${smallRadius}`);
    }
});
