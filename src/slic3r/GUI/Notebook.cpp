#include "Notebook.hpp"

//#ifdef _WIN32

#include "GUI_App.hpp"
#include "wxExtensions.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StateMotionMath.hpp"
#include "Widgets/MD3Menu.hpp"
#include <wx/weakref.h>

//BBS set font size
#include "Widgets/Label.hpp"

#include <algorithm>
#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>

namespace {
constexpr int tab_glyph_px = 20;
constexpr int navigation_inset = 8;
constexpr int destination_gap = 4;
constexpr int tab_horizontal_padding = 14;

// Map a legacy workspace-tab raster key to its kit Material Symbols glyph.
//
// MainFrame passes the SAME 'tab_*_active' key for both the active and inactive
// slots, so the icon never toggled its FILL/outline state. The vendored Material
// Symbols Outlined face is static (no FILL axis), so we keep a single glyph per
// tab and express active vs inactive purely by colour + label weight in
// StyleButton (container foreground/600 active, OnSurfaceVariant/400 inactive), never a
// FILL 0->1 swap. Settings sub-tabs (Tab.cpp uses 'cog'/'spool') and the trailing
// 'settings' navigation action are included so the whole chrome tab bar is glyph
// driven. Returns 0 for any key without a mapping, leaving that button on its
// raster ScalableBitmap icon (the same fallback used when the font is missing).
uint32_t tab_glyph_for(const std::string &bmp_name)
{
    struct Entry { const char *key; uint32_t glyph; };
    static const Entry table[] = {
        {"tab_home_active",         MaterialIcon::Home},
        {"tab_3d_active",           MaterialIcon::ViewInAr},
        {"tab_preview_active",      MaterialIcon::Layers},
        {"tab_print_active",        MaterialIcon::Print},
        {"tab_monitor_active",      MaterialIcon::Cast},
        {"tab_multi_active",        MaterialIcon::Devices},
        {"tab_auxiliary_avtice",    MaterialIcon::FolderOpen}, // legacy key spelling in MainFrame
        {"tab_auxiliary_active",    MaterialIcon::FolderOpen}, // corrected spelling, forward-compat
        {"tab_calibration_active",  MaterialIcon::Build},
        {"tab_filament_active",     MaterialIcon::Palette},
        // Settings / parameters sub-tabs + the trailing Settings nav action.
        {"settings",                MaterialIcon::Settings},
        {"cog",                     MaterialIcon::Settings},
        {"notebook_presets_active", MaterialIcon::Settings},
        {"spool",                   MaterialIcon::Palette},    // filament-settings sub-tab
    };
    for (const auto &e : table)
        if (bmp_name == e.key)
            return e.glyph;
    return 0;
}
}; // namespace

wxDEFINE_EVENT(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED, wxCommandEvent);

ButtonsListCtrl::ButtonsListCtrl(wxWindow *parent, wxBoxSizer* side_tools) :
    wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL),
    m_side_tools(side_tools)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    const int height = FromDIP(MD3::Metrics::navigation_bar_height);
    SetMinSize({0, height});
    SetMaxSize({-1, height});
    SetName(_L("Workspace navigation"));
    m_line_margin = FromDIP(MD3::Metrics::tab_active_indicator);
    if (m_side_tools) {
        for (auto *item : m_side_tools->GetChildren())
            if (auto *window = item->GetWindow()) window->Reparent(this);
        m_side_tools->SetContainingWindow(this);
    }
    m_overflow_button = new Button(this, wxEmptyString, "", wxNO_BORDER);
    m_overflow_button->SetGlyph(MaterialIcon::MoreHoriz, tab_glyph_px);
    m_overflow_button->SetName(_L("More workspaces"));
    m_overflow_button->SetToolTip(_L("More workspaces"));
    m_overflow_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ShowOverflow(); });
    m_overflow_button->Hide();
    Bind(wxEVT_SIZE, [this](wxSizeEvent &event) { LayoutNavigation(); event.Skip(); });
    Bind(wxEVT_PAINT, &ButtonsListCtrl::OnPaint, this);
    Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent &event) { ApplyTheme(); event.Skip(); });
    ApplyTheme();
}

bool ButtonsListCtrl::IsWorkflowPage(int page) const
{
    return page >= 0 && std::find(m_workflow.begin(), m_workflow.end(), page) != m_workflow.end();
}

