#include "LocalSecurityServices.hpp"
#include "SurfaceRegistry.hpp"
#include "../LocalSecurity/SecurityPanels.hpp"
#include "../LocalSecurity/IdentityHistoryPanel.hpp"
#include "../GUI_App.hpp"
#include "../MainFrame.hpp"
#include "../I18N.hpp"
#include "../Plater.hpp"
#include "../NotificationManager.hpp"
#include "../Export/ExportDialog.hpp"
#include "libslic3r/Utils.hpp"
#include <wx/weakref.h>

namespace Slic3r::GUI::FeatureServices {
namespace {
void notify(const wxString& message)
{
    if (auto* plater = wxGetApp().plater())
        plater->get_notification_manager()->push_notification(into_u8(message));
}
LocalSecurityUI::Hooks hooks(wxWindow* parent)
{
    LocalSecurityUI::Hooks result;
    result.text = [](const char* source) { return _L(source); };
    result.factual_text = [](const char* source) { return I18N::language_mode_service().factual(wxString::FromUTF8(source)).primary; };
    result.notify = notify;
    result.register_surface = [](wxWindow* window, const std::string& id) {
        if (!SurfaceRegistry::instance().register_surface(window, id))
            throw LocalSecurity::Failure(LocalSecurity::Error::Unavailable);
    };
    result.register_sensitive = [](wxWindow* window) { SurfaceRegistry::instance().register_sensitive(window); };
    result.record_label = [](wxWindow* window, const char* source) { SurfaceRegistry::instance().record_label(window, wxString::FromUTF8(source)); };
    result.record_factual_label = [](wxWindow* window, const char* source) {
        SurfaceRegistry::instance().record_label_renderer(window, wxString::FromUTF8(source), [](const wxString& original) {
            return I18N::language_mode_service().factual(original).primary;
        });
    };
    result.record_tooltip = [](wxWindow* window, const char* source) { SurfaceRegistry::instance().record_tooltip(window, wxString::FromUTF8(source)); };
    result.record_name = [](wxWindow* window, const char* source) { SurfaceRegistry::instance().record_name(window, wxString::FromUTF8(source)); };
    wxWeakRef<wxWindow> weak(parent);
    result.export_text = [weak](const wxString& title, const std::string& text) {
        if (!weak) return;
        // This is the service's explicit redacted dataset, never a window dump.
        Export::Dataset dataset;
        dataset.name = into_u8(title);
        dataset.schema_id = "bambustudio.local-security.redacted";
        dataset.kind = Export::DatasetKind::Prose;
        dataset.prose_title = into_u8(title);
        dataset.prose = text;
        dataset.file_stem = "local-security-redacted";
        ExportDialog::run(weak.get(), std::move(dataset));
    };
    result.open_support = [] {
        auto* frame = wxGetApp().mainframe;
        if (!frame) return;
        try {
            auto& services = wxGetApp().local_security();
            frame->open_service("local-support", _L("Local support tickets"), [&services](wxWindow* window) { return services.support(window); });
        } catch (...) { notify(_L("Local security is unavailable. No protected action was performed.")); }
    };
    return result;
}
LocalSecurity::HistoryAction action(LocalSecurity::IdentityAction value)
{
    using namespace LocalSecurity;
    switch (value) {
    case IdentityAction::AuthenticatorAdded: case IdentityAction::LockCreated: return HistoryAction::Created;
    case IdentityAction::AuthenticatorChanged: case IdentityAction::AuthenticatorReordered:
    case IdentityAction::DisplayNameChanged: return HistoryAction::Renamed;
    case IdentityAction::AuthenticatorRemoved: return HistoryAction::Removed;
    case IdentityAction::AuthenticatorRestored: return HistoryAction::Restored;
    }
    throw LocalSecurity::Failure(LocalSecurity::Error::InvalidInput);
}
}
LocalSecurityServices::LocalSecurityServices(std::filesystem::path application_data)
    : m_data(std::move(application_data)), m_vault(LocalSecurity::make_application_vault(m_data)),
      m_history(std::make_shared<LocalSecurity::IdentityHistory>(m_data, *m_vault)),
      m_authenticator(std::make_shared<LocalSecurity::AuthenticatorStore>(*m_vault,
          [history = m_history](LocalSecurity::IdentityAction value, const std::string& id, const LocalSecurity::Secret& snapshot) {
              history->append(action(value), id, snapshot);
          })),
      m_support(std::make_shared<LocalSecurity::SupportTickets>(*m_vault, m_data))
{}
LocalSecurityServices::~LocalSecurityServices() = default;
wxPanel* LocalSecurityServices::authenticator(wxWindow* parent)
{
    return new LocalSecurityUI::AuthenticatorPanel(parent, m_authenticator, hooks(parent));
}
wxPanel* LocalSecurityServices::history(wxWindow* parent)
{
    return new LocalSecurityUI::IdentityHistoryPanel(parent, m_history, hooks(parent),
        [store = m_authenticator](LocalSecurityUI::RestoreIdentityRequest request) {
            // restore_entry validates the full authenticator schema and records
            // a new encrypted Restored event before applying live state.
            store->restore_entry(request.identity, request.snapshot);
        });
}
wxPanel* LocalSecurityServices::support(wxWindow* parent)
{
    return new LocalSecurityUI::SupportTicketsPanel(parent, m_support, m_data, hooks(parent));
}
}
