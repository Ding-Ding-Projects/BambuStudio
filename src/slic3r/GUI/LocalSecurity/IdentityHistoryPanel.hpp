#pragma once
#include "SecurityPanels.hpp"
#include "libslic3r/LocalSecurity/IdentityHistory.hpp"

namespace Slic3r::GUI::LocalSecurityUI {
struct RestoreIdentityRequest {
    std::string source_revision;
    std::string identity;
    LocalSecurity::HistoryAction action;
    LocalSecurity::Secret snapshot;
};
// The live-state owner validates the snapshot, records an append-only restore,
// applies it, and throws on any incomplete transaction. No no-op implementation.
using RestoreIdentity = std::function<void(RestoreIdentityRequest)>;
class IdentityHistoryPanel final:public wxPanel {
public:
    IdentityHistoryPanel(wxWindow*,std::shared_ptr<LocalSecurity::IdentityHistory>,Hooks,RestoreIdentity);
    ~IdentityHistoryPanel() override;
    void LockHistory();
private:
    void safely(const std::function<void()>&);
    void read_page();
    void filter();
    void require_session();
    std::shared_ptr<LocalSecurity::IdentityHistory> m_history;
    Hooks m_hooks;
    RestoreIdentity m_restore;
    std::optional<LocalSecurity::Secret> m_answer;
    LocalSecurity::Time m_expires{};
    unsigned m_offset=0;
    SearchField* m_search{};
    wxListBox* m_list{};
    wxTextCtrl* m_from{};
    wxTextCtrl* m_to{};
    Label* m_status{};
    std::vector<LocalSecurity::IdentityHistoryMetadata> m_rows;
    std::vector<std::size_t> m_visible;
    std::vector<bool> m_actions;
    wxTimer m_timer;
};
}
