#include "slic3r/GUI/FeatureServices/SchoolCredentials.hpp"
#include <iostream>

using namespace Slic3r;
using namespace Slic3r::GUI;
class MemoryVault final : public LocalSecurity::Vault {
public:
    std::optional<LocalSecurity::Secret> read(const std::string& key) override {
        if (key != account || data.empty()) return {};
        return LocalSecurity::Secret(data);
    }
    void write(const std::string& key, const LocalSecurity::Secret& value) override {
        account = key;
        data.assign(value.data(), value.data() + value.size());
    }
    void erase(const std::string& key) override { if (key == account) data.clear(); }
    std::string account;
    std::vector<unsigned char> data;
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
    FeatureServices::SchoolCredentials service(mode, std::make_unique<MemoryVault>());
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
    std::cout << "PASS " << checks << " shared credential integration assertions\n";
}
