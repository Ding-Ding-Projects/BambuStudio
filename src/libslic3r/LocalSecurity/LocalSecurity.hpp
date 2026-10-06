#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace Slic3r::LocalSecurity {

enum class Error { InvalidInput, Unavailable, Missing, Corrupt, Authentication, RateLimited, History };
class Failure : public std::runtime_error {
public:
    explicit Failure(Error error);
    Error code() const noexcept { return m_code; }
private:
    Error m_code;
};

// Move-only buffer; no streaming operator, implicit string conversion or logging.
class Secret {
public:
    Secret() = default;
    explicit Secret(std::string_view value);
    explicit Secret(std::vector<unsigned char> value);
    ~Secret();
    Secret(Secret&&) noexcept;
    Secret& operator=(Secret&&) noexcept;
    Secret(const Secret&) = delete;
    Secret& operator=(const Secret&) = delete;
    const unsigned char* data() const noexcept { return m_bytes.data(); }
    unsigned char* data() noexcept { return m_bytes.data(); }
    std::size_t size() const noexcept { return m_bytes.size(); }
private:
    std::vector<unsigned char> m_bytes;
};

class Vault {
public:
    virtual ~Vault() = default;
    virtual std::optional<Secret> read(const std::string& stable_account) = 0;
    virtual void write(const std::string& stable_account, const Secret&) = 0;
    virtual void erase(const std::string& stable_account) = 0;
};
// Uses Windows Credential Manager, CRED_PERSIST_LOCAL_MACHINE. Other platforms
// fail closed until a native backend is supplied. Never uses a plaintext fallback.
std::unique_ptr<Vault> make_os_vault();
// Product-local services use this factory. Its non-secret instance marker lives
// inside application data. Deleting that folder starts a fresh vault namespace,
// leaving inaccessible old credential records for explicit OS-level management.
// Shared presentation credentials instead use make_os_vault().
std::unique_ptr<Vault> make_application_vault(const std::filesystem::path& application_data);
bool valid_stable_id(std::string_view id) noexcept;
std::string new_stable_id();

enum class CredentialKind { Pin, Password };
struct CredentialMetadata { bool configured = false; std::string generation; };
class Credentials {
public:
    explicit Credentials(Vault& vault) : m_vault(vault) {}
    CredentialMetadata metadata(const std::string& account);
    // Initial enrollment only. Replacement/reset require the existing answer.
    CredentialMetadata enroll(const std::string& account, CredentialKind kind, const Secret& answer);
    bool verify(const std::string& account, const Secret& answer);
    CredentialMetadata replace(const std::string& account, const Secret& old_answer,
                               CredentialKind kind, const Secret& new_answer);
    void reset(const std::string& account, const Secret& answer);
private:
    CredentialMetadata store(const std::string&, CredentialKind, const Secret&);
    Vault& m_vault;
};
inline constexpr const char* shared_mode_account = "org.dingding.shared.school.v1";

enum class Algorithm { Sha1, Sha256, Sha512 };
struct TotpParameters { Algorithm algorithm = Algorithm::Sha1; unsigned digits = 6; unsigned period = 30; };
struct TotpCode { std::string current; std::string next; unsigned seconds_remaining = 0; };
Secret decode_base32(std::string_view encoded);
std::string encode_base32(const Secret& secret);
std::string hotp(const Secret&, std::uint64_t counter, Algorithm, unsigned digits);
TotpCode totp(const Secret&, const TotpParameters&, std::uint64_t unix_seconds);
// Returns the matching step for replay protection, never the secret or input.
std::optional<std::uint64_t> verify_totp(const Secret&, const TotpParameters&,
                                     std::string_view code, std::uint64_t unix_seconds,
                                     unsigned skew_steps = 1);
struct Enrollment {
    std::string issuer;
    std::string account;
    Secret secret;
    TotpParameters parameters;
};
Enrollment parse_otpauth(std::string_view uri);
std::string pairing_uri(const Enrollment&); // One-time registration only.

enum class Factor { Pin, Password, Totp };
enum class Policy { Pin, Password, PinPassword, PasswordTotp, PinTotp, PasswordPinTotp };
std::vector<Factor> factors(Policy);
enum class Duration { ThisSurface, Minutes, UntilExit };
struct LockSettings { Policy policy = Policy::Pin; Duration duration = Duration::ThisSurface; unsigned minutes = 5; };
using Time = std::chrono::steady_clock::time_point;
struct AttemptState { unsigned remaining; unsigned wait_seconds; };
class AttemptBudget {
public:
    AttemptState state(Time now);
    void failed(Time now);
    void succeeded();
    // A ladder can clear at most three waits per rolling hour. It restores
    // exactly the attempts that ordinary expiry restores; never authentication.
    bool clear_wait(Time now);
private:
    unsigned m_remaining = 5;
    unsigned m_escalation = 0;
    Time m_wait_until{};
    std::vector<Time> m_skips;
};
class LockSession {
public:
    explicit LockSession(LockSettings settings);
    bool locked(Time now);
    std::optional<Factor> expected(Time now);
    // The caller supplies a locally verified factor result, never a UI assertion.
    bool submit(Factor factor, bool verified, Time now);
    void cancel();
    void leave_surface();
    void relock();
    AttemptState attempts(Time now) { return m_budget.state(now); }
    bool clear_wait(Time now) { return m_budget.clear_wait(now); }
private:
    LockSettings m_settings;
    std::vector<Factor> m_factors;
    std::size_t m_step = 0;
    bool m_unlocked = false;
    Time m_until{};
    Time m_attempt_until{};
    AttemptBudget m_budget;
};

// AEAD snapshots contain no plaintext key. The caller stores this key only in
// the native vault; authenticated headers bind an encrypted record to its ID.
Secret random_secret(std::size_t bytes);
std::vector<unsigned char> encrypt_snapshot(const Secret& key, const Secret& plaintext, std::string_view record_id);
Secret decrypt_snapshot(const Secret& key, const std::vector<unsigned char>& encrypted, std::string_view record_id);

} // namespace Slic3r::LocalSecurity
