#pragma once
#include "../PersonalModes/SchoolMode.hpp"
#include "libslic3r/LocalSecurity/LocalSecurity.hpp"

namespace Slic3r::GUI::FeatureServices {
// The attempt budget lives in the same vault as the shared credential, so it
// survives closing Preferences, restarting the application and every other
// process that shares the credential.
class SchoolCredentials {
public:
    explicit SchoolCredentials(PersonalModes::SchoolMode& mode)
        : SchoolCredentials(mode, LocalSecurity::make_os_vault()) {}
    SchoolCredentials(PersonalModes::SchoolMode& mode, std::unique_ptr<LocalSecurity::Vault> vault)
        : m_mode(mode), m_vault(std::move(vault)), m_credentials(*m_vault),
          m_attempts(*m_vault, LocalSecurity::shared_mode_attempts_account) {}
    LocalSecurity::CredentialMetadata metadata() { return m_credentials.metadata(LocalSecurity::shared_mode_account); }
    LocalSecurity::AttemptState attempts() { return m_attempts.state(std::chrono::steady_clock::now()); }
    // The persisted budget, for the unlock ladder.
    LocalSecurity::AttemptBudget& budget() noexcept { return m_attempts; }
    PersonalModes::RecordStatus enroll(LocalSecurity::CredentialKind kind, const LocalSecurity::Secret& answer) {
        const auto result = m_credentials.enroll(LocalSecurity::shared_mode_account, kind, answer);
        return m_mode.credential_changed(result.generation);
    }
    PersonalModes::RecordStatus enable() {
        const auto result = metadata();
        return result.configured ? m_mode.enable(result.generation) : PersonalModes::RecordStatus::Unavailable;
    }
    PersonalModes::RecordStatus disable(const LocalSecurity::Secret& answer) {
        check_budget();
        bool verified = false;
        const auto result = m_mode.disable([&](const std::string& generation) {
            const auto current = metadata();
            verified = current.configured && current.generation == generation &&
                m_credentials.verify(LocalSecurity::shared_mode_account, answer);
            return verified;
        });
        record_attempt(verified);
        return result;
    }
    PersonalModes::RecordStatus replace(const LocalSecurity::Secret& old_answer,
        LocalSecurity::CredentialKind kind, const LocalSecurity::Secret& answer) {
        check_budget();
        try {
            const auto result = m_credentials.replace(LocalSecurity::shared_mode_account, old_answer, kind, answer);
            record_attempt(true);
            return m_mode.credential_changed(result.generation);
        } catch (const LocalSecurity::Failure& failure) {
            if (failure.code() == LocalSecurity::Error::Authentication) record_attempt(false);
            throw;
        }
    }
private:
    void check_budget() {
        if (attempts().wait_seconds) throw LocalSecurity::Failure(LocalSecurity::Error::RateLimited);
    }
    void record_attempt(bool verified) {
        if (verified) m_attempts.succeeded();
        else m_attempts.failed(std::chrono::steady_clock::now());
    }
    PersonalModes::SchoolMode& m_mode;
    std::unique_ptr<LocalSecurity::Vault> m_vault;
    LocalSecurity::Credentials m_credentials;
    LocalSecurity::AttemptBudget m_attempts;
};
}
