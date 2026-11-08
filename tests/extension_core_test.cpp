// Standalone assertions for src/slic3r/GUI/Extensions/ExtensionCore.cpp and
// ExtensionProcess.cpp: manifests and their refusals, permissions and what they
// let through, consent on new and changed permissions, answers and their
// limits, the run record, the verdict of the check before a print is sent,
// and real extension processes (answered, timed out, too large, refused paths,
// a clean environment). Compiled and run by tests/extension_core.test.mjs.
#include "src/slic3r/GUI/Extensions/ExtensionCore.cpp"
#include "src/slic3r/GUI/Extensions/ExtensionProcess.cpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>

using namespace Slic3r::GUI::Extensions;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static const char *kSpool = R"({
  "id": "com.example.spool-weigh-in",
  "name": "Spool weigh-in",
  "version": "1.2.0",
  "apiVersion": 1,
  "description": "Tracks spool weights.",
  "run": { "windows": "powershell.exe -NoProfile -File weigh-in.ps1" },
  "hooks": ["plate.sliced", "print.before_send"],
  "actions": [{ "id": "show", "title": "Show spool weights" }],
  "permissions": ["slice.read", "printer.read", "send.warn"]
})";

static std::string with(const std::string &from, const std::string &to)
{
    std::string text = kSpool;
    const auto  at   = text.find(from);
    if (at != std::string::npos) text.replace(at, from.size(), to);
    return text;
}

static ManifestError refuse(const std::string &json)
{
    Manifest      m;
    ManifestError e;
    CHECK(!parse_manifest(json, m, e));
    return e;
}

static void manifests()
{
    Manifest      m;
    ManifestError e;
    CHECK(parse_manifest(kSpool, m, e) && e.ok());
    CHECK(m.id == "com.example.spool-weigh-in" && m.version == "1.2.0" && m.api_version == 1);
    CHECK(m.argv.size() == 4 && m.argv[0] == "powershell.exe" && m.argv[3] == "weigh-in.ps1");
    CHECK(m.actions.size() == 1 && m.actions[0].title == "Show spool weights");
    // Actions bring the command hook with them.
    CHECK(std::find(m.hooks.begin(), m.hooks.end(), Hook::Command) != m.hooks.end());
    CHECK((m.permissions == std::vector<Permission>{Permission::SliceRead, Permission::PrinterRead, Permission::SendWarn}));

    CHECK(refuse("{nope").code == "not-json");
    CHECK(refuse("[1]").code == "wrong-type");
    CHECK(refuse(with("\"description\"", "\"surprise\": 1, \"description\"")).code == "unknown-field");
    CHECK(refuse(with("com.example.spool-weigh-in", "Spool")).code == "bad-id");
    CHECK(refuse(with("com.example.spool-weigh-in", "com..spool")).code == "bad-id");
    CHECK(refuse(with("\"1.2.0\"", "\"1.2\"")).code == "bad-version");
    const ManifestError newer = refuse(with("\"apiVersion\": 1", "\"apiVersion\": 3"));
    CHECK(newer.code == "api-newer" && newer.detail == "3" && newer.field == "apiVersion");
    CHECK(refuse(with("\"apiVersion\": 1", "\"apiVersion\": 0")).code == "api-older");
    const ManifestError hook = refuse(with("\"plate.sliced\"", "\"plate.melted\""));
    CHECK(hook.code == "unknown-hook" && hook.detail == "plate.melted");
    CHECK(refuse(with("\"send.warn\"", "\"wallet.read\"")).code == "unknown-permission");
    CHECK(refuse(with("\"print.before_send\"", "\"plate.sliced\"")).code == "duplicate");
    CHECK(refuse(with("{ \"windows\": \"powershell.exe -NoProfile -File weigh-in.ps1\" }", "{ \"linux\": \"sh x.sh\" }")).code == "missing");
    CHECK(refuse(with("powershell.exe -NoProfile -File weigh-in.ps1", "   ")).code == "empty-run");
    CHECK(refuse(with("\"Spool weigh-in\"", "\"" + std::string(81, 'x') + "\"")).code == "too-long");
    CHECK(refuse(std::string(kManifestLimit + 1, ' ')).code == "too-large");

    CHECK((split_command_line(R"(a "b c" d)") == std::vector<std::string>{"a", "b c", "d"}));
    CHECK((split_command_line(R"(x\\"y z")") == std::vector<std::string>{"x\\y z"}));
    CHECK((split_command_line(R"(x\"y)") == std::vector<std::string>{"x\"y"}));
    CHECK((split_command_line(R"(C:\tools\a.exe "")") == std::vector<std::string>{"C:\\tools\\a.exe", ""}));
    CHECK(compare_versions("1.10.0", "1.9.9") == 1 && compare_versions("1.2.0", "1.2.0") == 0 && compare_versions("0.4.1", "0.5.0") == -1);
}

