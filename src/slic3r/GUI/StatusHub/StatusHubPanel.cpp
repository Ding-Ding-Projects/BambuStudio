#include "StatusHubPanel.hpp"
#include "libslic3r/StatusHub/StatusHubService.hpp"
#include "../I18N.hpp"
#include "../GUI_App.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/CollapsibleFilterBar.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include <wx/sizer.h>
#include <wx/datetime.h>

namespace Slic3r::GUI {
namespace {
wxString state_text(HubState state)
{
    switch (state) {
    case HubState::stopped: return _L("Stopped");
    case HubState::not_configured: return _L("Not configured. No data has been sent.");
    case HubState::enrollment_required: return _L("Enrollment required. No data has been sent.");
    case HubState::invalid_configuration: return _L("Invalid endpoint. Use an HTTPS origin without credentials or a path.");
    case HubState::pending: return _L("Waiting for a delivery result.");
    case HubState::delivered: return _L("The server accepted the latest status update.");
    case HubState::delivery_failed: return _L("Delivery failed. Previous acceptance is shown separately.");
    }
    return _L("Unknown");
}
wxString date_text(std::chrono::system_clock::time_point value)
{
    if (value.time_since_epoch().count() == 0) return _L("Never");
    return wxDateTime(std::chrono::system_clock::to_time_t(value)).FormatISOCombined(' ');
}
wxString number(std::uint64_t value) { return wxString::Format("%llu", static_cast<unsigned long long>(value)); }
}

StatusHubPanel::StatusHubPanel(wxWindow* parent) : wxPanel(parent), m_timer(this)
{
    SetName(_L("Status Hub"));
    auto* outer = new wxBoxSizer(wxVERTICAL);
    // The search sits in the shared collapsible filter bar; collapsed, it
    // still says when a query hides evidence rows.
    // TRN: Header of the collapsible search of a list.
    m_filters = new CollapsibleFilterBar(this, "status_hub", _L("Search"));
    m_search = new SearchField(m_filters->GetBody(), _L("Search status evidence"));
    m_filters->GetBodySizer()->Add(m_search, 0, wxEXPAND);
    outer->Add(m_filters->GetSectionSizer(), 0, wxEXPAND | wxALL, FromDIP(12));
    m_retry = new Button(this, _L("Retry delivery"));
    m_retry->SetVariant(Button::Variant::Filled);
    m_retry->SetMinSize(wxSize(-1, FromDIP(44)));
    outer->Add(m_retry, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    m_body = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_body->SetScrollRate(0, FromDIP(12));
    auto* rows = new wxBoxSizer(wxVERTICAL);
    for (int i = 0; i < 10; ++i) {
        auto* text = new Label(m_body, wxEmptyString, wxST_NO_AUTORESIZE);
        rows->Add(text, 0, wxEXPAND | wxALL, FromDIP(8));
        m_rows.push_back({text, {}});
    }
    m_empty = new Label(m_body, _L("No matching evidence."));
    rows->Add(m_empty, 0, wxALL, FromDIP(8));
    m_body->SetSizer(rows);
    outer->Add(m_body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    SetSizer(outer);
    m_search->SetOnQuery([this](const wxString&) { filter(); });
    m_search->SetOnRegexToggle([this](bool) { filter(); });
    m_retry->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { StatusHubService::instance().retry(); refresh(); });
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { refresh(); });
    m_body->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) { filter(); event.Skip(); });
    m_timer.Start(1000);
    refresh();
}
StatusHubPanel::~StatusHubPanel() { m_timer.Stop(); }

void StatusHubPanel::refresh()
{
    const auto state = StatusHubService::instance().snapshot();
    const bool dark = wxGetApp().dark_mode();
    SetBackgroundColour(MD3::resolve(MD3::Role::Surface, dark));
    m_body->SetBackgroundColour(MD3::resolve(MD3::Role::Surface, dark));
    const std::vector<wxString> values {
        _L("Connection") + ": " + state_text(state.state),
        _L("Endpoint") + ": " + (state.endpoint_present ? _L("Configured privately") : _L("Not configured")),
        _L("Enrollment") + ": " + (state.enrollment_present ? _L("Present") : _L("Required")),
        _L("Last checked") + ": " + date_text(state.updated_at),
        _L("Last accepted") + ": " + date_text(state.last_accepted_at),
        _L("Attempts") + ": " + number(state.attempts) + "; " + _L("Accepted") + ": " + number(state.accepted),
        _L("Result") + ": " + wxString::FromUTF8(state.result_code.c_str()) + "; HTTP: " + wxString::Format("%d", state.http_status),
        _L("Inbox receipts") + ": " + number(state.replies_received) + ". " + _L("Receipt counts do not apply replies to this application."),
        state.inventory_applicable ? _L("Development inventory is measured before delivery.") :
            _L("Repository inventory is not applicable to an installed application. Open model paths are not transmitted."),
        _L("Configure STATUS_HUB_URL and AGENT_INGEST_TOKEN in the trusted launch environment, then restart the application. Retry uses the current session. Endpoint values, credentials and response bodies are never displayed or exported.")
    };
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        m_rows[i].full_text = values[i];
        m_rows[i].text->SetForegroundColour(MD3::resolve(MD3::Role::OnSurface, dark));
    }
    m_retry->SetLabel(state.busy ? _L("Delivery in progress") : _L("Retry delivery"));
    m_retry->Enable(state.retry_available && !state.busy && state.endpoint_present && state.enrollment_present &&
                    state.state != HubState::invalid_configuration && state.state != HubState::stopped);
    filter();
}
void StatusHubPanel::filter()
{
    SearchField::MatchPass matcher(m_search->GetValue(), m_search->IsRegexEnabled(),
        m_search->IsCaseSensitive(), m_search->IsWholeWord(), m_search->IsMultiline());
    std::vector<wxString> active;
    const wxString search = CollapsibleFilterBar::SearchFilterLabel(m_search->GetValue());
    if (!search.IsEmpty())
        active.push_back(search);
    m_filters->SetActiveFilters(active);
    bool visible = false;
    const int width = std::max(1, m_body->GetClientSize().x - FromDIP(32));
    for (auto& row : m_rows) {
        const bool show = matcher.matches(row.full_text);
        row.text->Show(show);
        row.text->SetLabel(row.full_text);
        row.text->Wrap(width);
        row.text->SetName(row.full_text);
        visible = visible || show;
    }
    m_empty->Show(!visible);
    m_body->Layout();
    m_body->FitInside();
}
}
