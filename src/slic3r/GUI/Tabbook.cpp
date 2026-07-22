#include "Tabbook.hpp"

//#ifdef _WIN32

#include "GUI_App.hpp"
#include "wxExtensions.hpp"
#include "TabButton.hpp"

//BBS set font size
#include "Widgets/Label.hpp"

#include <wx/button.h>
#include <wx/sizer.h>

wxDEFINE_EVENT(wxCUSTOMEVT_TABBOOK_SEL_CHANGED, wxCommandEvent);

static const wxFont& TAB_BUTTON_FONT     = Label::Body_14;
static const wxFont& TAB_BUTTON_FONT_SEL = Label::Head_14;


static const int BUTTON_DEF_HEIGHT = 46;
static const int BUTTON_DEF_WIDTH  = 220;


TabButtonsListCtrl::TabButtonsListCtrl(wxWindow *parent, wxBoxSizer *side_tools) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));

    int em = em_unit(this);
    // BBS: no gap
    m_btn_margin = 0;
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
    m_sizer->Add(m_buttons_sizer, 0, wxLEFT | wxTOP, m_btn_margin);
    m_sizer->AddStretchSpacer(1);
}

void TabButtonsListCtrl::OnPaint(wxPaintEvent &)
{
    Slic3r::GUI::wxGetApp().UpdateDarkUI(this);
    wxPaintDC dc(this);
    // Kit selection model (navigation/TabBar): the tab surface is transparent and
    // the selected state is carried by the Primary label + the 3px rounded Primary
    // indicator that the selected TabButton paints on its inner edge. The legacy
    // filled SecondaryContainer pill and the neutral TextPrimary under-tab / bottom
    // marker are gone; this control just holds the SurfaceContainerLowest surround.
}

void TabButtonsListCtrl::Rescale()
{
    m_arrow_img = ScalableBitmap(this, "monitor_arrow", 14);

    int em = em_unit(this);
    for (TabButton *btn : m_pageButtons) {
        btn->SetMinSize({BUTTON_DEF_WIDTH * em / 10, BUTTON_DEF_HEIGHT * em / 10});
        btn->SetBitmap(m_arrow_img);
        btn->Rescale();
    }

    m_sizer->Layout();
}

void TabButtonsListCtrl::SetSelection(int sel)
{
    if (m_selection == sel)
        return;
    // Kit selection model: tabs stay transparent (SurfaceContainerLowest, the bar
    // surround) in both states; selection is carried by the Primary/600 label and
    // the TabButton's 3px Primary indicator. Inactive labels drop to
    // OnSurfaceVariant/400.
    if (m_selection >= 0) {
        TabButton *old = m_pageButtons[m_selection];
        old->SetSelected(false);
        old->SetTextColor(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        old->SetFont(TAB_BUTTON_FONT);
    }
    m_selection = sel;
    TabButton *cur = m_pageButtons[m_selection];
    cur->SetColorScheme(m_color_scheme);
    cur->SetSelected(true);
    cur->SetTextColor(StateColor::semantic(MD3::Role::Primary, m_color_scheme));
    cur->SetFont(TAB_BUTTON_FONT_SEL);
    Refresh();
}

void TabButtonsListCtrl::SetColorScheme(MD3::ColorScheme scheme)
{
    if (m_color_scheme == scheme)
        return;
    m_color_scheme = scheme;
    for (int idx = 0; idx < int(m_pageButtons.size()); ++idx) {
        TabButton *btn = m_pageButtons[idx];
        btn->SetColorScheme(scheme);
        if (idx == m_selection)
            btn->SetTextColor(StateColor::semantic(MD3::Role::Primary, scheme));
    }
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
    btn->SetCornerRadius(0);

    int em = em_unit(this);
    btn->SetMinSize({BUTTON_DEF_WIDTH * em / 10, BUTTON_DEF_HEIGHT * em / 10});

    // Transparent tab (bar surround) + inactive OnSurfaceVariant label per the
    // kit selection model; the scheme feeds the active indicator once selected.
    btn->SetBackgroundColor(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    btn->SetTextColor(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    btn->SetColorScheme(m_color_scheme);
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
    m_buttons_sizer->Insert(n, new wxSizerItem(btn));
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
    for (auto& btn : m_pageButtons) {
        btn->SetPaddingSize(size);
    }
}

//#endif // _WIN32


