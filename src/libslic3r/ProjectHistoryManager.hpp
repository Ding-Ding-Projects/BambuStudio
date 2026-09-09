#ifndef slic3r_ProjectHistoryManager_hpp_
#define slic3r_ProjectHistoryManager_hpp_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Slic3r {

// Project history stores complete, immutable .3mf snapshots in an isolated
// bare Git repository. The caller is responsible for producing a completed
// snapshot before enqueueing it; no model or GUI state is accessed here.
enum class ProjectHistoryErrorCode { None, InvalidArgument, NotFound, DestinationExists, IoError, RepositoryError, ShuttingDown, InternalError };

struct ProjectHistoryError
{
    ProjectHistoryErrorCode code{ProjectHistoryErrorCode::None};
    std::string             message;

    bool ok() const noexcept { return code == ProjectHistoryErrorCode::None; }
};

struct ProjectHistoryVersion
{
    std::string                           commit_id;
    std::string                           message;
    std::string                           author_name;
    std::string                           author_email;
    std::chrono::system_clock::time_point committed_at;
    int                                   utc_offset_minutes{0};
    std::uint64_t                         snapshot_size{0};
    // User labels attached with label_version(), in tag-name order.
    std::vector<std::string>              labels;
};

struct ProjectHistoryLabelResult
{
    ProjectHistoryError error;
    std::string         commit_id;
    std::string         label;    // the sanitized label that was stored
    std::string         tag_name; // full lightweight tag name below refs/tags/

    bool ok() const noexcept { return error.ok(); }
};

struct ProjectHistoryCommitOptions
{
    std::string                                          message{"Autosave project snapshot"};
    std::string                                          author_name{"Bambu Studio"};
    std::string                                          author_email{"project-history@localhost"};
    std::optional<std::chrono::system_clock::time_point> committed_at;
};

struct ProjectHistoryCommitResult
{
    ProjectHistoryError                  error;
    bool                                 committed{false};
    bool                                 history_migrated{false};
    std::optional<ProjectHistoryVersion> version;
    std::filesystem::path                previous_repository_path;
    std::filesystem::path                repository_path;

    bool ok() const noexcept { return error.ok(); }
};

struct ProjectHistoryMigrationResult
{
    ProjectHistoryError   error;
    bool                  migrated{false};
    std::filesystem::path source_repository_path;
    std::filesystem::path destination_repository_path;

    bool ok() const noexcept { return error.ok(); }
};

struct ProjectHistoryListResult
{
    ProjectHistoryError                error;
    std::vector<ProjectHistoryVersion> versions;
    std::filesystem::path              repository_path;

    bool ok() const noexcept { return error.ok(); }
};

struct ProjectHistoryRestoreResult
{
    ProjectHistoryError   error;
    ProjectHistoryVersion version;
    std::filesystem::path restored_path;
    std::uint64_t         bytes_written{0};

    bool ok() const noexcept { return error.ok(); }
};

class ProjectHistoryManager
{
public:
    // app_data_directory is Slic3r::data_dir() in production. Repositories are
    // always placed below app_data_directory/project_history/v1 and never in
    // the user's project directory.
    explicit ProjectHistoryManager(std::filesystem::path app_data_directory);
    ~ProjectHistoryManager();

    ProjectHistoryManager(const ProjectHistoryManager &)            = delete;
    ProjectHistoryManager &operator=(const ProjectHistoryManager &) = delete;
    ProjectHistoryManager(ProjectHistoryManager &&)                 = delete;
    ProjectHistoryManager &operator=(ProjectHistoryManager &&)      = delete;

    // Operations execute in submission order on one worker. Destruction stops
    // accepting work, drains already submitted operations, and joins it.
    std::future<ProjectHistoryCommitResult> commit_snapshot(std::filesystem::path       project_path,
                                                            std::filesystem::path       completed_snapshot_path,
                                                            ProjectHistoryCommitOptions options = {});

    // Forks the complete repository for previous_project_path to the identity
    // for new_project_path. The source remains intact, the destination must not
    // already exist, and a missing source is a successful no-op. Publication is
    // atomic: ownership metadata is changed and validated in a staging copy
    // before it becomes visible at the destination.
    std::future<ProjectHistoryMigrationResult> migrate_history_identity(std::filesystem::path previous_project_path,
                                                                        std::filesystem::path new_project_path);

    // Save As integration primitive. Migration and commit run as one queued
    // operation; a destination collision fails before any snapshot can be
    // appended to that destination's unrelated history.
    std::future<ProjectHistoryCommitResult> migrate_then_commit_snapshot(std::filesystem::path       previous_project_path,
                                                                         std::filesystem::path       new_project_path,
                                                                         std::filesystem::path       completed_snapshot_path,
                                                                         ProjectHistoryCommitOptions options = {});

    std::future<ProjectHistoryListResult> list_versions(std::filesystem::path project_path, std::size_t max_count = 0);

    // Restores into a new .3mf path. Existing destinations and destinations
    // inside the managed history root are rejected; the original project is
    // never overwritten by this primitive.
    std::future<ProjectHistoryRestoreResult> restore_version(std::filesystem::path project_path, std::string commit_id, std::filesystem::path destination_path);

    // Attaches a user label to an existing version as a lightweight Git tag
    // named "label/<sanitized label>/<commit id>". Labels never rewrite or
    // remove history; the same label may be attached to several versions, and
    // attaching a label a version already carries fails with DestinationExists.
    std::future<ProjectHistoryLabelResult> label_version(std::filesystem::path project_path, std::string commit_id, std::string label);

    const std::filesystem::path &history_root() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Slic3r

#endif // slic3r_ProjectHistoryManager_hpp_
