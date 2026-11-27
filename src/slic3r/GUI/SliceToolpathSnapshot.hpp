#pragma once

#include "libslic3r/GCode/GCodeProcessor.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Slic3r::GUI {

struct SliceToolpathKey {
    std::uint64_t generation;
    int plate_index;
    unsigned result_id;
    bool operator==(const SliceToolpathKey& other) const {
        return generation == other.generation && plate_index == other.plate_index && result_id == other.result_id;
    }
};

struct SliceToolpathSegment {
    float x1, y1, z1, x2, y2, z2, print_z;
    int object_label_id;
    unsigned extruder_id, gcode_id;
};

struct SliceToolpathSnapshot {
    SliceToolpathKey key{};
    std::vector<SliceToolpathSegment> segments;
    std::size_t source_segment_count{};
    float min_x{}, min_y{}, max_x{}, max_y{};
};

// Read a stable, finalized result or a writer-owned prefix synchronously. A
// snapshot owns all its coordinates; painting never touches mutable slicer data.
inline std::shared_ptr<const SliceToolpathSnapshot>
make_slice_toolpath_snapshot(SliceToolpathKey key, const GCodeProcessorResult& result,
                             std::size_t segment_limit = 32768)
{
    if (key.generation == 0 || key.plate_index < 0 || key.result_id != result.id || !segment_limit)
        return {};
    auto snapshot = std::make_shared<SliceToolpathSnapshot>();
    snapshot->key = key;

    // Match the native viewer's arc polyline: the prior actual move endpoint,
    // any interpolation vertices, and the actual final endpoint. Never join
    // disjoint retained segments, bridge travel, or sort sequential prints by Z.
    auto visit = [&](auto&& consume) {
        SliceToolpathSegment previous{};
        bool pending = false;
        bool first_in_run = true;
        for (std::size_t i = 1; i < result.moves.size(); ++i) {
            const auto& move = result.moves[i];
            if (move.type != EMoveType::Extrude)
                continue;
            Vec3f from = result.moves[i - 1].position;
            auto edge = [&](const Vec3f& to) {
                if (!from.allFinite() || !to.allFinite() || !std::isfinite(move.print_z))
                    return false;
                if (from != to) {
                    SliceToolpathSegment current{from.x(), from.y(), from.z(), to.x(), to.y(), to.z(),
                                                move.print_z, move.object_label_id, move.extruder_id, move.gcode_id};
                    bool new_run = !pending || previous.print_z != current.print_z ||
                                   previous.object_label_id != current.object_label_id ||
                                   previous.extruder_id != current.extruder_id;
                    if (pending)
                        consume(previous, first_in_run || new_run);
                    previous = current;
                    pending = true;
                    first_in_run = new_run;
                }
                from = to;
                return true;
            };
            if (move.is_arc_move_with_interpolation_points())
                for (const Vec3f& point : move.interpolation_points)
                    if (!edge(point)) return false;
            if (!edge(move.position)) return false;
        }
        if (pending) consume(previous, true);
        return true;
    };

    std::size_t boundaries = 0;
    if (!visit([&](const auto& s, bool boundary) {
        if (snapshot->source_segment_count++ == 0) {
            snapshot->min_x = snapshot->max_x = s.x1;
            snapshot->min_y = snapshot->max_y = s.y1;
        }
        snapshot->min_x = std::min({snapshot->min_x, s.x1, s.x2});
        snapshot->max_x = std::max({snapshot->max_x, s.x1, s.x2});
        snapshot->min_y = std::min({snapshot->min_y, s.y1, s.y2});
        snapshot->max_y = std::max({snapshot->max_y, s.y1, s.y2});
        if (boundary) ++boundaries;
    }) || !boundaries || boundaries > segment_limit)
        return {};

    const auto keep = std::min(segment_limit, snapshot->source_segment_count);
    const auto interior = snapshot->source_segment_count - boundaries;
    const auto interior_budget = keep - boundaries;
    snapshot->segments.reserve(keep);
    std::size_t carry = 0;
    visit([&](const auto& s, bool boundary) {
        // Reserve the first and last segment of each actual Z/object/tool run.
        // Any additional samples are real edges distributed across its interior.
        if (!boundary) {
            if (carry < interior - interior_budget) {
                carry += interior_budget;
                return;
            }
            carry -= interior - interior_budget;
        }
        snapshot->segments.push_back(s);
    });
    return snapshot;
}

} // namespace Slic3r::GUI
