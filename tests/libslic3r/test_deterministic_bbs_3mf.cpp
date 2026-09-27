#include <catch_main.hpp>

#include "libslic3r/Format/bbs_3mf.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/TriangleMesh.hpp"

#include <boost/filesystem/operations.hpp>

#include <fstream>
#include <iterator>
#include <type_traits>
#include <vector>

using namespace Slic3r;

namespace {

struct ScopedTestDirectory
{
    ScopedTestDirectory()
        : path(boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("bbs-3mf-deterministic-%%%%-%%%%-%%%%"))
    {
        boost::filesystem::create_directories(path);
    }

    ~ScopedTestDirectory()
    {
        boost::system::error_code error;
        boost::filesystem::remove_all(path, error);
    }

    boost::filesystem::path path;
};

std::vector<char> read_binary_file(const boost::filesystem::path &path)
{
    std::ifstream input(path.string(), std::ios::binary);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("Deterministic BBS 3MF exports are byte stable", "[3mf][bbs][deterministic]")
{
    using StrategyValue = std::underlying_type_t<SaveStrategy>;
    REQUIRE((static_cast<StrategyValue>(SaveStrategy::Deterministic) &
             static_cast<StrategyValue>(SaveStrategy::SplitModel)) == 0);

    ScopedTestDirectory temporary;
    Model               model;
    model.set_backup_path((temporary.path / "model-backup").string());
    model.add_object("first", "", make_cube(20., 20., 20.));
    model.add_object("second", "", make_cube(8., 13., 21.));
    REQUIRE(model.add_default_instances());

    DynamicPrintConfig config;
    auto export_project = [&model, &config](const boost::filesystem::path &output) {
        const std::string output_path = output.string();
        StoreParams       params;
        params.path     = output_path.c_str();
        params.model    = &model;
        params.config   = &config;
        params.strategy = SaveStrategy::Silence | SaveStrategy::SplitModel | SaveStrategy::ShareMesh |
                          SaveStrategy::SkipThumbnails | SaveStrategy::Deterministic;
        return store_bbs_3mf(params);
    };

    const boost::filesystem::path first_export  = temporary.path / "first.3mf";
    const boost::filesystem::path second_export = temporary.path / "second.3mf";
    REQUIRE(export_project(first_export));
    REQUIRE(export_project(second_export));

    const std::vector<char> first_bytes  = read_binary_file(first_export);
    const std::vector<char> second_bytes = read_binary_file(second_export);
    REQUIRE_FALSE(first_bytes.empty());
    REQUIRE(first_bytes == second_bytes);
}
