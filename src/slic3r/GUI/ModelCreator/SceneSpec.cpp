#include "SceneSpec.hpp"

#include <nlohmann/json.hpp>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Slic3r::GUI::ModelCreator {
namespace {
using json = nlohmann::json;

bool vector3(const json &value, std::array<double, 3> &out, double min, double max)
{
    if (!value.is_array() || value.size() != 3) return false;
    for (size_t i = 0; i < 3; ++i) {
        if (!value[i].is_number()) return false;
        out[i] = value[i].get<double>();
        if (!std::isfinite(out[i]) || out[i] < min || out[i] > max) return false;
    }
    return true;
}

bool keys_only(const json &value, std::initializer_list<const char *> keys)
{
    if (!value.is_object()) return false;
    for (auto it = value.begin(); it != value.end(); ++it) {
        bool known = false;
        for (auto key : keys) known |= it.key() == key;
        if (!known) return false;
    }
    return true;
}

std::string triple(const std::array<double, 3> &v)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << v[0] << ", " << v[1] << ", " << v[2];
    return out.str();
}
}

ParseResult parse_scene(const std::string &text)
{
    ParseResult result;
    if (text.size() > 65536) { result.error = "Scene specification exceeds 64 KiB"; return result; }
    auto data = json::parse(text, nullptr, false);
    if (data.is_discarded() || !keys_only(data, {"version", "title", "parts"}) ||
        !data.contains("version") || !data["version"].is_number_integer() || data["version"] != 1 ||
        !data.contains("title") || !data["title"].is_string() ||
        !data.contains("parts") || !data["parts"].is_array()) {
        result.error = "Expected a version 1 scene with title and parts"; return result;
    }
    result.scene.title = data["title"].get<std::string>();
    if (result.scene.title.empty() || result.scene.title.size() > 80 ||
        result.scene.title.find_first_of("\r\n\t") != std::string::npos ||
        data["parts"].empty() || data["parts"].size() > 32) {
        result.error = "Title or part count is outside allowed bounds"; return result;
    }
    for (const auto &part : data["parts"]) {
        if (!keys_only(part, {"kind", "size", "position", "radius", "height", "facets"}) ||
            !part.contains("kind") || !part["kind"].is_string() ||
            !part.contains("position")) {
            result.error = "Invalid part fields"; return result;
        }
        Primitive item;
        if (!vector3(part["position"], item.position, -500, 500)) {
            result.error = "Part position is outside the 500 mm workspace"; return result;
        }
        const auto kind = part["kind"].get<std::string>();
        if (kind == "box") {
            item.kind = Primitive::Kind::Box;
            if (!part.contains("size") || part.contains("radius") || part.contains("height") || part.contains("facets") ||
                !vector3(part["size"], item.size, 0.5, 500)) {
                result.error = "Invalid box dimensions"; return result;
            }
        } else if (kind == "cylinder" || kind == "sphere") {
            item.kind = kind == "cylinder" ? Primitive::Kind::Cylinder : Primitive::Kind::Sphere;
            if (!part.contains("radius") || !part["radius"].is_number() ||
                !std::isfinite(part["radius"].get<double>()) ||
                part["radius"].get<double>() < 0.25 || part["radius"].get<double>() > 250 ||
                part.contains("size") || (kind == "sphere" && part.contains("height"))) {
                result.error = "Invalid radial dimensions"; return result;
            }
            item.radius = part["radius"].get<double>();
            if (kind == "cylinder") {
                if (!part.contains("height") || !part["height"].is_number() ||
                    !std::isfinite(part["height"].get<double>()) ||
                    part["height"].get<double>() < 0.5 || part["height"].get<double>() > 500) {
                    result.error = "Invalid cylinder height"; return result;
                }
                item.height = part["height"].get<double>();
            }
            if (part.contains("facets")) {
                if (!part["facets"].is_number_integer() || part["facets"].get<int64_t>() < 12 ||
                    part["facets"].get<int64_t>() > 128) {
                    result.error = "Facets must be between 12 and 128"; return result;
                }
                item.facets = part["facets"].get<int>();
            }
        } else { result.error = "Unsupported primitive kind"; return result; }
        const double half_height = item.kind == Primitive::Kind::Sphere ? item.radius :
                                   item.kind == Primitive::Kind::Cylinder ? item.height / 2 : item.size[2] / 2;
        if (item.position[2] - half_height < -0.001 || item.position[2] + half_height > 500) {
            result.error = "Every solid must rest above the build plate and fit the 500 mm workspace";
            return result;
        }
        result.scene.parts.push_back(item);
    }
    return result;
}