void ButtonsListCtrl::OnPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();
    const wxColour surface = StateColor::semantic(MD3::Role::Surface);
    dc.SetBackground(wxBrush(surface));
    dc.Clear();
    const int line = std::max(1, FromDIP(1));
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), line));
    if (!m_workflow_rect.IsEmpty()) {
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerLow)));
        dc.DrawRoundedRectangle(m_workflow_rect, FromDIP(16));
    }
    dc.DrawLine(0, size.y - line, size.x, size.y - line);
    Button *selected = m_selection >= 0 && m_selection < int(m_pageButtons.size()) ? m_pageButtons[m_selection] : nullptr;
    if (!selected) return;
    if (!selected->IsShown()) selected = m_overflow_button;
    if (!selected->IsShown()) return;
    // Keep a non-colour selection cue below the selected pill or overflow.
    const wxRect rect = selected->GetRect();
    const int width = std::min(FromDIP(24), rect.width);
    const auto primary = StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Brand);
    const double emphasis = MD3::Motion::reduced() ? 1.0 : m_selection_emphasis;
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(
        MD3::Motion::color_channel(surface.Red(), primary.Red(), emphasis),
        MD3::Motion::color_channel(surface.Green(), primary.Green(), emphasis),
        MD3::Motion::color_channel(surface.Blue(), primary.Blue(), emphasis))));
    dc.DrawRoundedRectangle(rect.x + (rect.width - width) / 2,
        size.y - m_line_margin, width, m_line_margin * 2, m_line_margin);
}

void ButtonsListCtrl::StyleButton(Button *button, bool selected)
{
    const auto page = std::find(m_pageButtons.begin(), m_pageButtons.end(), button);
    const bool workflow = page != m_pageButtons.end() && IsWorkflowPage(int(page - m_pageButtons.begin()));
    const wxColour surface = StateColor::semantic(workflow ? MD3::Role::SurfaceContainerLow : MD3::Role::Surface);
    button->SetBackgroundColor(StateColor(
        std::pair{StateColor::semantic(MD3::Role::SurfaceContainerHighest), (int)StateColor::Pressed},
        std::pair{StateColor::semantic(MD3::Role::SurfaceContainerHigh), (int)StateColor::Hovered},
        std::pair{selected ? StateColor::semantic(MD3::Role::PrimaryContainer, MD3::ColorScheme::Brand) : surface, (int)StateColor::Normal}));
    button->SetTextColor(StateColor(
        std::pair{StateColor::semantic(MD3::Role::Outline), (int)StateColor::Disabled},
        std::pair{StateColor::semantic(MD3::Role::OnSurface), (int)StateColor::Pressed},
        std::pair{StateColor::semantic(MD3::Role::OnSurface), (int)StateColor::Hovered},
        std::pair{StateColor::semantic(selected ? MD3::Role::OnPrimaryContainer : MD3::Role::OnSurfaceVariant, MD3::ColorScheme::Brand), (int)StateColor::Normal}));
    button->SetBorderWidth(0);
    button->SetSelected(selected);
    button->SetCornerRadius(FromDIP(16));
    button->SetPaddingSize({FromDIP(tab_horizontal_padding), 0});
    button->SetAllowShrink(false);
    button->SetMaxSize(wxDefaultSize);
    const int height = FromDIP(MD3::Metrics::active().row_height);
    const wxFont regular = MD3::Metrics::active().font_size <= 13 ? Label::Body_13 : Label::Body_14;
    wxFont strong = regular;
    strong.SetNumericWeight(600);
    button->SetMinSize({-1, height});
    button->SetFont(strong);
    const int width = std::max(FromDIP(40), button->GetMinSize().x);
    button->SetFont(selected ? strong : regular);
    // Reserve semibold label width in both states so peers do not move.
    button->SetMinSize({width, height});
}

void ButtonsListCtrl::ApplyTheme()
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    for (size_t i = 0; i < m_pageButtons.size(); ++i) StyleButton(m_pageButtons[i], int(i) == m_selection);
    for (auto *button : m_actionButtons) StyleButton(button, false);
    StyleButton(m_overflow_button, false);
    LayoutNavigation();
    Refresh(false);
}

void ButtonsListCtrl::SetWorkflowPages(int prepare, int preview, int print, int monitor)
{
    m_workflow = {{prepare, preview, print, monitor}};
    ApplyTheme();
}

