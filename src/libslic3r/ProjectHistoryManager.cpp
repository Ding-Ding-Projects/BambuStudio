#include "ProjectHistoryManager.hpp"

#include <git2.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

namespace Slic3r {
namespace {

namespace fs = std::filesystem;

constexpr const char *HISTORY_DIRECTORY       = "project_history";
constexpr const char *HISTORY_LAYOUT_VERSION  = "v1";
constexpr const char *SNAPSHOT_TREE_PATH      = "project.3mf";
constexpr const char *CONFIG_LAYOUT_VERSION   = "bambu.projecthistoryversion";
constexpr const char *CONFIG_PROJECT_IDENTITY = "bambu.projectidentitysha256";

template<class Result> Result failure(ProjectHistoryErrorCode code, std::string message)
{
    Result result;
    result.error.code    = code;
    result.error.message = std::move(message);
    return result;
}

std::string git_error_message(const std::string &operation)
{
    const git_error *error = git_error_last();
    if (error != nullptr && error->message != nullptr) return operation + ": " + error->message;
    return operation + " failed";
}

std::string path_utf8(const fs::path &path) { return path.u8string(); }

std::string lowercase_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); });
    return value;
}

bool has_3mf_extension(const fs::path &path) { return lowercase_ascii(path_utf8(path.extension())) == ".3mf"; }

bool contains_nul(const std::string &value) { return value.find('\0') != std::string::npos; }

bool normalize_project_identity(const fs::path &project_path, std::string &normalized, ProjectHistoryError &error)
{
    if (project_path.empty() || !has_3mf_extension(project_path)) {
        error = {ProjectHistoryErrorCode::InvalidArgument, "Project identity must be a non-empty .3mf path"};
        return false;
    }

    std::error_code ec;
    fs::path        absolute = fs::absolute(project_path, ec);
    if (ec) {
        error = {ProjectHistoryErrorCode::InvalidArgument, "Could not resolve the project identity path: " + ec.message()};
        return false;
    }

    absolute = absolute.lexically_normal();
    if (fs::exists(absolute, ec) && !ec) {
        fs::path canonical = fs::weakly_canonical(absolute, ec);
        if (!ec) absolute = std::move(canonical);
    }

    normalized = absolute.generic_u8string();
#ifdef _WIN32
    // Windows paths are case-insensitive in supported Bambu Studio setups.
    // ASCII folding covers drive letters and ordinary path components while
    // preserving the exact UTF-8 bytes of non-ASCII project names.
    normalized = lowercase_ascii(std::move(normalized));
#endif
    return true;
}

bool sha256_hex(const std::string &value, std::string &digest, ProjectHistoryError &error)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
    unsigned int                               length = 0;
    if (EVP_Digest(value.data(), value.size(), bytes.data(), &length, EVP_sha256(), nullptr) != 1 || length != 32) {
        error = {ProjectHistoryErrorCode::InternalError, "Could not hash the normalized project identity"};
        return false;
    }

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < length; ++index) out << std::setw(2) << static_cast<unsigned int>(bytes[index]);
    digest = out.str();
    return true;
}

struct ResolvedProject
{
    ProjectHistoryError error;
    std::string         identity_hash;
    fs::path            repository_path;
};

ResolvedProject resolve_project(const fs::path &history_root, const fs::path &project_path)
{
    ResolvedProject result;
    std::string     normalized;
    if (!normalize_project_identity(project_path, normalized, result.error)) return result;
    if (!sha256_hex(normalized, result.identity_hash, result.error)) return result;
    result.repository_path = history_root / result.identity_hash;
    return result;
}

template<typename T, void (*FreeFunction)(T *)> struct GitDeleter
{
    void operator()(T *value) const noexcept
    {
        if (value != nullptr) FreeFunction(value);
    }
};

template<typename T, void (*FreeFunction)(T *)> using GitPtr = std::unique_ptr<T, GitDeleter<T, FreeFunction>>;

using RepositoryPtr = GitPtr<git_repository, git_repository_free>;
using ConfigPtr     = GitPtr<git_config, git_config_free>;
using ReferencePtr  = GitPtr<git_reference, git_reference_free>;
using CommitPtr     = GitPtr<git_commit, git_commit_free>;
using TreePtr       = GitPtr<git_tree, git_tree_free>;
using TreeEntryPtr  = GitPtr<git_tree_entry, git_tree_entry_free>;
using BlobPtr       = GitPtr<git_blob, git_blob_free>;
using IndexPtr      = GitPtr<git_index, git_index_free>;
using SignaturePtr  = GitPtr<git_signature, git_signature_free>;
using RevwalkPtr    = GitPtr<git_revwalk, git_revwalk_free>;

