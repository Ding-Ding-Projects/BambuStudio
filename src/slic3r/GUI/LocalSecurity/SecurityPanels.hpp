#pragma once
#include "libslic3r/LocalSecurity/Authenticator.hpp"
#include "libslic3r/LocalSecurity/ElementLock.hpp"
#include "libslic3r/LocalSecurity/SupportTickets.hpp"
#include <wx/panel.h>
#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/weakref.h>
#include <functional>
#include "../Widgets/SuperConfirmState.hpp"

class SearchField;
class Label;
class wxListBox;
class wxTextCtrl;

namespace Slic3r::GUI::LocalSecurityUI {
struct Hooks {
    // Resolve text using the current language, personal wording and tone.
    std::function<wxString(const char*)> text;
    // Localized factual copy with no tone rewriting, for the support disclosure.
    std::function<wxString(const char*)> factual_text;
    std::function<void(const wxString&)> notify;
    std::function<void(const wxString&, const std::string&)> export_text;
    std::function<void()> open_support;
    // Register every control with the shared appearance/context-menu registry.
    std::function<void(wxWindow*, const std::string&)> register_surface;
    // Exclude secret/code controls from history, logs, diagnostics and capture.
    std::function<void(wxWindow*)> register_sensitive;
    std::function<void(wxWindow*, const char*)> record_label;
    // Fixed safety disclosures use a language-only renderer.
    std::function<void(wxWindow*, const char*)> record_factual_label;
    std::function<void(wxWindow*, const char*)> record_tooltip;
    std::function<void(wxWindow*, const char*)> record_name;
    // Forward the actual native two-key/full-slider state, never synthesized
    // flags. The callback runs on the GUI thread only after authorization.
    std::function<void(wxWindow*, const wxString&, const std::vector<wxString>&,
                       std::function<void(SuperConfirm::State)>)> confirm_retention;
    // Populate only when a bundled, isolated decoder route is verified. Results
    // remain in memory; the callbacks must not log paths, pixels or payloads.
    std::function<std::optional<LocalSecurity::Secret>(const std::filesystem::path&)> decode_qr_file;
    std::function<std::optional<LocalSecurity::Secret>()> decode_qr_clipboard;
    std::function<void(std::function<void(std::optional<LocalSecurity::Secret>)>)> scan_qr_camera;
};

class AuthenticatorPanel final : public wxPanel {
public:
    AuthenticatorPanel(wxWindow*, std::shared_ptr<LocalSecurity::AuthenticatorStore>, Hooks);
    ~AuthenticatorPanel() override;
    void RefreshEntries();
private:
    void refresh_codes();
    void filter();
    void safely(const std::function<void()>&);
    std::string selected() const;
    std::shared_ptr<LocalSecurity::AuthenticatorStore> m_store;
    Hooks m_hooks;
    SearchField* m_search{};
    wxListBox* m_list{};
    Label* m_status{};
    Label* m_code{};
    Label* m_countdown{};
    wxTextCtrl* m_uri{};
    wxTextCtrl* m_confirmation{};
    std::vector<LocalSecurity::AuthenticatorEntry> m_entries;
    std::vector<std::string> m_visible;
    wxTimer m_timer;
};

// Non-modal, anchored to the exact target. It never executes the protected
// action. Successful authentication asks the user to activate the target again.
class LockWizard final : public wxDialog {
public:
    static void Open(wxWindow* anchor, std::shared_ptr<LocalSecurity::ElementLock>,
                     wxString target_name, wxString recovery_folder, Hooks);
private:
    LockWizard(wxWindow*,std::shared_ptr<LocalSecurity::ElementLock>,wxString,wxString,Hooks);
    void refresh_prompt();
    void safely(const std::function<void()>&);
    void close();
    std::shared_ptr<LocalSecurity::ElementLock> m_lock;
    Hooks m_hooks;
    wxWeakRef<wxWindow> m_anchor;
    wxTextCtrl* m_answer{};
    Label* m_status{};
};

class SupportTicketsPanel final : public wxPanel {
public:
    SupportTicketsPanel(wxWindow*,std::shared_ptr<LocalSecurity::SupportTickets>,
                        std::filesystem::path application_data,Hooks);
private:
    void refresh();
    void safely(const std::function<void()>&);
    std::shared_ptr<LocalSecurity::SupportTickets> m_store;
    std::filesystem::path m_folder;
    Hooks m_hooks;
    SearchField* m_search{};
    wxListBox* m_list{};
    Label* m_status{};
    std::vector<std::string> m_visible;
};
}
