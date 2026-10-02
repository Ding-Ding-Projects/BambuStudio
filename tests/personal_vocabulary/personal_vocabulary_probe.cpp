// Synthetic multi-process persistence probe. Never accepts or prints private data.
#include "slic3r/GUI/PersonalVocabulary.hpp"
#include <wx/app.h>
#include <wx/init.h>
#include <wx/stdpaths.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace PV = Slic3r::GUI::PersonalVocabulary;
int main(int argc, char **argv)
{
    wxInitializer runtime;
    if (!runtime.IsOk() || !wxTheApp || argc != 2) return 2;
    wxTheApp->SetAppName("BambuStudioVocabularyProbe");
    const std::string phase = argv[1];
    const auto root = std::filesystem::path(wxStandardPaths::Get().GetUserLocalDataDir().ToStdWstring());
    std::error_code error;
    std::filesystem::create_directories(root, error);
    if (error) return 3;
    const auto input = root / "synthetic-input.json";
    auto write = [&](const char *json) { std::ofstream file(input); file << json; file.close(); return bool(file); };
    auto shown = [&] { return PV::display(PV::remember("Open item")); };
    bool ok = false;
    if (phase == "load-first") {
        ok = PV::clear() && write(R"({"schemaVersion":1,"entries":{"Open":"Inspect","Inspect":"Again"}})") &&
             PV::load(input) && PV::loaded() && shown() == "Inspect item";
    } else if (phase == "restore-first") {
        ok = PV::loaded() && shown() == "Inspect item";
    } else if (phase == "invalid") {
        ok = PV::loaded() && write(R"({"schemaVersion":2,"entries":{"Open":"Invalid"}})") &&
             !PV::load(input) && shown() == "Inspect item";
    } else if (phase == "replace") {
        ok = write(R"({"schemaVersion":1,"entries":{"Open":"Review"}})") && PV::load(input) && shown() == "Review item";
    } else if (phase == "restore-second") {
        ok = PV::loaded() && shown() == "Review item";
    } else if (phase == "clear") {
        ok = PV::clear() && !PV::loaded() && shown() == "Open item";
    } else if (phase == "restore-empty") {
        ok = !PV::loaded() && shown() == "Open item";
    }
    std::filesystem::remove(input, error);
    std::cout << (ok ? "PASS" : "FAIL") << " synthetic persistence phase\n";
    return ok ? 0 : 1;
}
