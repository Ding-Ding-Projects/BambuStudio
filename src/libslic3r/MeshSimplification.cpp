#include "MeshSimplification.hpp"
#include "QuadricEdgeCollapse.hpp"

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

} // namespace Slic3r
