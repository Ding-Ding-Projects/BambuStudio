#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Slic3r { namespace OllamaSuite {
enum class LaunchKind { OllamaChat, LlamaServer };
struct PickerEvidence {
    std::string absolute_path;
    std::string content_identity; // Exact digest supplied by the privileged file verifier.
    std::string picker_receipt;   // Opaque, local-only receipt; never included in previews.
};
struct LaunchProfile {
    std::string id, name;
    LaunchKind kind = LaunchKind::OllamaChat;
    std::optional<PickerEvidence> executable;
    std::optional<PickerEvidence> model_file;
    std::string working_directory;
    std::string model_tag;
    unsigned port = 8080;
    unsigned context_length = 4096;
};
// Implementations must resolve symlinks, check file identity and executable provenance,
// validate picker receipts, and reject scripts/interpreters. No PATH discovery is allowed.
class RuntimeProbe {
public:
    virtual ~RuntimeProbe() = default;
    virtual bool verified_file(const PickerEvidence&, bool executable) const = 0;
    virtual bool directory_exists(const std::string&) const = 0;
    virtual bool port_available(unsigned) const = 0;
    virtual bool installed_model(const std::string&) const = 0;
    virtual bool hardware_fit(const LaunchProfile&) const = 0;
};
struct LaunchPreview {
    std::string profile_id, executable, working_directory, model;
    std::vector<std::string> arguments, required_files;
    std::vector<unsigned> required_ports;
    std::map<std::string, std::string> environment; // Only explicitly approved non-secret keys.
};
class ValidatedLaunchPlan {
public:
    const LaunchPreview& preview() const { return m_preview; }
    const LaunchProfile& profile() const { return m_profile; }
private:
    LaunchProfile m_profile;
    LaunchPreview m_preview;
    ValidatedLaunchPlan(LaunchProfile, LaunchPreview);
    friend class LaunchProfileRegistry;
    friend class LaunchTransaction;
};
struct LaunchPreparation {
    std::optional<ValidatedLaunchPlan> plan;
    std::vector<std::string> blockers;
};
class LaunchProfileRegistry {
public:
    static std::vector<LaunchProfile> prebuilt();
    static LaunchPreparation prepare(const LaunchProfile&, const RuntimeProbe&);
};
// Explicit safe fields only: no arbitrary configuration map, environment values,
// credentials, picker receipts, prompts, or chat messages can enter a snapshot.
struct RedactedLaunchSnapshot {
    std::string profile_id;
    std::string model_tag;
    unsigned context_length = 0;
    unsigned port = 0;
    bool enabled = false;
};
enum class LaunchState { Blocked, SnapshotFailed, Applying, Starting, Waiting,
                         Ready, RollingBack, Restored, RollbackFailed };
enum class ReadinessResult { Ready, Exited, TimedOut, Unhealthy };
struct LaunchOutcome {
    LaunchState state = LaunchState::Blocked;
    std::vector<LaunchState> transitions;
    std::optional<RedactedLaunchSnapshot> snapshot;
    std::string diagnostic; // Fixed messages only, never raw executor exception text.
};
class LaunchExecutor {
public:
    virtual ~LaunchExecutor() = default;
    // Read-only capture. Implementations may mutate only the fields represented
    // by this snapshot. Runtime secrets remain in the OS vault throughout.
    virtual std::optional<RedactedLaunchSnapshot> snapshot() = 0;
    virtual bool apply(const ValidatedLaunchPlan&) = 0;
    // Launch the exact executable using argv, never a shell. Recheck the verified
    // identity atomically at launch and use only the preview's environment, not
    // an inherited environment. Contain and own the child before returning, even
    // on failure; stop_owned must cover partial starts. Never alter external state.
    virtual bool start(const ValidatedLaunchPlan&) = 0;
    virtual ReadinessResult ready(unsigned timeout_ms) = 0;
    virtual bool stop_owned() = 0;
    virtual bool restore(const RedactedLaunchSnapshot&) = 0;
};
class LaunchTransaction {
public:
    static LaunchOutcome run(const ValidatedLaunchPlan&, const RuntimeProbe&,
                             LaunchExecutor&, unsigned timeout_ms = 30000);
    static LaunchOutcome restore(const RedactedLaunchSnapshot&, LaunchExecutor&);
};
}} // namespace Slic3r::OllamaSuite
