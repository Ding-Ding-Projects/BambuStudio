#include "LanModelDropFiles.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

namespace fs = std::filesystem;

namespace {

// The station's own name for bytes on their way to disk. It has no model extension, so it is never
// the name of a received file, and no device name looks like it.
constexpr const char *kIncomingName = "incoming.download";
// Windows refuses longer paths without long-path support (MAX_PATH less the terminating null).
constexpr std::size_t kMaxPathChars = 259;

bool write_file(const fs::path &file, const std::string &bytes)
{
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    return static_cast<bool>(out);
}

} // namespace

ReceivedState received_state(const fs::path &root, const std::string &id)
{
    if (!is_item_id(id)) return ReceivedState::Absent;
    std::error_code ec;
    const fs::path folder = root / id;
    if (!fs::exists(folder, ec)) return ReceivedState::Absent;
    if (fs::exists(folder / kWaitingMarker, ec)) return ReceivedState::Waiting;
    // Anything besides the station's temporary file means the user has it: opened, and perhaps
    // saved into. Only an empty folder, or one holding just that temporary file, is stale.
    for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().filename() != kIncomingName) return ReceivedState::Opened;
    return ReceivedState::Stale;
}

WriteResult write_received(const fs::path &root, const std::string &id, const std::string &file_name, FileType type,
                           const std::string &bytes, fs::path &file)
{
    if (!is_item_id(id)) return WriteResult::Failed;
    const fs::path folder = root / id;
    std::error_code ec;
    switch (received_state(root, id)) {
    case ReceivedState::Waiting:
    case ReceivedState::Opened: return WriteResult::Exists;
    case ReceivedState::Stale:
        fs::remove_all(folder, ec);
        if (ec) return WriteResult::Failed;
        break;
    case ReceivedState::Absent: break;
    }
    fs::create_directories(root, ec);
    if (ec) return WriteResult::Failed;
    // Claims the folder: create_directory answers false for one that exists by now, which is then
    // never written into.
    if (!fs::create_directory(folder, ec)) return ec ? WriteResult::Failed : WriteResult::Exists;

    const auto fail = [&folder] {
        std::error_code ignored;
        fs::remove_all(folder, ignored);
        return WriteResult::Failed;
    };
    // The marker first: a folder without it is the user's.
    if (!write_file(folder / kWaitingMarker, std::string())) return fail();
    const fs::path incoming = folder / kIncomingName;
    if (!write_file(incoming, bytes)) return fail();

    const bool usable = is_clean_file_name(file_name) && type_from_extension(file_name) == type;
    file = folder / fs::u8path(usable ? file_name : fallback_file_name(type));
    if (file.native().size() > kMaxPathChars) file = folder / fs::u8path(fallback_file_name(type));
    fs::rename(incoming, file, ec);
    if (ec) return fail();
    return WriteResult::Written;
}

bool mark_opened(const fs::path &file)
{
    std::error_code ec;
    const fs::path marker = file.parent_path() / kWaitingMarker;
    fs::remove(marker, ec);
    return !fs::exists(marker, ec);
}

void remove_waiting(const fs::path &root, const std::string &id)
{
    if (received_state(root, id) != ReceivedState::Waiting) return;
    std::error_code ec;
    fs::remove_all(root / id, ec);
}

void clean_unopened(const fs::path &root)
{
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return;
    std::vector<std::string> remove;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().u8string();
        const ReceivedState state = received_state(root, name);
        if (state == ReceivedState::Waiting || state == ReceivedState::Stale) remove.push_back(name);
    }
    for (const std::string &name : remove) {
        std::error_code inner;
        fs::remove_all(root / name, inner);
    }
}

std::vector<PendingDelete> load_pending_deletes(const fs::path &file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) return {};
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse_pending_deletes(text);
}

bool save_pending_deletes(const fs::path &file, const std::vector<PendingDelete> &pending)
{
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    fs::path partial = file;
    partial += ".part";
    if (!write_file(partial, format_pending_deletes(pending))) {
        fs::remove(partial, ec);
        return false;
    }
    fs::rename(partial, file, ec);
    if (ec) {
        fs::remove(partial, ec);
        return false;
    }
    return true;
}

}}} // namespace Slic3r::GUI::LanModelDrop
