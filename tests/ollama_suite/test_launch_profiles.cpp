#include "libslic3r/OllamaSuite/LaunchProfiles.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace Slic3r::OllamaSuite;
namespace {
int checks = 0;
void check(bool value) { ++checks; if (!value) throw std::runtime_error("launch profile assertion failed at check " + std::to_string(checks)); }
#ifdef _WIN32
const std::string root = "C:/verified/";
#else
const std::string root = "/verified/";
#endif
PickerEvidence file(const std::string& name) { return {root + name, std::string(64, 'a'), "picker-receipt"}; }
struct Probe : RuntimeProbe {
    bool files = true, directory = true, port = true, model = true, fit = true;
    bool verified_file(const PickerEvidence&, bool) const override { return files; }
    bool directory_exists(const std::string&) const override { return directory; }
    bool port_available(unsigned) const override { return port; }
    bool installed_model(const std::string&) const override { return model; }
    bool hardware_fit(const LaunchProfile&) const override { return fit; }
};
struct Executor : LaunchExecutor {
    std::string calls;
    bool has_snapshot = true, applied = true, started = true, stopped = true, restored = true, throws = false;
    ReadinessResult readiness = ReadinessResult::Ready;
    std::optional<RedactedLaunchSnapshot> snapshot() override {
        calls += 'S'; if (!has_snapshot) return {}; return RedactedLaunchSnapshot{"old-profile", "old:tag", 4096, 8080, false};
    }
    bool apply(const ValidatedLaunchPlan&) override { calls += 'A'; return applied; }
    bool start(const ValidatedLaunchPlan&) override { calls += 'L'; if (throws) throw std::runtime_error("private executor text"); return started; }
    ReadinessResult ready(unsigned) override { calls += 'H'; return readiness; }
    bool stop_owned() override { calls += 'T'; return stopped; }
    bool restore(const RedactedLaunchSnapshot&) override { calls += 'R'; return restored; }
};
}
int main() {
    Probe probe;
    const auto presets = LaunchProfileRegistry::prebuilt();
    check(presets.size() == 2);
    for (const auto& preset : presets) check(!LaunchProfileRegistry::prepare(preset, probe).plan);
    auto profile = presets[0]; profile.executable = file("ollama.exe");
    profile.working_directory = root; profile.model_tag = "llama3.2:3b";
    auto prepared = LaunchProfileRegistry::prepare(profile, probe); check(prepared.plan.has_value());
    check(prepared.plan->preview().arguments == std::vector<std::string>({"run", "llama3.2:3b"}));
    check(prepared.plan->preview().environment.at("OLLAMA_HOST") == "127.0.0.1:11434");
    for (const auto& bad : {"../ollama.exe", "cmd.exe", "powershell.exe", "ollama.cmd", "ollama.exe;calc"}) {
        auto p = profile; p.executable = file(bad); check(!LaunchProfileRegistry::prepare(p, probe).plan);
    }
    for (const auto& bad : {"--help", "$(calc)", "x;calc", "x\nrun", "%PATH%", "x|run", "x&run"}) {
        auto p = profile; p.model_tag = bad; check(!LaunchProfileRegistry::prepare(p, probe).plan);
    }
    for (const auto& bad : {"relative", "C:relative", "~/models", "%HOME%/models"}) {
        auto p = profile; p.working_directory = bad; check(!LaunchProfileRegistry::prepare(p, probe).plan);
    }
    auto bad = profile; bad.executable->picker_receipt.clear(); check(!LaunchProfileRegistry::prepare(bad, probe).plan);
    bad = profile; bad.executable->content_identity = "not-a-digest"; check(!LaunchProfileRegistry::prepare(bad, probe).plan);
    bad = profile; bad.kind = static_cast<LaunchKind>(55); check(!LaunchProfileRegistry::prepare(bad, probe).plan);
    auto server = presets[1]; server.executable = file("llama-server.exe"); server.model_file = file("model.gguf"); server.working_directory = root;
    check(LaunchProfileRegistry::prepare(server, probe).plan.has_value());
    probe.port = false; check(!LaunchProfileRegistry::prepare(server, probe).plan); probe.port = true;
    server.port = 65536; check(!LaunchProfileRegistry::prepare(server, probe).plan);
    server.port = 22; check(!LaunchProfileRegistry::prepare(server, probe).plan);
    probe.fit = false; check(!LaunchProfileRegistry::prepare(profile, probe).plan); probe.fit = true;
    probe.model = false; check(!LaunchProfileRegistry::prepare(profile, probe).plan); probe.model = true;
    Executor ok; auto outcome = LaunchTransaction::run(*prepared.plan, probe, ok);
    check(outcome.state == LaunchState::Ready); check(ok.calls == "SALH");
    Executor stale; probe.files = false; outcome = LaunchTransaction::run(*prepared.plan, probe, stale);
    check(outcome.state == LaunchState::Blocked); check(stale.calls.empty()); probe.files = true;
    Executor missing; missing.has_snapshot = false; outcome = LaunchTransaction::run(*prepared.plan, probe, missing);
    check(outcome.state == LaunchState::SnapshotFailed); check(missing.calls == "S");
    Executor mutation; mutation.applied = false; outcome = LaunchTransaction::run(*prepared.plan, probe, mutation);
    check(outcome.state == LaunchState::Restored); check(mutation.calls == "SATR");
    Executor launch; launch.started = false; outcome = LaunchTransaction::run(*prepared.plan, probe, launch);
    check(outcome.state == LaunchState::Restored); check(launch.calls == "SALTR");
    for (auto status : {ReadinessResult::Exited, ReadinessResult::TimedOut, ReadinessResult::Unhealthy}) {
        Executor health; health.readiness = status; outcome = LaunchTransaction::run(*prepared.plan, probe, health);
        check(outcome.state == LaunchState::Restored); check(health.calls == "SALHTR");
    }
    Executor partial; partial.started = false; partial.stopped = false;
    outcome = LaunchTransaction::run(*prepared.plan, probe, partial);
    check(outcome.state == LaunchState::RollbackFailed); check(partial.calls == "SALT");
    Executor restore_failure; restore_failure.started = false; restore_failure.restored = false;
    check(LaunchTransaction::run(*prepared.plan, probe, restore_failure).state == LaunchState::RollbackFailed);
    Executor exception; exception.throws = true; outcome = LaunchTransaction::run(*prepared.plan, probe, exception);
    check(outcome.state == LaunchState::Restored); check(outcome.diagnostic.find("private executor") == std::string::npos);
    Executor manual; check(LaunchTransaction::restore(*outcome.snapshot, manual).state == LaunchState::Restored);
    check(manual.calls == "STR");
    Executor restore_snapshot_missing; restore_snapshot_missing.has_snapshot = false;
    check(LaunchTransaction::restore(*outcome.snapshot, restore_snapshot_missing).state == LaunchState::SnapshotFailed);
    check(restore_snapshot_missing.calls == "S");
    Executor invalid; auto unsafe = *outcome.snapshot; unsafe.model_tag = "secret\nvalue";
    check(LaunchTransaction::restore(unsafe, invalid).state == LaunchState::Blocked); check(invalid.calls.empty());
    check(LaunchTransaction::run(*prepared.plan, probe, invalid, 0).state == LaunchState::Blocked);
    std::cout << "launch profiles: " << checks << " checks passed\n";
}
