#include <catch_main.hpp>

#include "libslic3r/ProjectHistoryManager.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

class TemporaryTree
{
public:
    TemporaryTree()
    {
        static std::atomic<unsigned long long> sequence{0};
        const auto                             stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = fs::temp_directory_path() / ("bambu-project-history-test-" + std::to_string(stamp) + "-" + std::to_string(sequence.fetch_add(1)));
        fs::create_directories(m_path);
    }

    ~TemporaryTree()
    {
        std::error_code error;
        fs::remove_all(m_path, error);
    }

    const fs::path &path() const { return m_path; }

private:
    fs::path m_path;
};

void write_binary(const fs::path &path, const std::vector<unsigned char> &bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(output.good());
}

std::vector<unsigned char> read_binary(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

Slic3r::ProjectHistoryCommitOptions commit_options(const std::string &message, std::int64_t unix_seconds)
{
    Slic3r::ProjectHistoryCommitOptions options;
    options.message      = message;
    options.author_name  = "Bambu Studio test";
    options.author_email = "project-history-test@localhost";
    options.committed_at = std::chrono::system_clock::time_point(std::chrono::seconds(unix_seconds));
    return options;
}

TEST_CASE("Project history stores complete snapshots in an isolated repository", "[project-history]")
{
    TemporaryTree                    temporary;
    const fs::path                   app_data     = temporary.path() / "app-data";
    const fs::path                   project      = temporary.path() / "user-projects" / fs::u8path(u8"project-\u6e2c\u8a66.3mf");
    const fs::path                   snapshot_one = temporary.path() / "completed" / "snapshot-one.3mf";
    const fs::path                   snapshot_two = temporary.path() / "completed" / "snapshot-two.3mf";
    const std::vector<unsigned char> bytes_one{'P', 'K', 3, 4, 0, 1, 2, 3, 0, 255};
    const std::vector<unsigned char> bytes_two{'P', 'K', 3, 4, 9, 8, 7, 0, 6, 5, 4, 3, 2, 1};
    write_binary(snapshot_one, bytes_one);
    write_binary(snapshot_two, bytes_two);

    Slic3r::ProjectHistoryManager manager(app_data);
    REQUIRE(manager.history_root() == fs::absolute(app_data).lexically_normal() / "project_history" / "v1");

    auto first = manager.commit_snapshot(project, snapshot_one, commit_options("Initial project snapshot", 1000)).get();
    INFO(first.error.message);
    REQUIRE(first.ok());
    REQUIRE(first.committed);
    REQUIRE(first.version.has_value());
    REQUIRE(first.version->snapshot_size == bytes_one.size());
    REQUIRE(first.version->message == "Initial project snapshot");
    REQUIRE(first.repository_path.parent_path() == manager.history_root());
    REQUIRE(first.repository_path.filename().string().size() == 64);
    REQUIRE(first.repository_path != project.parent_path());
    REQUIRE(fs::is_directory(first.repository_path));

    auto duplicate = manager.commit_snapshot(project, snapshot_one, commit_options("Duplicate", 1001)).get();
    INFO(duplicate.error.message);
    REQUIRE(duplicate.ok());
    REQUIRE_FALSE(duplicate.committed);
    REQUIRE(duplicate.version.has_value());
    REQUIRE(duplicate.version->commit_id == first.version->commit_id);
    REQUIRE(duplicate.version->message == "Initial project snapshot");

    auto second = manager.commit_snapshot(project, snapshot_two, commit_options("Edited project", 1002)).get();
    REQUIRE(second.ok());
    REQUIRE(second.committed);
    REQUIRE(second.version.has_value());
    REQUIRE(second.version->snapshot_size == bytes_two.size());

    auto latest_only = manager.list_versions(project, 1).get();
    REQUIRE(latest_only.ok());
    REQUIRE(latest_only.versions.size() == 1);
    REQUIRE(latest_only.versions.front().commit_id == second.version->commit_id);

    auto versions = manager.list_versions(project).get();
    REQUIRE(versions.ok());
    REQUIRE(versions.versions.size() == 2);
    REQUIRE(versions.versions[0].commit_id == second.version->commit_id);
    REQUIRE(versions.versions[1].commit_id == first.version->commit_id);

    const fs::path restored = temporary.path() / "restore" / "old-version.3mf";
    auto           restore  = manager.restore_version(project, first.version->commit_id, restored).get();
    REQUIRE(restore.ok());
    REQUIRE(restore.restored_path == restored);
    REQUIRE(restore.bytes_written == bytes_one.size());
    REQUIRE(read_binary(restored) == bytes_one);

    auto refuses_overwrite = manager.restore_version(project, first.version->commit_id, restored).get();
    REQUIRE_FALSE(refuses_overwrite.ok());
    REQUIRE(refuses_overwrite.error.code == Slic3r::ProjectHistoryErrorCode::DestinationExists);

    const fs::path internal_destination = manager.history_root() / "must-not-write.3mf";
    auto           refuses_internal     = manager.restore_version(project, first.version->commit_id, internal_destination).get();
    REQUIRE_FALSE(refuses_internal.ok());
    REQUIRE(refuses_internal.error.code == Slic3r::ProjectHistoryErrorCode::InvalidArgument);
    REQUIRE_FALSE(fs::exists(internal_destination));
}

TEST_CASE("Project history returns safe validation results without creating repositories", "[project-history]")
{
    TemporaryTree                 temporary;
    Slic3r::ProjectHistoryManager manager(temporary.path() / "app-data");

    const fs::path project = temporary.path() / "model.3mf";
    auto           empty   = manager.list_versions(project).get();
    REQUIRE(empty.ok());
    REQUIRE(empty.versions.empty());
    REQUIRE_FALSE(fs::exists(empty.repository_path));

    const fs::path wrong_extension = temporary.path() / "snapshot.zip";
    write_binary(wrong_extension, {'P', 'K', 3, 4});
    auto invalid_snapshot = manager.commit_snapshot(project, wrong_extension).get();
    REQUIRE_FALSE(invalid_snapshot.ok());
    REQUIRE(invalid_snapshot.error.code == Slic3r::ProjectHistoryErrorCode::InvalidArgument);

    auto invalid_project = manager.list_versions(temporary.path() / "model.stl").get();
    REQUIRE_FALSE(invalid_project.ok());
    REQUIRE(invalid_project.error.code == Slic3r::ProjectHistoryErrorCode::InvalidArgument);

    auto missing_restore = manager.restore_version(project, std::string(40, 'a'), temporary.path() / "missing.3mf").get();
    REQUIRE_FALSE(missing_restore.ok());
    REQUIRE(missing_restore.error.code == Slic3r::ProjectHistoryErrorCode::NotFound);
}

TEST_CASE("Project history serializes queued commits and drains them during shutdown", "[project-history]")
{
    TemporaryTree  temporary;
    const fs::path project     = temporary.path() / "queued-project.3mf";
    const fs::path first_path  = temporary.path() / "first.3mf";
    const fs::path second_path = temporary.path() / "second.3mf";
    write_binary(first_path, {'P', 'K', 3, 4, 1});
    write_binary(second_path, {'P', 'K', 3, 4, 2});

    auto manager       = std::make_unique<Slic3r::ProjectHistoryManager>(temporary.path() / "app-data");
    auto first_future  = manager->commit_snapshot(project, first_path, commit_options("Queued first", 2000));
    auto second_future = manager->commit_snapshot(project, second_path, commit_options("Queued second", 2001));
    manager.reset();

    auto first  = first_future.get();
    auto second = second_future.get();
    INFO(first.error.message);
    REQUIRE(first.ok());
    REQUIRE(first.committed);
    INFO(second.error.message);
    REQUIRE(second.ok());
    REQUIRE(second.committed);

    Slic3r::ProjectHistoryManager reopened(temporary.path() / "app-data");
    auto                          versions = reopened.list_versions(project).get();
    REQUIRE(versions.ok());
    REQUIRE(versions.versions.size() == 2);
    REQUIRE(versions.versions[0].message == "Queued second");
    REQUIRE(versions.versions[1].message == "Queued first");
}

} // namespace
