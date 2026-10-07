#include "Tabbook.hpp"

//#ifdef _WIN32

#include "GUI_App.hpp"
#include "wxExtensions.hpp"
#include "TabButton.hpp"

//BBS set font size
#include "Widgets/Label.hpp"

#include <wx/button.h>
#include <wx/dcclient.h>
#include <wx/sizer.h>

wxDEFINE_EVENT(wxCUSTOMEVT_TABBOOK_SEL_CHANGED, wxCommandEvent);

static const wxFont& TAB_BUTTON_FONT     = Label::Body_14;
static const wxFont& TAB_BUTTON_FONT_SEL = Label::Head_14;


static const int BUTTON_DEF_WIDTH  = 220;


TabButtonsListCtrl::TabButtonsListCtrl(wxWindow *parent, wxBoxSizer *side_tools) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));

    int em = em_unit(this);
    m_btn_margin = FromDIP(MD3::Metrics::active().gap);
    m_line_margin = std::lround(0.1 * em);

    m_arrow_img = ScalableBitmap(this, "monitor_arrow", 14);

    m_sizer = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(m_sizer);
    if (side_tools != NULL) {
        for (size_t idx = 0; idx < side_tools->GetItemCount(); idx++) {
            wxSizerItem *item     = side_tools->GetItem(idx);
            wxWindow *   item_win = item->GetWindow();
            if (item_win) { item_win->Reparent(this); }
        }
        m_sizer->Add(side_tools, 0, wxEXPAND | wxLEFT | wxTOP, m_btn_margin);
    }

    m_buttons_sizer = new wxFlexGridSizer(1, m_btn_margin, m_btn_margin);
    m_buttons_sizer->AddGrowableCol(0);
    m_sizer->Add(m_buttons_sizer, 0, wxEXPAND | wxALL, m_btn_margin);
    m_sizer->AddStretchSpacer(1);
}

void TabButtonsListCtrl::OnPaint(wxPaintEvent &)
{
    wxPaintDC dc(this);
    dc.SetBackground(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerLow)));
    dc.Clear();
}

void TabButtonsListCtrl::StyleButton(TabButton* btn, bool selected)
{
    const auto& metrics = MD3::Metrics::active();
    btn->SetFont(selected ? TAB_BUTTON_FONT_SEL : TAB_BUTTON_FONT);
    if (!m_custom_padding)
        btn->SetPaddingSize({FromDIP(metrics.padding), FromDIP(metrics.padding)});
    const wxSize padding = btn->GetPaddingSize();
    wxClientDC dc(btn);
    dc.SetFont(TAB_BUTTON_FONT_SEL);
    const wxSize title = dc.GetTextExtent(btn->GetLabel());
    const int width = std::max(FromDIP(BUTTON_DEF_WIDTH), title.x + padding.x + padding.y + FromDIP(28));
    const int height = std::max(FromDIP(metrics.row_height), title.y + FromDIP(metrics.gap));
    btn->SetMinSize({width, height});
    btn->SetCornerRadius(FromDIP(metrics.small_radius));
    btn->SetBorderWidth(std::max(1, FromDIP(2)));
    btn->SetColorScheme(m_color_scheme);
    btn->SetBackgroundColor(StateColor(
        std::make_pair(MD3::Light::scHigh, (int) StateColor::Disabled),
        std::make_pair(MD3::Light::scHighest, (int) StateColor::Pressed),
        std::make_pair(selected ? StateColor::semantic(MD3::Role::PrimaryContainer, m_color_scheme)
                               : StateColor::semantic(MD3::Role::SurfaceContainerHigh), (int) StateColor::Hovered),
        std::make_pair(selected ? StateColor::semantic(MD3::Role::PrimaryContainer, m_color_scheme)
                               : StateColor::semantic(MD3::Role::SurfaceContainerLow), (int) StateColor::Normal)));
    btn->SetTextColor(StateColor(
        std::make_pair(MD3::Light::onSurfaceVariant, (int) StateColor::Disabled),
        std::make_pair(MD3::Light::onSurface, (int) StateColor::Pressed),
        std::make_pair(selected ? StateColor::semantic(MD3::Role::OnPrimaryContainer, m_color_scheme)
                               : StateColor::semantic(MD3::Role::OnSurfaceVariant), (int) StateColor::Normal)));
    // The child has a legacy square border in addition to StaticBox's rounded one.
    // Keep that stroke transparent so the caller's rounded surface remains visible.
    btn->SetBorderColor(StateColor(wxColour(0, 0, 0, 0)));
    btn->StaticBox::SetBorderColor(StateColor(
        std::make_pair(StateColor::semantic(MD3::Role::Primary, m_color_scheme), (int) StateColor::Focused),
        std::make_pair(wxColour(0, 0, 0, 0), (int) StateColor::Normal)));
}

