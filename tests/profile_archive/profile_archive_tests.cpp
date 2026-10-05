#include <catch_main.hpp>
#include "slic3r/GUI/ConfigProfileArchive.hpp"
#include <filesystem>
#include <fstream>
#include <future>
#include <random>
#include <vector>
#include <string>
#include <iterator>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace PA = Slic3r::GUI::ConfigProfileArchive;
namespace fs = std::filesystem;
namespace {
struct Fixture {
    fs::path root;
    Fixture() {
        for (;;) {
            root = fs::temp_directory_path() / ("bambu-import-test-" + std::to_string(std::random_device{}()));
            if (fs::create_directory(root)) break;
        }
    }
    ~Fixture() { std::error_code ec; fs::remove_all(root, ec); }
    fs::path archive(const std::vector<std::string> &names) {
        const auto path = root / "input.zip";
        wxFFileOutputStream file(wxString(path.wstring()));
        wxZipOutputStream zip(file, 0);
        for (const auto &name : names) {
            auto *entry = new wxZipEntry;
            entry->SetName(wxString::FromUTF8(name), wxPATH_UNIX);
            REQUIRE(zip.PutNextEntry(entry));
            const std::string payload = "unique-payload";
            zip.Write(payload.data(), payload.size());
            REQUIRE(zip.CloseEntry());
        }
        REQUIRE(zip.Close());
        REQUIRE(file.Close());
        return path;
    }
};
std::string read(const fs::path &path) { std::ifstream file(path, std::ios::binary); return {std::istreambuf_iterator<char>(file), {}}; }
void write(const fs::path &path, const std::string &bytes) { std::ofstream file(path, std::ios::binary); file.write(bytes.data(), bytes.size()); }
}

TEST_CASE("Profile import reserves a fresh directory and preserves existing data", "[ProfileArchive]")
{
    Fixture f;
    const auto input = f.archive({"nested/settings.json"});
    const auto target = f.root / "profile";
    REQUIRE(PA::import_archive(input, target) == PA::ImportError::None);
    REQUIRE(read(target / "nested/settings.json") == "unique-payload");
    write(target / "keep.txt", "unchanged");
    REQUIRE(PA::import_archive(input, target) == PA::ImportError::Destination);
    REQUIRE(read(target / "keep.txt") == "unchanged");
}

TEST_CASE("Concurrent imports cannot share the same reserved destination", "[ProfileArchive]")
{
    Fixture f;
    const auto input = f.archive({"settings.json"});
    const auto target = f.root / "profile";
    auto first = std::async(std::launch::async, [&] { return PA::import_archive(input, target); });
    auto second = std::async(std::launch::async, [&] { return PA::import_archive(input, target); });
    const auto a = first.get(), b = second.get();
    REQUIRE(((a == PA::ImportError::None && b == PA::ImportError::Destination) ||
             (b == PA::ImportError::None && a == PA::ImportError::Destination)));
    REQUIRE(read(target / "settings.json") == "unique-payload");
}

TEST_CASE("Unsafe and duplicate paths leave no partial imported profile", "[ProfileArchive]")
{
    Fixture f;
    write(f.root / "victim.txt", "unchanged");
    for (const auto &name : {"../victim.txt", "../profile-sibling/file", "C:/outside", "item:stream", "NUL", "nested/../file"}) {
        const auto input = f.archive({"first.json", name});
        const auto target = f.root / "profile";
        REQUIRE(PA::import_archive(input, target) == PA::ImportError::UnsafePath);
        REQUIRE_FALSE(fs::exists(target));
        REQUIRE(read(f.root / "victim.txt") == "unchanged");
    }
    const auto input = f.archive({"same.json", "same.json"});
    REQUIRE(PA::import_archive(input, f.root / "profile") == PA::ImportError::Write);
    REQUIRE_FALSE(fs::exists(f.root / "profile"));
}

TEST_CASE("Corrupt archives and damaged CRCs are rejected", "[ProfileArchive]")
{
    Fixture f;
    const auto input = f.root / "input.zip";
    write(input, "not a zip archive");
    REQUIRE(PA::import_archive(input, f.root / "profile") == PA::ImportError::Corrupt);
    REQUIRE_FALSE(fs::exists(f.root / "profile"));
    f.archive({"first.json", "second.json"});
    auto bytes = read(input);
    bytes.resize(bytes.size() - 22);
    write(input, bytes);
    REQUIRE(PA::import_archive(input, f.root / "profile") == PA::ImportError::Corrupt);
    REQUIRE_FALSE(fs::exists(f.root / "profile"));
    f.archive({"first.json", "second.json"});
    bytes = read(input);
    const std::string central("PK\x01\x02", 4);
    const auto first_header = bytes.find(central);
    REQUIRE(first_header != std::string::npos);
    const auto second_header = bytes.find(central, first_header + 4);
    REQUIRE(second_header != std::string::npos);
    bytes[second_header] = 'X';
    write(input, bytes);
    REQUIRE(PA::import_archive(input, f.root / "profile") == PA::ImportError::Corrupt);
    REQUIRE_FALSE(fs::exists(f.root / "profile"));
    f.archive({"first.json"});
    bytes = read(input);
    const auto payload = bytes.find("unique-payload");
    REQUIRE(payload != std::string::npos);
    bytes[payload] = 'X';
    write(input, bytes);
    REQUIRE(PA::import_archive(input, f.root / "profile") == PA::ImportError::Corrupt);
    REQUIRE_FALSE(fs::exists(f.root / "profile"));
}
