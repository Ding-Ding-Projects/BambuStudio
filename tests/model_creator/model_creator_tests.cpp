#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "slic3r/GUI/ModelCreator/SceneSpec.hpp"
#include <nlohmann/json.hpp>

using namespace Slic3r::GUI::ModelCreator;

TEST_CASE("Scene specification validates dimensions and rejects executable fields")
{
    const auto valid = parse_scene(R"({"version":1,"title":"Holder","parts":[{"kind":"box","size":[20,30,4],"position":[0,0,2]},{"kind":"cylinder","height":8,"radius":3,"facets":32,"position":[0,0,8]}]})");
    REQUIRE(valid);
    REQUIRE(valid.scene.parts.size() == 2);
    REQUIRE_FALSE(parse_scene(R"({"version":1,"title":"Bad","parts":[{"kind":"box","size":[0,30,4],"position":[0,0,2]}]})"));
    REQUIRE_FALSE(parse_scene(R"({"version":1,"title":"Bad","parts":[{"kind":"box","size":[20,30,4],"position":[0,0,2],"script":"erase"}]})"));
    REQUIRE_FALSE(parse_scene(R"({"version":2,"title":"Bad","parts":[]})"));
    REQUIRE_FALSE(parse_scene(R"({"version":1,"title":"Bad","parts":[{"kind":"box","size":[20,30,4],"position":[0,0,2],"url":"https://example.invalid"}]})"));
    REQUIRE_FALSE(parse_scene(R"({"version":1,"title":"Below bed","parts":[{"kind":"box","size":[20,30,4],"position":[0,0,0]}]})"));
}

TEST_CASE("Emitters use only validated values and fixed operators")
{
    const auto parsed = parse_scene(R"({"version":1,"title":"Solid","parts":[{"kind":"sphere","radius":12,"facets":24,"position":[0,0,12]}]})");
    REQUIRE(parsed);
    const auto scad = emit_openscad(parsed.scene);
    const auto blend = emit_blender(parsed.scene);
    REQUIRE(scad.find("sphere(r=12.000, $fn=24)") != std::string::npos);
    REQUIRE(blend.find("primitive_uv_sphere_add") != std::string::npos);
    REQUIRE(scad.find("Solid") == std::string::npos);
    REQUIRE(blend.find("Solid") == std::string::npos);
}

TEST_CASE("Shared provider schema is a versioned closed contract")
{
    const auto schema = scene_json_schema();
    REQUIRE(schema.find("\"version\"") != std::string::npos);
    REQUIRE(schema.find("\"additionalProperties\":false") != std::string::npos);
    REQUIRE(schema.find("\"anyOf\"") != std::string::npos);
    const auto parsed = nlohmann::json::parse(schema, nullptr, false);
    REQUIRE_FALSE(parsed.is_discarded());
    REQUIRE(parsed.at("properties").at("version").at("enum").at(0) == 1);
}