void TabButtonsListCtrl::Rescale()
{
    m_arrow_img = ScalableBitmap(this, "monitor_arrow", 14);

    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    m_btn_margin = FromDIP(MD3::Metrics::active().gap);
    m_buttons_sizer->SetVGap(m_btn_margin);
    m_buttons_sizer->SetHGap(m_btn_margin);
    if (auto* item = m_sizer->GetItem(m_buttons_sizer)) item->SetBorder(m_btn_margin);
    for (int index = 0; index < int(m_pageButtons.size()); ++index) {
        auto* btn = m_pageButtons[index];
        btn->SetBitmap(m_arrow_img);
        StyleButton(btn, index == m_selection);
        btn->Rescale();
    }

    m_sizer->Layout();
}

void TabButtonsListCtrl::SetSelection(int sel)
{
    if (m_selection == sel)
        return;
    if (m_selection >= 0) {
        TabButton *old = m_pageButtons[m_selection];
        old->SetSelected(false);
        StyleButton(old, false);
    }
    m_selection = sel;
    TabButton *cur = m_pageButtons[m_selection];
    cur->SetColorScheme(m_color_scheme);
    cur->SetSelected(true);
    StyleButton(cur, true);
    m_sizer->Layout();
    Refresh();
}

void TabButtonsListCtrl::SetColorScheme(MD3::ColorScheme scheme)
{
    if (m_color_scheme == scheme)
        return;
    m_color_scheme = scheme;
    for (int idx = 0; idx < int(m_pageButtons.size()); ++idx)
        StyleButton(m_pageButtons[idx], idx == m_selection);
    Refresh();
}

void TabButtonsListCtrl::showNewTag(int sel, bool tag)
{
    if (m_pageButtons[sel]->GetShowNewTag() == tag)
    {
        return;
    }

    m_pageButtons[sel]->ShowNewTag(tag);
    Refresh();
}

bool TabButtonsListCtrl::InsertPage(size_t n, const wxString &text, bool bSelect /* = false*/, const std::string &bmp_name /* = ""*/)
{
    TabButton *btn = new TabButton(this, text, m_arrow_img, wxNO_BORDER);
    StyleButton(btn, false);
    btn->SetSelected(false);
    btn->Bind(wxEVT_BUTTON, [this, btn](wxCommandEvent& event) {
        if (auto it = std::find(m_pageButtons.begin(), m_pageButtons.end(), btn); it != m_pageButtons.end()) {
            auto sel = it - m_pageButtons.begin();
            SetSelection(sel);
            wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_TABBOOK_SEL_CHANGED);
            evt.SetId(sel);
            wxPostEvent(this->GetParent(), evt);
        }
    });
    Slic3r::GUI::wxGetApp().UpdateDarkUI(btn);
    m_pageButtons.insert(m_pageButtons.begin() + n, btn);
    m_buttons_sizer->Insert(n, new wxSizerItem(btn, 0, wxEXPAND, 0, nullptr));
    m_buttons_sizer->SetRows(m_pageButtons.size() + 1);
    m_sizer->Layout();
    return true;
}

void TabButtonsListCtrl::RemovePage(size_t n)
{
    if (n >= m_pageButtons.size()) return;
    TabButton *btn = m_pageButtons[n];
    m_pageButtons.erase(m_pageButtons.begin() + n);
    m_buttons_sizer->Remove(n);
    btn->Reparent(nullptr);
    btn->Destroy();
    m_sizer->Layout();
}

bool TabButtonsListCtrl::SetPageImage(size_t n, const std::string &bmp_name)
{
    if (n >= m_pageButtons.size())
        return false;

    ScalableBitmap bitmap;
    if (!bmp_name.empty())
        bitmap = ScalableBitmap(this, bmp_name, 14);
    m_pageButtons[n]->SetBitmap(bitmap);

    return true;
}

void TabButtonsListCtrl::SetPageText(size_t n, const wxString &strText)
{
    TabButton *btn = m_pageButtons[n];
    btn->SetLabel(strText);
    StyleButton(btn, int(n) == m_selection);
    m_sizer->Layout();
}

wxString TabButtonsListCtrl::GetPageText(size_t n) const
{
    TabButton *btn = m_pageButtons[n];
    return btn->GetLabel();
}

const wxSize& TabButtonsListCtrl::GetPaddingSize(size_t n) {
    return m_pageButtons[n]->GetPaddingSize();
}

void TabButtonsListCtrl::SetPaddingSize(const wxSize& size) {
    m_custom_padding = true;
    for (int index = 0; index < int(m_pageButtons.size()); ++index) {
        auto* btn = m_pageButtons[index];
        btn->SetPaddingSize(size);
        StyleButton(btn, index == m_selection);
    }
    m_sizer->Layout();
}

//#endif // _WIN32


