#include "ProjectHistoryManager.hpp"

#include <git2.h>
#include <openssl/evp.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <exception>
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
constexpr const char *LOCK_DIRECTORY          = "locks";
constexpr const char *SNAPSHOT_TREE_PATH      = "project.3mf";
constexpr const char *CONFIG_LAYOUT_VERSION   = "bambu.projecthistoryversion";
constexpr const char *CONFIG_PROJECT_IDENTITY = "bambu.projectidentitysha256";
constexpr auto        LOCK_WAIT_TIMEOUT        = std::chrono::seconds(30);
constexpr auto        LOCK_RETRY_INTERVAL      = std::chrono::milliseconds(20);

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
using OdbPtr        = GitPtr<git_odb, git_odb_free>;
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

bool update_repository_identity(git_repository *repository, const std::string &identity_hash, ProjectHistoryError &error)
{
    git_config *raw_config = nullptr;
    if (git_repository_config(&raw_config, repository) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not open project-history configuration for migration")};
        return false;
    }
    ConfigPtr config(raw_config);
    if (git_config_set_string(config.get(), CONFIG_PROJECT_IDENTITY, identity_hash.c_str()) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not update project-history ownership metadata")};
        return false;
    }
    return true;
}

bool path_entry_exists(const fs::path &path, bool &exists, ProjectHistoryError &error, const std::string &operation)
{
    std::error_code ec;
    const auto      status = fs::symlink_status(path, ec);
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            exists = false;
            return true;
        }
        error = {ProjectHistoryErrorCode::IoError, operation + ": " + ec.message()};
        return false;
    }
    exists = status.type() != fs::file_type::not_found;
    return true;
}

std::uint64_t current_process_id()
{
#ifdef _WIN32
    return static_cast<std::uint64_t>(::GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

bool path_is_link_like(const fs::path &path, bool &link_like, ProjectHistoryError &error, const std::string &operation)
{
    std::error_code ec;
    const auto      status = fs::symlink_status(path, ec);
    if (ec) {
        error = {ProjectHistoryErrorCode::IoError, operation + ": " + ec.message()};
        return false;
    }
    link_like = fs::is_symlink(status);
#ifdef _WIN32
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        error = {ProjectHistoryErrorCode::IoError,
                 operation + ": " + std::system_category().message(static_cast<int>(::GetLastError()))};
        return false;
    }
    link_like = link_like || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#endif
    return true;
}

bool ensure_private_directory(const fs::path &path, bool create, ProjectHistoryError &error, const std::string &operation)
{
    bool exists = false;
    if (!path_entry_exists(path, exists, error, operation)) return false;
    if (!exists) {
        if (!create) {
            error = {ProjectHistoryErrorCode::NotFound, operation + ": directory does not exist"};
            return false;
        }
        std::error_code ec;
        fs::create_directory(path, ec);
        if (ec) {
            error = {ProjectHistoryErrorCode::IoError, operation + ": " + ec.message()};
            return false;
        }
        if (!path_entry_exists(path, exists, error, operation) || !exists) {
            if (error.ok()) error = {ProjectHistoryErrorCode::IoError, operation + ": directory was not created"};
            return false;
        }
    }

    bool link_like = false;
    if (!path_is_link_like(path, link_like, error, operation)) return false;
    std::error_code ec;
    const auto      status = fs::symlink_status(path, ec);
    if (ec) {
        error = {ProjectHistoryErrorCode::IoError, operation + ": " + ec.message()};
        return false;
    }
    if (link_like || !fs::is_directory(status)) {
        error = {ProjectHistoryErrorCode::RepositoryError, operation + ": expected a private directory, not a link or reparse point"};
        return false;
    }
#ifndef _WIN32
    fs::permissions(path, fs::perms::owner_all, fs::perm_options::replace, ec);
    if (ec) {
        error = {ProjectHistoryErrorCode::IoError, operation + ": could not apply owner-only permissions: " + ec.message()};
        return false;
    }
#endif
    return true;
}

class ScopedTreeRemoval
{
public:
    explicit ScopedTreeRemoval(fs::path path) : m_path(std::move(path)) {}
    ~ScopedTreeRemoval()
    {
        if (!m_active) return;
        std::error_code ignored;
        fs::remove_all(m_path, ignored);
    }

    void release() noexcept { m_active = false; }

private:
    fs::path m_path;
    bool     m_active{true};
};

bool create_private_staging_directory(const fs::path &parent, const std::string &prefix, fs::path &staging_path, ProjectHistoryError &error)
{
    static std::atomic<std::uint64_t> sequence{0};
    const auto                        clock_value = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned int attempt = 0; attempt < 32; ++attempt) {
        std::ostringstream name;
        name << prefix << current_process_id() << '-' << std::hex << clock_value << '-' << sequence.fetch_add(1);
        const fs::path candidate = parent / name.str();
        std::error_code ec;
        if (!fs::create_directory(candidate, ec)) {
            if (ec) {
                bool exists = false;
                ProjectHistoryError inspect_error;
                if (!path_entry_exists(candidate, exists, inspect_error, "Could not inspect project-history staging candidate") || !exists) {
                    error = {ProjectHistoryErrorCode::IoError, "Could not create a private project-history staging directory: " + ec.message()};
                    return false;
                }
            }
            continue;
        }
        if (!ensure_private_directory(candidate, false, error, "Could not secure project-history staging directory")) {
            std::error_code ignored;
            fs::remove_all(candidate, ignored);
            return false;
        }
        staging_path = candidate;
        return true;
    }
    error = {ProjectHistoryErrorCode::IoError, "Could not allocate a private project-history staging directory"};
    return false;
}

