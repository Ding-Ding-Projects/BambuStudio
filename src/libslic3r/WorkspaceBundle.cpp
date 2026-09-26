#include "WorkspaceBundle.hpp"
#include "ProjectHistoryManager.hpp"

#include <miniz.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using nlohmann::json;

namespace Slic3r::Workspace {
namespace {

constexpr const char *MANIFEST = "Metadata/workspace.json";
constexpr std::uint64_t MAX_MANIFEST = 1024 * 1024;
constexpr std::uint64_t MAX_FILE = 1024ull * 1024 * 1024;
constexpr std::uint64_t MAX_TOTAL = 2ull * 1024 * 1024 * 1024;
constexpr std::size_t MAX_ENTRIES = 256;

struct Entry {
    std::string name;
    fs::path source;
    std::uint64_t size = 0;
    std::string digest;
};

FILE *open_file(const fs::path &path, bool write)
{
#ifdef _WIN32
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), write ? L"wb" : L"rb") != 0) return nullptr;
    return file;
#else
    return std::fopen(path.c_str(), write ? "wb" : "rb");
#endif
}

std::string hex_digest(const unsigned char *bytes, std::size_t size)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string value;
    value.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        value += digits[bytes[i] >> 4];
        value += digits[bytes[i] & 15];
    }
    return value;
}

std::string file_digest(const fs::path &path)
{
    FILE *file = open_file(path, false);
    if (!file) throw std::runtime_error("Could not open workspace source file");
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (!context) { std::fclose(file); throw std::runtime_error("Could not allocate digest context"); }
    std::array<unsigned char, 65536> bytes{};
    bool ok = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1;
    while (ok) {
        const std::size_t count = std::fread(bytes.data(), 1, bytes.size(), file);
        if (count && EVP_DigestUpdate(context, bytes.data(), count) != 1) ok = false;
        if (count < bytes.size()) {
            if (std::ferror(file)) ok = false;
            break;
        }
    }
    unsigned int length = 0;
    std::array<unsigned char, 32> digest{};
    if (ok) ok = EVP_DigestFinal_ex(context, digest.data(), &length) == 1 && length == digest.size();
    EVP_MD_CTX_free(context);
    std::fclose(file);
    if (!ok) throw std::runtime_error("Could not hash workspace source file");
    return hex_digest(digest.data(), digest.size());
}

bool safe_component(const std::string &value)
{
    if (value.empty() || value.size() > 120 || value == "." || value == "..") return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    });
}

