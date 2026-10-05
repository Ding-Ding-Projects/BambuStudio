#include "LaunchProfiles.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

namespace Slic3r { namespace OllamaSuite {
namespace {
bool safe_text(const std::string& s) {
    return s.size() <= 4096 && std::none_of(s.begin(), s.end(), [](unsigned char c) {
        return c < 32 || c == 127 || c == '$' || c == '%' || c == '`' || c == '|' ||
               c == ';' || c == '&' || c == '<' || c == '>' || c == '"';
    });
}
bool absolute_path(const std::string& s) {
    if (s.empty() || !safe_text(s)) return false;
    try {
    const std::filesystem::path p(s);
    if (!p.is_absolute()) return false;
    for (const auto& part : p) if (part == ".." || part == ".") return false;
#ifdef _WIN32
    // Local drive paths only; reject device paths, UNC, and alternate data streams.
    if (s.size() < 3 || !std::isalpha(static_cast<unsigned char>(s[0])) || s[1] != ':' ||
        (s[2] != '/' && s[2] != '\\') || s.find(':', 2) != std::string::npos) return false;
#endif
    return true;
    } catch (...) { return false; }
}
bool token(const std::string& s, bool model = false) {
    if (s.empty() || s.size() > 200 || s.front() == '-') return false;
    return std::all_of(s.begin(), s.end(), [model](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
               (model && (c == ':' || c == '/'));
    });
}
bool evidence_ok(const PickerEvidence& e, const RuntimeProbe& p, bool executable) {
    return absolute_path(e.absolute_path) && !e.picker_receipt.empty() &&
           e.picker_receipt.size() <= 512 && e.content_identity.size() == 64 &&
           std::all_of(e.content_identity.begin(), e.content_identity.end(), [](unsigned char c) {
               return std::isxdigit(c) != 0;
           }) && p.verified_file(e, executable);
}
bool snapshot_ok(const RedactedLaunchSnapshot& s) {
    return (s.profile_id.empty() || token(s.profile_id)) &&
           (s.model_tag.empty() || token(s.model_tag, true)) && s.port <= 65535 &&
           s.context_length <= 1048576;
}
void state(LaunchOutcome& o, LaunchState s) { o.state = s; o.transitions.push_back(s); }
void rollback(LaunchOutcome& o, LaunchExecutor& e) {
    state(o, LaunchState::RollingBack);
    bool stopped = false, restored = false;
    try { stopped = e.stop_owned(); } catch (...) {}
    // Do not restore files while a child may still be writing them.
    if (stopped) try { restored = e.restore(*o.snapshot); } catch (...) {}
    state(o, stopped && restored ? LaunchState::Restored : LaunchState::RollbackFailed);
}
}
ValidatedLaunchPlan::ValidatedLaunchPlan(LaunchProfile p, LaunchPreview v)
    : m_profile(std::move(p)), m_preview(std::move(v)) {}

std::vector<LaunchProfile> LaunchProfileRegistry::prebuilt() {
    LaunchProfile chat; chat.id = "ollama-chat"; chat.name = "Ollama terminal chat";
    LaunchProfile server; server.id = "llama-server"; server.name = "llama.cpp local server";
    server.kind = LaunchKind::LlamaServer;
    return {chat, server};
}
LaunchPreparation LaunchProfileRegistry::prepare(const LaunchProfile& p, const RuntimeProbe& probe) {
    LaunchPreparation result;
    auto block = [&](const char* message) { result.blockers.emplace_back(message); };
    if (!token(p.id)) block("Choose a valid registered profile identifier.");
    if (!absolute_path(p.working_directory)) block("Choose an absolute local working directory.");
    if (p.context_length < 512 || p.context_length > 1048576) block("Choose a context length from 512 to 1048576.");
    LaunchPreview v; v.profile_id = p.id; v.working_directory = p.working_directory;
    try {
        if (absolute_path(p.working_directory) && !probe.directory_exists(p.working_directory))
            block("The selected working directory is unavailable; select it again.");
        if (!p.executable || !evidence_ok(*p.executable, probe, true))
            block("Select and verify the executable with the executable picker.");
        else {
            v.executable = p.executable->absolute_path;
            std::string name = std::filesystem::path(v.executable).filename().string();
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            const std::string expected = p.kind == LaunchKind::OllamaChat ? "ollama" : "llama-server";
            if (name != expected && name != expected + ".exe") block("The executable does not match this allowlisted profile.");
        }
        switch (p.kind) {
        case LaunchKind::OllamaChat:
            if (!token(p.model_tag, true) || !probe.installed_model(p.model_tag))
                block("Choose a verified installed model from the model picker.");
            v.model = p.model_tag; v.arguments = {"run", p.model_tag};
            v.environment = {{"OLLAMA_HOST", "127.0.0.1:11434"}};
            break;
        case LaunchKind::LlamaServer:
            if (!p.model_file || !evidence_ok(*p.model_file, probe, false))
                block("Select and verify a local model file with the file picker.");
            else { v.model = p.model_file->absolute_path; v.required_files.push_back(v.model); }
            if (p.port < 1024 || p.port > 65535 || !probe.port_available(p.port))
                block("Choose an available local port from 1024 to 65535.");
            v.required_ports.push_back(p.port);
            v.arguments = {"--model", v.model, "--host", "127.0.0.1", "--port", std::to_string(p.port),
                           "--ctx-size", std::to_string(p.context_length)};
            break;
        default: block("This profile type has no reviewed argument schema.");
        }
        if (!probe.hardware_fit(p)) block("Verify hardware fit before enabling this profile.");
    } catch (...) { block("Local verification is unavailable; retry verification before launch."); }
    if (result.blockers.empty()) result.plan = ValidatedLaunchPlan(p, std::move(v));
    return result;
}
LaunchOutcome LaunchTransaction::run(const ValidatedLaunchPlan& plan, const RuntimeProbe& probe,
                                     LaunchExecutor& executor, unsigned timeout_ms) {
    LaunchOutcome out;
    if (timeout_ms == 0 || timeout_ms > 300000 ||
        !LaunchProfileRegistry::prepare(plan.m_profile, probe).plan) {
        state(out, LaunchState::Blocked); out.diagnostic = "Preflight changed or readiness timeout is invalid."; return out;
    }
    try { out.snapshot = executor.snapshot(); } catch (...) {}
    if (!out.snapshot || !snapshot_ok(*out.snapshot)) {
        out.snapshot.reset(); state(out, LaunchState::SnapshotFailed);
        out.diagnostic = "A safe snapshot could not be created; nothing was changed."; return out;
    }
    try {
        state(out, LaunchState::Applying);
        if (!executor.apply(plan)) { out.diagnostic = "Profile configuration failed."; rollback(out, executor); return out; }
        state(out, LaunchState::Starting);
        if (!executor.start(plan)) { out.diagnostic = "Process start failed."; rollback(out, executor); return out; }
        state(out, LaunchState::Waiting);
        const auto readiness = executor.ready(timeout_ms);
        if (readiness != ReadinessResult::Ready) {
            out.diagnostic = readiness == ReadinessResult::Exited ? "Process exited before readiness." :
                             readiness == ReadinessResult::TimedOut ? "Readiness verification timed out." :
                             "Process health verification failed.";
            rollback(out, executor); return out;
        }
        state(out, LaunchState::Ready); out.diagnostic = "Process readiness verified.";
    } catch (...) { out.diagnostic = "Launch interrupted; recovery was attempted."; rollback(out, executor); }
    return out;
}
LaunchOutcome LaunchTransaction::restore(const RedactedLaunchSnapshot& snapshot, LaunchExecutor& executor) {
    LaunchOutcome out;
    if (!snapshot_ok(snapshot)) { state(out, LaunchState::Blocked); out.diagnostic = "Snapshot validation failed."; return out; }
    // A manual restore is a mutation too: retain a safe undo point first.
    try { out.snapshot = executor.snapshot(); } catch (...) {}
    if (!out.snapshot || !snapshot_ok(*out.snapshot)) {
        out.snapshot.reset(); state(out, LaunchState::SnapshotFailed);
        out.diagnostic = "A safe snapshot could not be created; nothing was changed."; return out;
    }
    state(out, LaunchState::RollingBack);
    bool stopped = false, restored = false;
    try { stopped = executor.stop_owned(); } catch (...) {}
    if (stopped) try { restored = executor.restore(snapshot); } catch (...) {}
    if (stopped && !restored) {
        // Undo a partially applied restore, while keeping the requested operation failed.
        try { executor.restore(*out.snapshot); } catch (...) {}
    }
    state(out, stopped && restored ? LaunchState::Restored : LaunchState::RollbackFailed);
    out.diagnostic = restored ? "Snapshot restored." : "Recovery remains incomplete.";
    return out;
}
}} // namespace Slic3r::OllamaSuite