bool configure_repository(git_repository *repository, const std::string &identity_hash, bool initialize, ProjectHistoryError &error)
{
    git_config *raw_config = nullptr;
    if (git_repository_config(&raw_config, repository) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not open project-history configuration")};
        return false;
    }
    ConfigPtr config(raw_config);

    int32_t layout_version = 0;
    int     rc             = git_config_get_int32(&layout_version, config.get(), CONFIG_LAYOUT_VERSION);
    if (rc == GIT_ENOTFOUND && initialize) {
        // Write the identity first and the layout marker last. Presence of the
        // layout marker therefore means all repository ownership metadata was
        // persisted successfully.
        if (git_config_set_string(config.get(), CONFIG_PROJECT_IDENTITY, identity_hash.c_str()) != 0 || git_config_set_int32(config.get(), CONFIG_LAYOUT_VERSION, 1) != 0) {
            error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not initialize project-history configuration")};
            return false;
        }
        return true;
    }
    if (rc != 0 || layout_version != 1) {
        error = {ProjectHistoryErrorCode::RepositoryError, "Unsupported or invalid project-history repository layout"};
        return false;
    }

    git_buf           stored_identity_buffer = GIT_BUF_INIT;
    const int         identity_rc            = git_config_get_string_buf(&stored_identity_buffer, config.get(), CONFIG_PROJECT_IDENTITY);
    const std::string stored_identity        = identity_rc == 0 && stored_identity_buffer.ptr != nullptr ? stored_identity_buffer.ptr : "";
    git_buf_dispose(&stored_identity_buffer);
    if (identity_rc != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, "Project-history repository identity marker is missing"};
        return false;
    }
    if (identity_hash != stored_identity) {
        error = {ProjectHistoryErrorCode::RepositoryError, "Project-history repository identity does not match its location"};
        return false;
    }
    return true;
}

bool open_repository(const fs::path &repository_path, const std::string &identity_hash, bool create, RepositoryPtr &repository, ProjectHistoryError &error)
{
    std::error_code ec;
    const bool      exists = fs::exists(repository_path, ec);
    if (ec) {
        error = {ProjectHistoryErrorCode::IoError, "Could not inspect the project-history location: " + ec.message()};
        return false;
    }

    git_repository *raw_repository = nullptr;
    if (exists) {
        if (!fs::is_directory(repository_path, ec) || ec) {
            error = {ProjectHistoryErrorCode::RepositoryError, "Project-history location is not a directory"};
            return false;
        }
        if (git_repository_open_bare(&raw_repository, path_utf8(repository_path).c_str()) != 0) {
            error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not open project-history repository")};
            return false;
        }
        repository.reset(raw_repository);
        return configure_repository(repository.get(), identity_hash, false, error);
    }

    if (!create) {
        error = {ProjectHistoryErrorCode::NotFound, "No history exists for this project"};
        return false;
    }

    git_repository_init_options options = GIT_REPOSITORY_INIT_OPTIONS_INIT;
    options.flags                       = GIT_REPOSITORY_INIT_BARE | GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKPATH;
    options.mode                        = GIT_REPOSITORY_INIT_SHARED_UMASK;
    options.description                 = "Bambu Studio isolated project history";
    options.initial_head                = "main";
    if (git_repository_init_ext(&raw_repository, path_utf8(repository_path).c_str(), &options) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not initialize project-history repository")};
        return false;
    }
    repository.reset(raw_repository);
    return configure_repository(repository.get(), identity_hash, true, error);
}

bool load_head_commit(git_repository *repository, CommitPtr &commit, bool &has_head, ProjectHistoryError &error)
{
    has_head                     = false;
    git_reference *raw_reference = nullptr;
    const int      head_rc       = git_repository_head(&raw_reference, repository);
    if (head_rc == GIT_EUNBORNBRANCH || head_rc == GIT_ENOTFOUND) return true;
    if (head_rc != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not read project-history HEAD")};
        return false;
    }
    ReferencePtr reference(raw_reference);

    git_object *raw_object = nullptr;
    if (git_reference_peel(&raw_object, reference.get(), GIT_OBJECT_COMMIT) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not resolve project-history HEAD")};
        return false;
    }
    commit.reset(reinterpret_cast<git_commit *>(raw_object));
    has_head = true;
    return true;
}

