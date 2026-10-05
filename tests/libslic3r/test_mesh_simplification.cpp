#include <catch2/catch.hpp>

#include "libslic3r/MeshSimplification.hpp"
#include <limits>
#include <stdexcept>

using namespace Slic3r;

TEST_CASE("Automatic mesh simplification includes the threshold", "[mesh_simplification]")
{
    CHECK_FALSE(should_simplify_mesh(999999));
    CHECK(should_simplify_mesh(1000000));
    CHECK(should_simplify_mesh(1000001));
    CHECK(MeshSimplificationOptions{}.max_error == Approx(0.001f));
}

TEST_CASE("Mesh simplification handles empty input and reports completion", "[mesh_simplification]")
{
    indexed_triangle_set mesh;
    std::vector<int> progress;
    CHECK_FALSE(simplify_mesh(mesh, {}, {}, [&](int percent) { progress.push_back(percent); }));
    REQUIRE(progress.size() == 2);
    CHECK(progress.front() == 0);
    CHECK(progress.back() == 100);
}

TEST_CASE("Mesh simplification propagates cancellation before mutation", "[mesh_simplification]")
{
    auto mesh = its_make_cube(10., 10., 10.);
    const auto original = mesh;
    CHECK_THROWS_AS(simplify_mesh(mesh, {}, [] { throw std::runtime_error("cancelled"); }), std::runtime_error);
    CHECK(mesh.indices == original.indices);
    CHECK(mesh.vertices == original.vertices);
}

TEST_CASE("Mesh simplification shares the interactive quadric reduction engine", "[mesh_simplification]")
{
    indexed_triangle_set mesh;
    mesh.vertices = {Vec3f(-1.f, 0.f, 0.f), Vec3f(0.f, 1.f, 0.f),
                     Vec3f(1.f, 0.f, 0.f), Vec3f(0.f, 0.f, 1.f), Vec3f(0.9f, .1f, -.1f)};
    mesh.indices = {Vec3i(1, 0, 3), Vec3i(2, 1, 3), Vec3i(0, 2, 3),
                    Vec3i(0, 1, 4), Vec3i(1, 2, 4), Vec3i(2, 0, 4)};
    MeshSimplificationOptions options;
    options.target_triangle_count = 5;
    options.max_error = std::numeric_limits<float>::max();
    int progress = -1;
    CHECK(simplify_mesh(mesh, options, {}, [&](int percent) { progress = percent; }));
    CHECK(mesh.indices.size() == 4);
    CHECK(mesh.vertices.size() == 4);
    CHECK(progress == 100);
}

TEST_CASE("Extent retention maps translated reduced bounds to original dimensions", "[mesh_simplification]")
{
    indexed_triangle_set mesh;
    mesh.vertices = {Vec3f(2.f, 3.f, 4.f), Vec3f(4.f, 7.f, 10.f), Vec3f(3.f, 5.f, 7.f)};
    BoundingBoxf3 original;
    original.merge(Vec3d(-10., 20., 30.));
    original.merge(Vec3d(10., 60., 90.));
    REQUIRE(retain_mesh_extents(mesh, original));
    CHECK(mesh.vertices[0] == Vec3f(-10.f, 20.f, 30.f));
    CHECK(mesh.vertices[1] == Vec3f(10.f, 60.f, 90.f));
    CHECK(mesh.vertices[2] == Vec3f(0.f, 40.f, 60.f));
}

TEST_CASE("Extent retention handles flat axes and rejects collapsed dimensions", "[mesh_simplification]")
{
    indexed_triangle_set mesh;
    mesh.vertices = {Vec3f(1.f, 2.f, 9.f), Vec3f(3.f, 6.f, 9.f)};
    BoundingBoxf3 original;
    original.merge(Vec3d(0., 0., 5.));
    original.merge(Vec3d(10., 20., 5.));
    SECTION("An original flat axis returns to its original plane") {
        REQUIRE(retain_mesh_extents(mesh, original));
        CHECK(mesh.vertices[0] == Vec3f(0.f, 0.f, 5.f));
        CHECK(mesh.vertices[1] == Vec3f(10.f, 20.f, 5.f));
    }
    SECTION("A collapsed nonzero original dimension leaves the input unchanged") {
        original.max.z() = 15.;
        const auto vertices = mesh.vertices;
        CHECK_FALSE(retain_mesh_extents(mesh, original));
        CHECK(mesh.vertices == vertices);
    }
    SECTION("Undefined original bounds leave the input unchanged") {
        const auto vertices = mesh.vertices;
        CHECK_FALSE(retain_mesh_extents(mesh, BoundingBoxf3{}));
        CHECK(mesh.vertices == vertices);
    }
}
