// Standalone assertions for src/slic3r/GUI/Extensions/ExtensionHost.cpp: the
// built-in extensions (installed, updated only while unchanged, staying
// removed), adding a folder, duplicate and broken manifests, consent through
// the list, the master switch, persisted limits, and real runs that answer or
// fail until the extension turns itself off. Compiled and run by
// tests/extension_host.test.mjs.
#include "src/slic3r/GUI/Base64Toolkit/Base64Codec.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionCore.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionProcess.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionHost.cpp"

#include <cstdio>
#include <fstream>
#include <random>

using namespace Slic3r::GUI::Extensions;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static void write(const fs::path &path, const std::string &text)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
}

static std::string manifest(const std::string &id, const std::string &version, const std::string &run, const std::string &extra = "")
{
    return "{\"id\":\"" + id + "\",\"name\":\"" + id + "\",\"version\":\"" + version + "\",\"apiVersion\":1," +
           "\"run\":{\"windows\":\"" + run + "\"},\"hooks\":[\"plate.sliced\"],\"permissions\":[\"slice.read\",\"notes.show\"" + extra + "]}";
}

static void make_script(const fs::path &folder, const char *name, const std::string &body)
{
#ifdef _WIN32
    write(folder / name, "@echo off\r\n" + body + "\r\n");
#else
    write(folder / name, "#!/bin/sh\n" + body + "\n");
    fs::permissions(folder / name, fs::perms::owner_all);
#endif
}

#ifdef _WIN32
static const char *kAnswerRun = "cmd.exe /d /c answer.cmd";
static const char *kFailRun   = "cmd.exe /d /c fail.cmd";
static const char *kAnswerBody = "echo {\"notes\":[\"sliced\"]}";
static const char *kFailBody   = "exit /b 4";
#else
static const char *kAnswerRun = "answer.sh";
static const char *kFailRun   = "fail.sh";
static const char *kAnswerBody = "while read -r _; do :; done; echo '{\"notes\":[\"sliced\"]}'";
static const char *kFailBody   = "exit 4";
static const char *kAnswerName = "answer.sh";
static const char *kFailName   = "fail.sh";
#endif