static void consent_and_state()
{
    Manifest      m;
    ManifestError e;
    parse_manifest(kSpool, m, e);
    CHECK(state_for(e, m, nullptr, true) == State::WaitingNew);
    CHECK(state_for(e, m, nullptr, false) == State::AllOff);
    ManifestError broken{"api-newer", "apiVersion", "3"};
    CHECK(state_for(broken, m, nullptr, true) == State::ManifestError);

    Consent allowed_now = answer_consent(m, nullptr, true);
    CHECK(state_for(e, m, &allowed_now, true) == State::On);
    CHECK(allowed(allowed_now, Permission::SendWarn) && !allowed(allowed_now, Permission::NotesShow));

    // 1.3.0 asks for something new: it stays off until allowed again, and says what is new.
    Manifest next;
    parse_manifest(with("\"1.2.0\"", "\"1.3.0\"").replace(std::string(kSpool).find("\"send.warn\""), 0, "\"notes.show\", "), next, e);
    CHECK(e.ok());
    CHECK(state_for(e, next, &allowed_now, true) == State::WaitingChanged);
    CHECK((added_permissions(next, allowed_now) == std::vector<Permission>{Permission::NotesShow}));
    // A new version with the same list still asks again.
    Manifest same;
    parse_manifest(with("\"1.2.0\"", "\"1.2.1\""), same, e);
    CHECK(state_for(e, same, &allowed_now, true) == State::WaitingChanged && added_permissions(same, allowed_now).empty());

    const Consent kept_off = answer_consent(next, &allowed_now, false);
    CHECK(state_for(e, next, &kept_off, true) == State::OffByUser);

    Limits  limits;
    Consent c = allowed_now;
    c         = after_run(c, Outcome::TimedOut, limits);
    c         = after_run(c, Outcome::Answered, limits);
    CHECK(c.failures_in_row == 0);
    c = after_run(after_run(after_run(c, Outcome::NotJson, limits), Outcome::TooLarge, limits), Outcome::NotStarted, limits);
    CHECK(c.failures_in_row == 2 && c.enabled);
    // Stopping a run by hand, or closing the app, is never the extension's failure.
    c = after_run(after_run(c, Outcome::Stopped, limits), Outcome::Stopped, limits);
    CHECK(c.failures_in_row == 2 && c.enabled);
    c = after_run(c, Outcome::Failed, limits);
    CHECK(c.off_after_failures && !c.enabled);
    CHECK(state_for(e, m, &c, true) == State::OffAfterFailures);

    std::map<std::string, Consent> all{{m.id, allowed_now}, {"org.other.thing", kept_off}};
    std::map<std::string, Consent> back;
    CHECK(parse_consents(serialize_consents(all), back));
    CHECK(back.size() == 2 && back[m.id].allowed == allowed_now.allowed && back[m.id].enabled && back["org.other.thing"].off_by_user);
    CHECK(parse_consents(R"({"Bad Id":{"version":"1.0.0"},"a.b":{"allowed":["slice.read","bogus"]}})", back));
    CHECK(back.size() == 1 && back["a.b"].allowed == std::vector<Permission>{Permission::SliceRead});
    const Limits clamped = clamp_limits(Limits{50, 10, 99});
    CHECK(clamped.timeout_ms == 1000 && clamped.max_answer == 1024 && clamped.off_after_failures == 20);
}