std::string emit_openscad(const SceneSpec &scene)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << "// Generated from a validated scene specification.\nunion() {\n";
    for (const auto &p : scene.parts) {
        out << "  translate([" << triple(p.position) << "]) ";
        if (p.kind == Primitive::Kind::Box) out << "cube([" << triple(p.size) << "], center=true);\n";
        else if (p.kind == Primitive::Kind::Cylinder)
            out << "cylinder(h=" << p.height << ", r=" << p.radius << ", center=true, $fn=" << p.facets << ");\n";
        else out << "sphere(r=" << p.radius << ", $fn=" << p.facets << ");\n";
    }
    return out.str() + "}\n";
}

std::string emit_blender(const SceneSpec &scene)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3)
        << "import bpy\n"
        << "bpy.ops.object.select_all(action='SELECT')\n"
        << "bpy.ops.object.delete(use_global=False)\n";
    for (const auto &p : scene.parts) {
        if (p.kind == Primitive::Kind::Box)
            out << "bpy.ops.mesh.primitive_cube_add(size=1, location=(" << triple(p.position) << "))\n"
                << "bpy.context.object.dimensions = (" << triple(p.size) << ")\n";
        else if (p.kind == Primitive::Kind::Cylinder)
            out << "bpy.ops.mesh.primitive_cylinder_add(vertices=" << p.facets << ", radius=" << p.radius
                << ", depth=" << p.height << ", location=(" << triple(p.position) << "))\n";
        else out << "bpy.ops.mesh.primitive_uv_sphere_add(segments=" << p.facets << ", ring_count="
                 << p.facets / 2 << ", radius=" << p.radius << ", location=(" << triple(p.position) << "))\n";
        out << "bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)\n";
    }
    out << "bpy.ops.object.select_all(action='SELECT')\n"
        << "bpy.ops.wm.stl_export(filepath=__import__('sys').argv[-1], export_selected_objects=True)\n";
    return out.str();
}

std::string scene_prompt(const std::string &request, const std::string &revision_note)
{
    const std::string safe_request = request.substr(0, 4000);
    const std::string safe_revision = revision_note.substr(0, 2000);
    return "Return only JSON for a version 1 3D scene, no markdown or executable code. "
           "Schema: {\"version\":1,\"title\":string,\"parts\":[{"
           "\"kind\":\"box\"|\"cylinder\"|\"sphere\",\"position\":[x,y,z],"
           "\"size\":[x,y,z] for box, \"height\":number for cylinder, "
           "\"radius\":number for cylinder/sphere,"
           "\"facets\":integer optional for cylinder/sphere}]}. "
           "Use millimeters, 1 to 32 solid primitives, positive printable dimensions. "
           "Request: " + safe_request + "\nRevision: " + safe_revision;
}

std::string scene_json_schema()
{
    return R"({"type":"object","properties":{"version":{"type":"integer","enum":[1]},"title":{"type":"string"},"parts":{"type":"array","items":{"anyOf":[{"type":"object","properties":{"kind":{"type":"string","enum":["box"]},"size":{"type":"array","items":{"type":"number"}},"position":{"type":"array","items":{"type":"number"}}},"required":["kind","size","position"],"additionalProperties":false},{"type":"object","properties":{"kind":{"type":"string","enum":["cylinder"]},"height":{"type":"number"},"radius":{"type":"number"},"position":{"type":"array","items":{"type":"number"}},"facets":{"type":"integer"}},"required":["kind","height","radius","position","facets"],"additionalProperties":false},{"type":"object","properties":{"kind":{"type":"string","enum":["sphere"]},"radius":{"type":"number"},"position":{"type":"array","items":{"type":"number"}},"facets":{"type":"integer"}},"required":["kind","radius","position","facets"],"additionalProperties":false}]} }},"required":["version","title","parts"],"additionalProperties":false})";
}

} // namespace Slic3r::GUI::ModelCreator
