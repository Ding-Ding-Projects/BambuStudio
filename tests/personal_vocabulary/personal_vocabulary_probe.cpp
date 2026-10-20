// Synthetic multi-process persistence probe. Never accepts or prints private data.
#include "slic3r/GUI/PersonalVocabulary.hpp"
#include "slic3r/GUI/PersonalModes/SchoolMode.hpp"
#include <wx/app.h>
#include <wx/init.h>
#include <wx/stdpaths.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace PV = Slic3r::GUI::PersonalVocabulary;
int main(int argc, char **argv)
{
    wxInitializer runtime;
    if (!runtime.IsOk() || !wxTheApp || argc != 2) return 2;
    wxTheApp->SetAppName("BambuStudioVocabularyProbe");
    // Display stays suppressed until the School mode record is read. This probe
    // has no record and exercises a session with School mode off.
    Slic3r::GUI::PersonalModes::school_presentation_suppressed.store(false);
    const std::string phase = argv[1];
    const auto root = std::filesystem::path(wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring());
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error) return 3;
    const auto input = root / ("synthetic-" + phase + ".json");
    // The private copy, written directly only to model a stale or damaged cache.
    const auto cache = root / "private-display" / "vocabulary.json";
    auto write_to = [](const std::filesystem::path &path, const std::string &json) {
        std::error_code ignored;
        std::filesystem::create_directories(path.parent_path(), ignored);
        std::ofstream file(path, std::ios::binary); file << json; file.close(); return bool(file); };
    auto write = [&](const std::string &json) { return write_to(input, json); };
    auto shown = [&] { return PV::display(PV::remember("Open item")); };
    // Synthetic neutral entries "w0".."w4999"; far more than any former fixed count.
    auto many = [] {
        std::string json = R"({"schemaVersion":1,"entries":{"Open":"Inspect")";
        for (int i = 0; i < 5000; ++i) json += ",\"w" + std::to_string(i) + "\":\"r" + std::to_string(i) + "\"";
        return json + "}}";
    };
    bool ok = false;
    if (phase == "load-first") {
        ok = PV::clear() && write(R"({"schemaVersion":1,"entries":{"Open":"Inspect","Inspect":"Again"}})") &&
             PV::load(input) && PV::loaded() && shown() == "Inspect item";
    } else if (phase == "restore-first") {
        ok = PV::loaded() && shown() == "Inspect item";
    } else if (phase == "invalid") {
        PV::Rejection reason = PV::Rejection::None;
        ok = PV::loaded() && write(R"({"schemaVersion":2,"entries":{"Open":"Invalid"}})") &&
             !PV::load(input, &reason) && reason == PV::Rejection::UnsupportedVersion && shown() == "Inspect item";
    } else if (phase == "replace") {
        ok = write(R"({"schemaVersion":1,"entries":{"Open":"Review"}})") && PV::load(input) && shown() == "Review item";
    } else if (phase == "restore-second") {
        ok = PV::loaded() && shown() == "Review item";
    } else if (phase == "clear") {
        ok = PV::clear() && !PV::loaded() && shown() == "Open item" && PV::cache_rejection() == PV::Rejection::None;
    } else if (phase == "race-a" || phase == "race-b") {
        ok = write(phase == "race-a" ? R"({"schemaVersion":1,"entries":{"Open":"Inspect"}})" :
                                    R"({"schemaVersion":1,"entries":{"Open":"Review"}})");
        int successes = 0;
        for (int attempt = 0; ok && attempt < 40; ++attempt) {
            if (PV::load(input)) {
                ++successes;
                ok = shown() == (phase == "race-a" ? "Inspect item" : "Review item");
            }
        }
        ok = ok && successes > 0;
    } else if (phase == "verify-race") {
        ok = PV::loaded() && (shown() == "Inspect item" || shown() == "Review item");
    } else if (phase == "restore-empty") {
        ok = !PV::loaded() && shown() == "Open item" && PV::cache_rejection() == PV::Rejection::None;
    } else if (phase == "large") {
        ok = write(many()) && PV::load(input) && shown() == "Inspect item" &&
             PV::display(PV::remember("w4999 w12")) == "r4999 r12";
    } else if (phase == "restore-large") {
        ok = PV::loaded() && shown() == "Inspect item" && PV::display(PV::remember("w4999 w12")) == "r4999 r12";
    } else if (phase == "stale-cache") {
        // A copy written under rules that allowed larger files: valid version 1
        // JSON, but over the current file limit. The next process must ignore it.
        std::string json = R"({"schemaVersion":1,"entries":{"Open":"Stale")";
        for (int i = 0; json.size() <= PV::max_file_bytes; ++i)
            json += ",\"s" + std::to_string(i) + "\":\"" + std::string(100, 'x') + "\"";
        ok = write_to(cache, json + "}}");
    } else if (phase == "restore-stale") {
        ok = !PV::loaded() && shown() == "Open item" && PV::cache_rejection() == PV::Rejection::TooLarge;
    } else if (phase == "corrupt-cache") {
        ok = write_to(cache, R"({"schemaVersion":1,"entries":{"Open":"Damaged")");
    } else if (phase == "restore-corrupt") {
        ok = !PV::loaded() && shown() == "Open item" && PV::cache_rejection() == PV::Rejection::Malformed;
    }
    std::filesystem::remove(input, error);
    std::cout << (ok ? "PASS" : "FAIL") << " synthetic persistence phase\n";
    return ok ? 0 : 1;
}
