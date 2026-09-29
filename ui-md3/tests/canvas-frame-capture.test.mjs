import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Every release capture taken on a hidden desktop shows the 3D canvas as a blank
// white area: PrintWindow cannot capture an OpenGL surface. The gizmo panels, the
// notifications and the Daily Tips panel are ImGui drawn on that canvas, so no
// release could show them in any language mode. The layout probe's "canvas-png"
// command makes the canvas save its own frame, read back before the buffer swap.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = path.join(repoDir, 'src', 'slic3r', 'GUI');
const stripComments = (text) => text.replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const probe = stripComments(await readFile(path.join(gui, 'LayoutProbe.cpp'), 'utf8'));
const probeHeader = stripComments(await readFile(path.join(gui, 'LayoutProbe.hpp'), 'utf8'));
const canvas = stripComments(await readFile(path.join(gui, 'GLCanvas3D.cpp'), 'utf8'));
const body = (source, signature) => {
  const start = source.indexOf(signature);
  assert.ok(start >= 0, `${signature} is defined`);
  return source.slice(start, source.indexOf('\n}\n', start) + 2);
};

test('the probe takes a canvas-png request and asks the canvas for a frame', () => {
  assert.match(probe, /std::string g_canvas_png;/);
  assert.match(probe, /const std::wstring canvas_png = L"canvas-png ";\s*if \(frame && payload\.compare\(0, canvas_png\.size\(\), canvas_png\) == 0\) \{\s*g_canvas_png = boost::nowide::narrow\(payload\.substr\(canvas_png\.size\(\)\)\);/);
  assert.match(probe, /canvas->set_as_dirty\(\);\s*canvas->request_extra_frame\(\);/, 'an idle canvas draws again, so the request is served');
  assert.match(probe, /return !g_canvas_png\.empty\(\);/);
  assert.match(body(probe, 'std::string take_canvas_png_request()'), /path\.swap\(g_canvas_png\);/, 'one request saves one frame');
  for (const declaration of ['bool canvas_png_requested();', 'std::string take_canvas_png_request();']) {
    assert.ok(probeHeader.includes(declaration), `LayoutProbe.hpp declares ${declaration}`);
  }
});

test('the canvas saves the frame after ImGui has drawn and before the swap', () => {
  const render = body(canvas, 'void GLCanvas3D::render(bool only_init)');
  const imgui = render.indexOf('wxGetApp().imgui()->render();');
  const save = render.search(/if \(LayoutProbe::canvas_png_requested\(\)\)\s*save_frame_png\(LayoutProbe::take_canvas_png_request\(\), get_canvas_size\(\)\);/);
  const swap = render.indexOf('m_canvas->SwapBuffers();');
  assert.ok(imgui >= 0 && save > imgui, 'the saved frame includes the ImGui panels');
  assert.ok(swap > save, 'after the swap the back buffer no longer holds the frame');
  assert.match(canvas, /#include "slic3r\/GUI\/LayoutProbe\.hpp"/);
});

test('the saved frame is upright and never half-written', () => {
  const saver = body(canvas, 'static void save_frame_png(const std::string &path, const Size &size)');
  assert.match(saver, /glReadPixels\(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba\.data\(\)\)/);
  assert.match(saver, /rgba\.data\(\) \+ size_t\(height - 1 - y\) \* size_t\(width\) \* 4/, 'OpenGL rows run bottom-up');
  assert.match(saver, /image\.SaveFile\(part, wxBITMAP_TYPE_PNG\) && wxRenameFile\(part, target, true\)/,
    'a driver waiting for the file sees it only once it is complete');
});
