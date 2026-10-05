#pragma once
#include "../Widgets/TabStrip.hpp"
#include "../I18N.hpp"
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <map>

namespace Slic3r::GUI::FeatureServices {
class ServiceWorkspace final : public wxPanel {
public:
    explicit ServiceWorkspace(wxWindow* parent) : wxPanel(parent)
    {
        TabStrip::Options options;
        options.surface_key = "local_services";
        options.surface_name = _L("Local tools");
        options.strip_name = _L("Open tools");
        options.close_mode = TabStrip::CloseMode::Hide;
        m_strip = new TabStrip(this, options);
        m_book = new wxSimplebook(this);
        m_layout = new wxBoxSizer(wxHORIZONTAL);
        SetSizer(m_layout);
        m_strip->Bind(EVT_TABSTRIP_ACTIVATE, [this](wxCommandEvent& event) {
            const auto found = m_pages.find(event.GetString().ToStdString());
            if (found != m_pages.end()) m_book->SetSelection(found->second);
        });
        m_strip->Bind(EVT_TABSTRIP_DOCK_CHANGED, [this](wxCommandEvent&) { place_strip(); });
        place_strip();
    }
    wxPanel* open(const std::string& id, const wxString& title, const std::function<wxPanel*(wxWindow*)>& create)
    {
        auto found = m_pages.find(id);
        if (found == m_pages.end()) {
            auto* panel = create(m_book);
            if (!panel) return nullptr;
            const auto index = m_book->GetPageCount();
            if (!m_book->AddPage(panel, title)) { panel->Destroy(); return nullptr; }
            found = m_pages.emplace(id, index).first;
            m_strip->AddTab(id, title);
        }
        m_book->SetSelection(found->second);
        m_strip->Activate(id, false);
        auto* panel = static_cast<wxPanel*>(m_book->GetPage(found->second));
        panel->SetFocus();
        Layout();
        return panel;
    }
private:
    void place_strip()
    {
        m_layout->Detach(m_strip); m_layout->Detach(m_book);
        const auto edge = m_strip->GetDockEdge();
        m_layout->SetOrientation(MD3::Tabs::is_vertical(edge) ? wxHORIZONTAL : wxVERTICAL);
        const bool first = edge == MD3::Tabs::DockEdge::Left || edge == MD3::Tabs::DockEdge::Top;
        if (first) m_layout->Add(m_strip, 0, wxEXPAND);
        m_layout->Add(m_book, 1, wxEXPAND);
        if (!first) m_layout->Add(m_strip, 0, wxEXPAND);
        Layout();
    }
    TabStrip* m_strip{};
    wxSimplebook* m_book{};
    wxBoxSizer* m_layout{};
    std::map<std::string, std::size_t> m_pages;
};
}
