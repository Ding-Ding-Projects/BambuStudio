#include <catch2/catch.hpp>
#include "slic3r/GUI/SliceToolpathSnapshot.hpp"

using namespace Slic3r;
using namespace Slic3r::GUI;

TEST_CASE("Finalized slice playback preserves travel gaps, arcs and sequential layer resets", "[slice-playback]")
{
    GCodeProcessorResult result{};
    result.id = 7;
    auto append = [&](EMoveType type, float x, float y, float z, int object, unsigned tool) {
        auto& move = result.moves.emplace_back();
        move.type = type;
        move.position = Vec3f(x, y, z);
        move.print_z = z;
        move.object_label_id = object;
        move.extruder_id = static_cast<unsigned char>(tool);
        move.gcode_id = static_cast<unsigned>(result.moves.size());
    };
    append(EMoveType::Travel, 1, 2, 0.25f, -1, 0);
    append(EMoveType::Extrude, 4, 2, 0.25f, 15, 0);
    append(EMoveType::Travel, 4, 2, 1.50f, 15, 0);
    append(EMoveType::Extrude, 7, 2, 1.50f, 15, 0);
    append(EMoveType::Travel, 101, 9, 0.2f, -1, 1);
    append(EMoveType::Extrude, 102, 8, 0.2f, 9, 1);
    result.moves.back().move_path_type = EMovePathType::Arc_move_cw;
    result.moves.back().interpolation_points.emplace_back(102, 9, 0.2f);

    SliceToolpathKey key{3, 2, 7};
    REQUIRE_FALSE(make_slice_toolpath_snapshot({3, 2, 8}, result));
    auto snapshot = make_slice_toolpath_snapshot(key, result);
    REQUIRE(snapshot);
    REQUIRE(snapshot->key == key);
    REQUIRE(snapshot->source_segment_count == 4);
    REQUIRE(snapshot->segments.size() == 4);
    CHECK(snapshot->segments[1].print_z == 1.50f);
    CHECK(snapshot->segments[2].print_z == 0.2f);
    CHECK(snapshot->segments[2].object_label_id == 9);
    CHECK(snapshot->segments[2].extruder_id == 1);
    CHECK(snapshot->segments[2].x1 == 101);
    CHECK(snapshot->segments[2].x2 == 102);
    CHECK(snapshot->segments[3].y1 == 9);
    CHECK(snapshot->segments[3].y2 == 8);
    CHECK(snapshot->segments[2].gcode_id == snapshot->segments[3].gcode_id);
    CHECK(snapshot->min_x == 1);
    CHECK(snapshot->max_x == 102);
    CHECK_FALSE(make_slice_toolpath_snapshot(key, result, 3));
    result.moves.clear();
    CHECK(snapshot->segments.size() == 4);
    CHECK_FALSE(make_slice_toolpath_snapshot(key, result));
}

TEST_CASE("Bounded slice playback samples source edges without bridging omitted moves", "[slice-playback]")
{
    GCodeProcessorResult result{};
    result.id = 1;
    for (int i = 0; i < 20; ++i) {
        auto& move = result.moves.emplace_back();
        move.position = Vec3f(static_cast<float>(i), 0, 0.2f);
        move.print_z = 0.2f;
        move.type = i == 0 ? EMoveType::Travel : EMoveType::Extrude;
        move.object_label_id = i < 10 ? 3 : 4;
    }
    auto snapshot = make_slice_toolpath_snapshot({2, 0, 1}, result, 6);
    REQUIRE(snapshot);
    REQUIRE(snapshot->segments.size() == 6);
    CHECK(snapshot->source_segment_count == 19);
    CHECK(snapshot->segments.front().x1 == 0);
    CHECK(snapshot->segments.back().x2 == 19);
    for (const auto& edge : snapshot->segments)
        CHECK(edge.x2 == edge.x1 + 1);
}