bool version_from_commit(git_repository *repository, git_commit *commit, ProjectHistoryVersion &version, ProjectHistoryError &error)
{
    char oid_buffer[GIT_OID_MAX_HEXSIZE + 1]{};
    if (git_oid_tostr(oid_buffer, sizeof(oid_buffer), git_commit_id(commit)) == nullptr) {
        error = {ProjectHistoryErrorCode::RepositoryError, "Could not format a project-history commit identifier"};
        return false;
    }
    version.commit_id = oid_buffer;

    const char *message = git_commit_message(commit);
    version.message     = message != nullptr ? message : "";
    while (!version.message.empty() && (version.message.back() == '\n' || version.message.back() == '\r')) version.message.pop_back();

    const git_signature *author = git_commit_author(commit);
    if (author != nullptr) {
        version.author_name  = author->name != nullptr ? author->name : "";
        version.author_email = author->email != nullptr ? author->email : "";
    }

    version.committed_at       = std::chrono::system_clock::time_point(std::chrono::seconds(git_commit_time(commit)));
    version.utc_offset_minutes = git_commit_time_offset(commit);

    git_tree *raw_tree = nullptr;
    if (git_commit_tree(&raw_tree, commit) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not read project-history tree")};
        return false;
    }
    TreePtr tree(raw_tree);

    git_tree_entry *raw_entry = nullptr;
    if (git_tree_entry_bypath(&raw_entry, tree.get(), SNAPSHOT_TREE_PATH) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, "Project-history commit does not contain a complete .3mf snapshot"};
        return false;
    }
    TreeEntryPtr entry(raw_entry);
    if (git_tree_entry_type(entry.get()) != GIT_OBJECT_BLOB) {
        error = {ProjectHistoryErrorCode::RepositoryError, "Project-history snapshot entry is not a Git blob"};
        return false;
    }

    git_blob *raw_blob = nullptr;
    if (git_blob_lookup(&raw_blob, repository, git_tree_entry_id(entry.get())) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not read project-history snapshot")};
        return false;
    }
    BlobPtr blob(raw_blob);
    version.snapshot_size = static_cast<std::uint64_t>(git_blob_rawsize(blob.get()));
    return true;
}

bool head_has_blob(git_commit *head, const git_oid &blob_oid, bool &duplicate, ProjectHistoryError &error)
{
    duplicate = false;
    if (head == nullptr) return true;

    git_tree *raw_tree = nullptr;
    if (git_commit_tree(&raw_tree, head) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not read project-history HEAD tree")};
        return false;
    }
    TreePtr tree(raw_tree);

    git_tree_entry *raw_entry = nullptr;
    const int       entry_rc  = git_tree_entry_bypath(&raw_entry, tree.get(), SNAPSHOT_TREE_PATH);
    if (entry_rc == GIT_ENOTFOUND) return true;
    if (entry_rc != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not inspect project-history HEAD snapshot")};
        return false;
    }
    TreeEntryPtr entry(raw_entry);
    duplicate = git_tree_entry_type(entry.get()) == GIT_OBJECT_BLOB && git_oid_equal(git_tree_entry_id(entry.get()), &blob_oid) == 1;
    return true;
}

std::string comparable_path(fs::path path)
{
    std::error_code ec;
    path              = fs::absolute(path, ec).lexically_normal();
    std::string value = path.generic_u8string();
#ifdef _WIN32
    value = lowercase_ascii(std::move(value));
#endif
    return value;
}

bool path_is_within(const fs::path &candidate, const fs::path &root)
{
    const std::string candidate_string = comparable_path(candidate);
    std::string       root_string      = comparable_path(root);
    if (candidate_string == root_string) return true;
    if (!root_string.empty() && root_string.back() != '/') root_string.push_back('/');
    return candidate_string.size() > root_string.size() && candidate_string.compare(0, root_string.size(), root_string) == 0;
}