void ButtonsListCtrl::LayoutNavigation()
{
    if (m_laying_out || !m_overflow_button) return;
    m_laying_out = true;
    const wxSize size = GetClientSize();
    const int inset = FromDIP(navigation_inset), gap = FromDIP(destination_gap);
    int available = std::max(0, size.x - 2 * inset);
    if (m_side_tools) {
        const wxSize tools = m_side_tools->CalcMin();
        m_side_tools->SetDimension(std::max(inset, size.x - inset - tools.x),
            std::max(0, (size.y - tools.y) / 2), tools.x, tools.y);
        available = std::max(0, available - tools.x - gap);
    }
    std::vector<NotebookNavigation::Item> items;
    const auto order = NotebookNavigation::order(int(m_pageButtons.size()), m_workflow);
    for (int page : order) items.push_back({page, m_pageButtons[page]->GetMinSize().x, IsWorkflowPage(page)});
    for (size_t i = 0; i < m_actionButtons.size(); ++i)
        items.push_back({int(m_pageButtons.size() + i), m_actionButtons[i]->GetMinSize().x, false});
    const auto layout = NotebookNavigation::fit(items, available, m_overflow_button->GetMinSize().x, gap, m_selection);
    wxWindow *focused = wxWindow::FindFocus();
    bool focus_hidden = false;
    // Hide only overflowing controls. Hiding a still-visible focused button
    // transiently would discard keyboard focus on every resize or selection.
    for (int id : layout.overflow) {
        Button *button = id < int(m_pageButtons.size()) ? m_pageButtons[id] : m_actionButtons[id - m_pageButtons.size()];
        if (button == focused) focus_hidden = true;
        button->Hide();
    }
    m_workflow_rect = wxRect();
    Button *previous = nullptr;
    for (const auto &position : layout.visible) {
        Button *button = position.id < int(m_pageButtons.size()) ? m_pageButtons[position.id]
            : m_actionButtons[position.id - m_pageButtons.size()];
        const int height = button->GetMinSize().y;
        const wxRect rect(inset + position.x, std::max(0, (size.y - height) / 2), position.width, height);
        button->SetSize(rect);
        button->Show();
        if (previous) button->MoveAfterInTabOrder(previous);
        previous = button;
        if (button == focused) focus_hidden = false;
        if (position.workflow) {
            if (m_workflow_rect.IsEmpty()) m_workflow_rect = rect;
            else m_workflow_rect.Union(rect);
        }
    }
    if (!m_workflow_rect.IsEmpty()) m_workflow_rect.Inflate(FromDIP(1));
    m_overflow = layout.overflow;
    const bool selected_hidden = std::find(m_overflow.begin(), m_overflow.end(), m_selection) != m_overflow.end();
    StyleButton(m_overflow_button, selected_hidden);
    m_overflow_button->Show(!m_overflow.empty());
    if (!m_overflow.empty()) {
        const int height = m_overflow_button->GetMinSize().y;
        m_overflow_button->SetSize(inset + layout.overflow_x, std::max(0, (size.y - height) / 2), layout.overflow_width, height);
        const wxString name = selected_hidden ? _L("More workspaces") + ": " + m_pageButtons[m_selection]->GetLabel() : _L("More workspaces");
        m_overflow_button->SetName(name);
        m_overflow_button->SetToolTip(name);
        if (previous) m_overflow_button->MoveAfterInTabOrder(previous);
        if (focus_hidden) m_overflow_button->SetFocus();
    } else if (focused == m_overflow_button && m_selection >= 0 && m_selection < int(m_pageButtons.size())) {
        m_pageButtons[m_selection]->SetFocus();
    }
    m_laying_out = false;
    Refresh(false);
}

void ButtonsListCtrl::ShowOverflow()
{
    wxMenu menu;
    // A nested menu event loop may insert or remove pages. Bind weak button
    // identities instead of positions, then let the existing handler resolve it.
    for (int id : m_overflow) {
        Button *button = id < int(m_pageButtons.size()) ? m_pageButtons[id] : m_actionButtons[id - m_pageButtons.size()];
        wxString label = button->GetLabel();
        if (label.empty()) label = button->GetToolTipText();
        if (label.empty()) label = button->GetName();
        label.Replace("&", "&&");
        const int menu_id = wxWindow::NewControlId();
        menu.AppendCheckItem(menu_id, label)->Check(id == m_selection);
        menu.Enable(menu_id, button->IsEnabled());
        wxWeakRef<Button> target(button);
        menu.Bind(wxEVT_MENU, [target](wxCommandEvent &) {
            if (!target || !target->IsEnabled()) return;
            wxCommandEvent event(wxEVT_BUTTON, target->GetId());
            event.SetEventObject(target.get());
            target->GetEventHandler()->ProcessEvent(event);
        }, menu_id);
    }
    MD3::PopupMenuBelow(m_overflow_button, &menu, true);
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
    for (auto *button : m_pageButtons) button->Rescale();
    for (auto *button : m_actionButtons) button->Rescale();
    m_overflow_button->Rescale();
    const int height = FromDIP(MD3::Metrics::navigation_bar_height);
    SetMinSize({0, height});
    SetMaxSize({-1, height});
    m_line_margin = FromDIP(MD3::Metrics::tab_active_indicator);
    ApplyTheme();
}