bool safe_path(const std::string &value)
{
    if (value.empty() || value.size() > 512 || value.front() == '/' || value.back() == '/' ||
        value.find('\\') != std::string::npos || value.find(':') != std::string::npos) return false;
    std::size_t start = 0;
    while (start < value.size()) {
        const auto slash = value.find('/', start);
        const auto part = value.substr(start, slash == std::string::npos ? slash : slash - start);
        if (part.empty() || part == "." || part == ".." || part.size() > 240 ||
            part.back() == '.' || part.back() == ' ') return false;
        for (unsigned char c : part)
            if (c < 0x20 || c == 0x7f || c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*')
                return false;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

bool valid_text(const std::string &value, std::size_t maximum)
{
    return value.size() <= maximum && value.find('\0') == std::string::npos;
}

void require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}

Entry make_entry(const std::string &name, const fs::path &source)
{
    require(safe_path(name), "Unsafe workspace entry path");
    require(fs::is_regular_file(source), "Workspace source file is missing");
    const auto size = fs::file_size(source);
    require(size <= MAX_FILE, "Workspace source file exceeds the per-file limit");
    return {name, source, size, file_digest(source)};
}

bool member_3mf_is_valid(const fs::path &source, std::string *error = nullptr)
{
    FILE *file = open_file(source, false);
    if (!file) {
        if (error) *error = "Member project cannot be opened";
        return false;
    }
    mz_zip_archive archive{};
    bool valid = mz_zip_reader_init_cfile(&archive, file, 0, 0) != 0;
    int manifest_count = 0;
    int pack_count = 0;
    if (valid) {
        valid = mz_zip_reader_locate_file(&archive, "3D/3dmodel.model", nullptr, 0) >= 0 &&
                mz_zip_reader_locate_file(&archive, "Metadata/workspace.json", nullptr, 0) < 0;
        for (mz_uint index = 0; valid && index < mz_zip_reader_get_num_files(&archive); ++index) {
            mz_zip_archive_file_stat stat{};
            valid = mz_zip_reader_file_stat(&archive, index, &stat) != 0 && safe_path(stat.m_filename);
            if (valid && std::string(stat.m_filename) == "Metadata/bambu_project_history.json") ++manifest_count;
            if (valid && std::string(stat.m_filename) == "Metadata/bambu_project_history.pack") ++pack_count;
        }
        valid = valid && manifest_count <= 1 && pack_count <= 1 && manifest_count == pack_count &&
                mz_zip_validate_archive(&archive, 0) != 0;
        mz_zip_reader_end(&archive);
    }
    std::fclose(file);
    if (!valid) {
        if (error) *error = "Member project is not a readable 3MF archive";
        return false;
    }
    if (manifest_count == 0) return true;
    // Inspect the optional history in an isolated, disposable private store.
    // Never import its identity into the application's live history state.
    fs::path scratch;
    bool created = false;
    try {
        scratch = fs::temp_directory_path() / ("bambu-member-history-" + new_id());
        created = fs::create_directory(scratch);
        if (!created) throw std::runtime_error("Could not create history validation directory");
        fs::permissions(scratch, fs::perms::owner_all, fs::perm_options::replace);
        Slic3r::ProjectHistoryPortableResult inspected;
        {
            Slic3r::ProjectHistoryManager validator(scratch);
            inspected = validator.inspect_portable_history(source).get();
        }
        std::error_code ignored;
        if (created) fs::remove_all(scratch, ignored);
        if (!inspected.ok() || !inspected.present) {
            if (error) *error = "Embedded project history is corrupt: " + inspected.error.message;
            return false;
        }
    } catch (const std::exception &ex) {
        std::error_code ignored;
        if (created) fs::remove_all(scratch, ignored);
        if (error) *error = std::string("Could not validate embedded project history: ") + ex.what();
        return false;
    }
    return valid;
}

json item_json(const ChecklistItem &item)
{
    return {{"id", item.id}, {"text", item.text}, {"completed", item.completed}, {"order", item.order},
            {"due_date", item.due_date}, {"due_utc_offset_minutes", item.due_utc_offset_minutes},
            {"linked_member_id", item.linked_member_id}, {"linked_slot_id", item.linked_slot_id}};
}

json slot_json(const CalendarSlot &slot)
{
    return {{"id", slot.id}, {"title", slot.title}, {"printer_id", slot.printer_id}, {"member_id", slot.member_id},
            {"time_zone", slot.time_zone}, {"start_utc", slot.start_utc}, {"end_utc", slot.end_utc},
            {"utc_offset_minutes", slot.utc_offset_minutes}, {"enabled", slot.enabled}, {"completed", slot.completed},
            {"reminder_minutes", slot.reminder_minutes}, {"snoozed_until_utc", slot.snoozed_until_utc},
            {"reminder_dismissed", slot.reminder_dismissed}};
}

json create_manifest(const Workspace &workspace, std::vector<Entry> &entries)
{
    require(safe_component(workspace.id), "Invalid workspace ID");
    require(valid_text(workspace.title, 512) && valid_text(workspace.notes, 262144) &&
            valid_text(workspace.time_zone, 128), "Workspace text exceeds a limit");
    require(workspace.members.size() <= 64 && workspace.checklist.size() <= 512 && workspace.slots.size() <= 512,
            "Workspace item count exceeds a limit");
    require(workspace.deadline_reminder_hour >= 0 && workspace.deadline_reminder_hour <= 23,
            "Invalid deadline reminder hour");
    json members = json::array();
    std::set<std::string> ids;
    std::set<std::string> names;
    std::set<std::string> folded_names;
    std::uint64_t total = 0;
    for (const Member &member : workspace.members) {
        require(safe_component(member.id) && ids.insert(member.id).second && valid_text(member.name, 512),
                "Invalid or duplicate workspace member");
        const std::string project_entry = "Members/" + member.id + "/project.3mf";
        Entry project = make_entry(project_entry, member.project_path);
        std::string member_error;
        require(member_3mf_is_valid(member.project_path, &member_error), member_error);
        total += project.size;
        entries.push_back(project);
        names.insert(project_entry);
        folded_names.insert(project_entry);
        json sources = json::array();
        for (const SourceFile &source : member.editable_sources) {
            require(safe_path(source.relative_path), "Unsafe editable-source path");
            const std::string name = "Sources/" + member.id + "/" + source.relative_path;
            require(names.insert(name).second, "Duplicate editable-source path");
            std::string folded = name;
            std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            require(folded_names.insert(folded).second, "Case-colliding editable-source path");
            Entry entry = make_entry(name, source.local_path);
            total += entry.size;
            entries.push_back(entry);
            sources.push_back({{"path", source.relative_path}, {"entry", name}, {"size", entry.size}, {"sha256", entry.digest}});
        }
        members.push_back({{"id", member.id}, {"name", member.name}, {"project_entry", project_entry},
                           {"project_size", project.size}, {"project_sha256", project.digest}, {"sources", sources}});
    }
    require(total <= MAX_TOTAL && entries.size() + 1 <= MAX_ENTRIES, "Workspace bundle exceeds archive limits");
    json checklist = json::array();
    for (const auto &item : workspace.checklist) checklist.push_back(item_json(item));
    json slots = json::array();
    for (const auto &slot : workspace.slots) slots.push_back(slot_json(slot));
    return {{"version", 1}, {"bundle_id", workspace.id}, {"title", workspace.title}, {"notes", workspace.notes},
            {"time_zone", workspace.time_zone}, {"members", members}, {"checklist", checklist}, {"slots", slots},
            {"deadline_reminder_hour", workspace.deadline_reminder_hour}, {"reminders_enabled", workspace.reminders_enabled}};
}

struct HashSink {
    EVP_MD_CTX *context = nullptr;
    std::uint64_t offset = 0;
};

size_t digest_callback(void *opaque, mz_uint64 offset, const void *buffer, size_t size)
{
    auto *sink = static_cast<HashSink *>(opaque);
    if (offset != sink->offset || EVP_DigestUpdate(sink->context, buffer, size) != 1) return 0;
    sink->offset += size;
    return size;
}

std::string archive_digest(mz_zip_archive &archive, mz_uint index, std::uint64_t expected_size)
{
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    require(context != nullptr, "Could not allocate archive digest context");
    HashSink sink{context, 0};
    bool ok = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1 &&
              mz_zip_reader_extract_to_callback(&archive, index, digest_callback, &sink, 0) != 0;
    std::array<unsigned char, 32> bytes{};
    unsigned int length = 0;
    if (ok) ok = sink.offset == expected_size && EVP_DigestFinal_ex(context, bytes.data(), &length) == 1 && length == bytes.size();
    EVP_MD_CTX_free(context);
    require(ok, "Could not verify workspace entry content");
    return hex_digest(bytes.data(), bytes.size());
}

void exact_keys(const json &value, std::initializer_list<const char *> keys)
{
    require(value.is_object() && value.size() == keys.size(), "Invalid workspace manifest fields");
    for (const char *key : keys) require(value.contains(key), "Missing workspace manifest field");
}

std::string get_string(const json &value, const char *key, std::size_t maximum)
{
    require(value.at(key).is_string(), "Invalid workspace string field");
    std::string result = value.at(key).get<std::string>();
    require(valid_text(result, maximum), "Workspace string exceeds a limit");
    return result;
}

std::int64_t get_int(const json &value, const char *key)
{
    require(value.at(key).is_number_integer(), "Invalid workspace integer field");
    return value.at(key).get<std::int64_t>();
}

std::uint64_t get_size(const json &value, const char *key)
{
    const auto number = get_int(value, key);
    require(number >= 0 && static_cast<std::uint64_t>(number) <= MAX_FILE, "Invalid workspace entry size");
    return static_cast<std::uint64_t>(number);
}

bool get_bool(const json &value, const char *key)
{
    require(value.at(key).is_boolean(), "Invalid workspace boolean field");
    return value.at(key).get<bool>();
}

void append_expected(std::map<std::string, std::pair<std::uint64_t, std::string>> &expected,
                     const std::string &name, std::uint64_t size, const std::string &digest)
{
    require(safe_path(name) && digest.size() == 64 &&
            std::all_of(digest.begin(), digest.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }) &&
            expected.emplace(name, std::make_pair(size, digest)).second, "Invalid or duplicate workspace entry");
}

Workspace parse_manifest(const json &manifest, std::map<std::string, std::pair<std::uint64_t, std::string>> &expected)
{
    exact_keys(manifest, {"version", "bundle_id", "title", "notes", "time_zone", "members", "checklist", "slots",
                          "deadline_reminder_hour", "reminders_enabled"});
    require(get_int(manifest, "version") == 1, "Unsupported workspace version");
    Workspace workspace;
    workspace.id = get_string(manifest, "bundle_id", 120);
    require(safe_component(workspace.id), "Invalid workspace ID");
    workspace.title = get_string(manifest, "title", 512);
    workspace.notes = get_string(manifest, "notes", 262144);
    workspace.time_zone = get_string(manifest, "time_zone", 128);
    workspace.deadline_reminder_hour = static_cast<int>(get_int(manifest, "deadline_reminder_hour"));
    require(workspace.deadline_reminder_hour >= 0 && workspace.deadline_reminder_hour <= 23, "Invalid reminder hour");
    workspace.reminders_enabled = get_bool(manifest, "reminders_enabled");
    require(manifest.at("members").is_array() && manifest.at("members").size() <= 64, "Invalid workspace members");
    std::set<std::string> ids;
    for (const auto &value : manifest.at("members")) {
        exact_keys(value, {"id", "name", "project_entry", "project_size", "project_sha256", "sources"});
        Member member;
        member.id = get_string(value, "id", 120);
        require(safe_component(member.id) && ids.insert(member.id).second, "Invalid or duplicate member ID");
        member.name = get_string(value, "name", 512);
        const std::string project_entry = get_string(value, "project_entry", 512);
        require(project_entry == "Members/" + member.id + "/project.3mf", "Invalid member project entry");
        append_expected(expected, project_entry, get_size(value, "project_size"), get_string(value, "project_sha256", 64));
        require(value.at("sources").is_array(), "Invalid editable-source list");
        for (const auto &source : value.at("sources")) {
            exact_keys(source, {"path", "entry", "size", "sha256"});
            SourceFile file;
            file.relative_path = get_string(source, "path", 512);
            require(safe_path(file.relative_path), "Unsafe editable-source path");
            const std::string entry = get_string(source, "entry", 512);
            require(entry == "Sources/" + member.id + "/" + file.relative_path, "Invalid editable-source entry");
            append_expected(expected, entry, get_size(source, "size"), get_string(source, "sha256", 64));
            member.editable_sources.push_back(std::move(file));
        }
        workspace.members.push_back(std::move(member));
    }
    require(manifest.at("checklist").is_array() && manifest.at("checklist").size() <= 512, "Invalid checklist");
    ids.clear();
    for (const auto &value : manifest.at("checklist")) {
        exact_keys(value, {"id", "text", "completed", "order", "due_date", "due_utc_offset_minutes",
                           "linked_member_id", "linked_slot_id"});
        ChecklistItem item;
        item.id = get_string(value, "id", 120);
        require(safe_component(item.id) && ids.insert(item.id).second, "Invalid or duplicate checklist ID");
        item.text = get_string(value, "text", 4096);
        item.completed = get_bool(value, "completed");
        item.order = static_cast<int>(get_int(value, "order"));
        item.due_date = get_string(value, "due_date", 10);
        item.due_utc_offset_minutes = static_cast<int>(get_int(value, "due_utc_offset_minutes"));
        require(item.due_utc_offset_minutes >= -840 && item.due_utc_offset_minutes <= 840,
                "Invalid deadline UTC offset");
        item.linked_member_id = get_string(value, "linked_member_id", 120);
        item.linked_slot_id = get_string(value, "linked_slot_id", 120);
        workspace.checklist.push_back(std::move(item));
    }
    require(manifest.at("slots").is_array() && manifest.at("slots").size() <= 512, "Invalid calendar slots");
    ids.clear();
    for (const auto &value : manifest.at("slots")) {
        exact_keys(value, {"id", "title", "printer_id", "member_id", "time_zone", "start_utc", "end_utc",
                           "utc_offset_minutes", "enabled", "completed", "reminder_minutes", "snoozed_until_utc", "reminder_dismissed"});
        CalendarSlot slot;
        slot.id = get_string(value, "id", 120);
        require(safe_component(slot.id) && ids.insert(slot.id).second, "Invalid or duplicate slot ID");
        slot.title = get_string(value, "title", 512);
        slot.printer_id = get_string(value, "printer_id", 120);
        slot.member_id = get_string(value, "member_id", 120);
        slot.time_zone = get_string(value, "time_zone", 128);
        slot.start_utc = get_int(value, "start_utc");
        slot.end_utc = get_int(value, "end_utc");
        slot.utc_offset_minutes = static_cast<int>(get_int(value, "utc_offset_minutes"));
        slot.enabled = get_bool(value, "enabled");
        slot.completed = get_bool(value, "completed");
        slot.reminder_minutes = static_cast<int>(get_int(value, "reminder_minutes"));
        slot.snoozed_until_utc = get_int(value, "snoozed_until_utc");
        slot.reminder_dismissed = get_bool(value, "reminder_dismissed");
        require(slot.end_utc > slot.start_utc && slot.utc_offset_minutes >= -840 && slot.utc_offset_minutes <= 840 &&
                slot.reminder_minutes >= 0 && slot.reminder_minutes <= 10080, "Invalid calendar slot timing");
        workspace.slots.push_back(std::move(slot));
    }
    require(expected.size() + 1 <= MAX_ENTRIES, "Workspace entry count exceeds limit");
    return workspace;
}

Result inspect_impl(const fs::path &archive_path, bool extract, const fs::path &staging_root)
{
    Result result;
    mz_zip_archive archive{};
    FILE *file = open_file(archive_path, false);
    if (!file) { result.error = "Could not open workspace archive"; return result; }
    if (!mz_zip_reader_init_cfile(&archive, file, 0, 0)) {
        std::fclose(file);
        result.error = "Could not read workspace ZIP";
        return result;
    }
    try {
        const mz_uint count = mz_zip_reader_get_num_files(&archive);
        require(count >= 1 && count <= MAX_ENTRIES, "Workspace ZIP entry count exceeds limit");
        std::map<std::string, mz_uint> indices;
        std::set<std::string> folded_names;
        std::uint64_t total = 0;
        for (mz_uint i = 0; i < count; ++i) {
            mz_zip_archive_file_stat stat{};
            require(mz_zip_reader_file_stat(&archive, i, &stat) != 0, "Could not inspect ZIP entry");
            const std::string name = stat.m_filename;
            require(safe_path(name) && indices.emplace(name, i).second, "Unsafe or duplicate ZIP entry path");
            std::string folded = name;
            std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            require(folded_names.insert(folded).second, "Case-colliding ZIP entry path");
            require(!mz_zip_reader_is_file_a_directory(&archive, i) && stat.m_uncomp_size <= MAX_FILE,
                    "Unsupported or oversized ZIP entry");
            total += stat.m_uncomp_size;
            require(total <= MAX_TOTAL, "Workspace expanded size exceeds limit");
            require(stat.m_uncomp_size <= 1024 * 1024 || stat.m_comp_size == 0 ||
                    stat.m_uncomp_size / stat.m_comp_size <= 1000, "Workspace compression ratio exceeds limit");
        }
        const auto manifest_it = indices.find(MANIFEST);
        require(manifest_it != indices.end(), "Workspace manifest is missing");
        mz_zip_archive_file_stat manifest_stat{};
        require(mz_zip_reader_file_stat(&archive, manifest_it->second, &manifest_stat) != 0 &&
                manifest_stat.m_uncomp_size <= MAX_MANIFEST, "Workspace manifest exceeds limit");
        std::vector<char> text(static_cast<std::size_t>(manifest_stat.m_uncomp_size));
        require(mz_zip_reader_extract_to_mem(&archive, manifest_it->second, text.data(), text.size(), 0) != 0,
                "Could not read workspace manifest");
        std::vector<std::set<std::string>> object_keys;
        const json manifest = json::parse(text.begin(), text.end(),
            [&object_keys](int, json::parse_event_t event, json &parsed) {
                if (event == json::parse_event_t::object_start) object_keys.emplace_back();
                else if (event == json::parse_event_t::object_end) object_keys.pop_back();
                else if (event == json::parse_event_t::key &&
                         !object_keys.back().insert(parsed.get<std::string>()).second)
                    throw std::runtime_error("Duplicate workspace manifest key");
                return true;
            });
        std::map<std::string, std::pair<std::uint64_t, std::string>> expected;
        result.workspace = parse_manifest(manifest, expected);
        require(indices.size() == expected.size() + 1, "Workspace ZIP contains an unlisted entry");
        for (const auto &[name, record] : expected) {
            const auto found = indices.find(name);
            require(found != indices.end(), "Workspace member is missing");
            mz_zip_archive_file_stat stat{};
            require(mz_zip_reader_file_stat(&archive, found->second, &stat) != 0 && stat.m_uncomp_size == record.first,
                    "Workspace entry size changed");
            require(archive_digest(archive, found->second, record.first) == record.second,
                    "Workspace entry hash changed");
        }
        if (extract) {
            require(!staging_root.empty() && fs::is_directory(staging_root), "Private staging root is unavailable");
            result.staging_directory = staging_root / ("workspace-" + new_id());
            fs::create_directory(result.staging_directory);
            for (const auto &[name, record] : expected) {
                const fs::path target = result.staging_directory / fs::u8path(name);
                fs::create_directories(target.parent_path());
                FILE *output = open_file(target, true);
                require(output != nullptr, "Could not create staged workspace entry");
                const bool extracted = mz_zip_reader_extract_to_cfile(&archive, indices.at(name), output, 0) != 0;
                const bool closed = std::fclose(output) == 0;
                require(extracted && closed, "Could not extract workspace entry");
                require(fs::file_size(target) == record.first && file_digest(target) == record.second,
                        "Staged workspace entry changed");
            }
            for (auto &member : result.workspace.members) {
                member.project_path = result.staging_directory / fs::u8path("Members/" + member.id + "/project.3mf");
                for (auto &source : member.editable_sources)
                    source.local_path = result.staging_directory / fs::u8path("Sources/" + member.id + "/" + source.relative_path);
                std::string member_error;
                if (!member_3mf_is_valid(member.project_path, &member_error))
                    result.warnings.push_back("Member '" + member.name + "' has unreadable project history or geometry: " + member_error);
            }
        }
    } catch (const std::exception &error) {
        result.error = error.what();
        if (!result.staging_directory.empty()) {
            std::error_code ignored;
            fs::remove_all(result.staging_directory, ignored);
            result.staging_directory.clear();
        }
    }
    mz_zip_reader_end(&archive);
    std::fclose(file);
    return result;
}

} // namespace

