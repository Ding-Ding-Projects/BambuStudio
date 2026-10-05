#pragma once
#include "LocalSecurity.hpp"
#include <filesystem>

namespace Slic3r::LocalSecurity {
enum class HistoryAction { Created, Renamed, CredentialChanged, Removed, Restored };
struct IdentityHistoryEntry {
    std::string revision;
    std::string identity;
    HistoryAction action;
    Secret snapshot;
};
// The caller supplies the application's stable, private data directory, never a
// project path. Do not expose this location through export or sync operations.
class IdentityHistory {
public:
    explicit IdentityHistory(std::filesystem::path application_data, Vault& vault);
    ~IdentityHistory();
    IdentityHistory(const IdentityHistory&) = delete;
    IdentityHistory& operator=(const IdentityHistory&) = delete;
    // Explicit first enrollment. Existing or partially initialized state is
    // never replaced, repaired, or silently re-keyed.
    void initialize(CredentialKind, const Secret& history_answer);
    // Trusted mutation sink. Seed-bearing snapshots stay in Secret memory until
    // encrypted here. Never include PINs, passwords, codes, or recovery answers.
    std::string append(HistoryAction, const std::string& identity, const Secret& snapshot);
    // Independent history credential required on every bounded read. Offset
    // has a hard ceiling; callers cannot select arbitrary object IDs or paths.
    std::vector<IdentityHistoryEntry> read(const Secret& history_answer,
                                         unsigned offset = 0, unsigned count = 10);
    void replace_credential(const Secret& old_answer, CredentialKind, const Secret& new_answer);
    static constexpr const char* credential_account = "org.dingding.bambu.identity-history.credential.v1";
    static constexpr const char* key_account = "org.dingding.bambu.identity-history.key.v1";
    static constexpr const char* anchor_account = "org.dingding.bambu.identity-history.anchor.v1";
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