static void requests_carry_only_what_is_allowed()
{
    Manifest      m;
    ManifestError e;
    parse_manifest(kSpool, m, e);
    Consent c = answer_consent(m, nullptr, true);
    c.allowed = {Permission::SliceRead};
    const nlohmann::json context = {
        {"slice", {{"seconds", 8040}, {"grams", 38.2}, {"access_code", "SECRET-ONE"}, {"plates", {{{"name", "p1"}, {"token", "SECRET-TWO"}}}}}},
        {"model", {{"name", "bracket"}}},
        {"printer", {{"model", "X1C"}, {"password", "SECRET-THREE"}}},
        {"cloud", {{"user", "someone"}}}};
    const nlohmann::json request = build_request(Hook::PlateSliced, "", context, m, c);
    const std::string    text    = request.dump();
    CHECK(request["hook"] == "plate.sliced" && request["apiVersion"] == 1);
    CHECK(request["context"].contains("slice") && !request["context"].contains("model") && !request["context"].contains("printer"));
    CHECK(!request["context"].contains("cloud"));
    CHECK(request["context"]["slice"]["seconds"] == 8040);
    CHECK(text.find("SECRET") == std::string::npos);
    CHECK(looks_sensitive("printerAccessCode") && looks_sensitive("access_code") && looks_sensitive("X-Token"));
    CHECK(!looks_sensitive("printerModel") && !looks_sensitive("author") && !looks_sensitive("seconds"));
}

