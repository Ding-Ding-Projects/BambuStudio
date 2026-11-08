// Checks the extensions that come with Bambu Studio (resources/extensions).
// Everywhere: every manifest parses, its id is studio.builtin.*, it never asks
// to block a print, its program is Windows PowerShell running a script in its
// own folder, and every file is plain ASCII (Windows PowerShell 5.1 reads a
// script without a byte order mark in the ANSI code page, so anything else
// would be garbled). On Windows each extension then really runs, through the
// request building, process runner and answer rules the app itself uses, with
// the sample request of every hook and command it declares and with a request
// that has no slice at all, and must answer. A copy whose script prints
// something that is not JSON must be caught. Compiled and run by
// tests/extension_builtins.test.mjs.
#include "src/slic3r/GUI/Base64Toolkit/Base64Codec.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionCore.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionProcess.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionHost.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionContext.cpp"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>

using namespace Slic3r::GUI::Extensions;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static std::string read(const fs::path &path)
{
    std::ifstream      in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

static bool ascii(const std::string &text)
{
    for (unsigned char c : text)
        if (c > 0x7E || (c < 0x20 && c != '\n' && c != '\r' && c != '\t')) return false;
    return true;
}

#ifdef _WIN32
static RunResult run_once(const fs::path &folder, const Manifest &m, Hook hook, const std::string &action, const nlohmann::json &context)
{
    Consent all;
    all.version = m.version;
    all.allowed = m.permissions;
    all.enabled = true;
    Limits limits;
    limits.timeout_ms = 30000; // hosted machines can be slow to start PowerShell the first time
    const PlannedRun plan{m.id, folder, m, all, limits, hook, action};
    RunResult        r = execute(plan, context, "Sample request", std::int64_t(std::time(nullptr)));
    if (r.outcome != Outcome::Answered) {
        // Say why: run it again and show what the program wrote.
        ProcessRequest p;
        p.folder     = folder;
        p.argv       = m.argv;
        p.input      = build_request(hook, action, context, m, all).dump();
        p.timeout_ms = limits.timeout_ms;
        const ProcessResult again = run_process(p);
        std::printf("%s %s%s: %s (%s)\n  output: %.400s\n  errors: %.1200s\n", m.id.c_str(), info(hook).id, action.empty() ? "" : (":" + action).c_str(),
                    outcome_id(r.outcome), r.problem.c_str(), again.output.c_str(), again.errors.c_str());
    }
    return r;
}
#endif

int main(int argc, char **argv)
{
    const fs::path root  = argc > 1 ? fs::u8path(argv[1]) : fs::path("resources/extensions");
    int            count = 0;
    for (const auto &entry : fs::directory_iterator(root)) {
        if (!entry.is_directory()) continue;
        ++count;
        const fs::path folder = entry.path();
        Manifest       m;
        ManifestError  e;
        const bool     parsed = parse_manifest(read(folder / "manifest.json"), m, e);
        CHECK(parsed);
        if (!parsed) {
            std::printf("%s: %s %s %s\n", folder.filename().u8string().c_str(), e.code.c_str(), e.field.c_str(), e.detail.c_str());
            continue;
        }
        CHECK(m.id.rfind("studio.builtin.", 0) == 0);
        CHECK(m.api_version == kApiVersion && !m.description.empty());
        CHECK(std::find(m.permissions.begin(), m.permissions.end(), Permission::SendBlock) == m.permissions.end());
        CHECK(m.argv.size() >= 3 && m.argv.front() == "powershell.exe");
        const auto file = std::find(m.argv.begin(), m.argv.end(), "-File");
        CHECK(file != m.argv.end() && file + 1 != m.argv.end());
        if (file != m.argv.end() && file + 1 != m.argv.end()) {
            const fs::path script = folder / fs::u8path(*(file + 1));
            CHECK(fs::exists(script) && script.parent_path() == folder);
            // The script reads standard input as UTF-8 and writes its answer as UTF-8.
            const std::string text = read(script);
            CHECK(text.find("OpenStandardInput") != std::string::npos && text.find("OpenStandardOutput") != std::string::npos);
        }
        for (const auto &f : fs::recursive_directory_iterator(folder)) {
            if (!f.is_regular_file()) continue;
            const bool plain = ascii(read(f.path()));
            CHECK(plain);
            if (!plain) std::printf("not ASCII: %s\n", f.path().u8string().c_str());
        }
#ifdef _WIN32
        for (Hook h : m.hooks) {
            std::vector<std::string> actions;
            if (h == Hook::Command)
                for (const Action &a : m.actions) actions.push_back(a.id);
            else
                actions.push_back(std::string());
            for (const std::string &action : actions) {
                const RunResult r = run_once(folder, m, h, action, sample_context(h));
                CHECK(r.outcome == Outcome::Answered);
                if (h == Hook::GcodeExported) CHECK(r.answer.gcode_prepend.size() >= 2 && r.answer.gcode_append.size() == 1);
                if (h == Hook::PrintBeforeSend) CHECK(!r.answer.checklist.empty());
                if (h == Hook::Command && m.id == "studio.builtin.slice-report")
                    CHECK(r.answer.file_name == "slice-report-plate-1.md" && r.answer.file_text.rfind("# Slice report: Plate 1", 0) == 0);
                if (h == Hook::PlateSliced) CHECK(!r.answer.notes.empty()); // the sample has a warning, so the checklist speaks too
                // Without a slice section (not sliced, or not allowed) it still answers.
                const RunResult bare = run_once(folder, m, h, action, nlohmann::json::object());
                CHECK(bare.outcome == Outcome::Answered);
            }
        }
        if (m.id == "studio.builtin.slice-report") {
            // A copy that prints something else is caught as not JSON.
            const fs::path copy = fs::temp_directory_path() / ("ext-builtin-" + std::to_string(std::random_device{}()));
            fs::create_directories(copy);
            fs::copy_file(folder / "manifest.json", copy / "manifest.json");
            std::ofstream(copy / "slice-report.ps1", std::ios::binary) << "Write-Output 'this is not JSON'\r\n";
            Consent all;
            all.version = m.version;
            all.allowed = m.permissions;
            all.enabled = true;
            Limits limits;
            limits.timeout_ms = 30000;
            const RunResult broken = execute(PlannedRun{m.id, copy, m, all, limits, Hook::PlateSliced, {}}, sample_context(Hook::PlateSliced),
                                             "Sample request", std::int64_t(std::time(nullptr)));
            CHECK(broken.outcome == Outcome::NotJson);
            std::error_code ec;
            fs::remove_all(copy, ec);
        }
#endif
    }
    CHECK(count == 3);
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
#ifdef _WIN32
    std::printf("built-in extensions: %d checked and run under Windows PowerShell\n", count);
#else
    std::printf("built-in extensions: %d manifests and scripts checked (they run only on Windows)\n", count);
#endif
    return 0;
}