bool open_existing_repository(const fs::path &repository_path, const std::string &identity_hash, RepositoryPtr &repository, ProjectHistoryError &error)
{
    if (!ensure_private_directory(repository_path, false, error, "Could not validate project-history location")) return false;

    git_repository *raw_repository = nullptr;
    if (git_repository_open_bare(&raw_repository, path_utf8(repository_path).c_str()) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not open project-history repository")};
        return false;
    }
    repository.reset(raw_repository);
    return configure_repository(repository.get(), identity_hash, false, error);
}

bool open_repository(const fs::path   &repository_path,
                     const std::string &identity_hash,
                     bool              create,
                     RepositoryPtr     &repository,
                     ProjectHistoryError &error,
                     bool              *created = nullptr)
{
    if (created != nullptr) *created = false;
    bool exists = false;
    if (!path_entry_exists(repository_path, exists, error, "Could not inspect the project-history location")) return false;
    if (exists) return open_existing_repository(repository_path, identity_hash, repository, error);
    if (!create) {
        error = {ProjectHistoryErrorCode::NotFound, "No history exists for this project"};
        return false;
    }

    fs::path staging_path;
    if (!create_private_staging_directory(repository_path.parent_path(), ".create-" + identity_hash.substr(0, 12) + '-', staging_path, error)) return false;
    ScopedTreeRemoval remove_staging(staging_path);

    git_repository_init_options options = GIT_REPOSITORY_INIT_OPTIONS_INIT;
    options.flags                       = GIT_REPOSITORY_INIT_BARE | GIT_REPOSITORY_INIT_NO_REINIT;
    options.mode                        = static_cast<std::uint32_t>(0700);
    options.description                 = "Bambu Studio isolated project history";
    options.initial_head                = "main";

    git_repository *raw_repository = nullptr;
    if (git_repository_init_ext(&raw_repository, path_utf8(staging_path).c_str(), &options) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not initialize staged project-history repository")};
        if (raw_repository != nullptr) git_repository_free(raw_repository);
        return false;
    }
    RepositoryPtr staged_repository(raw_repository);
    if (!configure_repository(staged_repository.get(), identity_hash, true, error)) {
        staged_repository.reset();
        return false;
    }
    staged_repository.reset();

    RepositoryPtr validated_repository;
    if (!open_existing_repository(staging_path, identity_hash, validated_repository, error)) return false;
    validated_repository.reset();

    if (!path_entry_exists(repository_path, exists, error, "Could not recheck the project-history location")) return false;
    if (exists) {
        error = {ProjectHistoryErrorCode::DestinationExists, "Project-history location was created by another operation"};
        return false;
    }

    std::error_code ec;
    fs::rename(staging_path, repository_path, ec);
    if (ec) {
        error = {ProjectHistoryErrorCode::IoError, "Could not publish initialized project-history repository: " + ec.message()};
        return false;
    }
    remove_staging.release();
    if (created != nullptr) *created = true;
    return open_existing_repository(repository_path, identity_hash, repository, error);
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

struct CommitMutationState
{
    bool        repository_created{false};
    bool        baseline_known{false};
    bool        baseline_has_head{false};
    std::string baseline_head;
    bool        commit_attempted{false};
    bool        head_advanced{false};
};

bool oid_to_string(const git_oid *oid, std::string &value)
{
    char oid_buffer[GIT_OID_MAX_HEXSIZE + 1]{};
    if (oid == nullptr || git_oid_tostr(oid_buffer, sizeof(oid_buffer), oid) == nullptr) return false;
    value = oid_buffer;
    return true;
}

bool repository_head_matches(const fs::path              &repository_path,
                             const std::string           &identity_hash,
                             const CommitMutationState   &expected,
                             bool                        &matches,
                             ProjectHistoryError         &error)
{
    matches = false;
    if (!expected.baseline_known) {
        error = {ProjectHistoryErrorCode::InternalError, "Project-history HEAD baseline is unavailable"};
        return false;
    }

    RepositoryPtr repository;
    if (!open_repository(repository_path, identity_hash, false, repository, error)) return false;

    CommitPtr head;
    bool      has_head = false;
    if (!load_head_commit(repository.get(), head, has_head, error)) return false;
    if (has_head != expected.baseline_has_head) return true;
    if (!has_head) {
        matches = true;
        return true;
    }

    std::string current_head;
    if (!oid_to_string(git_commit_id(head.get()), current_head)) {
        error = {ProjectHistoryErrorCode::RepositoryError, "Could not format the current project-history HEAD"};
        return false;
    }
    matches = current_head == expected.baseline_head;
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

    git_odb *raw_odb = nullptr;
    if (git_repository_odb(&raw_odb, repository) != 0) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not open project-history object database")};
        return false;
    }
    OdbPtr       odb(raw_odb);
    std::size_t  object_size = 0;
    git_object_t object_type = GIT_OBJECT_INVALID;
    if (git_odb_read_header(&object_size, &object_type, odb.get(), git_tree_entry_id(entry.get())) != 0 || object_type != GIT_OBJECT_BLOB) {
        error = {ProjectHistoryErrorCode::RepositoryError, git_error_message("Could not read project-history snapshot header")};
        return false;
    }
    version.snapshot_size = static_cast<std::uint64_t>(object_size);
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

bool comparable_path(const fs::path &path, std::string &value, ProjectHistoryError &error, const std::string &operation)
{
    std::error_code ec;
    fs::path        absolute = fs::absolute(path, ec);
    if (ec) {
        error = {ProjectHistoryErrorCode::IoError, operation + ": " + ec.message()};
        return false;
    }
    absolute = fs::weakly_canonical(absolute, ec);
    if (ec) {
        error = {ProjectHistoryErrorCode::IoError, operation + ": " + ec.message()};
        return false;
    }
    value = absolute.lexically_normal().generic_u8string();
#ifdef _WIN32
    value = lowercase_ascii(std::move(value));
#endif
    return true;
}

bool path_is_within(const fs::path &candidate, const fs::path &root, bool &within, ProjectHistoryError &error)
{
    std::string candidate_string;
    std::string root_string;
    if (!comparable_path(candidate, candidate_string, error, "Could not resolve restore destination") ||
        !comparable_path(root, root_string, error, "Could not resolve managed project-history directory"))
        return false;
    within = candidate_string == root_string;
    if (within) return true;
    if (!root_string.empty() && root_string.back() != '/') root_string.push_back('/');
    within = candidate_string.size() > root_string.size() && candidate_string.compare(0, root_string.size(), root_string) == 0;
    return true;
}

fs::path unique_staging_path(const fs::path &destination)
{
    static std::atomic<std::uint64_t> sequence{0};
    const auto                        clock_value = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned int attempt = 0; attempt < 16; ++attempt) {
        std::ostringstream suffix;
        suffix << ".bambu-history-part-" << current_process_id() << '-' << std::hex << clock_value << '-' << sequence.fetch_add(1);
        fs::path candidate = destination;
        candidate += suffix.str();
        bool                exists = false;
        ProjectHistoryError error;
        if (path_entry_exists(candidate, exists, error, "Could not inspect restore staging path") && !exists) return candidate;
    }
    return {};
}

