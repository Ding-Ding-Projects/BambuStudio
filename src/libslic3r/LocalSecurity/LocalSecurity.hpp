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
// Attempt budget of the shared presentation-mode credential, shared like it.
inline constexpr const char* shared_mode_attempts_account = "org.dingding.shared.school.attempts.v1";

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
// Pairs one steady-clock instant with the wall clock, so a deadline can be kept
// in Unix milliseconds across restarts and processes while every caller keeps
// passing steady-clock times.
struct ClockAnchor {
    Time steady{};
    std::int64_t unix_ms = 0;
    static ClockAnchor now();
    std::int64_t unix_ms_at(Time t) const;
};
// `lockout` identifies the wait this budget started most recently; every new
// wait gets a new random identity.
struct AttemptState { unsigned remaining; unsigned wait_seconds; std::uint64_t lockout = 0; };
// The unlock ladder's rungs in their fixed order. Clock means only the wait is left.
enum class LadderRung : unsigned char { DimSum, Sums, Moles, Clock };
// Where the ladder stands inside the current lockout. Every new lockout, expiry,
// cleared wait and successful answer discards it.
struct LadderProgress { bool started = false; LadderRung rung = LadderRung::DimSum; unsigned wrong_dishes = 0; };

inline constexpr unsigned ladder_skips_per_hour = 3;
// One allowance for every lockout surface of every product sharing the
// operating-system vault: any surface's ladder win spends it.
inline constexpr const char* shared_ladder_account = "org.dingding.shared.unlock-ladder.v1";
// The ladder may clear at most three waits per rolling hour, whichever
// surface they belong to. Every call re-reads the persisted record, so any
// number of instances over the same vault and account share one allowance,
// across restarts and processes. A damaged record counts as spent.
class LadderAllowance {
public:
    LadderAllowance() = default; // this object only, for tests and transient surfaces
    LadderAllowance(Vault& vault, std::string account = shared_ladder_account, ClockAnchor anchor = ClockAnchor::now());
    unsigned remaining(Time now);
    bool consume(Time now);
private:
    std::vector<std::int64_t> load(std::int64_t now_ms);
    void save(const std::vector<std::int64_t>& skips);
    Vault* m_vault = nullptr;
    std::string m_account;
    ClockAnchor m_anchor{};
    std::vector<unsigned char> m_memory;
};

// Five attempts, then a wait of 30 seconds doubling to 900. A persisted budget
// re-reads its record on every call, so restarting the application, opening a
// second window or another process sharing the record never refunds attempts,
// shortens a wait or resets escalation. A wall clock set back cannot stretch a
// wait past 900 seconds, and a damaged record becomes the longest wait.
class AttemptBudget {
public:
    AttemptBudget() = default; // this object only, for tests and transient surfaces
    AttemptBudget(Vault& vault, std::string account, ClockAnchor anchor = ClockAnchor::now());
    AttemptState state(Time now);
    void failed(Time now);
    void succeeded();
    // The unlock ladder's only way to end a wait. Spends one of the shared
    // allowance's skips and restores exactly the attempts that ordinary expiry
    // restores; never authentication and never the escalation.
    bool clear_wait(LadderAllowance& allowance, Time now);
    // Ladder progress for the wait in progress; empty when nothing is waiting.
    LadderProgress ladder(Time now);
    // Ignored unless `lockout` is still the wait in progress.
    void set_ladder(std::uint64_t lockout, const LadderProgress& progress, Time now);
private:
    std::int64_t refresh(Time now);
    void load(std::int64_t now_ms);
    void reset() noexcept;
    void save();
    Vault* m_vault = nullptr;
    std::string m_account;
    ClockAnchor m_anchor{};
    std::vector<unsigned char> m_memory;
    unsigned m_remaining = 5;
    unsigned m_escalation = 0;
    bool m_waiting = false;
    std::int64_t m_wait_until = 0; // Unix milliseconds
    std::uint64_t m_lockout = 0;
    LadderProgress m_ladder;
};
class LockSession {
public:
    explicit LockSession(LockSettings settings);
    // A lock that survives restarts passes its persisted budget here.
    LockSession(LockSettings settings, AttemptBudget budget);
    bool locked(Time now);
    std::optional<Factor> expected(Time now);
    // The caller supplies a locally verified factor result, never a UI assertion.
    bool submit(Factor factor, bool verified, Time now);
    void cancel();
    void leave_surface();
    void relock();
    AttemptState attempts(Time now) { return m_budget.state(now); }
    bool clear_wait(LadderAllowance& allowance, Time now) { return m_budget.clear_wait(allowance, now); }
    // For the unlock ladder, which may clear the wait and never the lock.
    AttemptBudget& budget() noexcept { return m_budget; }
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