void ButtonsListCtrl::SetSelection(int sel)
{
    if (sel < 0 || sel >= int(m_pageButtons.size()))
        return;
    if (m_selection == sel) {
        StyleButton(m_pageButtons[sel], true);
        LayoutNavigation();
        Refresh(false);
        return;
    }
    if (m_selection >= 0 && m_selection < int(m_pageButtons.size()))
        StyleButton(m_pageButtons[m_selection], false);

    m_selection = sel;
    StyleButton(m_pageButtons[m_selection], true);
    LayoutNavigation();
    // Selection and page dispatch are immediate. The decorative wash and marker
    // gain emphasis without moving the page or waiting to dispatch its event.
    m_selection_motion.Play(MD3::Motion::medium1, [this](double t) {
        m_selection_emphasis = MD3::Motion::interpolate(0.65, 1.0, t);
        if (m_selection >= 0 && m_selection < int(m_pageButtons.size())) {
            Button *button = m_pageButtons[m_selection];
            if (!button->IsShown()) button = m_overflow_button;
            const auto surface = StateColor::semantic(MD3::Role::Surface);
            const auto container = StateColor::semantic(MD3::Role::PrimaryContainer, MD3::ColorScheme::Brand);
            const double emphasis = MD3::Motion::reduced() ? 1.0 : m_selection_emphasis;
            button->SetBackgroundColor(StateColor(
                std::pair{StateColor::semantic(MD3::Role::SurfaceContainerHighest), (int)StateColor::Pressed},
                std::pair{StateColor::semantic(MD3::Role::SurfaceContainerHigh), (int)StateColor::Hovered},
                std::pair{wxColour(MD3::Motion::color_channel(surface.Red(), container.Red(), emphasis),
                                  MD3::Motion::color_channel(surface.Green(), container.Green(), emphasis),
                                  MD3::Motion::color_channel(surface.Blue(), container.Blue(), emphasis)), (int)StateColor::Normal}));
        }
        Refresh(false);
    }, nullptr, &MD3::Motion::easeStandard, this);
    Refresh(false);
}

bool ButtonsListCtrl::InsertPage(size_t n, const wxString &text, bool bSelect /* = false*/, const std::string &bmp_name /* = ""*/, const std::string &inactive_bmp_name)
{
    if (n > m_pageButtons.size()) return false;
    Button *btn = new Button(this, text, bmp_name, wxNO_BORDER);
    if (!text.empty()) { btn->SetToolTip(text); btn->SetName(text); }
    NotebookNavigation::inserted(m_workflow, int(n));
    if (m_selection >= int(n)) ++m_selection;
    btn->SetInactiveIcon(inactive_bmp_name);
    // Kit tab bar draws a Material Symbols glyph (20px) instead of the legacy
    // 'tab_*_active' raster. The Button glyph path recolours the static face via
    // text_color (container foreground when active / OnSurfaceVariant otherwise, set in
    // StyleButton) rather than a FILL swap. The raster active/inactive icons the
    // Button was constructed with remain as the capability-gated fallback: when
    // MaterialIcon::available() is false, render()/messureSize() ignore the glyph
    // and use those bitmaps instead.
    if (const uint32_t glyph = tab_glyph_for(bmp_name))
        btn->SetGlyph(glyph, tab_glyph_px);
    btn->Bind(wxEVT_BUTTON, [this, btn](wxCommandEvent& event) {
        if (auto it = std::find(m_pageButtons.begin(), m_pageButtons.end(), btn); it != m_pageButtons.end()) {
            auto sel = it - m_pageButtons.begin();
            wxCommandEvent evt = wxCommandEvent(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED);
            evt.SetId(sel);
            wxPostEvent(this->GetParent(), evt);
        }
    });
    Slic3r::GUI::wxGetApp().UpdateDarkUI(btn);
    m_pageButtons.insert(m_pageButtons.begin() + n, btn);
    ApplyTheme();
    return true;
}

void ButtonsListCtrl::AddAction(const wxString &text, const std::string &bmp_name, std::function<void()> action)
{
    Button *button = new Button(this, text, bmp_name, wxNO_BORDER);
    button->SetToolTip(text);
    button->SetName(text);
    if (const uint32_t glyph = tab_glyph_for(bmp_name)) button->SetGlyph(glyph, tab_glyph_px);
    button->Bind(wxEVT_BUTTON, [action = std::move(action)](wxCommandEvent &) { if (action) action(); });
    m_actionButtons.push_back(button);
    ApplyTheme();
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
    NotebookNavigation::removed(m_workflow, int(n));
#if __WXOSX__
    RemoveChild(btn);
#else
    btn->Reparent(nullptr);
#endif
    btn->Destroy();
    ApplyTheme();
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
    if (n >= m_pageButtons.size()) return;
    Button* btn = m_pageButtons[n];
    btn->SetLabel(strText);
    btn->SetName(strText);
    // Keep the tooltip and overflow discovery name in sync with the full label.
    if (!strText.empty()) btn->SetToolTip(strText);
    ApplyTheme();
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