fs::path unique_staging_path(const fs::path &destination)
{
    static std::atomic<std::uint64_t> sequence{0};
    const auto                        clock_value = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned int attempt = 0; attempt < 16; ++attempt) {
        std::ostringstream suffix;
        suffix << ".bambu-history-part-" << std::hex << clock_value << '-' << sequence.fetch_add(1);
        fs::path candidate = destination;
        candidate += suffix.str();
        std::error_code ec;
        if (!fs::exists(candidate, ec) && !ec) return candidate;
    }
    return {};
}

} // namespace

class ProjectHistoryManager::Impl
{
public:
    explicit Impl(fs::path app_data_directory)
    {
        if (app_data_directory.empty()) {
            m_initialization_error = {ProjectHistoryErrorCode::InvalidArgument, "Application data directory is empty"};
            return;
        }

        std::error_code ec;
        app_data_directory = fs::absolute(app_data_directory, ec).lexically_normal();
        if (ec) {
            m_initialization_error = {ProjectHistoryErrorCode::InvalidArgument, "Could not resolve the application data directory: " + ec.message()};
            return;
        }
        m_history_root = app_data_directory / HISTORY_DIRECTORY / HISTORY_LAYOUT_VERSION;

        if (git_libgit2_init() < 0) {
            m_initialization_error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not initialize the project-history engine")};
            return;
        }
        m_libgit2_initialized = true;

        try {
            m_worker = std::thread([this] { worker_loop(); });
        } catch (const std::exception &exception) {
            m_initialization_error = {ProjectHistoryErrorCode::InternalError, std::string("Could not start the project-history worker: ") + exception.what()};
        }
    }

