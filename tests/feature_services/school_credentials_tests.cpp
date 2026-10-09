#include "slic3r/GUI/FeatureServices/SchoolCredentials.hpp"
#include <iostream>
#include <map>
#include <memory>

using namespace Slic3r;
using namespace Slic3r::GUI;
// Records outlive one service object, the way the operating-system vault
// outlives one application run.
using Records = std::map<std::string, std::vector<unsigned char>>;
class MemoryVault final : public LocalSecurity::Vault {
public:
    explicit MemoryVault(std::shared_ptr<Records> records) : m_records(std::move(records)) {}
    std::optional<LocalSecurity::Secret> read(const std::string& key) override {
        const auto found = m_records->find(key);
        if (found == m_records->end()) return {};
        return LocalSecurity::Secret(found->second);
    }
    void write(const std::string& key, const LocalSecurity::Secret& value) override {
        (*m_records)[key].assign(value.data(), value.data() + value.size());
    }
    void erase(const std::string& key) override { m_records->erase(key); }
private:
    std::shared_ptr<Records> m_records;
};
int main()
{
    using namespace PersonalModes;
    SchoolRecord stored;
    RecordStatus stored_status = RecordStatus::Missing;
    SchoolMode mode({[&] { return SchoolRead{stored_status, stored}; },
        [&](const SchoolRecord& next, std::uint64_t revision, const std::function<bool(const std::string&)>& verify) {
            if (revision != stored.revision) return RecordStatus::Conflict;
            if (stored.enabled && !next.enabled && (!verify || !verify(stored.credential_generation))) return RecordStatus::Unavailable;
            stored = next; ++stored.revision; stored_status = RecordStatus::Ready; return RecordStatus::Ready;
        }, {}});
    const auto records = std::make_shared<Records>();
    FeatureServices::SchoolCredentials service(mode, std::make_unique<MemoryVault>(records));
    LocalSecurity::Secret answer("test-only-value"), wrong("incorrect-test-value");
    unsigned checks = 0;
    auto check = [&](bool value) { ++checks; if (!value) { std::cerr << "FAILED assertion " << checks << '\n'; std::exit(1); } };
    check(service.enable() == RecordStatus::Unavailable);
    check(service.enroll(LocalSecurity::CredentialKind::Password, answer) == RecordStatus::Ready);
    check(service.enable() == RecordStatus::Ready && mode.suppressed());
    check(service.disable(wrong) != RecordStatus::Ready && mode.suppressed());
    check(service.attempts().remaining == 4);
    check(service.disable(answer) == RecordStatus::Ready && !mode.suppressed());
    check(service.attempts().remaining == 5);
    check(service.enable() == RecordStatus::Ready);
    for (int i = 0; i < 5; ++i) service.disable(wrong);
    check(service.attempts().wait_seconds > 0);
    bool limited = false;
    try { service.disable(answer); } catch (const LocalSecurity::Failure& e) { limited = e.code() == LocalSecurity::Error::RateLimited; }
    check(limited && mode.suppressed());
    // Restarting the application must not refund the wait or the escalation.
    FeatureServices::SchoolCredentials restarted(mode, std::make_unique<MemoryVault>(records));
    check(restarted.attempts().wait_seconds > 0 && restarted.attempts().remaining == 0);
    limited = false;
    try { restarted.disable(answer); } catch (const LocalSecurity::Failure& e) { limited = e.code() == LocalSecurity::Error::RateLimited; }
    check(limited && mode.suppressed());
    check(records->count(LocalSecurity::shared_mode_attempts_account) == 1 && records->count(LocalSecurity::shared_mode_account) == 1);
    check(&restarted.budget() != &service.budget() && restarted.budget().state(std::chrono::steady_clock::now()).lockout == service.attempts().lockout);
    std::cout << "PASS " << checks << " shared credential integration assertions\n";
}
