#include "Notebook.hpp"

//#ifdef _WIN32

#include "GUI_App.hpp"
#include "wxExtensions.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/StateColor.hpp"

//BBS set font size
#include "Widgets/Label.hpp"

#include <algorithm>
#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>

namespace {
constexpr int btn_width_icon      = 40;
constexpr int btn_width_label_min = 48;
constexpr int btn_width_label_max = 136;
constexpr int selection_line_inset  = 12;
constexpr int tab_bottom_space      = 4;
// Kit per-tab horizontal padding (TabBar.jsx padding '0 16px'); the compact
// density padding (10) reads too tight against the kit's 16px cells.
constexpr int tab_horizontal_padding = 16;
// Active-indicator thickness comes from the shared chrome metric so the tab bar
// stays in lock-step with the kit (MD3::Metrics::tab_active_indicator == 3).
}; // namespace

wxDEFINE_EVENT(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED, wxCommandEvent);

ButtonsListCtrl::ButtonsListCtrl(wxWindow *parent, wxBoxSizer* side_tools) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    SetBackgroundStyle(wxBG_STYLE_PAINT);

    m_btn_margin = 0;
    m_line_margin = FromDIP(MD3::Metrics::tab_active_indicator);
    const int navigation_height = FromDIP(MD3::Metrics::navigation_bar_height);
    SetMinSize({-1, navigation_height});
    SetMaxSize({-1, navigation_height});

    m_sizer = new wxBoxSizer(wxHORIZONTAL);
    this->SetSizer(m_sizer);

    // a horizontal box sizer (instead of a flex grid) so the tab buttons can
    // shrink Chrome-style when the window is too narrow
    m_buttons_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_sizer->Add(m_buttons_sizer, 1, wxALIGN_TOP | wxLEFT, m_btn_margin);

    // Navigation actions stay fixed at the trailing edge instead of consuming
    // one of the equal-width workspace tab slots.
    m_actions_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_sizer->Add(m_actions_sizer, 0, wxALIGN_CENTER_VERTICAL);

    if (side_tools != NULL) {
        for (size_t idx = 0; idx < side_tools->GetItemCount(); idx++) {
            wxSizerItem* item = side_tools->GetItem(idx);
            wxWindow* item_win = item->GetWindow();
            if (item_win) {
                item_win->Reparent(this);
            }
        }
        m_sizer->Add(side_tools, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, m_btn_margin);
    }

    Bind(wxEVT_PAINT, &ButtonsListCtrl::OnPaint, this);
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
        ApplyTheme();
        event.Skip();
    });

    ApplyTheme();
}

void ButtonsListCtrl::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();
    // Kit tab-bar background is the base Surface role (not sc-lowest), so the
    // strip reads as the workspace shell rather than a raised card.
    const wxColour surface = StateColor::semantic(MD3::Role::Surface);

    dc.SetBackground(wxBrush(surface));
    dc.Clear();

    // Keep a quiet boundary between navigation and the active workspace.
    const int divider_width = std::max(1, FromDIP(1));
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), divider_width));
    dc.DrawLine(0, size.y - divider_width, size.x, size.y - divider_width);

    if (m_selection < 0 || m_selection >= int(m_pageButtons.size()))
        return;

    // Selection remains on the surface; only this underline carries emphasis.
    // The tab bar is chrome rendered ABOVE the per-workspace data-scheme scope,
    // so its accent is ALWAYS the brand/seed value (MD3 §4/§13) -- it never
    // adopts the Preview/Device scheme even while those workspaces are active.
    const wxRect button_rect = m_pageButtons[m_selection]->GetRect();
    const int inset = FromDIP(selection_line_inset);
    const int indicator_height = m_line_margin;
    // §4: a 3px underline with only the TOP corners rounded (radius 3px 3px 0 0),
    // anchored flush to the bar's bottom edge. wxDC has no per-corner radius, so
    // the shape is drawn at double height with a full radius and its lower
    // (rounded) half is clipped away below the client area, leaving crisp square
    // bottom corners flush to the bottom while the visible top corners stay round.
    wxRect indicator(button_rect.x + inset,
                     size.y - indicator_height,
                     std::max(1, button_rect.width - 2 * inset),
                     indicator_height * 2);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Brand)));
    dc.DrawRoundedRectangle(indicator, indicator_height);
}

