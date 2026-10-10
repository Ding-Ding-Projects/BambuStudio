// Test for the LAN model drop station's received files on disk
// (src/slic3r/GUI/LanModelDrop/LanModelDropFiles.cpp, docs/features/application-integration/lan-model-drop.md).
// Build and run from the repository root (one line):
//   g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/lan_model_drop/lan_model_drop_files_test.cpp
//       src/slic3r/GUI/LanModelDrop/LanModelDropFiles.cpp src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp
//       -o lan_model_drop_files_test
//   ./lan_model_drop_files_test <empty scratch folder>
// tests/lan_model_drop/model_native.test.mjs does exactly that when g++ is present.
#undef NDEBUG
#include "../../src/slic3r/GUI/LanModelDrop/LanModelDropFiles.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace Slic3r::GUI::LanModelDrop;
namespace fs = std::filesystem;

static int assertions = 0;
#define CHECK(expr) do { assert(expr); ++assertions; } while (0)

static const std::string kId1 = "0123456789abcdef0123456789abcdef";
static const std::string kId2 = "fedcba9876543210fedcba9876543210";
static const std::string kId3 = "00000000000000000000000000000003";
static const std::string kId4 = "00000000000000000000000000000004";
static const std::string kStl = "solid a\n facet normal 0 0 1\nendsolid a\n";

