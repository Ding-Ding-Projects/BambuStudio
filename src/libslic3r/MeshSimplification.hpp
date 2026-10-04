#ifndef slic3r_MeshSimplification_hpp_
#define slic3r_MeshSimplification_hpp_

#include <cstddef>
#include <cstdint>
#include <functional>

#include "TriangleMesh.hpp"

namespace Slic3r {

inline constexpr size_t automatic_mesh_simplification_threshold = 1000000;

inline bool should_simplify_mesh(size_t triangle_count)
{
    return triangle_count >= automatic_mesh_simplification_threshold;
}

struct MeshSimplificationOptions {
    uint32_t target_triangle_count = 0;
    // The highest-detail setting of the interactive simplification tool.
    float max_error = 0.001f;
};

// Simplifies in place and returns whether the triangle count decreased.
// Cancellation callbacks may throw, and their exception is propagated unchanged.
// Callers requiring transactional replacement must pass a working copy.
// Coordinates use the existing quadric engine without rescaling: exact bounds,
// volume, topology and dimensions are not guaranteed to remain unchanged.
bool simplify_mesh(indexed_triangle_set &mesh,
                   const MeshSimplificationOptions &options = {},
                   std::function<void()> throw_on_cancel = {},
                   std::function<void(int)> progress = {});

} // namespace Slic3r

#endif
