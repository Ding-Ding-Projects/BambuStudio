#include <catch_main.hpp>

#include "libslic3r/ProjectHistoryManager.hpp"
#include "miniz/miniz.h"
#include <git2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

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

void write_model_archive(const fs::path &path, const std::string &model = "<model unit=\"millimeter\"/>")
{
    fs::create_directories(path.parent_path());
    mz_zip_archive archive{};
    FILE *file = nullptr;
#ifdef _WIN32
    REQUIRE(_wfopen_s(&file, path.c_str(), L"wb") == 0);
#else
    file = std::fopen(path.c_str(), "wb");
#endif
    REQUIRE(file != nullptr);
    REQUIRE(mz_zip_writer_init_cfile(&archive, file, 0));
    REQUIRE(mz_zip_writer_add_mem(&archive, "3D/3dmodel.model", model.data(), model.size(), MZ_BEST_COMPRESSION));
    REQUIRE(mz_zip_writer_finalize_archive(&archive));
    REQUIRE(mz_zip_writer_end(&archive));
    REQUIRE(std::fclose(file) == 0);
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

TEST_CASE("Portable pack preflight bounds highly compressible expanded objects", "[project-history][portable]")
{
    TemporaryTree temporary;
    REQUIRE(git_libgit2_init() > 0);
    git_repository *repository = nullptr;
    REQUIRE(git_repository_init(&repository, temporary.path().string().c_str(), 1) == 0);
    const std::string zeros(2 * 1024 * 1024, '\0');
    git_oid blob_id{};
    REQUIRE(git_blob_create_frombuffer(&blob_id, repository, zeros.data(), zeros.size()) == 0);
    git_packbuilder *builder = nullptr;
    REQUIRE(git_packbuilder_new(&builder, repository) == 0);
    REQUIRE(git_packbuilder_insert(builder, &blob_id, nullptr) == 0);
    git_buf buffer = GIT_BUF_INIT;
    REQUIRE(git_packbuilder_write_buf(&buffer, builder) == 0);
    const std::vector<unsigned char> pack(buffer.ptr, buffer.ptr + buffer.size);
    git_buf_dispose(&buffer);
    git_packbuilder_free(builder);
    git_repository_free(repository);
    git_libgit2_shutdown();

    REQUIRE(pack.size() < 64 * 1024); // The compressed size is not a safe quota.
    REQUIRE(Slic3r::project_history_pack_within_budget(pack, 3 * 1024 * 1024, 3 * 1024 * 1024, 1));
    REQUIRE_FALSE(Slic3r::project_history_pack_within_budget(pack, 1024 * 1024, 3 * 1024 * 1024, 1));
    REQUIRE_FALSE(Slic3r::project_history_pack_within_budget(pack, 3 * 1024 * 1024, 1024 * 1024, 1));
    REQUIRE_FALSE(Slic3r::project_history_pack_within_budget(pack, 3 * 1024 * 1024, 3 * 1024 * 1024, 0));
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

TEST_CASE("Project history identity migration preserves complete ancestry and both identities", "[project-history][migration]")
{
    TemporaryTree                    temporary;
    const fs::path                   app_data      = temporary.path() / "app-data";
    const fs::path                   old_project   = temporary.path() / "projects" / "original.3mf";
    const fs::path                   new_project   = temporary.path() / "projects" / "saved-as.3mf";
    const fs::path                   snapshot_one  = temporary.path() / "snapshots" / "one.3mf";
    const fs::path                   snapshot_two  = temporary.path() / "snapshots" / "two.3mf";
    const fs::path                   snapshot_three = temporary.path() / "snapshots" / "three.3mf";
    const std::vector<unsigned char> bytes_one{'P', 'K', 3, 4, 1};
    const std::vector<unsigned char> bytes_two{'P', 'K', 3, 4, 2};
    const std::vector<unsigned char> bytes_three{'P', 'K', 3, 4, 3};
    write_binary(snapshot_one, bytes_one);
    write_binary(snapshot_two, bytes_two);
    write_binary(snapshot_three, bytes_three);

    Slic3r::ProjectHistoryManager manager(app_data);
    auto first  = manager.commit_snapshot(old_project, snapshot_one, commit_options("Original one", 3000)).get();
    auto second = manager.commit_snapshot(old_project, snapshot_two, commit_options("Original two", 3001)).get();
    REQUIRE(first.ok());
    REQUIRE(second.ok());

    auto migration = manager.migrate_history_identity(old_project, new_project).get();
    INFO(migration.error.message);
    REQUIRE(migration.ok());
    REQUIRE(migration.migrated);
    REQUIRE(migration.source_repository_path == first.repository_path);
    REQUIRE(migration.destination_repository_path != migration.source_repository_path);
    REQUIRE(fs::is_directory(migration.source_repository_path));
    REQUIRE(fs::is_directory(migration.destination_repository_path));

    auto migrated_versions = manager.list_versions(new_project).get();
    REQUIRE(migrated_versions.ok());
    REQUIRE(migrated_versions.versions.size() == 2);
    REQUIRE(migrated_versions.versions[0].commit_id == second.version->commit_id);
    REQUIRE(migrated_versions.versions[1].commit_id == first.version->commit_id);

    auto third = manager.commit_snapshot(new_project, snapshot_three, commit_options("Saved-as edit", 3002)).get();
    REQUIRE(third.ok());
    REQUIRE(third.committed);

    auto new_versions = manager.list_versions(new_project).get();
    REQUIRE(new_versions.ok());
    REQUIRE(new_versions.versions.size() == 3);
    REQUIRE(new_versions.versions[0].commit_id == third.version->commit_id);
    REQUIRE(new_versions.versions[1].commit_id == second.version->commit_id);
    REQUIRE(new_versions.versions[2].commit_id == first.version->commit_id);

    auto old_versions = manager.list_versions(old_project).get();
    REQUIRE(old_versions.ok());
    REQUIRE(old_versions.versions.size() == 2);
    REQUIRE(old_versions.versions[0].commit_id == second.version->commit_id);
    REQUIRE(old_versions.versions[1].commit_id == first.version->commit_id);

    const fs::path restored = temporary.path() / "restored" / "from-migrated-ancestry.3mf";
    auto restore = manager.restore_version(new_project, first.version->commit_id, restored).get();
    REQUIRE(restore.ok());
    REQUIRE(read_binary(restored) == bytes_one);
}

TEST_CASE("Project history identity migration handles missing and equivalent identities safely", "[project-history][migration]")
{
    TemporaryTree                 temporary;
    Slic3r::ProjectHistoryManager manager(temporary.path() / "app-data");
    const fs::path                missing_project = temporary.path() / "missing.3mf";
    const fs::path                new_project     = temporary.path() / "new.3mf";

    auto missing = manager.migrate_history_identity(missing_project, new_project).get();
    INFO(missing.error.message);
    REQUIRE(missing.ok());
    REQUIRE_FALSE(missing.migrated);
    REQUIRE_FALSE(fs::exists(missing.source_repository_path));
    REQUIRE_FALSE(fs::exists(missing.destination_repository_path));

    const fs::path snapshot = temporary.path() / "snapshot.3mf";
    write_binary(snapshot, {'P', 'K', 3, 4, 7});
    auto committed = manager.commit_snapshot(new_project, snapshot, commit_options("Equivalent identity", 3100)).get();
    REQUIRE(committed.ok());

    const fs::path equivalent_project = new_project.parent_path() / "unused-component" / ".." / new_project.filename();
    auto           equivalent         = manager.migrate_history_identity(new_project, equivalent_project).get();
    INFO(equivalent.error.message);
    REQUIRE(equivalent.ok());
    REQUIRE_FALSE(equivalent.migrated);
    REQUIRE(equivalent.source_repository_path == equivalent.destination_repository_path);

    auto invalid_source = manager.migrate_history_identity(temporary.path() / "source.stl", new_project).get();
    REQUIRE_FALSE(invalid_source.ok());
    REQUIRE(invalid_source.error.code == Slic3r::ProjectHistoryErrorCode::InvalidArgument);

    auto invalid_destination = manager.migrate_history_identity(new_project, temporary.path() / "destination.zip").get();
    REQUIRE_FALSE(invalid_destination.ok());
    REQUIRE(invalid_destination.error.code == Slic3r::ProjectHistoryErrorCode::InvalidArgument);
}

TEST_CASE("Project history migration fails closed when destination history exists", "[project-history][migration]")
{
    TemporaryTree                 temporary;
    Slic3r::ProjectHistoryManager manager(temporary.path() / "app-data");
    const fs::path                old_project          = temporary.path() / "old.3mf";
    const fs::path                destination_project  = temporary.path() / "destination.3mf";
    const fs::path                old_snapshot         = temporary.path() / "old-snapshot.3mf";
    const fs::path                destination_snapshot = temporary.path() / "destination-snapshot.3mf";
    const fs::path                save_as_snapshot     = temporary.path() / "save-as-snapshot.3mf";
    write_binary(old_snapshot, {'P', 'K', 3, 4, 1, 1});
    write_binary(destination_snapshot, {'P', 'K', 3, 4, 2, 2});
    write_binary(save_as_snapshot, {'P', 'K', 3, 4, 3, 3});

    auto old_head = manager.commit_snapshot(old_project, old_snapshot, commit_options("Old history", 3200)).get();
    auto destination_head = manager.commit_snapshot(destination_project, destination_snapshot, commit_options("Unrelated destination", 3201)).get();
    REQUIRE(old_head.ok());
    REQUIRE(destination_head.ok());

    auto migration = manager.migrate_history_identity(old_project, destination_project).get();
    REQUIRE_FALSE(migration.ok());
    REQUIRE(migration.error.code == Slic3r::ProjectHistoryErrorCode::DestinationExists);
    REQUIRE_FALSE(migration.migrated);

    auto composite = manager
                         .migrate_then_commit_snapshot(old_project, destination_project, save_as_snapshot, commit_options("Must not append", 3202))
                         .get();
    REQUIRE_FALSE(composite.ok());
    REQUIRE(composite.error.code == Slic3r::ProjectHistoryErrorCode::DestinationExists);
    REQUIRE_FALSE(composite.committed);
    REQUIRE_FALSE(composite.history_migrated);

    auto destination_versions = manager.list_versions(destination_project).get();
    REQUIRE(destination_versions.ok());
    REQUIRE(destination_versions.versions.size() == 1);
    REQUIRE(destination_versions.versions.front().commit_id == destination_head.version->commit_id);
    REQUIRE(destination_versions.versions.front().message == "Unrelated destination");

    auto old_versions = manager.list_versions(old_project).get();
    REQUIRE(old_versions.ok());
    REQUIRE(old_versions.versions.size() == 1);
    REQUIRE(old_versions.versions.front().commit_id == old_head.version->commit_id);
}

TEST_CASE("Project history drains a queued migration and commit during shutdown", "[project-history][migration]")
{
    TemporaryTree  temporary;
    const fs::path old_project  = temporary.path() / "queued-old.3mf";
    const fs::path new_project  = temporary.path() / "queued-new.3mf";
    const fs::path first_path   = temporary.path() / "queued-first.3mf";
    const fs::path second_path  = temporary.path() / "queued-second.3mf";
    write_binary(first_path, {'P', 'K', 3, 4, 1});
    write_binary(second_path, {'P', 'K', 3, 4, 2});

    auto manager = std::make_unique<Slic3r::ProjectHistoryManager>(temporary.path() / "app-data");
    auto first_future = manager->commit_snapshot(old_project, first_path, commit_options("Before Save As", 3300));
    auto save_as_future = manager->migrate_then_commit_snapshot(old_project, new_project, second_path, commit_options("Saved As", 3301));
    manager.reset();

    auto first   = first_future.get();
    auto save_as = save_as_future.get();
    INFO(first.error.message);
    REQUIRE(first.ok());
    INFO(save_as.error.message);
    REQUIRE(save_as.ok());
    REQUIRE(save_as.history_migrated);
    REQUIRE(save_as.committed);
    REQUIRE(save_as.previous_repository_path == first.repository_path);

    Slic3r::ProjectHistoryManager reopened(temporary.path() / "app-data");
    auto                          versions = reopened.list_versions(new_project).get();
    REQUIRE(versions.ok());
    REQUIRE(versions.versions.size() == 2);
    REQUIRE(versions.versions[0].message == "Saved As");
    REQUIRE(versions.versions[1].commit_id == first.version->commit_id);
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

TEST_CASE("Project history serializes independent managers for one project identity", "[project-history][concurrency]")
{
    TemporaryTree  temporary;
    const fs::path app_data = temporary.path() / "app-data";
    const fs::path project  = temporary.path() / "shared-project.3mf";

    Slic3r::ProjectHistoryManager first_manager(app_data);
    Slic3r::ProjectHistoryManager second_manager(app_data);

    constexpr std::size_t commit_count = 12;
    std::vector<std::future<Slic3r::ProjectHistoryCommitResult>> futures;
    std::set<std::string>                                        expected_messages;
    futures.reserve(commit_count);
    for (std::size_t index = 0; index < commit_count; ++index) {
        const fs::path snapshot = temporary.path() / ("concurrent-" + std::to_string(index) + ".3mf");
        std::vector<unsigned char> bytes(128u * 1024u, static_cast<unsigned char>(index + 1));
        bytes[0] = 'P';
        bytes[1] = 'K';
        bytes[2] = 3;
        bytes[3] = 4;
        write_binary(snapshot, bytes);

        const std::string message = "Independent manager " + std::to_string(index);
        expected_messages.emplace(message);
        Slic3r::ProjectHistoryManager &manager = index % 2 == 0 ? first_manager : second_manager;
        futures.emplace_back(manager.commit_snapshot(project, snapshot, commit_options(message, 4000 + static_cast<std::int64_t>(index))));
    }

    fs::path              repository_path;
    std::set<std::string> commit_ids;
    for (auto &future : futures) {
        auto result = future.get();
        INFO(result.error.message);
        REQUIRE(result.ok());
        REQUIRE(result.committed);
        REQUIRE(result.version.has_value());
        repository_path = result.repository_path;
        commit_ids.emplace(result.version->commit_id);
    }
    REQUIRE(commit_ids.size() == commit_count);

    auto versions = first_manager.list_versions(project).get();
    INFO(versions.error.message);
    REQUIRE(versions.ok());
    REQUIRE(versions.versions.size() == commit_count);
    std::set<std::string> actual_messages;
    for (const auto &version : versions.versions) actual_messages.emplace(version.message);
    REQUIRE(actual_messages == expected_messages);

    const fs::path lock_path = app_data / "project_history" / "locks" / "v1" / (repository_path.filename().string() + ".lock");
    REQUIRE(fs::is_regular_file(lock_path));
    REQUIRE(lock_path.parent_path() != repository_path);
    for (const auto &entry : fs::directory_iterator(first_manager.history_root()))
        REQUIRE(entry.path().filename().string().rfind(".create-", 0) != 0);

#ifndef _WIN32
    constexpr fs::perms non_owner_permissions = fs::perms::group_all | fs::perms::others_all;
    REQUIRE((fs::status(repository_path).permissions() & non_owner_permissions) == fs::perms::none);
    REQUIRE((fs::status(lock_path).permissions() & non_owner_permissions) == fs::perms::none);
#endif
}

TEST_CASE("Portable history survives a copied project and keeps its stable document identity", "[project-history][portable]")
{
    TemporaryTree temporary;
    const fs::path source = temporary.path() / "source.3mf";
    const fs::path snapshot = temporary.path() / "model.3mf";
    const fs::path copy = temporary.path() / "moved.3mf";
    write_model_archive(snapshot);
    Slic3r::ProjectHistoryManager first(temporary.path() / "first-app");
    const auto committed = first.commit_snapshot(source, snapshot, commit_options("Portable version", 5000)).get();
    INFO(committed.error.message);
    REQUIRE(committed.ok());
    const auto published = first.publish_portable_history(source, snapshot, source).get();
    INFO(published.error.message);
    REQUIRE(published.ok());
    REQUIRE(published.present);
    REQUIRE(published.document_id.size() == 36);
    fs::copy_file(source, copy);

    Slic3r::ProjectHistoryManager second(temporary.path() / "second-app");
    const auto legacy = second.commit_snapshot(copy, snapshot, commit_options("Local path lineage", 5001)).get();
    REQUIRE(legacy.ok());
    const auto inspected = second.inspect_portable_history(copy).get();
    INFO(inspected.error.message);
    REQUIRE(inspected.ok());
    REQUIRE(inspected.document_id == published.document_id);
    REQUIRE(inspected.head_id == committed.version->commit_id);
    const auto imported = second.import_portable_history(copy, copy).get();
    INFO(imported.error.message);
    REQUIRE(imported.ok());
    const auto versions = second.list_versions(copy).get();
    INFO(versions.error.message);
    REQUIRE(versions.ok());
    REQUIRE(versions.versions.size() == 1);
    REQUIRE(versions.versions.front().commit_id == committed.version->commit_id);

    const auto fork = second.publish_portable_history(copy, snapshot, copy, true).get();
    INFO(fork.error.message);
    REQUIRE(fork.ok());
    REQUIRE(fork.document_id != published.document_id);
    REQUIRE(fork.head_id == published.head_id);
    mz_zip_archive fork_archive{};
    REQUIRE(mz_zip_reader_init_file(&fork_archive, copy.u8string().c_str(), 0));
    const int manifest_index = mz_zip_reader_locate_file(&fork_archive, "Metadata/bambu_project_history.json", nullptr, 0);
    REQUIRE(manifest_index >= 0);
    mz_zip_archive_file_stat manifest_stat{};
    REQUIRE(mz_zip_reader_file_stat(&fork_archive, static_cast<mz_uint>(manifest_index), &manifest_stat));
    std::string manifest(static_cast<std::size_t>(manifest_stat.m_uncomp_size), '\0');
    REQUIRE(mz_zip_reader_extract_to_mem(&fork_archive, static_cast<mz_uint>(manifest_index), manifest.data(), manifest.size(), 0));
    REQUIRE(mz_zip_reader_end(&fork_archive));
    REQUIRE(manifest.find(legacy.version->commit_id) != std::string::npos);
    REQUIRE(manifest.find(committed.version->commit_id) != std::string::npos);
}

#ifdef _WIN32
TEST_CASE("Failed atomic publication retains the verified history-bearing archive", "[project-history][portable]")
{
    TemporaryTree temporary;
    const fs::path source = temporary.path() / "source.3mf";
    const fs::path snapshot = temporary.path() / "model.3mf";
    write_model_archive(snapshot);
    Slic3r::ProjectHistoryManager manager(temporary.path() / "app");
    REQUIRE(manager.commit_snapshot(source, snapshot).get().ok());
    REQUIRE(manager.publish_portable_history(source, snapshot, source).get().ok());
    const auto original = read_binary(source);
    HANDLE hold = CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(hold != INVALID_HANDLE_VALUE);
    const auto rejected = manager.publish_portable_history(source, snapshot, source).get();
    CloseHandle(hold);
    REQUIRE_FALSE(rejected.ok());
    REQUIRE(rejected.archive_path != source);
    REQUIRE(fs::is_regular_file(rejected.archive_path));
    REQUIRE(read_binary(source) == original);
    const auto pending = manager.inspect_portable_history(rejected.archive_path).get();
    INFO(pending.error.message);
    REQUIRE(pending.ok());
    REQUIRE(pending.present);
}
#endif

TEST_CASE("Portable history rejects corrupt and traversal manifests without replacing a valid archive", "[project-history][portable]")
{
    TemporaryTree temporary;
    const fs::path source = temporary.path() / "source.3mf";
    const fs::path snapshot = temporary.path() / "model.3mf";
    const fs::path invalid = temporary.path() / "invalid.3mf";
    write_model_archive(snapshot);
    Slic3r::ProjectHistoryManager manager(temporary.path() / "app");
    REQUIRE(manager.commit_snapshot(source, snapshot).get().ok());
    REQUIRE(manager.publish_portable_history(source, snapshot, source).get().ok());
    const auto original = read_binary(source);
    const fs::path tampered = temporary.path() / "tampered.3mf";
    auto tampered_bytes = original;
    const auto pack_marker = std::search(tampered_bytes.begin(), tampered_bytes.end(),
                                         std::begin("PACK") , std::begin("PACK") + 4);
    REQUIRE(pack_marker != tampered_bytes.end());
    *(pack_marker + 1) ^= 0x01;
    write_binary(tampered, tampered_bytes);
    REQUIRE_FALSE(manager.inspect_portable_history(tampered).get().ok());
    mz_zip_archive geometry_archive{};
    REQUIRE(mz_zip_reader_init_file(&geometry_archive, tampered.u8string().c_str(), 0));
    REQUIRE(mz_zip_reader_locate_file(&geometry_archive, "3D/3dmodel.model", nullptr, 0) >= 0);
    REQUIRE(mz_zip_reader_end(&geometry_archive));

    mz_zip_archive archive{};
    REQUIRE(mz_zip_writer_init_file(&archive, invalid.u8string().c_str(), 0));
    const std::string traversal = R"({"version":1,"document_id":"00000000-0000-4000-8000-000000000000","head":"0000000000000000000000000000000000000000","lineages":["0000000000000000000000000000000000000000"],"pack_path":"../escape.pack","pack_size":4,"pack_sha256":"0000000000000000000000000000000000000000000000000000000000000000"})";
    const std::string bogus_pack = "PACK";
    REQUIRE(mz_zip_writer_add_mem(&archive, "Metadata/bambu_project_history.json", traversal.data(), traversal.size(), MZ_NO_COMPRESSION));
    REQUIRE(mz_zip_writer_add_mem(&archive, "Metadata/bambu_project_history.pack", bogus_pack.data(), bogus_pack.size(), MZ_NO_COMPRESSION));
    REQUIRE(mz_zip_writer_finalize_archive(&archive));
    REQUIRE(mz_zip_writer_end(&archive));
    const auto rejected = manager.inspect_portable_history(invalid).get();
    REQUIRE_FALSE(rejected.ok());
    const auto no_overwrite = manager.publish_portable_history(source, invalid, source).get();
    REQUIRE_FALSE(no_overwrite.ok());
    REQUIRE(read_binary(source) == original);
}

TEST_CASE("Portable history supports Unicode project paths", "[project-history][portable]")
{
    TemporaryTree temporary;
    const fs::path project = temporary.path() / fs::u8path(u8"模型.3mf");
    const fs::path snapshot = temporary.path() / fs::u8path(u8"快照.3mf");
    write_model_archive(snapshot);
    Slic3r::ProjectHistoryManager manager(temporary.path() / "app");
    REQUIRE(manager.commit_snapshot(project, snapshot).get().ok());
    const auto published = manager.publish_portable_history(project, snapshot, project).get();
    INFO(published.error.message);
    REQUIRE(published.ok());
    const auto inspected = manager.inspect_portable_history(project).get();
    INFO(inspected.error.message);
    REQUIRE(inspected.ok());
    REQUIRE(inspected.document_id == published.document_id);
}

TEST_CASE("Divergent copies receive separate document identities on save", "[project-history][portable]")
{
    TemporaryTree temporary;
    const fs::path original = temporary.path() / "original.3mf";
    const fs::path copy = temporary.path() / "copy.3mf";
    const fs::path snapshot = temporary.path() / "model.3mf";
    write_model_archive(snapshot);
    Slic3r::ProjectHistoryManager manager(temporary.path() / "app");
    REQUIRE(manager.commit_snapshot(original, snapshot).get().ok());
    const auto first = manager.publish_portable_history(original, snapshot, original).get();
    REQUIRE(first.ok());
    fs::copy_file(original, copy);
    REQUIRE(manager.import_portable_history(copy, copy).get().ok());
    const auto saved_copy = manager.publish_portable_history(copy, snapshot, copy).get();
    INFO(saved_copy.error.message);
    REQUIRE(saved_copy.ok());
    REQUIRE(saved_copy.document_id != first.document_id);
    REQUIRE(saved_copy.head_id == first.head_id);
}

TEST_CASE("Reopening a saved archive keeps newer local recovery history active", "[project-history][portable]")
{
    TemporaryTree temporary;
    const fs::path project = temporary.path() / "project.3mf";
    const fs::path saved_snapshot = temporary.path() / "saved.3mf";
    const fs::path local_snapshot = temporary.path() / "local.3mf";
    write_model_archive(saved_snapshot);
    write_model_archive(local_snapshot, "<model unit=\"inch\"/>");
    Slic3r::ProjectHistoryManager manager(temporary.path() / "app");
    const auto initial = manager.commit_snapshot(project, saved_snapshot, commit_options("Saved", 5000)).get();
    REQUIRE(initial.ok());
    REQUIRE(manager.publish_portable_history(project, saved_snapshot, project).get().ok());
    const auto local = manager.commit_snapshot(project, local_snapshot, commit_options("Recovered edit", 5001)).get();
    REQUIRE(local.ok());
    REQUIRE(local.version->commit_id != initial.version->commit_id);
    const auto imported = manager.import_portable_history(project, project).get();
    INFO(imported.error.message);
    REQUIRE(imported.ok());
    const auto versions = manager.list_versions(project).get();
    INFO(versions.error.message);
    REQUIRE(versions.ok());
    REQUIRE(versions.versions.front().commit_id == local.version->commit_id);
}

} // namespace
