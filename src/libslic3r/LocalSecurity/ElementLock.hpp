#pragma once
#include "Authenticator.hpp"

namespace Slic3r::LocalSecurity {
struct LockEnrollment {
    LockSettings settings;
    Secret pin;
    Secret password;
    std::optional<Enrollment> otp;
    std::string confirmation_code;
};
// One object per stable element ID. All action routes must share this object;
// recreating it per click would reset attempt throttling.
class ElementLock {
public:
    ElementLock(Vault&, std::string stable_id, RecordIdentityMutation history);
    bool configured() const noexcept { return bool(m_session); }
    bool allows_action(Time now);
    void create(LockEnrollment, std::uint64_t unix_seconds);
    std::optional<Factor> expected(Time now);
    bool submit(const Secret& answer, Time now, std::uint64_t unix_seconds);
    AttemptState attempts(Time now);
    // The persisted budget for this element's unlock ladder; null until created.
    AttemptBudget* budget() noexcept { return m_session ? &m_session->budget() : nullptr; }
    void cancel();
    void leave_surface();
    void relock();
private:
    std::string account(const char*) const;
    Vault& m_vault;
    std::string m_id;
    RecordIdentityMutation m_history;
    std::unique_ptr<LockSession> m_session;
    std::optional<std::uint64_t> m_last_otp_step;
};
}