std::string new_id()
{
    std::random_device entropy;
    std::array<unsigned char, 16> bytes{};
    for (auto &byte : bytes) byte = static_cast<unsigned char>(entropy());
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    const std::string value = hex_digest(bytes.data(), bytes.size());
    return value.substr(0, 8) + "-" + value.substr(8, 4) + "-" + value.substr(12, 4) + "-" +
           value.substr(16, 4) + "-" + value.substr(20);
}

bool validate_member_3mf(const fs::path &path, std::string *error)
{
    return member_3mf_is_valid(path, error);
}

Result inspect_bundle(const fs::path &archive)
{
    return inspect_impl(archive, false, {});
}

Result load_bundle(const fs::path &archive, const fs::path &staging_root)
{
    return inspect_impl(archive, true, staging_root);
}

Result save_bundle(const Workspace &workspace, const fs::path &destination)
{
    Result result;
    fs::path staged;
    try {
        require(!destination.empty() && fs::is_directory(destination.parent_path()), "Workspace destination folder is unavailable");
        std::vector<Entry> entries;
        const json manifest = create_manifest(workspace, entries);
        const std::string text = manifest.dump();
        require(text.size() <= MAX_MANIFEST, "Workspace manifest exceeds limit");
        staged = destination.parent_path() / (destination.filename().u8string() + ".pending-" + new_id());
        FILE *file = open_file(staged, true);
        require(file != nullptr, "Could not stage workspace archive");
        mz_zip_archive archive{};
        bool ok = mz_zip_writer_init_cfile(&archive, file, 0) != 0;
        if (ok) ok = mz_zip_writer_add_mem(&archive, MANIFEST, text.data(), text.size(), MZ_BEST_COMPRESSION) != 0;
        for (const Entry &entry : entries) {
            if (!ok) break;
            FILE *source = open_file(entry.source, false);
            if (!source) { ok = false; break; }
            ok = mz_zip_writer_add_cfile(&archive, entry.name.c_str(), source, entry.size, nullptr, nullptr, 0,
                                         MZ_BEST_COMPRESSION, nullptr, 0, nullptr, 0) != 0;
            std::fclose(source);
        }
        if (ok) ok = mz_zip_writer_finalize_archive(&archive) != 0;
        mz_zip_writer_end(&archive);
        if (std::fclose(file) != 0) ok = false;
        require(ok, "Could not finish staged workspace archive");
        const Result verified = inspect_bundle(staged);
        require(verified.ok() && verified.workspace.id == workspace.id, "Staged workspace verification failed");
#ifdef _WIN32
        const bool destination_exists = fs::exists(destination);
        const fs::path backup = fs::path(staged.native() + fs::path::string_type(L".previous"));
        const BOOL published = destination_exists
            ? ::ReplaceFileW(destination.c_str(), staged.c_str(), backup.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)
            : ::MoveFileExW(staged.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH);
        require(published != 0, "Could not atomically publish workspace archive");
        if (destination_exists) { std::error_code ignored; fs::remove(backup, ignored); }
#else
        fs::rename(staged, destination);
#endif
        result.workspace = verified.workspace;
    } catch (const std::exception &error) {
        result.error = error.what();
        result.staging_directory = staged;
    }
    return result;
}

} // namespace Slic3r::Workspace