    ~Impl()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_condition.notify_all();
        if (m_worker.joinable()) m_worker.join();
        if (m_libgit2_initialized) git_libgit2_shutdown();
    }

    template<class Result, class Function> std::future<Result> enqueue(Function function)
    {
        auto                promise = std::make_shared<std::promise<Result>>();
        std::future<Result> future  = promise->get_future();

        if (!m_initialization_error.ok()) {
            Result result;
            result.error = m_initialization_error;
            promise->set_value(std::move(result));
            return future;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopping || !m_worker.joinable()) {
                promise->set_value(failure<Result>(ProjectHistoryErrorCode::ShuttingDown, "Project-history manager is shutting down"));
                return future;
            }
            m_jobs.emplace_back([promise, function = std::move(function)]() mutable {
                try {
                    promise->set_value(function());
                } catch (const std::exception &exception) {
                    promise->set_value(failure<Result>(ProjectHistoryErrorCode::InternalError, std::string("Unexpected project-history error: ") + exception.what()));
                } catch (...) {
                    promise->set_value(failure<Result>(ProjectHistoryErrorCode::InternalError, "Unexpected project-history error"));
                }
            });
        }
        m_condition.notify_one();
        return future;
    }

    ProjectHistoryCommitResult commit(const fs::path &project_path, const fs::path &snapshot_path, const ProjectHistoryCommitOptions &options)
    {
        ResolvedProject project = resolve_project(m_history_root, project_path);
        if (!project.error.ok()) return failure<ProjectHistoryCommitResult>(project.error.code, project.error.message);

        ProjectHistoryCommitResult result;
        result.repository_path = project.repository_path;

        if (snapshot_path.empty() || !has_3mf_extension(snapshot_path))
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::InvalidArgument, "Completed snapshot must be a .3mf file");
        if (options.message.empty() || options.author_name.empty() || options.author_email.empty() || contains_nul(options.message) || contains_nul(options.author_name) ||
            contains_nul(options.author_email))
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::InvalidArgument,
                                                                       "Commit metadata must be non-empty UTF-8 text without NUL bytes");

        std::error_code ec;
        if (!fs::is_regular_file(snapshot_path, ec) || ec)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::NotFound,
                                                                       "Completed .3mf snapshot does not exist or is not a regular file");
        const std::uintmax_t size_before = fs::file_size(snapshot_path, ec);
        if (ec)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::IoError,
                                                                       "Could not inspect the completed .3mf snapshot: " + ec.message());

        RepositoryPtr       repository;
        ProjectHistoryError repository_error;
        if (!open_repository(project.repository_path, project.identity_hash, true, repository, repository_error))
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, repository_error.code, repository_error.message);

        git_oid blob_oid{};
        if (git_blob_create_from_disk(&blob_oid, repository.get(), path_utf8(snapshot_path).c_str()) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not import completed .3mf snapshot"));

        const std::uintmax_t size_after = fs::file_size(snapshot_path, ec);
        if (ec || size_before != size_after)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::IoError,
                                                                       "Completed .3mf snapshot changed while it was being imported");

        CommitPtr           head;
        bool                has_head = false;
        ProjectHistoryError head_error;
        if (!load_head_commit(repository.get(), head, has_head, head_error))
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, head_error.code, head_error.message);

        bool duplicate = false;
        if (!head_has_blob(head.get(), blob_oid, duplicate, head_error))
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, head_error.code, head_error.message);
        if (duplicate) {
            ProjectHistoryVersion version;
            if (!version_from_commit(repository.get(), head.get(), version, head_error))
                return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, head_error.code, head_error.message);
            result.committed = false;
            result.version   = std::move(version);
            return result;
        }

        git_index *raw_index = nullptr;
        if (git_index_new(&raw_index) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not create project-history index"));
        IndexPtr index(raw_index);

        git_index_entry entry{};
        entry.mode = GIT_FILEMODE_BLOB;
        git_oid_cpy(&entry.id, &blob_oid);
        entry.path = SNAPSHOT_TREE_PATH;
        if (git_index_add(index.get(), &entry) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not stage completed .3mf snapshot"));

        git_oid tree_oid{};
        if (git_index_write_tree_to(&tree_oid, index.get(), repository.get()) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not write project-history tree"));

        git_tree *raw_tree = nullptr;
        if (git_tree_lookup(&raw_tree, repository.get(), &tree_oid) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not load new project-history tree"));
        TreePtr tree(raw_tree);

        const auto     committed_at  = options.committed_at.value_or(std::chrono::system_clock::now());
        const auto     seconds       = std::chrono::duration_cast<std::chrono::seconds>(committed_at.time_since_epoch()).count();
        git_signature *raw_signature = nullptr;
        if (git_signature_new(&raw_signature, options.author_name.c_str(), options.author_email.c_str(), static_cast<git_time_t>(seconds), 0) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::InvalidArgument,
                                                                       git_error_message("Could not create project-history signature"));
        SignaturePtr signature(raw_signature);

        git_oid           commit_oid{};
        const git_commit *parents[] = {head.get()};
        if (git_commit_create(&commit_oid, repository.get(), "HEAD", signature.get(), signature.get(), nullptr, options.message.c_str(), tree.get(), has_head ? 1 : 0,
                              has_head ? parents : nullptr) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not commit project-history snapshot"));

        git_commit *raw_commit = nullptr;
        if (git_commit_lookup(&raw_commit, repository.get(), &commit_oid) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not read the new project-history commit"));
        CommitPtr             commit(raw_commit);
        ProjectHistoryVersion version;
        if (!version_from_commit(repository.get(), commit.get(), version, head_error))
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, head_error.code, head_error.message);

        result.committed = true;
        result.version   = std::move(version);
        return result;
    }

    ProjectHistoryListResult list(const fs::path &project_path, std::size_t max_count)
    {
        ResolvedProject project = resolve_project(m_history_root, project_path);
        if (!project.error.ok()) return failure<ProjectHistoryListResult>(project.error.code, project.error.message);

        ProjectHistoryListResult result;
        result.repository_path = project.repository_path;
        RepositoryPtr       repository;
        ProjectHistoryError repository_error;
        if (!open_repository(project.repository_path, project.identity_hash, false, repository, repository_error)) {
            if (repository_error.code == ProjectHistoryErrorCode::NotFound) return result;
            return failure_with_repository<ProjectHistoryListResult>(result.repository_path, repository_error.code, repository_error.message);
        }

        git_revwalk *raw_walk = nullptr;
        if (git_revwalk_new(&raw_walk, repository.get()) != 0)
            return failure_with_repository<ProjectHistoryListResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                     git_error_message("Could not create project-history revision walk"));
        RevwalkPtr walk(raw_walk);
        if (git_revwalk_sorting(walk.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME) != 0)
            return failure_with_repository<ProjectHistoryListResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                     git_error_message("Could not sort project-history revisions"));

        const int push_rc = git_revwalk_push_head(walk.get());
        if (push_rc == GIT_EUNBORNBRANCH || push_rc == GIT_ENOTFOUND) return result;
        if (push_rc != 0)
            return failure_with_repository<ProjectHistoryListResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                     git_error_message("Could not read project-history revisions"));

        git_oid commit_oid{};
        int     walk_rc = 0;
        while ((walk_rc = git_revwalk_next(&commit_oid, walk.get())) == 0) {
            git_commit *raw_commit = nullptr;
            if (git_commit_lookup(&raw_commit, repository.get(), &commit_oid) != 0)
                return failure_with_repository<ProjectHistoryListResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                         git_error_message("Could not load a project-history revision"));
            CommitPtr             commit(raw_commit);
            ProjectHistoryVersion version;
            if (!version_from_commit(repository.get(), commit.get(), version, repository_error))
                return failure_with_repository<ProjectHistoryListResult>(result.repository_path, repository_error.code, repository_error.message);
            result.versions.emplace_back(std::move(version));
            if (max_count != 0 && result.versions.size() >= max_count) break;
        }
        if (walk_rc != 0 && walk_rc != GIT_ITEROVER)
            return failure_with_repository<ProjectHistoryListResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                     git_error_message("Could not finish reading project-history revisions"));
        return result;
    }

    ProjectHistoryRestoreResult restore(const fs::path &project_path, const std::string &commit_id, const fs::path &destination_path)
    {
        if (commit_id.size() != GIT_OID_SHA1_HEXSIZE || !std::all_of(commit_id.begin(), commit_id.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::InvalidArgument, "History version must be a full 40-character Git commit identifier");
        if (destination_path.empty() || !has_3mf_extension(destination_path))
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::InvalidArgument, "Restore destination must be a new .3mf path");
        if (path_is_within(destination_path, m_history_root))
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::InvalidArgument, "Restore destination cannot be inside the managed project-history directory");

        std::error_code ec;
        if (fs::exists(destination_path, ec)) return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::DestinationExists, "Restore destination already exists");
        if (ec) return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not inspect restore destination: " + ec.message());

        ResolvedProject project = resolve_project(m_history_root, project_path);
        if (!project.error.ok()) return failure<ProjectHistoryRestoreResult>(project.error.code, project.error.message);

        RepositoryPtr       repository;
        ProjectHistoryError repository_error;
        if (!open_repository(project.repository_path, project.identity_hash, false, repository, repository_error))
            return failure<ProjectHistoryRestoreResult>(repository_error.code, repository_error.message);

        git_oid requested_oid{};
        if (git_oid_fromstr(&requested_oid, commit_id.c_str()) != 0)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::InvalidArgument, "History version identifier is invalid");

        git_commit *raw_commit = nullptr;
        if (git_commit_lookup(&raw_commit, repository.get(), &requested_oid) != 0)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::NotFound, "Requested project-history version does not exist");
        CommitPtr commit(raw_commit);

        CommitPtr head;
        bool      has_head = false;
        if (!load_head_commit(repository.get(), head, has_head, repository_error) || !has_head)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::RepositoryError,
                                                        repository_error.ok() ? "Project-history repository has no HEAD commit" : repository_error.message);
        if (git_oid_equal(git_commit_id(head.get()), &requested_oid) != 1) {
            const int reachable = git_graph_descendant_of(repository.get(), git_commit_id(head.get()), &requested_oid);
            if (reachable < 0)
                return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not verify project-history version reachability"));
            if (reachable == 0) return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::NotFound, "Requested version is not part of this project's current history");
        }

        ProjectHistoryVersion version;
        if (!version_from_commit(repository.get(), commit.get(), version, repository_error))
            return failure<ProjectHistoryRestoreResult>(repository_error.code, repository_error.message);

        git_tree *raw_tree = nullptr;
        if (git_commit_tree(&raw_tree, commit.get()) != 0)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not read project-history tree for restore"));
        TreePtr         tree(raw_tree);
        git_tree_entry *raw_entry = nullptr;
        if (git_tree_entry_bypath(&raw_entry, tree.get(), SNAPSHOT_TREE_PATH) != 0)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::RepositoryError, "Requested history version does not contain a complete .3mf snapshot");
        TreeEntryPtr entry(raw_entry);
        git_blob    *raw_blob = nullptr;
        if (git_blob_lookup(&raw_blob, repository.get(), git_tree_entry_id(entry.get())) != 0)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not load project-history snapshot for restore"));
        BlobPtr blob(raw_blob);

        fs::path parent = destination_path.parent_path();
        if (parent.empty())
            parent = fs::current_path(ec);
        else
            fs::create_directories(parent, ec);
        if (ec) return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not prepare restore destination: " + ec.message());

        const fs::path staging_path = unique_staging_path(destination_path);
        if (staging_path.empty()) return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not allocate a restore staging path");

        const auto remove_staging = [&staging_path] {
            std::error_code remove_error;
            fs::remove(staging_path, remove_error);
        };

        std::ofstream output(staging_path, std::ios::binary | std::ios::trunc);
        if (!output) return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not create restore staging file");

        const char             *data        = static_cast<const char *>(git_blob_rawcontent(blob.get()));
        std::uint64_t           remaining   = static_cast<std::uint64_t>(git_blob_rawsize(blob.get()));
        constexpr std::uint64_t chunk_limit = 8u * 1024u * 1024u;
        while (remaining != 0) {
            const std::uint64_t chunk = std::min(remaining, chunk_limit);
            output.write(data, static_cast<std::streamsize>(chunk));
            if (!output) {
                output.close();
                remove_staging();
                return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not write restored .3mf snapshot");
            }
            data += chunk;
            remaining -= chunk;
        }
        output.close();
        if (!output) {
            remove_staging();
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not finalize restored .3mf snapshot");
        }

        if (fs::exists(destination_path, ec) || ec) {
            remove_staging();
            return failure<ProjectHistoryRestoreResult>(ec ? ProjectHistoryErrorCode::IoError : ProjectHistoryErrorCode::DestinationExists,
                                                        ec ? "Could not recheck restore destination: " + ec.message() : "Restore destination was created by another operation");
        }
        fs::rename(staging_path, destination_path, ec);
        if (ec) {
            remove_staging();
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not publish restored .3mf snapshot: " + ec.message());
        }

        ProjectHistoryRestoreResult result;
        result.version       = std::move(version);
        result.restored_path = destination_path;
        result.bytes_written = result.version.snapshot_size;
        return result;
    }

    const fs::path &history_root() const noexcept { return m_history_root; }