bool write_new_private_file(const fs::path &path, const char *data, std::uint64_t size, ProjectHistoryError &error)
{
#ifdef _WIN32
    HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = {ProjectHistoryErrorCode::IoError,
                 "Could not create restore staging file exclusively: " +
                     std::system_category().message(static_cast<int>(::GetLastError()))};
        return false;
    }
    BY_HANDLE_FILE_INFORMATION information{};
    const bool                 information_ok = ::GetFileInformationByHandle(handle, &information) != FALSE;
    if (!information_ok || (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || information.nNumberOfLinks != 1) {
        const DWORD native_error = information_ok ? ERROR_SUCCESS : ::GetLastError();
        ::CloseHandle(handle);
        error = {ProjectHistoryErrorCode::IoError,
                 native_error == ERROR_SUCCESS ? "Restore staging file is not a private regular file"
                                               : "Could not validate restore staging file: " +
                                                     std::system_category().message(static_cast<int>(native_error))};
        return false;
    }
    while (size != 0) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::uint64_t>(size, 8u * 1024u * 1024u));
        DWORD       written = 0;
        if (!::WriteFile(handle, data, chunk, &written, nullptr) || written != chunk) {
            const DWORD native_error = ::GetLastError();
            ::CloseHandle(handle);
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not write restored .3mf snapshot: " + std::system_category().message(static_cast<int>(native_error))};
            return false;
        }
        data += written;
        size -= written;
    }
    if (!::FlushFileBuffers(handle)) {
        const DWORD native_error = ::GetLastError();
        ::CloseHandle(handle);
        error = {ProjectHistoryErrorCode::IoError,
                 "Could not finalize restored .3mf snapshot: " + std::system_category().message(static_cast<int>(native_error))};
        return false;
    }
    ::CloseHandle(handle);
    return true;
#else
    int flags = O_WRONLY | O_CREAT | O_EXCL;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    int descriptor = -1;
    do {
        descriptor = ::open(path.c_str(), flags, S_IRUSR | S_IWUSR);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) {
        error = {ProjectHistoryErrorCode::IoError,
                 "Could not create restore staging file exclusively: " + std::generic_category().message(errno)};
        return false;
    }
    while (size != 0) {
        const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(size, 8u * 1024u * 1024u));
        const ssize_t     written = ::write(descriptor, data, chunk);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            const int native_error = errno;
            ::close(descriptor);
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not write restored .3mf snapshot: " + std::generic_category().message(native_error)};
            return false;
        }
        data += written;
        size -= static_cast<std::uint64_t>(written);
    }
    if (::fsync(descriptor) != 0) {
        const int native_error = errno;
        ::close(descriptor);
        error = {ProjectHistoryErrorCode::IoError,
                 "Could not finalize restored .3mf snapshot: " + std::generic_category().message(native_error)};
        return false;
    }
    if (::close(descriptor) != 0) {
        error = {ProjectHistoryErrorCode::IoError,
                 "Could not close restored .3mf snapshot: " + std::generic_category().message(errno)};
        return false;
    }
    return true;
#endif
}

