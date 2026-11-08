// Standalone assertions for src/slic3r/GUI/Drawing/DrawingModel.cpp: the seven
// pens and the eraser, widths by speed, direction and pressure, smoothing,
// Shift's eight directions, an eraser that removes only what it touches,
// undo and redo, saving with versions, and the SVG export. Compiled and run
// by tests/drawing_model.test.mjs.
#include "src/slic3r/GUI/Drawing/DrawingModel.cpp"

#include <cmath>
#include <cstdio>

using namespace Slic3r::GUI::Drawing;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static Stroke line(Pen pen, float x0, float x1, float y, int n)
{
    StrokeBuilder b;
    b.start(pen, 0x00AE42, 4, 1, 0, x0, y, 0);
    for (int i = 1; i <= n; ++i) b.add(x0 + (x1 - x0) * i / n, y, i * 0.01);
    return b.finish();
}

int main()
{
    CHECK(pens().size() == 8 && info(Pen::Eraser).key == 'E' && info(Pen::Fountain).key == '7');
    CHECK(find_pen("highlighter") && find_pen("highlighter")->default_opacity < 0.5f && !find_pen("crayon"));

    // Fineliner keeps one width; Pen thins as it speeds up; pressure thins too.
    const Stroke fine = line(Pen::Fineliner, 0, 100, 10, 10);
    CHECK(fine.points.size() == 11);
    for (const Point &p : fine.points) CHECK(p.width == 4.0f);
    StrokeBuilder slow, fast;
    slow.start(Pen::Pen, 0, 4, 1, 0, 0, 0, 0);
    slow.add(10, 0, 1.0);
    fast.start(Pen::Pen, 0, 4, 1, 0, 0, 0, 0);
    fast.add(400, 0, 0.1);
    CHECK(slow.stroke().points.back().width > fast.stroke().points.back().width);
    StrokeBuilder light;
    light.start(Pen::Pen, 0, 4, 1, 0, 0, 0, 0);
    light.add(10, 0, 1.0, 0.3f);
    CHECK(light.stroke().points.back().width < slow.stroke().points.back().width);
    // Fountain pen: wide across its nib, thin along it.
    StrokeBuilder across, along;
    across.start(Pen::Fountain, 0, 6, 1, 0, 0, 0, 0);
    across.add(10, -10, 0.1); // up and right, across a 45 degree nib
    along.start(Pen::Fountain, 0, 6, 1, 0, 0, 0, 0);
    along.add(10, 10, 0.1);
    CHECK(across.stroke().points.back().width > along.stroke().points.back().width * 2);

    // Shift: the stroke stays on the nearest of eight directions, start and end only.
    StrokeBuilder locked;
    locked.start(Pen::Pen, 0, 4, 1, 0, 0, 0, 0);
    locked.add(50, 4, 0.1, 1, true);
    locked.add(100, 9, 0.2, 1, true);
    const Stroke ls = locked.finish();
    CHECK(ls.points.size() == 2 && std::fabs(ls.points.back().y) < 1e-3 && std::fabs(ls.points.back().x - 100) < 1);
    float dx = 30, dy = 33;
    lock_direction(dx, dy);
    CHECK(std::fabs(dx - dy) < 1e-3);

    // Smoothing pulls each point toward the last one.
    StrokeBuilder smooth;
    smooth.start(Pen::Pen, 0, 4, 1, 0.5f, 0, 0, 0);
    smooth.add(100, 0, 0.1);
    CHECK(std::fabs(smooth.stroke().points.back().x - 50) < 1e-3);

    // The eraser removes the middle only: one stroke becomes two.
    std::vector<Stroke> strokes{line(Pen::Fineliner, 0, 100, 10, 20), line(Pen::Fineliner, 0, 100, 200, 20)};
    CHECK(erase(strokes, 50, 10, 6));
    CHECK(strokes.size() == 3);
    CHECK(strokes[0].points.back().x < 50 && strokes[1].points.front().x > 50 && strokes[2].points.front().y == 200);
    CHECK(!erase(strokes, 500, 500, 6));

    // Undo and redo.
    History h;
    h.reset({});
    h.push(strokes);
    std::vector<Stroke> more = strokes;
    more.push_back(fine);
    h.push(more);
    CHECK(h.can_undo() && !h.can_redo() && h.current().size() == 4);
    CHECK(h.undo().size() == 3 && h.can_redo());
    CHECK(h.redo().size() == 4);
    h.undo();
    h.push({});
    CHECK(!h.can_redo() && h.current().empty());
    // The limit is the number of changes that can be undone; older ones fall away.
    History small(3);
    small.reset({});
    for (int i = 0; i < 5; ++i) small.push(std::vector<Stroke>(std::size_t(i + 1)));
    int undos = 0;
    while (small.can_undo()) {
        small.undo();
        ++undos;
    }
    CHECK(undos == 3 && small.current().size() == 2);

    // Saving keeps strokes and versions; junk is skipped.
    Document doc;
    doc.width   = 800;
    doc.height  = 600;
    doc.strokes = more;
    std::vector<Version> versions{{1762581600, "Arrow to the brim", strokes}};
    Document             back;
    std::vector<Version> back_versions;
    CHECK(from_json(to_json(doc, versions), back, back_versions));
    CHECK(back.width == 800 && back.strokes.size() == 4 && back.strokes[0].color == 0x00AE42);
    CHECK(back_versions.size() == 1 && back_versions[0].label == "Arrow to the brim" && back_versions[0].strokes.size() == 3);
    CHECK(from_json(R"({"strokes":[{"pen":"eraser","points":[[1,1,1]]},{"pen":"crayon","points":[[1,1,1]]},{"pen":"pen","points":[[1,2,3]]}]})", back, back_versions));
    CHECK(back.strokes.size() == 1);
    CHECK(!from_json("nope", back, back_versions));

    // SVG: what is on the board, erasing included; see-through pens fade as one group.
    Document svgdoc;
    svgdoc.strokes = {line(Pen::Highlighter, 0, 10, 5, 2)};
    svgdoc.strokes[0].opacity = 0.35f;
    const std::string svg = to_svg(svgdoc, 2, "");
    CHECK(svg.find("width=\"2000\"") != std::string::npos && svg.find("<g opacity=\"0.35\"") != std::string::npos);
    CHECK(svg.find("stroke-linecap=\"square\"") != std::string::npos && svg.find("<line") != std::string::npos);
    CHECK(to_svg(svgdoc, 1, "QUJD").find("data:image/png;base64,QUJD") != std::string::npos);

    std::uint32_t rgb = 0;
    CHECK(parse_hex("#ff5a4f", rgb) && rgb == 0xFF5A4F && hex_color(rgb) == "#FF5A4F" && !parse_hex("#12345", rgb));

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("drawing model: all checks passed\n");
    return 0;
}