private:
    template<class Result> static Result failure_with_repository(const fs::path &repository_path, ProjectHistoryErrorCode code, std::string message)
    {
        Result result          = failure<Result>(code, std::move(message));
        result.repository_path = repository_path;
        return result;
    }

    void worker_loop()
    {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_condition.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
                if (m_stopping && m_jobs.empty()) return;
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
            }
            job();
        }
    }

    fs::path                          m_history_root;
    ProjectHistoryError               m_initialization_error;
    bool                              m_libgit2_initialized{false};
    bool                              m_stopping{false};
    std::mutex                        m_mutex;
    std::condition_variable           m_condition;
    std::deque<std::function<void()>> m_jobs;
    std::thread                       m_worker;
};

ProjectHistoryManager::ProjectHistoryManager(fs::path app_data_directory) : m_impl(std::make_unique<Impl>(std::move(app_data_directory))) {}

ProjectHistoryManager::~ProjectHistoryManager() = default;

std::future<ProjectHistoryCommitResult> ProjectHistoryManager::commit_snapshot(fs::path project_path, fs::path completed_snapshot_path, ProjectHistoryCommitOptions options)
{
    return m_impl->enqueue<ProjectHistoryCommitResult>([impl = m_impl.get(), project_path = std::move(project_path), snapshot_path = std::move(completed_snapshot_path),
                                                        options = std::move(options)] { return impl->commit(project_path, snapshot_path, options); });
}

std::future<ProjectHistoryListResult> ProjectHistoryManager::list_versions(fs::path project_path, std::size_t max_count)
{
    return m_impl->enqueue<ProjectHistoryListResult>([impl = m_impl.get(), project_path = std::move(project_path), max_count] { return impl->list(project_path, max_count); });
}

std::future<ProjectHistoryRestoreResult> ProjectHistoryManager::restore_version(fs::path project_path, std::string commit_id, fs::path destination_path)
{
    return m_impl->enqueue<ProjectHistoryRestoreResult>([impl = m_impl.get(), project_path = std::move(project_path), commit_id = std::move(commit_id),
                                                         destination_path = std::move(destination_path)] { return impl->restore(project_path, commit_id, destination_path); });
}

const fs::path &ProjectHistoryManager::history_root() const noexcept { return m_impl->history_root(); }

} // namespace Slic3r
