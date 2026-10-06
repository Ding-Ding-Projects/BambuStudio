#pragma once
#include <filesystem>
#include <memory>
class wxPanel;
class wxWindow;
namespace Slic3r::LocalSecurity {
class Vault;
class IdentityHistory;
class AuthenticatorStore;
class SupportTickets;
}
namespace Slic3r::GUI::FeatureServices {
// Application lifetime owns the vault before every service referring to it.
class LocalSecurityServices final {
public:
    explicit LocalSecurityServices(std::filesystem::path application_data);
    ~LocalSecurityServices();
    wxPanel* authenticator(wxWindow*);
    wxPanel* history(wxWindow*);
    wxPanel* support(wxWindow*);
private:
    std::filesystem::path m_data;
    std::unique_ptr<LocalSecurity::Vault> m_vault;
    std::shared_ptr<LocalSecurity::IdentityHistory> m_history;
    std::shared_ptr<LocalSecurity::AuthenticatorStore> m_authenticator;
    std::shared_ptr<LocalSecurity::SupportTickets> m_support;
};
}
