#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Slic3r::GUI::ModelCreator {

// Only this bounded data contract may cross from a provider into geometry code.
// Provider-supplied source code, paths, commands and URLs are never executed.
struct Primitive {
    enum class Kind { Box, Cylinder, Sphere };
    Kind kind = Kind::Box;
    std::array<double, 3> size {20, 20, 20};
    std::array<double, 3> position {0, 0, 0};
    double radius = 10;
    double height = 20;
    int facets = 48;
};

struct SceneSpec {
    std::string title;
    std::vector<Primitive> parts;
};

struct ParseResult {
    SceneSpec scene;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};

ParseResult parse_scene(const std::string &text);
std::string emit_openscad(const SceneSpec &scene);
std::string emit_blender(const SceneSpec &scene);
std::string scene_prompt(const std::string &request, const std::string &revision_note = {});
std::string scene_json_schema();

} // namespace Slic3r::GUI::ModelCreator
