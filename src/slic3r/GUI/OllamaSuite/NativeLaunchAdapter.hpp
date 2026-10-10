#pragma once
#include "libslic3r/OllamaSuite/LaunchProfiles.hpp"
#include <functional>
#include <memory>

namespace Slic3r { namespace OllamaSuite {
struct NativeLaunchPolicy {
    std::function<bool(const std::string&)> installed_model;
    std::function<bool(const LaunchProfile&)> hardware_fit;
    // Populate only from a reviewed application manifest, never from editable UI input.
    // Signed executables require matching original-filename metadata and the
    // explicit picker confirmation. Missing metadata needs a reviewed digest.
    std::vector<std::string> reviewed_unsigned_sha256;
    // Asks the person to register the verified executable (full path, SHA-256) and
    // answers true only for an explicit Yes. The owning window asks through the
    // Material message dialog, so the question follows the theme and the language
    // modes; this adapter shows no window of its own. Without it, nothing registers.
    std::function<bool(const std::string& path, const std::string& sha256)> confirm_registration;
};
struct NativePickerResult {
    std::optional<PickerEvidence> evidence;
    std::string diagnostic;
    bool cancelled = false;
};
struct NativeDirectoryResult {
    std::optional<std::string> path;
    std::string diagnostic;
    bool cancelled = false;
};
// Own this object for the lifetime of the launched child. Destruction stops its
// job, including descendants. Native handles are never accepted from callers.
// Serialize access to this object. Identity hashing streams files in bounded
// chunks but can take time for large models; present progress while it runs.
class NativeLaunchAdapter final : public RuntimeProbe, public LaunchExecutor {
public:
    explicit NativeLaunchAdapter(NativeLaunchPolicy);
    ~NativeLaunchAdapter() override;
    NativeLaunchAdapter(const NativeLaunchAdapter&) = delete;
    NativeLaunchAdapter& operator=(const NativeLaunchAdapter&) = delete;
    bool available() const;
    std::string unavailable_reason() const;
    NativePickerResult pick_executable(LaunchKind, void* native_owner = nullptr);
    NativePickerResult pick_model_file(void* native_owner = nullptr);
    NativeDirectoryResult pick_working_directory(void* native_owner = nullptr);
    bool verified_file(const PickerEvidence&, bool executable) const override;
    bool directory_exists(const std::string&) const override;
    bool port_available(unsigned) const override;
    bool installed_model(const std::string&) const override;
    bool hardware_fit(const LaunchProfile&) const override;
    std::optional<RedactedLaunchSnapshot> snapshot() override;
    bool apply(const ValidatedLaunchPlan&) override;
    bool start(const ValidatedLaunchPlan&) override;
    ReadinessResult ready(unsigned timeout_ms) override;
    bool stop_owned() override;
    bool restore(const RedactedLaunchSnapshot&) override;
    bool recovery_pending() const;
    // Explicit recovery after a previous interrupted operation. The journal stores
    // typed, non-secret configuration only, never paths, receipts, or process ids.
    bool recover_interrupted();
    // The only method safe to call concurrently. Interrupts current validation
    // and readiness. A new explicit executable selection clears the stop latch;
    // an operation cannot silently resume between preflight and snapshot.
    void request_cancel() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
namespace NativeLaunchDetail {
// Portable helpers are exposed for exact, non-launching regression checks.
bool supported_kind(LaunchKind);
std::string profile_unavailable_reason(LaunchKind);
std::wstring quote_argument(const std::wstring&);
bool safe_snapshot(const RedactedLaunchSnapshot&);
std::string encode_snapshot(const RedactedLaunchSnapshot&);
std::optional<RedactedLaunchSnapshot> decode_snapshot(const std::string&);
}
}} // namespace Slic3r::OllamaSuite