void ButtonsListCtrl::StyleButton(Button* button, bool selected)
{
    // Tabs render transparent over the bar: the Normal fill matches the bar
    // Surface, hover lifts to SurfaceContainerLow, and there is no distinct
    // pressed fill (per kit). Selection is carried by the primary text colour
    // plus the active indicator, not a filled tab.
    //
    // This is the intended MD3 secondary-tab spec, not a behaviour-preserving
    // port, so two legacy interaction states are deliberately absent:
    //  - No Pressed fill entry. While a tab is pressed it is also hovered, and
    //    StateColor::colorForStates() matches the Hovered entry as a subset of
    //    Pressed|Hovered, so the SurfaceContainerLow hover fill persists through
    //    the press -- the omission is not a loss of feedback, only of a distinct
    //    stronger pressed tint.
    //  - Inactive tab labels stay OnSurfaceVariant across all states (no Hovered
    //    label-darken). Hover on an inactive tab is signalled by the background
    //    lift above rather than by darkening the label.
    const wxColour surface = StateColor::semantic(MD3::Role::Surface);
    const StateColor background(
        std::pair{StateColor::semantic(MD3::Role::SurfaceContainerLow), (int) StateColor::Hovered},
        std::pair{surface, (int) StateColor::Normal});
    // The selected-tab label, like the active indicator, is chrome and therefore
    // pinned to the brand accent (ColorScheme::Brand) regardless of the active
    // workspace scheme -- see the note in OnPaint and MD3 §4/§13.
    const StateColor text = selected
        ? StateColor(
            std::pair{StateColor::semantic(MD3::Role::Outline), (int) StateColor::Disabled},
            std::pair{StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Brand), (int) StateColor::Normal})
        : StateColor(
            std::pair{StateColor::semantic(MD3::Role::Outline), (int) StateColor::Disabled},
            std::pair{StateColor::semantic(MD3::Role::OnSurfaceVariant), (int) StateColor::Normal});

    button->SetBackgroundColor(background);
    button->SetTextColor(text);
    button->SetSelected(selected);
    // §4.3: the tab hover/selection fill is a flat, full-cell rectangle that
    // spans the tab cell edge-to-edge -- NOT a rounded pill inset in the cell.
    // wxDC draws a radius-0 rounded rect as a plain rectangle, so pinning the
    // tab Button corner radius to 0 yields the kit's flat hover layer.
    button->SetCornerRadius(0);
    // Kit tab metrics (TabBar.jsx): 16px per-side horizontal padding. The
    // icon-label gap is a separate 8px handled inside Button::messureSize and
    // is already correct, so only the horizontal padding moves off the compact
    // density value (10). Vertical padding stays at the compact gap since the
    // tab height is pinned by SetMinSize.
    button->SetPaddingSize({FromDIP(tab_horizontal_padding), FromDIP(MD3::Metrics::compact.gap)});

    // Kit tab label is body-s 13.5px: active SemiBold/600, inactive Normal/400.
    // Head_13 already carries the 13.5px/600 face (px-pinned), so clone it and
    // drop to weight 400 for inactive tabs -- this preserves the design px size
    // in both states, unlike the previous em-driven normal_font().
    wxFont font = Label::Head_13;
    if (!selected) {
        font.SetWeight(wxFONTWEIGHT_NORMAL);
        font.SetNumericWeight(400);
    }
    button->SetFont(font);
}

void ButtonsListCtrl::ApplyTheme()
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    for (size_t idx = 0; idx < m_pageButtons.size(); ++idx)
        StyleButton(m_pageButtons[idx], int(idx) == m_selection);
    for (Button *button : m_actionButtons)
        StyleButton(button, false);
    Refresh(false);
}

void ButtonsListCtrl::SetColorScheme(MD3::ColorScheme scheme)
{
    if (m_color_scheme == scheme)
        return;
    m_color_scheme = scheme;
    ApplyTheme();
}