bool publish_new_file(const fs::path &staging_path, const fs::path &destination_path, ProjectHistoryError &error)
{
#ifdef _WIN32
    if (::MoveFileExW(staging_path.c_str(), destination_path.c_str(), MOVEFILE_WRITE_THROUGH)) return true;
    const DWORD native_error = ::GetLastError();
    error = {native_error == ERROR_FILE_EXISTS || native_error == ERROR_ALREADY_EXISTS ? ProjectHistoryErrorCode::DestinationExists
                                                                                       : ProjectHistoryErrorCode::IoError,
             native_error == ERROR_FILE_EXISTS || native_error == ERROR_ALREADY_EXISTS
                 ? "Restore destination was created by another operation"
                 : "Could not publish restored .3mf snapshot: " + std::system_category().message(static_cast<int>(native_error))};
    return false;
#else
    if (::link(staging_path.c_str(), destination_path.c_str()) != 0) {
        const int native_error = errno;
        error = {native_error == EEXIST ? ProjectHistoryErrorCode::DestinationExists : ProjectHistoryErrorCode::IoError,
                 native_error == EEXIST ? "Restore destination was created by another operation"
                                        : "Could not publish restored .3mf snapshot: " + std::generic_category().message(native_error)};
        return false;
    }
    std::error_code ignored;
    fs::remove(staging_path, ignored);
    return true;
#endif
}

bool repository_tree_is_copy_safe(const fs::path &repository_path, ProjectHistoryError &error)
{
    std::error_code ec;
    fs::recursive_directory_iterator iterator(repository_path, fs::directory_options::none, ec);
    const fs::recursive_directory_iterator end;
    if (ec) {
        error = {ProjectHistoryErrorCode::IoError, "Could not inspect project-history repository before migration: " + ec.message()};
        return false;
    }
    while (iterator != end) {
        bool link_like = false;
        if (!path_is_link_like(iterator->path(), link_like, error, "Could not validate project-history migration source")) return false;
        if (link_like) {
            error = {ProjectHistoryErrorCode::RepositoryError, "Project-history migration source contains a link or reparse point"};
            return false;
        }
        iterator.increment(ec);
        if (ec) {
            error = {ProjectHistoryErrorCode::IoError, "Could not inspect project-history repository before migration: " + ec.message()};
            return false;
        }
    }
    return true;
}

class InterprocessFileLock
{
public:
    InterprocessFileLock() = default;
    InterprocessFileLock(const InterprocessFileLock &)            = delete;
    InterprocessFileLock &operator=(const InterprocessFileLock &) = delete;

    ~InterprocessFileLock()
    {
#ifdef _WIN32
        if (m_handle != INVALID_HANDLE_VALUE) ::CloseHandle(m_handle);
#else
        if (m_fd >= 0) {
            ::flock(m_fd, LOCK_UN);
            ::close(m_fd);
        }
#endif
    }

    bool acquire(const fs::path &lock_path, const std::function<bool()> &stop_requested, ProjectHistoryError &error)
    {
        const auto deadline = std::chrono::steady_clock::now() + LOCK_WAIT_TIMEOUT;
        const auto interrupted = [&error, &stop_requested]() {
            if (!stop_requested || !stop_requested()) return false;
            error = {ProjectHistoryErrorCode::ShuttingDown,
                     "Project-history manager is shutting down while waiting for another process"};
            return true;
        };
        const auto timed_out = [&error, deadline]() {
            if (std::chrono::steady_clock::now() < deadline) return false;
            error = {ProjectHistoryErrorCode::IoError,
                     "Timed out waiting for another Bambu Studio process to release the project-history lock; the operation can be retried"};
            return true;
        };
        if (interrupted()) return false;
#ifdef _WIN32
        while (true) {
            if (interrupted()) return false;
            HANDLE handle = ::CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (handle != INVALID_HANDLE_VALUE) {
                if (::GetFileType(handle) != FILE_TYPE_DISK) {
                    ::CloseHandle(handle);
                    error = {ProjectHistoryErrorCode::IoError, "Project-history lock is not a regular disk file"};
                    return false;
                }
                BY_HANDLE_FILE_INFORMATION attributes{};
                if (!::GetFileInformationByHandle(handle, &attributes)) {
                    const DWORD native_error = ::GetLastError();
                    ::CloseHandle(handle);
                    error = {ProjectHistoryErrorCode::IoError,
                             "Could not validate project-history lock: " + std::system_category().message(static_cast<int>(native_error))};
                    return false;
                }
                if ((attributes.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                    ::CloseHandle(handle);
                    error = {ProjectHistoryErrorCode::IoError, "Project-history lock cannot be a link or reparse point"};
                    return false;
                }
                m_handle = handle;
                return true;
            }

            const DWORD native_error = ::GetLastError();
            if (native_error == ERROR_SHARING_VIOLATION || native_error == ERROR_LOCK_VIOLATION) {
                if (timed_out()) return false;
                std::this_thread::sleep_for(LOCK_RETRY_INTERVAL);
                continue;
            }
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not acquire project-history lock: " + std::system_category().message(static_cast<int>(native_error))};
            return false;
        }
#else
        int flags = O_CREAT | O_RDWR;
#ifdef O_CLOEXEC
        flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        int descriptor = -1;
        do {
            descriptor = ::open(lock_path.c_str(), flags, S_IRUSR | S_IWUSR);
        } while (descriptor < 0 && errno == EINTR);
        if (descriptor < 0) {
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not open project-history lock: " + std::generic_category().message(errno)};
            return false;
        }

        struct stat descriptor_status {};
        if (::fstat(descriptor, &descriptor_status) != 0) {
            const int native_error = errno;
            ::close(descriptor);
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not inspect project-history lock: " + std::generic_category().message(native_error)};
            return false;
        }
        if (!S_ISREG(descriptor_status.st_mode) || descriptor_status.st_uid != ::geteuid() || descriptor_status.st_nlink != 1) {
            ::close(descriptor);
            error = {ProjectHistoryErrorCode::IoError, "Project-history lock is not a private, singly linked regular file"};
            return false;
        }
        if (::fchmod(descriptor, S_IRUSR | S_IWUSR) != 0) {
            const int native_error = errno;
            ::close(descriptor);
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not secure project-history lock: " + std::generic_category().message(native_error)};
            return false;
        }
        while (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
            if (errno == EINTR) {
                if (interrupted() || timed_out()) {
                    ::close(descriptor);
                    return false;
                }
                continue;
            }
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (interrupted() || timed_out()) {
                    ::close(descriptor);
                    return false;
                }
                std::this_thread::sleep_for(LOCK_RETRY_INTERVAL);
                continue;
            }
            const int native_error = errno;
            ::close(descriptor);
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not acquire project-history lock: " + std::generic_category().message(native_error)};
            return false;
        }

        struct stat path_status {};
        if (::lstat(lock_path.c_str(), &path_status) != 0) {
            const int native_error = errno;
            ::flock(descriptor, LOCK_UN);
            ::close(descriptor);
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not recheck project-history lock: " + std::generic_category().message(native_error)};
            return false;
        }
        if (path_status.st_dev != descriptor_status.st_dev || path_status.st_ino != descriptor_status.st_ino) {
            ::flock(descriptor, LOCK_UN);
            ::close(descriptor);
            error = {ProjectHistoryErrorCode::IoError, "Project-history lock changed while it was acquired"};
            return false;
        }
        m_fd = descriptor;
        return true;
#endif
    }

private:
#ifdef _WIN32
    HANDLE m_handle{INVALID_HANDLE_VALUE};
#else
    int m_fd{-1};
#endif
};