int main()
{
#ifdef _WIN32
    const char *kAnswerName = "answer.cmd";
    const char *kFailName   = "fail.cmd";
#endif
    const fs::path base    = fs::temp_directory_path() / ("ext-host-" + std::to_string(std::random_device{}()));
    const fs::path root    = base / "extensions";
    const fs::path state   = base / "extension-state";
    const fs::path builtin = base / "resources-extensions";

    // A built-in, version 1.0.0.
    write(builtin / "report" / "manifest.json", manifest("studio.builtin.report", "1.0.0", kAnswerRun));
    make_script(builtin / "report", kAnswerName, kAnswerBody);

    Host host(root, state, builtin);
    host.load();
    const Installed *report = host.find("studio.builtin.report");
    CHECK(report && report->builtin && !report->modified);
    CHECK(fs::exists(root / "studio.builtin.report" / ".builtin.json"));
    // Built-ins wait for consent like any other extension.
    CHECK(report && host.state(*report) == State::WaitingNew);
    CHECK(host.plan(Hook::PlateSliced).empty());

    host.allow("studio.builtin.report");
    report = host.find("studio.builtin.report");
    CHECK(report && host.state(*report) == State::On);

    // A real run answers and is recorded.
    std::vector<PlannedRun> plans = host.plan(Hook::PlateSliced);
    CHECK(plans.size() == 1);
    if (!plans.empty()) {
        const RunResult r = execute(plans.front(), nlohmann::json{{"slice", {{"seconds", 60}}}}, "Plate 1", 1762581161);
        CHECK(r.outcome == Outcome::Answered && r.answer.notes == std::vector<std::string>{"sliced"});
        host.record(r);
        CHECK(host.runs().records().size() == 1 && host.runs().records().back().subject == "Plate 1");
    }

    // An unchanged built-in is updated to 1.1.0, and asks again.
    write(builtin / "report" / "manifest.json", manifest("studio.builtin.report", "1.1.0", kAnswerRun));
    host.load();
    report = host.find("studio.builtin.report");
    CHECK(report && report->manifest.version == "1.1.0" && host.state(*report) == State::WaitingChanged);
    host.allow("studio.builtin.report");

    // A built-in you changed is left alone, and says so.
    write(root / "studio.builtin.report" / "notes.txt", "mine");
    write(builtin / "report" / "manifest.json", manifest("studio.builtin.report", "1.2.0", kAnswerRun));
    host.load();
    report = host.find("studio.builtin.report");
    CHECK(report && report->manifest.version == "1.1.0" && report->modified);
    CHECK(fs::exists(root / "studio.builtin.report" / "notes.txt"));

    // On and off from the list.
    CHECK(host.set_enabled("studio.builtin.report", false));
    CHECK(host.state(*host.find("studio.builtin.report")) == State::OffByUser);
    CHECK(host.set_enabled("studio.builtin.report", true));
    CHECK(host.state(*host.find("studio.builtin.report")) == State::On);

    // The master switch turns everything off and plans nothing.
    host.set_master(false);
    CHECK(host.state(*host.find("studio.builtin.report")) == State::AllOff && host.plan(Hook::PlateSliced).empty());
    host.set_master(true);

    // Adding folders: new, the same version, a newer one; duplicates and broken manifests.
    const fs::path incoming = base / "picked" / "weigh-in";
    write(incoming / "manifest.json", manifest("com.example.weigh-in", "1.0.0", kFailRun));
    make_script(incoming, kFailName, kFailBody);
    Manifest      m;
    ManifestError e;
    CHECK(host.inspect(incoming, m, e) == Host::Inspection::New);
    std::string error;
    CHECK(host.install(incoming, error));
    CHECK(host.inspect(incoming, m, e) == Host::Inspection::SameVersion);
    write(incoming / "manifest.json", manifest("com.example.weigh-in", "1.1.0", kFailRun, ",\"printer.read\""));
    CHECK(host.inspect(incoming, m, e) == Host::Inspection::NewerVersion);
    write(root / "copy-of-weigh-in" / "manifest.json", manifest("com.example.weigh-in", "0.9.0", kFailRun));
    write(root / "future" / "manifest.json", "{\"id\":\"com.example.future\",\"name\":\"Future\",\"version\":\"1.0.0\",\"apiVersion\":3,"
                                             "\"run\":{\"windows\":\"x.exe\"},\"hooks\":[\"plate.sliced\"]}");
    host.load();
    int duplicates = 0, newer = 0;
    for (const Installed &x : host.extensions()) {
        if (x.error.code == "duplicate") ++duplicates;
        if (x.error.code == "api-newer" && x.error.detail == "3") {
            ++newer;
            CHECK(host.state(x) == State::ManifestError);
        }
    }
    CHECK(duplicates == 1 && newer == 1);
    fs::remove_all(root / "copy-of-weigh-in");
    host.load();

    // A failing extension turns itself off after three failures in a row.
    host.allow("com.example.weigh-in");
    for (int i = 0; i < 3; ++i) {
        plans = host.plan(Hook::PlateSliced);
        for (const PlannedRun &p : plans)
            if (p.id == "com.example.weigh-in") {
                const RunResult r = execute(p, nlohmann::json::object(), "Plate 1", 1762581162 + i);
                CHECK(r.outcome == Outcome::Failed && r.problem == "exit code 4");
                host.record(r);
            }
    }
    CHECK(host.state(*host.find("com.example.weigh-in")) == State::OffAfterFailures);
    for (const PlannedRun &p : host.plan(Hook::PlateSliced)) CHECK(p.id != "com.example.weigh-in");
    CHECK(host.set_enabled("com.example.weigh-in", true));
    CHECK(host.state(*host.find("com.example.weigh-in")) == State::On);

    // A changed permission list needs the prompt before it can be turned on.
    write(incoming / "manifest.json", manifest("com.example.weigh-in", "1.2.0", kFailRun, ",\"printer.read\""));
    CHECK(host.install(incoming, error));
    CHECK(host.state(*host.find("com.example.weigh-in")) == State::WaitingChanged);
    CHECK(!host.set_enabled("com.example.weigh-in", true));

    // Limits persist; a removed built-in stays removed.
    host.set_limits(Limits{5000, 32 * 1024, 5});
    CHECK(host.remove("studio.builtin.report", error));
    Host again(root, state, builtin);
    again.load();
    CHECK(again.limits().timeout_ms == 5000 && again.limits().max_answer == 32 * 1024 && again.limits().off_after_failures == 5);
    CHECK(again.find("studio.builtin.report") == nullptr);
    CHECK(again.runs().records().size() == 4);

    std::error_code ec;
    fs::remove_all(base, ec);
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("extension host: all checks passed\n");
    return 0;
}