void ButtonsListCtrl::UpdateMode()
{
    //m_mode_sizer->SetMode(Slic3r::GUI::wxGetApp().get_mode());
}

void ButtonsListCtrl::Rescale()
{
    //m_mode_sizer->msw_rescale();
    int em = em_unit(this);
    Button* selected_button = m_selection >= 0 && m_selection < int(m_pageButtons.size())
        ? m_pageButtons[m_selection]
        : nullptr;
    for (Button* btn : m_pageButtons) {
        const int tab_height = FromDIP(MD3::Metrics::navigation_bar_height - tab_bottom_space);
        // BBS: keep the Chrome-style shrinkable range in sync with the DPI scale.
        if (btn->GetLabel().empty()) {
            btn->SetMinSize({btn_width_icon * em / 10, tab_height});
        } else {
            btn->SetMinSize({btn_width_label_min * em / 10, tab_height});
            btn->SetMaxSize({btn_width_label_max * em / 10, tab_height});
        }
        StyleButton(btn, btn == selected_button);
        btn->Rescale();
    }
    for (Button *btn : m_actionButtons) {
        const int tab_height = FromDIP(MD3::Metrics::navigation_bar_height - tab_bottom_space);
        btn->SetMinSize({btn_width_label_min * em / 10, tab_height});
        btn->SetMaxSize({btn_width_label_max * em / 10, tab_height});
        StyleButton(btn, false);
        btn->Rescale();
    }

    // BBS: no gap
    //m_btn_margin = std::lround(0.3 * em);
    //m_line_margin = std::lround(0.1 * em);
    //m_buttons_sizer->SetVGap(m_btn_margin);
    //m_buttons_sizer->SetHGap(m_btn_margin);

    const int navigation_height = FromDIP(MD3::Metrics::navigation_bar_height);
    SetMinSize({-1, navigation_height});
    SetMaxSize({-1, navigation_height});
    m_line_margin = FromDIP(MD3::Metrics::tab_active_indicator);
    m_sizer->Layout();
    Refresh(false);
}

void ButtonsListCtrl::SetSelection(int sel)
{
    if (sel < 0 || sel >= int(m_pageButtons.size()))
        return;
    if (m_selection == sel) {
        StyleButton(m_pageButtons[sel], true);
        Refresh(false);
        return;
    }
    if (m_selection >= 0 && m_selection < int(m_pageButtons.size()))
        StyleButton(m_pageButtons[m_selection], false);

    m_selection = sel;
    StyleButton(m_pageButtons[m_selection], true);
    Refresh(false);
}

bool ButtonsListCtrl::InsertPage(size_t n, const wxString &text, bool bSelect /* = false*/, const std::string &bmp_name /* = ""*/, const std::string &inactive_bmp_name)
{
    Button * btn = new Button(this, text.empty() ? text : " " + text, bmp_name, wxNO_BORDER);

    // always show the tab name as a tooltip so the user can identify a tab
    // by hovering even when it is shrunk and the label is truncated with an ellipsis.
    // Icon-only tabs (empty label) get their tooltip set by the caller via Notebook::SetPageToolTip.
    if (!text.empty()) btn->SetToolTip(text);

    int em = em_unit(this);
    const int tab_height = FromDIP(MD3::Metrics::navigation_bar_height - tab_bottom_space);
    // BBS set size for button
    //  Chrome-style labeled tabs may shrink down to a small floor when crowded and
    //  grow up to the preferred width (136) when there is room, so the side tools
    //  (slice/print) on the right always stay visible. Icon-only tabs keep a fixed size.
    if (text.empty()) {
        btn->SetMinSize({btn_width_icon * em / 10, tab_height});
    } else {
        btn->SetAllowShrink(true);
        btn->SetMinSize({btn_width_label_min * em / 10, tab_height});
        btn->SetMaxSize({btn_width_label_max * em / 10, tab_height});
    }

    StyleButton(btn, bSelect);
    btn->SetInactiveIcon(inactive_bmp_name);
    btn->Bind(wxEVT_BUTTON, [this, btn](wxCommandEvent& event) {
        if (auto it = std::find(m_pageButtons.begin(), m_pageButtons.end(), btn); it != m_pageButtons.end()) {
            auto sel = it - m_pageButtons.begin();
            //do it later
            //SetSelection(sel);
            
            wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED);
            evt.SetId(sel);
            wxPostEvent(this->GetParent(), evt);
        }
    });
    Slic3r::GUI::wxGetApp().UpdateDarkUI(btn);
    m_pageButtons.insert(m_pageButtons.begin() + n, btn);
    // Labeled tabs get proportion 1 so they share the tab region equally (Chrome style)
    // Icon-only stay fixed (proportion 0) at their own min size
    wxSizerFlags flags = text.empty() ? wxSizerFlags(0) : wxSizerFlags(1);
    m_buttons_sizer->Insert(n, btn, flags.Align(wxALIGN_CENTER_VERTICAL));
    m_sizer->Layout();
    return true;
}