static std::string read_all(const fs::path &file)
{
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static void write_all(const fs::path &file, const std::string &text)
{
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}

static std::vector<std::string> names_in(const fs::path &folder)
{
    std::vector<std::string> names;
    for (const auto &entry : fs::directory_iterator(folder)) names.push_back(entry.path().filename().u8string());
    std::sort(names.begin(), names.end());
    return names;
}

// Open takes the folder away from the station: a later download of the same item (its DELETE was
// lost, or went to another address) never writes into it, never brings its marker back, and the
// next start keeps it. Before this, the second download overwrote the opened file, or the project
// saved over it, and the returned marker made the next start delete the whole folder.
static void opened_folder_is_never_written(const fs::path &root)
{
    fs::path file;
    CHECK(received_state(root, kId1) == ReceivedState::Absent);
    CHECK(write_received(root, kId1, "Bracket.stl", FileType::Stl, kStl, file) == WriteResult::Written);
    CHECK(file == root / kId1 / "Bracket.stl");
    CHECK(read_all(file) == kStl);
    CHECK(received_state(root, kId1) == ReceivedState::Waiting);
    CHECK((names_in(root / kId1) == std::vector<std::string>{".waiting", "Bracket.stl"})); // nothing temporary left

    // Listed again while it waits: nothing is written.
    fs::path again;
    CHECK(write_received(root, kId1, "Bracket.stl", FileType::Stl, "other bytes", again) == WriteResult::Exists);
    CHECK(read_all(file) == kStl);

    // Open, then the user saves work over the file (a 3MF opened as a project and saved).
    CHECK(mark_opened(file));
    CHECK(received_state(root, kId1) == ReceivedState::Opened);
    write_all(file, "the user's saved work");
    CHECK(write_received(root, kId1, "Bracket.stl", FileType::Stl, kStl, again) == WriteResult::Exists);
    CHECK(read_all(file) == "the user's saved work");
    CHECK(!fs::exists(root / kId1 / kWaitingMarker));
    CHECK(received_state(root, kId1) == ReceivedState::Opened);
    // Neither the next start nor a discard or a refusal of the same id removes it.
    clean_unopened(root);
    remove_waiting(root, kId1);
    CHECK(read_all(file) == "the user's saved work");
}

// The name a sender chose is never opened for writing: the bytes go to a name of the station's own
// and are renamed, and a name Windows would take for a device is replaced by "model.<type>".
static void names_on_disk(const fs::path &root)
{
    fs::path file;
    const std::vector<std::string> devices = {"COM3 .x.stl", "LPT1 .print.stl", "CONOUT$.stl", "con .stl", "nul.stl"};
    for (std::size_t i = 0; i < devices.size(); ++i) {
        const std::string id = std::string(31, 'a') + std::string(1, static_cast<char>('0' + i));
        CHECK(write_received(root, id, devices[i], FileType::Stl, kStl, file) == WriteResult::Written);
        CHECK(file == root / id / "model.stl");
        CHECK((names_in(root / id) == std::vector<std::string>{".waiting", "model.stl"}));
    }
    CHECK(write_received(root, kId3, u8"COM¹.stl", FileType::Stl, kStl, file) == WriteResult::Written);
    CHECK(file.filename() == "model.stl");
    fs::remove_all(root / kId3);
    // A name that does not match the type, or is not clean at all, is not used either.
    CHECK(write_received(root, kId3, "plate.stl", FileType::ThreeMF, "PK\x03\x04", file) == WriteResult::Written);
    CHECK(file.filename() == "model.3mf");
    fs::remove_all(root / kId3);
    CHECK(write_received(root, kId3, "../escape.stl", FileType::Stl, kStl, file) == WriteResult::Written);
    CHECK(file == root / kId3 / "model.stl");
    fs::remove_all(root / kId3);
    // An ordinary cleaned name is kept, Unicode included.
    CHECK(write_received(root, kId3, u8"齒輪 支架.stl", FileType::Stl, kStl, file) == WriteResult::Written);
    CHECK(file.filename().u8string() == u8"齒輪 支架.stl");
    fs::remove_all(root / kId3);

    // Without long-path support Windows refuses paths over 259 characters: such a name is shortened
    // to the fallback rather than failing every download.
    const fs::path deep = root / std::string(120, 'd');
    fs::create_directories(deep);
    CHECK(write_received(deep, kId3, std::string(190, 'n') + ".step", FileType::Step, "ISO-10303-21;", file) == WriteResult::Written);
    CHECK(file == deep / kId3 / "model.step");
    CHECK(read_all(file) == "ISO-10303-21;");
    // Exactly at the limit the name is kept; one character more and it is not.
    const std::size_t folder_chars = (root / kId4 / "x").native().size() - 1;
    if (folder_chars + 6 <= 259) {
        const std::string at_limit = std::string(259 - folder_chars - 5, 'n') + ".step";
        CHECK(write_received(root, kId4, at_limit, FileType::Step, "ISO-10303-21;", file) == WriteResult::Written);
        CHECK(file.filename() == at_limit && file.native().size() == 259);
        fs::remove_all(root / kId4);
        CHECK(write_received(root, kId4, "n" + at_limit, FileType::Step, "ISO-10303-21;", file) == WriteResult::Written);
        CHECK(file.filename() == "model.step");
        fs::remove_all(root / kId4);
    }
}

static void failures_and_cleanup(const fs::path &root)
{
    fs::path file;
    // The folder cannot be made: nothing is left behind and the download is tried again later.
    const fs::path blocked = root / "blocked";
    write_all(blocked, "a file where a folder should be");
    CHECK(write_received(blocked, kId3, "a.stl", FileType::Stl, kStl, file) == WriteResult::Failed);
    CHECK(fs::is_regular_file(blocked));

    // A folder with neither the marker nor a file, left by an interrupted write, may be replaced.
    fs::create_directories(root / kId4);
    CHECK(received_state(root, kId4) == ReceivedState::Stale);
    CHECK(write_received(root, kId4, "b.stl", FileType::Stl, kStl, file) == WriteResult::Written);
    CHECK(received_state(root, kId4) == ReceivedState::Waiting);

    // Discard removes a waiting folder.
    remove_waiting(root, kId4);
    CHECK(received_state(root, kId4) == ReceivedState::Absent);
    remove_waiting(root, "../" + kId4);

    // The next start removes waiting and stale folders and keeps opened ones and anything else.
    CHECK(write_received(root, kId4, "c.stl", FileType::Stl, kStl, file) == WriteResult::Written);
    fs::create_directories(root / kId3);
    fs::create_directories(root / "not-an-item");
    clean_unopened(root);
    CHECK(received_state(root, kId4) == ReceivedState::Absent);
    CHECK(received_state(root, kId3) == ReceivedState::Absent);
    CHECK(received_state(root, kId1) == ReceivedState::Opened);
    CHECK(fs::is_directory(root / "not-an-item"));
    CHECK(received_state(root, "nonsense") == ReceivedState::Absent);
}

static void pending_on_disk(const fs::path &root)
{
    const fs::path file = root / "state" / "pending-deletes.txt";
    CHECK(load_pending_deletes(file).empty());
    const std::vector<PendingDelete> pending = {{kId1, "http://localhost:8833"}, {kId2, "http://192.0.2.20:8833"}};
    CHECK(save_pending_deletes(file, pending));
    CHECK(load_pending_deletes(file) == pending);
    CHECK((names_in(file.parent_path()) == std::vector<std::string>{"pending-deletes.txt"}));
    CHECK(save_pending_deletes(file, {}));
    CHECK(load_pending_deletes(file).empty());
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "usage: lan_model_drop_files_test <empty scratch folder>\n";
        return 2;
    }
    const fs::path root = fs::u8path(argv[1]) / "received";
    fs::create_directories(root);
    opened_folder_is_never_written(root);
    names_on_disk(root);
    failures_and_cleanup(root);
    pending_on_disk(fs::u8path(argv[1]));
    std::cout << "lan_model_drop_files_test: " << assertions << " assertions passed\n";
    return 0;
}
