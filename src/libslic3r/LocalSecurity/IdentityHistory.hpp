#pragma once
#include "LocalSecurity.hpp"
#include <filesystem>

namespace Slic3r::GUI::SuperConfirm { struct State; }

namespace Slic3r::LocalSecurity {
enum class HistoryAction { Created, Renamed, CredentialChanged, Removed, Restored, Labelled, Pruned };
struct IdentityHistoryEntry {
    std::string revision;
    std::string identity;
    HistoryAction action;
    Secret snapshot;
    std::int64_t committed_at_utc_seconds = 0;
    int utc_offset_minutes = 0;
    unsigned format_version = 1;
    bool pruned = false;
};
struct IdentityHistoryMetadata {
    std::string revision;
    std::string identity;
    HistoryAction action;
    std::int64_t committed_at_utc_seconds = 0;
    int utc_offset_minutes = 0;
    unsigned format_version = 1;
    bool pruned = false;
};
struct IdentityHistoryRedactedDiff {
    bool same_identity = false;
    bool action_changed = false;
    // No snapshot contents, sizes, labels, or secret-derived hashes.
};
class HistoryPrunePreview {
public:
    const std::vector<IdentityHistoryMetadata>& rows() const noexcept { return m_rows; }
private:
    friend class IdentityHistory;
    HistoryPrunePreview() = default;
    std::vector<IdentityHistoryMetadata> m_rows;
    std::string m_head, m_owner;
};
struct IdentityHistoryCapacity {
    unsigned active_keys = 0;
    unsigned tombstones = 0;
    unsigned maximum_active_keys = 512;
    unsigned maximum_tombstones = 4096;
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
    // Authenticated reachable-only lookup; rejects pruned payloads and is bounded
    // to 10,000 ancestors without decrypting unrelated events.
    IdentityHistoryEntry read_revision(const Secret& history_answer, const std::string& revision);
    std::vector<IdentityHistoryMetadata> read_metadata(const Secret& history_answer,
                                                      unsigned offset = 0, unsigned count = 10);
    // Labels are UTF-8 text of at most 256 bytes, encrypted like snapshots.
    // They are events for the stable identity, not mutable commit messages.
    std::string append_label(const Secret& history_answer, const std::string& identity, const Secret& label);
    Secret read_label(const Secret& history_answer, const std::string& revision);
    // Exports metadata only. Payloads and labels are excluded by construction.
    std::string export_redacted(const Secret& history_answer, unsigned offset = 0, unsigned count = 10);
    static IdentityHistoryRedactedDiff redacted_diff(const IdentityHistoryMetadata&, const IdentityHistoryMetadata&);
    HistoryPrunePreview preview_prune(const Secret& history_answer, const std::vector<std::string>& revisions);
    // The native two-key/full-slider state is consumed once execution starts.
    // A stale preview, v1 selection, or unsuccessful validation consumes nothing.
    std::string prune(const Secret& history_answer, const HistoryPrunePreview&,
                      GUI::SuperConfirm::State& confirmation);
    IdentityHistoryCapacity capacity(const Secret& history_answer);
    void replace_credential(const Secret& old_answer, CredentialKind, const Secret& new_answer);
    static constexpr const char* credential_account = "org.dingding.bambu.identity-history.credential.v1";
    static constexpr const char* key_account = "org.dingding.bambu.identity-history.key.v1";
    static constexpr const char* key_store_anchor_account = "org.dingding.bambu.identity-history.key-store-anchor.v2";
    static constexpr const char* anchor_account = "org.dingding.bambu.identity-history.anchor.v1";
private:
    std::string append_impl(HistoryAction, const std::string&, const Secret&, const Secret* authorization);
    std::vector<IdentityHistoryEntry> read_impl(const Secret&, unsigned offset, unsigned count,
                                              bool decrypt_payload, const std::string& revision = {});
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
