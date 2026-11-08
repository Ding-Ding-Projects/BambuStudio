#include "ExtensionContext.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Slic3r { namespace GUI { namespace Extensions {

namespace fs = std::filesystem;
using json   = nlohmann::json;

namespace {

constexpr double kPi = 3.14159265358979323846;

double round_to(double value, int places)
{
    const double scale = std::pow(10.0, places);
    return std::round(value * scale) / scale;
}

const char *warning_level(int level) { return level <= 0 ? "tip" : level == 1 ? "warning" : "error"; }

bool is_comment(const std::string &line)
{
    return !line.empty() && line.front() == ';' && line.find_first_of("\r\n") == std::string::npos && line.find('\0') == std::string::npos;
}

bool move_over(const fs::path &from, const fs::path &to)
{
#ifdef _WIN32
    return ::MoveFileExW(from.wstring().c_str(), to.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return std::rename(from.c_str(), to.c_str()) == 0;
#endif
}

} // namespace

double grams(const FilamentUse &use) { return use.density > 0 ? use.volume_mm3 * use.density / 1000.0 : 0.0; }

double meters(const FilamentUse &use)
{
    const double radius = use.diameter / 2.0;
    return radius > 0 ? use.volume_mm3 / (kPi * radius * radius) / 1000.0 : 0.0;
}

json slice_section(const SliceFacts &facts)
{
    json   filaments = json::array();
    double total_g = 0, total_m = 0, total_cost = 0;
    bool   any_cost = false;
    for (const FilamentUse &use : facts.filaments) {
        const double g = grams(use), m = meters(use);
        json         row{{"slot", use.slot}, {"type", use.type}, {"grams", round_to(g, 2)}, {"meters", round_to(m, 2)}};
        if (!use.color.empty()) row["color"] = use.color;
        if (use.cost_per_kg > 0) {
            row["cost"] = round_to(g / 1000.0 * use.cost_per_kg, 2);
            total_cost += g / 1000.0 * use.cost_per_kg;
            any_cost = true;
        }
        total_g += g;
        total_m += m;
        filaments.push_back(std::move(row));
    }
    json warnings = json::array();
    for (const SliceWarningFact &w : facts.warnings) {
        json row{{"level", warning_level(w.level)}};
        if (!w.code.empty()) row["code"] = w.code;
        if (!w.message.empty()) row["message"] = w.message;
        warnings.push_back(std::move(row));
    }
    json out{{"plate", facts.plate},
             {"plateName", facts.plate_name},
             {"seconds", std::llround(std::max(0.0, facts.seconds))},
             {"prepareSeconds", std::llround(std::max(0.0, facts.prepare_seconds))},
             {"layers", std::max(0, facts.layers)},
             {"bedType", facts.bed_type},
             {"filamentChanges", std::max(0, facts.filament_changes)},
             {"filaments", filaments},
             {"totalGrams", round_to(total_g, 2)},
             {"totalMeters", round_to(total_m, 2)},
             {"warnings", warnings}};
    if (any_cost) out["totalCost"] = round_to(total_cost, 2);
    if (!facts.file.empty()) out["file"] = fs::u8path(facts.file).filename().u8string();
    return out;
}

json printer_section(const PrinterFacts &facts)
{
    json nozzles = json::array();
    for (double d : facts.nozzles) nozzles.push_back(round_to(d, 2));
    return json{{"preset", facts.preset}, {"model", facts.model}, {"nozzles", nozzles}, {"filamentPresets", facts.filament_presets}};
}

json model_section(const std::vector<ModelObjectFacts> &objects, int plate)
{
    json        list     = json::array();
    std::size_t triangles = 0;
    int         selected = 0;
    for (const ModelObjectFacts &o : objects) {
        if (list.size() >= 200) break;
        list.push_back(json{{"name", o.name},
                            {"instances", o.instances},
                            {"triangles", o.triangles},
                            {"size", json::array({round_to(o.size[0], 2), round_to(o.size[1], 2), round_to(o.size[2], 2)})},
                            {"selected", o.selected}});
        triangles += o.triangles;
        if (o.selected) ++selected;
    }
    return json{{"plate", plate}, {"objects", list}, {"objectCount", objects.size()}, {"selectedCount", selected}, {"triangles", triangles}};
}

json sample_context(Hook hook)
{
    SliceFacts slice;
    slice.plate      = 1;
    slice.plate_name = "Sample plate";
    slice.seconds    = 5025;
    slice.prepare_seconds = 360;
    slice.layers     = 245;
    slice.bed_type   = "textured_plate";
    slice.filament_changes = 4;
    slice.filaments  = {{1, "PLA", "#00AE42", 12000.0, 1.24, 1.75, 24.99}, {2, "PETG", "#FFFFFF", 3500.0, 1.27, 1.75, 0}};
    slice.warnings   = {{1, "", "Sample warning: a floating region was detected"}};
    if (hook == Hook::GcodeExported) slice.file = "Sample plate.gcode";
    json context{{"slice", slice_section(slice)}};
    context["slice"]["sample"] = true;
    context["printer"]         = printer_section(PrinterFacts{"Sample printer 0.4 nozzle", "Sample printer", {0.4}, {"Sample PLA", "Sample PETG"}});
    context["printer"]["sample"] = true;
    context["model"] = model_section({{"Sample bracket", 2, 12480, {{42.0, 18.5, 9.0}}, true}, {"Sample hook", 1, 3302, {{20.0, 20.0, 35.0}}, false}}, 1);
    context["model"]["sample"] = true;
    return context;
}

// ---------------------------------------------------------------- G-code comment lines

bool add_comment_lines(const fs::path &gcode, const std::vector<CommentBlock> &blocks, std::string &problem)
{
    problem.clear();
    std::vector<std::string> head, tail;
    for (const CommentBlock &b : blocks) {
        for (const std::vector<std::string> *lines : {&b.prepend, &b.append})
            for (const std::string &line : *lines)
                if (!is_comment(line)) {
                    problem = "not-comment";
                    return false;
                }
        const std::string intro = "; added by extension " + b.extension + " " + b.version;
        if (!b.prepend.empty()) {
            head.push_back(intro);
            head.insert(head.end(), b.prepend.begin(), b.prepend.end());
        }
        if (!b.append.empty()) {
            tail.push_back(intro);
            tail.insert(tail.end(), b.append.begin(), b.append.end());
        }
    }
    if (head.empty() && tail.empty()) return true;

    std::error_code ec;
    const auto      original_size = fs::file_size(gcode, ec);
    std::ifstream   in(gcode, std::ios::binary);
    if (ec || !in) {
        problem = "unreadable";
        return false;
    }
    std::string first(std::size_t(256) * 1024, '\0');
    in.read(&first[0], std::streamsize(first.size()));
    first.resize(std::size_t(in.gcount()));

    const std::size_t lf  = first.find('\n');
    const std::string eol = lf != std::string::npos && lf > 0 && first[lf - 1] == '\r' ? "\r\n" : "\n";
    // After Bambu Studio's header block, which printers read from the very top.
    std::size_t insert_at = 0;
#ifndef SLIC3R_EXTENSION_CONTEXT_NEGATIVE_FIXTURE
    static const std::string kStart = "; HEADER_BLOCK_START", kEnd = "; HEADER_BLOCK_END";
    if (first.compare(0, kStart.size(), kStart) == 0) {
        const std::size_t end = first.find(kEnd);
        if (end != std::string::npos) {
            const std::size_t line_end = first.find('\n', end);
            // A header line cut by the read window: the very start is the only safe place.
            if (line_end != std::string::npos) insert_at = line_end + 1;
            else if (std::uint64_t(first.size()) == original_size) insert_at = first.size();
        }
    }
#endif

    std::string added_head, added_tail;
    for (const std::string &line : head) added_head += line + eol;
    for (const std::string &line : tail) added_tail += line + eol;

    fs::path partial = gcode;
    partial += ".partial";
    fs::remove(partial, ec);
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) {
        problem = "unwritable";
        return false;
    }
    char last = '\n';
    const auto put = [&out, &last](const char *data, std::size_t size) {
        if (size == 0) return;
        out.write(data, std::streamsize(size));
        last = data[size - 1];
    };
    if (insert_at > 0 && first[insert_at - 1] != '\n') {
        // The header's last line has no line ending: end it before ours.
        put(first.data(), insert_at);
        put(eol.data(), eol.size());
    } else {
        put(first.data(), insert_at);
    }
    put(added_head.data(), added_head.size());
    put(first.data() + insert_at, first.size() - insert_at);
    std::string chunk(std::size_t(1) << 20, '\0');
    while (in) {
        in.read(&chunk[0], std::streamsize(chunk.size()));
        put(chunk.data(), std::size_t(in.gcount()));
    }
    std::size_t extra = added_head.size() + added_tail.size() + (insert_at > 0 && first[insert_at - 1] != '\n' ? eol.size() : 0);
    if (!added_tail.empty() && last != '\n') {
        put(eol.data(), eol.size());
        extra += eol.size();
    }
    put(added_tail.data(), added_tail.size());
    out.flush();
    const bool written = bool(out);
    out.close();
    in.close();
    if (!written) {
        fs::remove(partial, ec);
        problem = "write-failed";
        return false;
    }
    const auto size = fs::file_size(partial, ec);
    if (ec || size != original_size + extra) {
        fs::remove(partial, ec);
        problem = "size-mismatch";
        return false;
    }
    if (!move_over(partial, gcode)) {
        fs::remove(partial, ec);
        problem = "rename-failed";
        return false;
    }
    return true;
}

}}} // namespace Slic3r::GUI::Extensions
