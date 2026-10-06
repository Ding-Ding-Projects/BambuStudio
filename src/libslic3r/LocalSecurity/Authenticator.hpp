#pragma once
#include "LocalSecurity.hpp"
#include <functional>

namespace Slic3r::LocalSecurity {
enum class IdentityAction { AuthenticatorAdded, AuthenticatorChanged, AuthenticatorRemoved, AuthenticatorReordered, DisplayNameChanged, LockCreated, AuthenticatorRestored };
// The implementation must synchronously append an encrypted snapshot and throw
// on failure. A UI must never substitute a no-op history callback.
using RecordIdentityMutation = std::function<void(IdentityAction, const std::string&, const Secret&)>;
struct AuthenticatorEntry {
    std::string id;
    std::string issuer;
    std::string account;
    std::string group;
    TotpParameters parameters;
};
class AuthenticatorStore {
public:
    AuthenticatorStore(Vault& vault, RecordIdentityMutation record);
    std::vector<AuthenticatorEntry> list();
    std::string add(Enrollment enrollment, std::string_view confirmation_code, std::uint64_t seconds);
    void remove(const std::string& id);
    void rename(const std::string& id, std::string issuer, std::string account, std::string group);
    void move(const std::string& id, int direction);
    // Called only by the authenticated history dispatcher for an authenticator
    // identity. Validates a complete historical entry before recording/restoring.
    void restore_entry(const std::string& id, const Secret& snapshot);
    TotpCode code(const std::string& id, std::uint64_t seconds);
    // Redacted metadata only; no secret export route is exposed by this API.
    std::string export_redacted();
private:
    std::vector<std::string> ids();
    void store_ids(const std::vector<std::string>&);
    Secret read_record(const std::string& id);
    Vault& m_vault;
    RecordIdentityMutation m_record;
};
}