struct SnapshotPreflight
{
    ProjectHistoryError error;
    std::uintmax_t       size{0};
};

SnapshotPreflight validate_snapshot_commit(const fs::path &snapshot_path, const ProjectHistoryCommitOptions &options)
{
    SnapshotPreflight result;
    if (snapshot_path.empty() || !has_3mf_extension(snapshot_path)) {
        result.error = {ProjectHistoryErrorCode::InvalidArgument, "Completed snapshot must be a .3mf file"};
        return result;
    }
    if (options.message.empty() || options.author_name.empty() || options.author_email.empty() || contains_nul(options.message) || contains_nul(options.author_name) ||
        contains_nul(options.author_email)) {
        result.error = {ProjectHistoryErrorCode::InvalidArgument, "Commit metadata must be non-empty UTF-8 text without NUL bytes"};
        return result;
    }

    std::error_code ec;
    if (!fs::is_regular_file(snapshot_path, ec) || ec) {
        result.error = {ProjectHistoryErrorCode::NotFound, "Completed .3mf snapshot does not exist or is not a regular file"};
        return result;
    }
    result.size = fs::file_size(snapshot_path, ec);
    if (ec) result.error = {ProjectHistoryErrorCode::IoError, "Could not inspect the completed .3mf snapshot: " + ec.message()};
    return result;
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
        m_app_data_directory = std::move(app_data_directory);
        m_history_base       = m_app_data_directory / HISTORY_DIRECTORY;
        m_history_root       = m_history_base / HISTORY_LAYOUT_VERSION;
        m_lock_root          = m_history_base / LOCK_DIRECTORY / HISTORY_LAYOUT_VERSION;

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

    bool prepare_private_storage(ProjectHistoryError &error)
    {
        std::error_code ec;
        fs::create_directories(m_app_data_directory, ec);
        if (ec || !fs::is_directory(m_app_data_directory, ec)) {
            error = {ProjectHistoryErrorCode::IoError,
                     "Could not prepare the application data directory for project history" + (ec ? ": " + ec.message() : std::string())};
            return false;
        }
        return ensure_private_directory(m_history_base, true, error, "Could not prepare private project-history directory") &&
               ensure_private_directory(m_history_root, true, error, "Could not prepare private project-history repository root") &&
               ensure_private_directory(m_lock_root.parent_path(), true, error, "Could not prepare private project-history lock directory") &&
               ensure_private_directory(m_lock_root, true, error, "Could not prepare private project-history lock root");
    }

    bool acquire_identity_locks(std::vector<std::string> identity_hashes,
                                std::vector<std::unique_ptr<InterprocessFileLock>> &locks,
                                ProjectHistoryError &error)
    {
        if (!prepare_private_storage(error)) return false;
        std::sort(identity_hashes.begin(), identity_hashes.end());
        identity_hashes.erase(std::unique(identity_hashes.begin(), identity_hashes.end()), identity_hashes.end());
        for (const std::string &identity_hash : identity_hashes) {
            auto lock = std::make_unique<InterprocessFileLock>();
            // Shutdown rejects newly enqueued work but joins the worker only after
            // work already accepted by it has completed.  Do not cancel a
            // queued operation merely because it is waiting on another process's
            // bounded lock; otherwise a Save As or commit can be silently lost
            // during application shutdown.
            if (!lock->acquire(m_lock_root / (identity_hash + ".lock"), {}, error))
                return false;
            locks.emplace_back(std::move(lock));
        }
        return true;
    }

    ProjectHistoryMigrationResult migrate(const fs::path &previous_project_path, const fs::path &new_project_path)
    {
        const ResolvedProject source      = resolve_project(m_history_root, previous_project_path);
        const ResolvedProject destination = resolve_project(m_history_root, new_project_path);

        ProjectHistoryMigrationResult result;
        result.source_repository_path      = source.repository_path;
        result.destination_repository_path = destination.repository_path;
        if (!source.error.ok()) {
            result.error = source.error;
            return result;
        }
        if (!destination.error.ok()) {
            result.error = destination.error;
            return result;
        }

        std::vector<std::unique_ptr<InterprocessFileLock>> locks;
        ProjectHistoryError                                lock_error;
        if (!acquire_identity_locks({source.identity_hash, destination.identity_hash}, locks, lock_error)) {
            result.error = lock_error;
            return result;
        }
        return migrate_resolved(source, destination);
    }

    ProjectHistoryMigrationResult migrate_resolved(const ResolvedProject &source, const ResolvedProject &destination)
    {
        ProjectHistoryMigrationResult result;
        result.source_repository_path      = source.repository_path;
        result.destination_repository_path = destination.repository_path;
        const auto fail = [&result](ProjectHistoryErrorCode code, std::string message) {
            result.error = {code, std::move(message)};
            return result;
        };

        if (source.identity_hash == destination.identity_hash) return result;

        bool                destination_exists = false;
        ProjectHistoryError path_error;
        if (!path_entry_exists(destination.repository_path, destination_exists, path_error, "Could not inspect destination project-history location"))
            return fail(path_error.code, path_error.message);
        if (destination_exists)
            return fail(ProjectHistoryErrorCode::DestinationExists,
                        "History already exists for the destination project identity; unrelated histories will not be merged");

        bool source_exists = false;
        if (!path_entry_exists(source.repository_path, source_exists, path_error, "Could not inspect source project-history location"))
            return fail(path_error.code, path_error.message);
        if (!source_exists) return result;

        RepositoryPtr       source_repository;
        ProjectHistoryError repository_error;
        if (!open_repository(source.repository_path, source.identity_hash, false, source_repository, repository_error))
            return fail(repository_error.code, repository_error.message);

        CommitPtr source_head;
        bool      source_has_head = false;
        if (!load_head_commit(source_repository.get(), source_head, source_has_head, repository_error))
            return fail(repository_error.code, repository_error.message);

        if (!repository_tree_is_copy_safe(source.repository_path, repository_error))
            return fail(repository_error.code, repository_error.message);

        fs::path staging_path;
        if (!create_private_staging_directory(m_history_root, ".migrate-", staging_path, repository_error))
            return fail(repository_error.code, repository_error.message);
        ScopedTreeRemoval remove_staging(staging_path);

        std::error_code ec;
        fs::directory_iterator source_entry(source.repository_path, ec);
        const fs::directory_iterator source_end;
        if (ec) return fail(ProjectHistoryErrorCode::IoError, "Could not read project-history migration source: " + ec.message());
        for (; source_entry != source_end; source_entry.increment(ec)) {
            if (ec) return fail(ProjectHistoryErrorCode::IoError, "Could not read project-history migration source: " + ec.message());
            fs::copy(source_entry->path(), staging_path / source_entry->path().filename(), fs::copy_options::recursive | fs::copy_options::skip_symlinks, ec);
            if (ec) return fail(ProjectHistoryErrorCode::IoError, "Could not stage project-history migration: " + ec.message());
        }
        if (ec) return fail(ProjectHistoryErrorCode::IoError, "Could not read project-history migration source: " + ec.message());

        RepositoryPtr staged_repository;
        if (!open_repository(staging_path, source.identity_hash, false, staged_repository, repository_error))
            return fail(repository_error.code, "Could not validate staged project history: " + repository_error.message);
        if (!update_repository_identity(staged_repository.get(), destination.identity_hash, repository_error))
            return fail(repository_error.code, repository_error.message);
        staged_repository.reset();

        if (!open_repository(staging_path, destination.identity_hash, false, staged_repository, repository_error))
            return fail(repository_error.code, "Could not validate migrated project-history ownership: " + repository_error.message);

        CommitPtr staged_head;
        bool      staged_has_head = false;
        if (!load_head_commit(staged_repository.get(), staged_head, staged_has_head, repository_error))
            return fail(repository_error.code, repository_error.message);
        if (source_has_head != staged_has_head ||
            (source_has_head && git_oid_equal(git_commit_id(source_head.get()), git_commit_id(staged_head.get())) != 1))
            return fail(ProjectHistoryErrorCode::RepositoryError, "Staged project history did not preserve the source HEAD commit");

        // Close all handles before the directory rename; Windows does not allow
        // publication while libgit2 still has configuration files open.
        staged_head.reset();
        staged_repository.reset();
        source_head.reset();
        source_repository.reset();

        if (!path_entry_exists(destination.repository_path, destination_exists, path_error, "Could not recheck destination project-history location"))
            return fail(path_error.code, path_error.message);
        if (destination_exists)
            return fail(ProjectHistoryErrorCode::DestinationExists,
                        "History was created for the destination project identity during migration; no histories were merged");

        fs::rename(staging_path, destination.repository_path, ec);
        if (ec) return fail(ProjectHistoryErrorCode::IoError, "Could not publish migrated project history: " + ec.message());

        remove_staging.release();
        result.migrated = true;
        return result;
    }

    ProjectHistoryCommitResult migrate_then_commit(const fs::path                       &previous_project_path,
                                                    const fs::path                       &new_project_path,
                                                    const fs::path                       &snapshot_path,
                                                    const ProjectHistoryCommitOptions    &options)
    {
        const ResolvedProject source      = resolve_project(m_history_root, previous_project_path);
        const ResolvedProject destination = resolve_project(m_history_root, new_project_path);

        ProjectHistoryCommitResult result;
        result.previous_repository_path = source.repository_path;
        result.repository_path          = destination.repository_path;
        if (!source.error.ok()) {
            result.error = source.error;
            return result;
        }
        if (!destination.error.ok()) {
            result.error = destination.error;
            return result;
        }

        std::vector<std::unique_ptr<InterprocessFileLock>> locks;
        ProjectHistoryError                                lock_error;
        if (!acquire_identity_locks({source.identity_hash, destination.identity_hash}, locks, lock_error)) {
            result.error = lock_error;
            return result;
        }

        bool                destination_preexisted = false;
        ProjectHistoryError path_error;
        if (!path_entry_exists(destination.repository_path, destination_preexisted, path_error,
                               "Could not inspect destination project-history location before Save As")) {
            result.error = path_error;
            return result;
        }

        // Preflight before publishing a fork so an invalid or incomplete saved
        // snapshot cannot create a new project identity by itself.
        const SnapshotPreflight preflight = validate_snapshot_commit(snapshot_path, options);
        if (!preflight.error.ok()) {
            result.error = preflight.error;
            return result;
        }

        ProjectHistoryMigrationResult migration = migrate_resolved(source, destination);
        if (!migration.ok()) {
            result.error                    = migration.error;
            result.previous_repository_path = migration.source_repository_path;
            result.repository_path          = migration.destination_repository_path;
            return result;
        }

        CommitMutationState mutation_state;
        result                          = commit_resolved(destination, snapshot_path, options, &mutation_state);
        result.history_migrated         = migration.migrated;
        result.previous_repository_path = migration.source_repository_path;
        if (!result.ok() && !destination_preexisted) {
            // Delete only a repository created by this composite operation and
            // only while its HEAD still matches the baseline observed before
            // git_commit_create. A successful ref update followed by a metadata
            // read error must never erase a valid newly committed history.
            const bool operation_created_destination = migration.migrated || mutation_state.repository_created;
            bool       safe_to_remove                 = operation_created_destination && !mutation_state.head_advanced;
            if (safe_to_remove && mutation_state.commit_attempted) {
                ProjectHistoryError verification_error;
                bool                head_matches = false;
                if (!repository_head_matches(destination.repository_path, destination.identity_hash, mutation_state, head_matches,
                                             verification_error) || !head_matches) {
                    safe_to_remove = false;
                    result.error.message += verification_error.ok()
                                                ? "; destination history was retained because its HEAD advanced during the failed commit"
                                                : "; destination history was retained because its HEAD could not be safely verified: " + verification_error.message;
                }
            } else if (operation_created_destination && mutation_state.head_advanced) {
                result.error.message += "; destination history was retained because its HEAD advanced before the post-commit error";
            }

            if (safe_to_remove) {
                std::error_code rollback_error;
                fs::remove_all(destination.repository_path, rollback_error);
                if (rollback_error) {
                    result.error.code = ProjectHistoryErrorCode::IoError;
                    result.error.message += "; could not roll back destination project history: " + rollback_error.message();
                }
            }
        }
        return result;
    }

    ProjectHistoryCommitResult commit(const fs::path &project_path, const fs::path &snapshot_path, const ProjectHistoryCommitOptions &options)
    {
        ResolvedProject project = resolve_project(m_history_root, project_path);
        if (!project.error.ok()) return failure<ProjectHistoryCommitResult>(project.error.code, project.error.message);

        ProjectHistoryCommitResult result;
        result.repository_path = project.repository_path;

        std::vector<std::unique_ptr<InterprocessFileLock>> locks;
        ProjectHistoryError                                lock_error;
        if (!acquire_identity_locks({project.identity_hash}, locks, lock_error)) {
            result.error = lock_error;
            return result;
        }
        return commit_resolved(project, snapshot_path, options);
    }

    ProjectHistoryCommitResult commit_resolved(const ResolvedProject              &project,
                                                const fs::path                     &snapshot_path,
                                                const ProjectHistoryCommitOptions  &options,
                                                CommitMutationState                *mutation_state = nullptr)
    {
        if (mutation_state != nullptr) *mutation_state = {};
        ProjectHistoryCommitResult result;
        result.repository_path = project.repository_path;

        const SnapshotPreflight preflight = validate_snapshot_commit(snapshot_path, options);
        if (!preflight.error.ok())
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, preflight.error.code, preflight.error.message);
        const std::uintmax_t size_before = preflight.size;

        RepositoryPtr       repository;
        ProjectHistoryError repository_error;
        bool                repository_created = false;
        if (!open_repository(project.repository_path, project.identity_hash, true, repository, repository_error, &repository_created)) {
            if (mutation_state != nullptr) mutation_state->repository_created = repository_created;
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, repository_error.code, repository_error.message);
        }
        if (mutation_state != nullptr) mutation_state->repository_created = repository_created;

        git_oid blob_oid{};
        if (git_blob_create_from_disk(&blob_oid, repository.get(), path_utf8(snapshot_path).c_str()) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not import completed .3mf snapshot"));

        std::error_code      ec;
        const std::uintmax_t size_after = fs::file_size(snapshot_path, ec);
        if (ec || size_before != size_after)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::IoError,
                                                                       "Completed .3mf snapshot changed while it was being imported");

        CommitPtr           head;
        bool                has_head = false;
        ProjectHistoryError head_error;
        if (!load_head_commit(repository.get(), head, has_head, head_error))
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, head_error.code, head_error.message);
        if (mutation_state != nullptr) {
            mutation_state->baseline_known    = true;
            mutation_state->baseline_has_head = has_head;
            if (has_head && !oid_to_string(git_commit_id(head.get()), mutation_state->baseline_head))
                return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                           "Could not format the baseline project-history HEAD");
        }

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
        if (mutation_state != nullptr) mutation_state->commit_attempted = true;
        if (git_commit_create(&commit_oid, repository.get(), "HEAD", signature.get(), signature.get(), nullptr, options.message.c_str(), tree.get(), has_head ? 1 : 0,
                              has_head ? parents : nullptr) != 0)
            return failure_with_repository<ProjectHistoryCommitResult>(result.repository_path, ProjectHistoryErrorCode::RepositoryError,
                                                                       git_error_message("Could not commit project-history snapshot"));
        if (mutation_state != nullptr) mutation_state->head_advanced = true;

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

        std::vector<std::unique_ptr<InterprocessFileLock>> locks;
        ProjectHistoryError                                lock_error;
        if (!acquire_identity_locks({project.identity_hash}, locks, lock_error))
            return failure_with_repository<ProjectHistoryListResult>(result.repository_path, lock_error.code, lock_error.message);

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

        ResolvedProject project = resolve_project(m_history_root, project_path);
        if (!project.error.ok()) return failure<ProjectHistoryRestoreResult>(project.error.code, project.error.message);

        std::vector<std::unique_ptr<InterprocessFileLock>> locks;
        ProjectHistoryError                                lock_error;
        if (!acquire_identity_locks({project.identity_hash}, locks, lock_error))
            return failure<ProjectHistoryRestoreResult>(lock_error.code, lock_error.message);

        bool                within_history = false;
        ProjectHistoryError path_error;
        if (!path_is_within(destination_path, m_history_root, within_history, path_error))
            return failure<ProjectHistoryRestoreResult>(path_error.code, path_error.message);
        if (within_history)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::InvalidArgument, "Restore destination cannot be inside the managed project-history directory");

        bool destination_exists = false;
        if (!path_entry_exists(destination_path, destination_exists, path_error, "Could not inspect restore destination"))
            return failure<ProjectHistoryRestoreResult>(path_error.code, path_error.message);
        if (destination_exists)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::DestinationExists, "Restore destination already exists");

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

        std::error_code ec;
        fs::path parent = destination_path.parent_path();
        if (parent.empty())
            parent = fs::current_path(ec);
        else
            fs::create_directories(parent, ec);
        if (ec) return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not prepare restore destination: " + ec.message());
        if (!path_is_within(destination_path, m_history_root, within_history, path_error))
            return failure<ProjectHistoryRestoreResult>(path_error.code, path_error.message);
        if (within_history)
            return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::InvalidArgument, "Restore destination cannot resolve inside the managed project-history directory");

        const fs::path staging_path = unique_staging_path(destination_path);
        if (staging_path.empty()) return failure<ProjectHistoryRestoreResult>(ProjectHistoryErrorCode::IoError, "Could not allocate a restore staging path");

        const auto remove_staging = [&staging_path] {
            std::error_code remove_error;
            fs::remove(staging_path, remove_error);
        };

        ProjectHistoryError write_error;
        if (!write_new_private_file(staging_path, static_cast<const char *>(git_blob_rawcontent(blob.get())),
                                    static_cast<std::uint64_t>(git_blob_rawsize(blob.get())), write_error)) {
            remove_staging();
            return failure<ProjectHistoryRestoreResult>(write_error.code, write_error.message);
        }

        if (!path_entry_exists(destination_path, destination_exists, path_error, "Could not recheck restore destination") || destination_exists) {
            remove_staging();
            return failure<ProjectHistoryRestoreResult>(path_error.ok() ? ProjectHistoryErrorCode::DestinationExists : path_error.code,
                                                        path_error.ok() ? "Restore destination was created by another operation" : path_error.message);
        }
        ProjectHistoryError publish_error;
        if (!publish_new_file(staging_path, destination_path, publish_error)) {
            remove_staging();
            return failure<ProjectHistoryRestoreResult>(publish_error.code, publish_error.message);
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

    fs::path                          m_app_data_directory;
    fs::path                          m_history_base;
    fs::path                          m_history_root;
    fs::path                          m_lock_root;
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

std::future<ProjectHistoryMigrationResult> ProjectHistoryManager::migrate_history_identity(fs::path previous_project_path, fs::path new_project_path)
{
    return m_impl->enqueue<ProjectHistoryMigrationResult>([impl = m_impl.get(), previous_project_path = std::move(previous_project_path),
                                                            new_project_path = std::move(new_project_path)] {
        return impl->migrate(previous_project_path, new_project_path);
    });
}

std::future<ProjectHistoryCommitResult> ProjectHistoryManager::migrate_then_commit_snapshot(fs::path previous_project_path,
                                                                                             fs::path new_project_path,
                                                                                             fs::path completed_snapshot_path,
                                                                                             ProjectHistoryCommitOptions options)
{
    return m_impl->enqueue<ProjectHistoryCommitResult>([impl = m_impl.get(), previous_project_path = std::move(previous_project_path),
                                                         new_project_path = std::move(new_project_path), snapshot_path = std::move(completed_snapshot_path),
                                                         options = std::move(options)] {
        return impl->migrate_then_commit(previous_project_path, new_project_path, snapshot_path, options);
    });
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
