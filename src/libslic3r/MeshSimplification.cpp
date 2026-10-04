#include "MeshSimplification.hpp"
#include "QuadricEdgeCollapse.hpp"
#include <cmath>

namespace Slic3r {

bool simplify_mesh(indexed_triangle_set &mesh,
                   const MeshSimplificationOptions &options,
                   std::function<void()> throw_on_cancel,
                   std::function<void(int)> progress)
{
    if (throw_on_cancel)
        throw_on_cancel();
    if (progress)
        progress(0);

    const size_t original_count = mesh.indices.size();
    float max_error = options.max_error;
    its_quadric_edge_collapse(mesh, options.target_triangle_count, &max_error,
                             throw_on_cancel, progress);

    if (throw_on_cancel)
        throw_on_cancel();
    if (progress)
        progress(100);
    return mesh.indices.size() < original_count;
}

bool retain_mesh_extents(indexed_triangle_set &mesh, const BoundingBoxf3 &original)
{
    BoundingBoxf3 reduced;
    for (const auto &vertex : mesh.vertices) {
        if (!vertex.allFinite())
            return false;
        reduced.merge(vertex.cast<double>());
    }
    if (!original.defined || !reduced.defined ||
        !original.min.allFinite() || !original.max.allFinite())
        return false;

    const Vec3d original_size = original.size();
    const Vec3d reduced_size = reduced.size();
    for (int axis = 0; axis < 3; ++axis)
        if (original_size[axis] < 0. || !std::isfinite(original_size[axis]) ||
            !std::isfinite(reduced_size[axis]) ||
            (reduced_size[axis] <= 1e-12 && original_size[axis] > 0.))
            return false;

    for (auto &vertex : mesh.vertices) {
        Vec3d point = vertex.cast<double>();
        for (int axis = 0; axis < 3; ++axis)
            point[axis] = original_size[axis] == 0. ? original.min[axis] :
                original.min[axis] + (point[axis] - reduced.min[axis]) *
                    original_size[axis] / reduced_size[axis];
        vertex = point.cast<float>();
    }
    return true;
}

} // namespace Slic3r