void ButtonsListCtrl::AddAction(const wxString &text, const std::string &bmp_name, std::function<void()> action)
{
    Button *button = new Button(this, text.empty() ? text : " " + text, bmp_name, wxNO_BORDER);
    button->SetToolTip(text);
    button->SetAllowShrink(true);
    const int em = em_unit(this);
    const int tab_height = FromDIP(MD3::Metrics::navigation_bar_height - tab_bottom_space);
    button->SetMinSize({btn_width_label_min * em / 10, tab_height});
    button->SetMaxSize({btn_width_label_max * em / 10, tab_height});
    StyleButton(button, false);
    button->Bind(wxEVT_BUTTON, [action = std::move(action)](wxCommandEvent &) {
        if (action)
            action();
    });
    Slic3r::GUI::wxGetApp().UpdateDarkUI(button);
    m_actionButtons.push_back(button);
    m_actions_sizer->Add(button, wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL));
    m_sizer->Layout();
}

void ButtonsListCtrl::RemovePage(size_t n)
{
    if (n >= m_pageButtons.size())
        return;

    Button* btn = m_pageButtons[n];
    if (int(n) == m_selection)
        m_selection = -1;
    else if (int(n) < m_selection)
        --m_selection;

    m_pageButtons.erase(m_pageButtons.begin() + n);
    m_buttons_sizer->Remove(n);
#if __WXOSX__
    RemoveChild(btn);
#else
    btn->Reparent(nullptr);
#endif
    btn->Destroy();
    m_sizer->Layout();
}

bool ButtonsListCtrl::SetPageImage(size_t n, const std::string& bmp_name) const
{
    if (n >= m_pageButtons.size())
        return false;
     
    // BBS
    //return m_pageButtons[n]->SetBitmap_(bmp_name);
    ScalableBitmap bitmap(NULL, bmp_name);
    //m_pageButtons[n]->SetBitmap_(bitmap);
    return true;
}

void ButtonsListCtrl::SetPageText(size_t n, const wxString& strText)
{
    Button* btn = m_pageButtons[n];
    btn->SetLabel(strText);
    // keep the hover tooltip in sync with the (possibly truncated) label
    if (!strText.empty()) btn->SetToolTip(strText);
}

void ButtonsListCtrl::SetPageToolTip(size_t n, const wxString &strToolTip)
{
    if (n >= m_pageButtons.size()) return;
    m_pageButtons[n]->SetToolTip(strToolTip);
}

wxString ButtonsListCtrl::GetPageText(size_t n) const
{
    Button* btn = m_pageButtons[n];
    return btn->GetLabel();
}

//#endif // _WIN32

void Notebook::Init()
{
    // We don't need any border as we don't have anything to separate the
    // page contents from.
    SetInternalBorder(0);

    // No effects by default.
    m_showEffect = m_hideEffect = wxSHOW_EFFECT_NONE;

    m_showTimeout = m_hideTimeout = 0;

    /* On Linux, Gstreamer wxMediaCtrl does not seem to get along well with
     * 32-bit X11 visuals (the overlay does not work).  Is this a wxWindows
     * bug?  Is this a Gstreamer bug?  No idea, but it is our problem ... 
     * and anyway, this transparency thing just isn't all that interesting,
     * so we just don't do it on Linux. 
     */
#ifndef __WXGTK__
    SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
#endif
}