static void answers_and_their_limits()
{
    Manifest      m;
    ManifestError e;
    parse_manifest(kSpool, m, e);
    const Consent c = answer_consent(m, nullptr, true); // slice.read, printer.read, send.warn
    Answer        a;
    std::string   problem;
    CHECK(parse_answer(R"({"apiVersion":1,"checklist":[{"state":"warn","text":"Spool 2 has 40 g left"}]})", Hook::PrintBeforeSend, m, c, a, problem));
    CHECK(a.checklist.size() == 1 && a.checklist[0].state == "warn");
    CHECK(!parse_answer(R"({"notes":["hi"]})", Hook::PlateSliced, m, c, a, problem) && problem == "not-allowed:notes.show");
    CHECK(!parse_answer(R"({"checklist":[]})", Hook::PlateSliced, m, c, a, problem) && problem == "wrong-hook:checklist");
    CHECK(!parse_answer(R"({"block":true,"blockReason":"no"})", Hook::PrintBeforeSend, m, c, a, problem) && problem == "not-allowed:send.block");
    CHECK(!parse_answer(R"({"checklist":[{"state":"maybe","text":"x"}]})", Hook::PrintBeforeSend, m, c, a, problem) && problem == "bad-state");
    CHECK(!parse_answer(R"({"apiVersion":2})", Hook::PlateSliced, m, c, a, problem) && problem == "wrong-api");
    CHECK(!parse_answer(R"({"surprise":1})", Hook::PlateSliced, m, c, a, problem) && problem == "unknown-field");
    CHECK(!parse_answer("not json", Hook::PlateSliced, m, c, a, problem) && problem == "not-json");
    CHECK(!parse_answer("[1]", Hook::PlateSliced, m, c, a, problem) && problem == "not-object");

    Consent all = c;
    all.allowed = {Permission::NotesShow, Permission::GcodeComment, Permission::FileSave, Permission::SendWarn, Permission::SendBlock};
    CHECK(parse_answer(R"({"gcode":{"prepend":["; exported 2026-11-08"],"append":["; done"]}})", Hook::GcodeExported, m, all, a, problem));
    CHECK(a.gcode_prepend.size() == 1 && a.gcode_append.size() == 1);
    CHECK(!parse_answer(R"({"gcode":{"append":["G1 X0"]}})", Hook::GcodeExported, m, all, a, problem) && problem == "not-comment");
    CHECK(!parse_answer(R"({"gcode":{"append":["; a\nG1 X0"]}})", Hook::GcodeExported, m, all, a, problem) && problem == "not-comment");
    CHECK(parse_answer(R"({"file":{"name":"report.md","text":"# Plate 1"}})", Hook::Command, m, all, a, problem) && a.file_name == "report.md");
    CHECK(!parse_answer(R"({"file":{"name":"..\\evil.exe","text":"x"}})", Hook::Command, m, all, a, problem) && problem == "bad-file-name");
    CHECK(!parse_answer(R"({"file":{"name":"run.bat","text":"x"}})", Hook::Command, m, all, a, problem) && problem == "bad-file-name");
    CHECK(parse_answer(R"({"block":true,"blockReason":"Plate 2 is empty"})", Hook::PrintBeforeSend, m, all, a, problem) && a.block);
    nlohmann::json many = {{"notes", nlohmann::json::array()}};
    for (int i = 0; i < 11; ++i) many["notes"].push_back("n");
    CHECK(!parse_answer(many.dump(), Hook::PlateSliced, m, all, a, problem) && problem == "too-many:notes");
    CHECK(!parse_answer(nlohmann::json{{"notes", {std::string(301, 'x')}}}.dump(), Hook::PlateSliced, m, all, a, problem) && problem == "too-long:notes");
}

// print.before_send runs in this build, and only a valid answer can stop a print.
static void before_send()
{
    CHECK(info(Hook::PrintBeforeSend).built);
    CHECK(info(Hook::ModelImported).built);
    CHECK(info(Hook::PrintFinished).built);
    for (const HookInfo &h : hooks()) CHECK(h.built); // every declared hook runs in this build
    CHECK(std::string(info(Hook::PrintBeforeSend).id) == "print.before_send");

    // The send section needs printer.read, like the printer section, and loses secrets at every depth.
    Manifest      m;
    ManifestError e;
    parse_manifest(kSpool, m, e);
    Consent c = answer_consent(m, nullptr, true);
    const nlohmann::json context = {
        {"send", {{"printerModel", "Bambu Lab X1 Carbon"}, {"accessCode", "SECRET-SEND"}, {"filaments", {{{"slot", 1}, {"token", "SECRET-TRAY"}}}}}},
        {"slice", {{"seconds", 60}}}};
    c.allowed = {Permission::SliceRead};
    CHECK(!build_request(Hook::PrintBeforeSend, "", context, m, c)["context"].contains("send"));
    c.allowed = {Permission::SliceRead, Permission::PrinterRead};
    const nlohmann::json request = build_request(Hook::PrintBeforeSend, "", context, m, c);
    CHECK(request["hook"] == "print.before_send");
    CHECK(request["context"]["send"]["printerModel"] == "Bambu Lab X1 Carbon");
    CHECK(request["context"]["send"]["filaments"][0]["slot"] == 1);
    CHECK(request.dump().find("SECRET") == std::string::npos);
    // print.finished's section takes printer.read too.
    const nlohmann::json ended = {{"print", {{"result", "finished"}, {"accessCode", "SECRET-AC"}}}};
    CHECK(build_request(Hook::PrintFinished, "", ended, m, c)["context"]["print"]["result"] == "finished");
    CHECK(build_request(Hook::PrintFinished, "", ended, m, c).dump().find("SECRET-AC") == std::string::npos);
    Consent no_printer = c;
    no_printer.allowed = {Permission::SliceRead};
    CHECK(!build_request(Hook::PrintFinished, "", ended, m, no_printer)["context"].contains("print"));

    auto answered = [](std::vector<ChecklistRow> rows, std::vector<std::string> notes = {}, bool block = false) {
        SendCheckRun r;
        r.id               = "com.example.x";
        r.outcome          = Outcome::Answered;
        r.answer.checklist = std::move(rows);
        r.answer.notes     = std::move(notes);
        r.answer.block     = block;
        return r;
    };
    auto other = [](Outcome outcome, bool block = false) {
        SendCheckRun r;
        r.id           = "com.example.y";
        r.outcome      = outcome;
        r.answer.block = block;
        return r;
    };

    // Nothing ran, or nothing to look at: sending carries on.
    CHECK(summarise_send_check({}).verdict == SendVerdict::Continue);
    SendSummary s = summarise_send_check({answered({{"pass", "a"}, {"pass", "b"}}), answered({})});
    CHECK(s.verdict == SendVerdict::Continue && s.passed == 2 && s.warned == 0 && s.blockers.empty());
    // Rows that warn or fail, notes, and runs without a valid answer wait for the person.
    s = summarise_send_check({answered({{"pass", "a"}, {"warn", "b"}})});
    CHECK(s.verdict == SendVerdict::Look && s.passed == 1 && s.warned == 1);
    s = summarise_send_check({answered({{"fail", "a"}})});
    CHECK(s.verdict == SendVerdict::Look && s.failed == 1);
    s = summarise_send_check({answered({}, {"Weights were read 2 hours ago."})});
    CHECK(s.verdict == SendVerdict::Look && s.notes == 1);
    for (Outcome o : {Outcome::TimedOut, Outcome::TooLarge, Outcome::NotJson, Outcome::Invalid, Outcome::Failed, Outcome::NotStarted}) {
        s = summarise_send_check({answered({{"pass", "a"}}), other(o)});
        CHECK(s.verdict == SendVerdict::Look && s.not_answered == 1 && s.passed == 1);
    }
    // Skipped by the person: counts for nothing.
    s = summarise_send_check({other(Outcome::Stopped), answered({{"pass", "a"}})});
    CHECK(s.verdict == SendVerdict::Continue && s.stopped == 1);
    // A valid answer that blocks stops the print, whatever else answered.
    s = summarise_send_check({answered({{"pass", "a"}}), answered({{"fail", "b"}}, {}, true), other(Outcome::TimedOut)});
    CHECK(s.verdict == SendVerdict::Stop && s.blockers.size() == 1 && s.blockers[0] == 1 && s.failed == 1 && s.not_answered == 1);
    s = summarise_send_check({answered({}, {}, true), answered({}, {}, true)});
    CHECK(s.verdict == SendVerdict::Stop && s.blockers.size() == 2 && s.blockers[0] == 0 && s.blockers[1] == 1);
    // An answer that broke a rule never blocks, even if it said so.
    s = summarise_send_check({other(Outcome::Invalid, true)});
    CHECK(s.verdict == SendVerdict::Look && s.blockers.empty());
    // A block is only a valid answer when the extension may block.
    Answer      a;
    std::string problem;
    Consent     warn_only = answer_consent(m, nullptr, true);
    warn_only.allowed     = {Permission::SendWarn};
    CHECK(!parse_answer(R"({"block":true,"blockReason":"no"})", Hook::PrintBeforeSend, m, warn_only, a, problem) && problem == "not-allowed:send.block");
    CHECK(parse_answer(R"({"block":false,"checklist":[{"state":"pass","text":"ok"}]})", Hook::PrintBeforeSend, m, warn_only, a, problem));
}

static void run_record()
{
    const fs::path dir = fs::temp_directory_path() / ("ext-runs-" + std::to_string(std::random_device{}()));
    RunLog         log(dir / "runs.jsonl");
    log.load();
    RunRecord r;
    r.time = 1762581161; r.extension = "com.example.a"; r.hook = "plate.sliced"; r.subject = "Plate 1";
    r.seconds = 0.38; r.answer_bytes = 412; r.outcome = Outcome::Answered;
    for (int i = 0; i < 510; ++i) log.append(r);
    CHECK(log.records().size() == RunLog::kLimit);
    RunLog again(dir / "runs.jsonl");
    again.load();
    CHECK(again.records().size() == RunLog::kLimit && again.records().back().subject == "Plate 1");
    CHECK(again.for_extension("com.example.a").size() == RunLog::kLimit && again.for_extension("nope").empty());
    RunRecord bad;
    CHECK(!from_line(R"({"t":1,"x":"a.b","o":"exploded"})", bad));
    RunRecord stopped = r;
    stopped.outcome   = Outcome::Stopped;
    RunRecord back;
    CHECK(from_line(to_line(stopped), back) && back.outcome == Outcome::Stopped);
    CHECK(again.export_json().find("\"answered\"") != std::string::npos);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

#ifndef _WIN32
static fs::path script(const fs::path &dir, const char *name, const std::string &body)
{
    const fs::path p = dir / name;
    std::ofstream(p) << "#!/bin/sh\n" << body << "\n";
    fs::permissions(p, fs::perms::owner_all);
    return p;
}

static void real_processes()
{
    const fs::path dir = fs::temp_directory_path() / ("ext-proc-" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    script(dir, "echo.sh", "read line; printf '{\"notes\":[\"got %s bytes\"]}' \"${#line}\"");
    script(dir, "slow.sh", "sleep 5; echo '{}'");
    script(dir, "big.sh", "head -c 100000 /dev/zero | tr '\\\\0' 'x'");
    script(dir, "env.sh", "env");
    script(dir, "fail.sh", "echo oops >&2; exit 3");

    ProcessRequest r;
    r.folder = dir;
    r.input  = R"({"apiVersion":1,"hook":"plate.sliced"})";
    r.argv   = {"echo.sh"};
    ProcessResult res = run_process(r);
    CHECK(res.status == ProcessResult::Status::Exited && res.exit_code == 0);
    CHECK(res.output.find("got 38 bytes") != std::string::npos);

    r.argv       = {"slow.sh"};
    r.timeout_ms = 300;
    res          = run_process(r);
    CHECK(res.status == ProcessResult::Status::TimedOut && res.seconds < 3.0);

    r.argv       = {"big.sh"};
    r.timeout_ms = 5000;
    res          = run_process(r);
    CHECK(res.status == ProcessResult::Status::TooLarge && res.output.size() <= r.max_output);

    ::setenv("BAMBU_TEST_ACCESS_TOKEN", "SECRET-ENV-VALUE", 1);
    r.argv = {"env.sh"};
    res    = run_process(r);
    CHECK(res.status == ProcessResult::Status::Exited);
    CHECK(res.output.find("SECRET-ENV-VALUE") == std::string::npos);
    CHECK(res.output.find("BAMBU_STUDIO_EXTENSION_API=1") != std::string::npos);

    r.argv = {"fail.sh"};
    res    = run_process(r);
    CHECK(res.status == ProcessResult::Status::Exited && res.exit_code == 3 && res.errors.find("oops") != std::string::npos);

    r.argv = {"../escape.sh"};
    res    = run_process(r);
    CHECK(res.status == ProcessResult::Status::StartFailed && res.start_error.find("leaves") != std::string::npos);
    r.argv = {"missing-program-xyz"};
    res    = run_process(r);
    CHECK(res.status == ProcessResult::Status::StartFailed);
    // A bare name falls back to the system path.
    r.argv = {"sh", "-c", "echo '{}'"};
    res    = run_process(r);
    CHECK(res.status == ProcessResult::Status::Exited && res.output.find("{}") != std::string::npos);
    std::error_code ec;
    fs::remove_all(dir, ec);
}
#endif

#ifdef _WIN32
static void batch(const fs::path &dir, const char *name, const std::string &body)
{
    std::ofstream(dir / name, std::ios::binary) << "@echo off\r\n" << body << "\r\n";
}

static void real_processes_windows()
{
    const fs::path dir = fs::temp_directory_path() / ("ext-proc-" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    batch(dir, "reply.cmd", "set /p line=\r\necho {\"notes\":[\"got it\"]}");
    batch(dir, "slow.cmd", "ping -n 6 127.0.0.1 >nul\r\necho {}");
    batch(dir, "big.cmd", "for /l %%i in (1,1,3000) do echo xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    batch(dir, "env.cmd", "set");
    batch(dir, "fail.cmd", "echo oops 1>&2\r\nexit /b 3");

    ProcessRequest r;
    r.folder = dir;
    r.input  = "{\"apiVersion\":1}\r\n";
    r.argv   = {"cmd.exe", "/d", "/c", "reply.cmd"}; // not "echo.cmd": cmd reads that as the echo command
    ProcessResult res = run_process(r);
    CHECK(res.status == ProcessResult::Status::Exited && res.exit_code == 0 && res.output.find("got it") != std::string::npos);

    r.argv       = {"cmd.exe", "/d", "/c", "slow.cmd"};
    r.timeout_ms = 300;
    res          = run_process(r);
    CHECK(res.status == ProcessResult::Status::TimedOut && res.seconds < 3.0);

    r.argv       = {"cmd.exe", "/d", "/c", "big.cmd"};
    r.timeout_ms = 20000;
    res          = run_process(r);
    CHECK(res.status == ProcessResult::Status::TooLarge && res.output.size() <= r.max_output);

    ::SetEnvironmentVariableW(L"BAMBU_TEST_ACCESS_TOKEN", L"SECRET-ENV-VALUE");
    r.argv = {"cmd.exe", "/d", "/c", "env.cmd"};
    res    = run_process(r);
    CHECK(res.status == ProcessResult::Status::Exited && res.output.find("SECRET-ENV-VALUE") == std::string::npos);
    CHECK(res.output.find("BAMBU_STUDIO_EXTENSION_API=1") != std::string::npos);

    r.argv = {"cmd.exe", "/d", "/c", "fail.cmd"};
    res    = run_process(r);
    CHECK(res.status == ProcessResult::Status::Exited && res.exit_code == 3 && res.errors.find("oops") != std::string::npos);

    r.argv = {"..\\escape.cmd"};
    res    = run_process(r);
    CHECK(res.status == ProcessResult::Status::StartFailed && res.start_error.find("leaves") != std::string::npos);
    std::error_code ec;
    fs::remove_all(dir, ec);
}
#endif

int main()
{
    manifests();
    consent_and_state();
    requests_carry_only_what_is_allowed();
    answers_and_their_limits();
    before_send();
    run_record();
#ifndef _WIN32
    real_processes();
#else
    real_processes_windows();
#endif
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("extension core: all checks passed\n");
    return 0;
}
