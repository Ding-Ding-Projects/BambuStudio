#include <wx/dc.h>
#include "TabCtrl.hpp"
#include "StateColor.hpp"

wxDEFINE_EVENT( wxEVT_TAB_SEL_CHANGING, wxCommandEvent );
wxDEFINE_EVENT( wxEVT_TAB_SEL_CHANGED, wxCommandEvent );

BEGIN_EVENT_TABLE(TabCtrl, StaticBox)

EVT_KEY_DOWN(TabCtrl::keyDown)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

#define TAB_BUTTON_SPACE 2
#define TAB_BUTTON_PADDING_X 2
#define TAB_BUTTON_PADDING_Y 2
#define TAB_BUTTON_PADDING TAB_BUTTON_PADDING_X, TAB_BUTTON_PADDING_Y

TabCtrl::TabCtrl(wxWindow *      parent,
                   wxWindowID      id,
                   const wxPoint & pos,
                   const wxSize &  size,
                   long            style)
    : StaticBox(parent, id, pos, size, style)
{
#if 0
    radius = 5;
#else
    radius = 1;
#endif
    border_width = 1;
    SetBorderColor(ThemeColor::Grey400);
    sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->AddSpacer(10);
    auto hsizer = new wxBoxSizer(wxVERTICAL);
    hsizer->Add(sizer, 0, wxEXPAND | wxBOTTOM, border_width * 4);
    SetSizer(hsizer);
    Bind(wxEVT_COMMAND_BUTTON_CLICKED, &TabCtrl::buttonClicked, this);
    //wxString reason;
    //IsTransparentBackgroundSupported(&reason);
}

TabCtrl::~TabCtrl()
{
    delete images;
}

int TabCtrl::GetSelection() const { return sel; }

void TabCtrl::SelectItem(int item)
{
    if (item == sel || !sendTabCtrlEvent(true))
        return;
    if (sel >= 0) {
        wxCommandEvent e(wxEVT_CHECKBOX);
        auto b = btns[sel];
        e.SetEventObject(b);
        b->GetEventHandler()->ProcessEvent(e);
    }
    sel = item;
    if (sel >= 0) {
        wxCommandEvent e(wxEVT_CHECKBOX);
        auto b = btns[sel];
        e.SetEventObject(b);
        b->GetEventHandler()->ProcessEvent(e);
    }
    sendTabCtrlEvent();
    relayout();
    Refresh();
}

void TabCtrl::Unselect()
{
    SelectItem(-1);
}

void TabCtrl::Rescale()
{
    for (auto & b : btns) {
        b->Rescale();
        // Re-bake the pill radius/height/fill for the new DPI (and theme). Gated
        // on pill mode so the flat-strip consumers' per-item padding overrides
        // (SetItemPaddingSize) are never reset here.
        if (m_pill_style)
            applyItemStyle(b);
    }
}

bool TabCtrl::SetFont(wxFont const& font)
{
    StaticBox::SetFont(font);
    bold = font.Bold();
    for (size_t i = 0; i < btns.size(); ++i)
        btns[i]->SetFont(i == sel ? bold : font);
    return true;
}

int TabCtrl::AppendItem(const wxString &item,
                     int image, int selImage,
                     void * clientData)
{
    Button * btn = new Button();
    btn->Create(this, item, "", wxBORDER_NONE);
    btn->SetFont(GetFont());
    btn->SetTextColor(tabTextColor());
    applyItemStyle(btn);
    btns.push_back(btn);
    if (btns.size() > 1)
        sizer->GetItem(sizer->GetItemCount() - 1)->SetMinSize({0, 0});
    sizer->Add(btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, TAB_BUTTON_SPACE);
    sizer->AddStretchSpacer(1);
    relayout();
    return btns.size() - 1;
}

bool TabCtrl::DeleteItem(int item)
{
    return false;
}

void TabCtrl::DeleteAllItems()
{
    sizer->Clear(true);
    sizer->AddSpacer(10);
    btns.clear();
    if (sel >= 0) {
        sel = -1;
        sendTabCtrlEvent();
    }
}

unsigned int TabCtrl::GetCount() const { return btns.size(); }

wxString TabCtrl::GetItemText(unsigned int item) const
{
    return item < btns.size() ? btns[item]->GetLabel() : wxString{};
}

void TabCtrl::SetItemText(unsigned int item, wxString const &value)
{
    if (item >= btns.size()) return;
    btns[item]->SetLabel(value);
}

bool TabCtrl::GetItemBold(unsigned int item) const
{
    if (item >= btns.size()) return false;
    return btns[item]->GetFont() == bold;
}

void TabCtrl::SetItemBold(unsigned int item, bool bold)
{
    if (item >= btns.size()) return;
    btns[item]->SetFont(bold ? this->bold : GetFont());
    btns[item]->Rescale();
}

void* TabCtrl::GetItemData(unsigned int item) const
{
    if (item >= btns.size()) return nullptr;
    return btns[item]->GetClientData();
}

void TabCtrl::SetItemData(unsigned int item, void* clientData)
{
    if (item >= btns.size()) return;
    btns[item]->SetClientData(clientData);
}

void TabCtrl::AssignImageList(wxImageList* imageList)
{
    if (images == imageList) return;
    delete images;
    images = imageList;
}

void TabCtrl::SetItemPaddingSize(unsigned int item, const wxSize &size)
{
    if (item >= btns.size()) return;
    btns[item]->SetPaddingSize(size);
}

void TabCtrl::SetItemTextColour(unsigned int item, const StateColor &col)
{
    if (item >= btns.size()) return;
    btns[item]->SetTextColor(col);
}

void TabCtrl::SetItemGlyph(unsigned int item, uint32_t glyph)
{
    if (item >= btns.size()) return;
    // Leading Material Symbol at 20 design-px. Button::SetGlyph resolves the
    // codepoint through the shared MaterialIcon font path (coloured by the item's
    // text colour) when the face is available, else keeps its raster-icon
    // fallback; the assigned wxImageList is untouched either way.
    btns[item]->SetGlyph(glyph, 20);
}

StateColor TabCtrl::tabTextColor() const
{
    if (m_pill_style) {
        // Kit NavItem pill labels + leading glyph: the active tab (carrying the
        // Checked state, toggled in SelectItem) reads OnSecondaryContainer for
        // contrast on the SecondaryContainer pill fill; every other state falls
        // through to OnSurfaceVariant. The leading glyph inherits this colour.
        return StateColor(
            std::make_pair(StateColor::semantic(MD3::Role::OnSecondaryContainer, m_scheme), (int) StateColor::Checked),
            std::make_pair(StateColor::semantic(MD3::Role::OnSurfaceVariant), (int) StateColor::Normal));
    }
    // Kit secondary-tab labels (navigation/TabBar): the active tab (the one
    // carrying the Checked state, toggled in SelectItem) is Primary in the
    // current scheme; every other state falls through to the OnSurfaceVariant
    // Normal entry. This replaces the legacy TextMuted / raw wxLIGHT_GREY pair.
    return StateColor(
        std::make_pair(StateColor::semantic(MD3::Role::Primary, m_scheme), (int) StateColor::Checked),
        std::make_pair(StateColor::semantic(MD3::Role::OnSurfaceVariant), (int) StateColor::Normal));
}

StateColor TabCtrl::navPillBg() const
{
    // Kit NavItem pill fill: selected SecondaryContainer (current scheme), hover
    // SurfaceContainerHigh, idle = the strip background so the resting pill is
    // invisible (mirrors the IconButton "rest = parent background" idiom rather
    // than a transparent brush). Checked is listed first so a hovered selected
    // pill keeps its selected fill. GetBackgroundColour() is the same value the
    // item buttons clear to, and is re-read on theme toggle via RefreshItemStyles.
    const wxColour strip_bg = GetBackgroundColour();
    return StateColor(
        std::make_pair(StateColor::semantic(MD3::Role::SecondaryContainer, m_scheme), (int) StateColor::Checked),
        std::make_pair(StateColor::semantic(MD3::Role::SurfaceContainerHigh), (int) StateColor::Hovered),
        std::make_pair(strip_bg, (int) StateColor::Normal));
}

void TabCtrl::applyItemStyle(Button *btn)
{
    // Geometry + fill only (the text colour is owned separately: AppendItem seeds
    // it and the preset editor overrides it per item to encode the modified /
    // sys-value decoration). Keeping text out of here lets Rescale re-bake the
    // pill radius/height/fill for a new DPI or theme without clobbering those
    // per-item text colours.
    if (m_pill_style) {
        // MD3 NavItem: h44 pill, r22, SecondaryContainer/High/strip-bg fill,
        // 14px horizontal inset. FromDIP keeps the geometry crisp on HiDPI.
        btn->SetCornerRadius(FromDIP(22));
        btn->SetBackgroundColor(navPillBg());
        btn->SetPaddingSize({FromDIP(14), FromDIP(4)});
        btn->SetMinSize({0, FromDIP(44)});
    } else {
        btn->SetCornerRadius(0);
        btn->SetBackgroundColor(StateColor());
        btn->SetPaddingSize({TAB_BUTTON_PADDING});
    }
}

void TabCtrl::SetColorScheme(MD3::ColorScheme scheme)
{
    if (m_scheme == scheme)
        return;
    m_scheme = scheme;
    // Re-resolve the scheme-aware active-label accent for every tab; the active
    // indicator reads m_scheme live in doRender(), so a repaint recolours it. In
    // pill mode the selected SecondaryContainer fill is scheme-aware too.
    for (auto &b : btns) {
        b->SetTextColor(tabTextColor());
        if (m_pill_style)
            b->SetBackgroundColor(navPillBg());
    }
    Refresh();
}

void TabCtrl::SetNavItemStyle(bool pill)
{
    if (m_pill_style == pill)
        return;
    m_pill_style = pill;
    for (auto &b : btns) {
        applyItemStyle(b);
        b->SetTextColor(tabTextColor());
    }
    relayout();
    Refresh();
}

void TabCtrl::RefreshItemStyles()
{
    if (!m_pill_style)
        return;
    for (auto &b : btns)
        b->SetBackgroundColor(navPillBg());
    Refresh();
}

int TabCtrl::GetFirstVisibleItem() const
{
    return btns.size() == 0 ? -1 : 0;
}

int TabCtrl::GetNextVisible(int item) const
{
    return ++item < btns.size() ? item : -1;
}

bool TabCtrl::IsVisible(unsigned int item) const
{
    return true;
}

void TabCtrl::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    auto size = GetSize();
    wxWindow::DoSetSize(x, y, width, height, sizeFlags);
    if (size == GetSize()) return;
    relayout();
}

#ifdef __WIN32__

WXLRESULT TabCtrl::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if (nMsg == WM_GETDLGCODE) { return DLGC_WANTARROWS; }
    return wxWindow::MSWWindowProc(nMsg, wParam, lParam);
}

#endif

void TabCtrl::relayout()
{
    int offset = 10;
    int item = sel + 1;
    int first = 0;
    for (int i = 0; i < item; ++i)
        offset += btns[i]->GetMinSize().x + TAB_BUTTON_SPACE * 2;
    if (item < btns.size())
        offset += btns[item]->GetMinSize().x + TAB_BUTTON_SPACE * 2;
    int  width = GetSize().x;
    for (int i = 0; i < btns.size(); ++i) {
        auto size = btns[i]->GetMinSize().x + TAB_BUTTON_SPACE * 2;
        if (i < sel && offset > width) {
            sizer->Show(i * 2 + 1, false);
            sizer->Show(i * 2 + 2, false);
            offset -= size;
            first = i + 1;
        } else if (i <= item) {
            sizer->Show(i * 2 + 1, true);
            sizer->Show(i * 2 + 2, true);
        } else if (offset <= width) {
            sizer->Show(i * 2 + 1, true);
            sizer->Show(i * 2 + 2, true);
            offset += size;
            item = i;
        } else {
            sizer->Show(i * 2 + 1, false);
            sizer->Show(i * 2 + 2, false);
        }
        sizer->GetItem(i * 2 + 2)->SetMinSize({0, 0});
    }
    if (item >= btns.size())
        -- item;
    // Keep spacing 2 ~ 10 TAB_BUTTON_SPACE
    int b = GetSize().x - offset - 10 - (item + 1 - first) * TAB_BUTTON_SPACE * 8;
    sizer->GetItem(item * 2 + 2)->SetMinSize({b > 0 ? b : 0, 0});
    Layout();
}

void TabCtrl::buttonClicked(wxCommandEvent &event)
{
    SetFocus();
    auto btn  = event.GetEventObject();
    auto iter = std::find(btns.begin(), btns.end(), btn);
    SelectItem(iter == btns.end() ? -1 : iter - btns.begin());
}

void TabCtrl::keyDown(wxKeyEvent &event)
{
    switch (event.GetKeyCode()) {
    case WXK_UP:
    case WXK_DOWN:
    case WXK_LEFT:
    case WXK_RIGHT:
        if ((event.GetKeyCode() == WXK_UP || event.GetKeyCode() == WXK_LEFT) && GetSelection() > 0) {
            SelectItem(GetSelection() - 1);
        } else if ((event.GetKeyCode() == WXK_DOWN || event.GetKeyCode() == WXK_RIGHT) && GetSelection() + 1 < btns.size()) {
            SelectItem(GetSelection() + 1);
        }
        break;
    }
}

void TabCtrl::doRender(wxDC& dc)
{
    wxSize size = GetSize();
    int states = state_handler.states();

    if (m_pill_style) {
        // NavItem strip: the selected pill's SecondaryContainer fill is the
        // selection cue, so the Primary underline indicator is suppressed. A 1px
        // OutlineVariant bottom divider (role-based, replacing the legacy Grey400
        // border line) keeps the strip boundary and adapts to light/dark by role.
        const int bw  = std::max(1, border_width);
        const int bs2 = (1 + border_width) / 2;
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), bw));
        dc.DrawLine(0, size.y - bs2, size.x, size.y - bs2);
        return;
    }

    if (sel < 0) { return; }

    auto x1 = btns[sel]->GetPosition().x;
    auto x2 = x1 + btns[sel]->GetSize().x;
    const int BS2 = (1 + border_width) / 2;
#if 0
    const int BS = border_width / 2;
    x1 -= TAB_BUTTON_SPACE; x2 += TAB_BUTTON_SPACE;
    dc.SetPen(wxPen(border_color.colorForStates(states), border_width));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawLine(0, size.y - BS2, x1 - radius + BS2, size.y - BS2);
    dc.DrawArc(x1 - radius, size.y, x1, size.y - radius, x1 - radius, size.y - radius);
    dc.DrawLine(x1, size.y - radius, x1, radius);
    dc.DrawArc(x1 + radius, 0, x1, radius, x1 + radius, radius);
    dc.DrawLine(x1 + radius, BS, x2 - radius, BS);
    dc.DrawArc(x2, radius, x2 - radius, 0, x2 - radius, radius);
    dc.DrawLine(x2, radius, x2, size.y - radius);
    dc.DrawArc(x2, size.y - radius, x2 + radius, size.y, x2 + radius, size.y - radius);
    dc.DrawLine(x2 + radius - BS2, size.y - BS2, size.x, size.y - BS2);
#else
    dc.SetPen(wxPen(border_color.colorForStates(states), border_width));
    dc.DrawLine(0, size.y - BS2, size.x, size.y - BS2);
    // Kit active indicator (navigation/TabBar): a 3px bar in the current scheme's
    // Primary, inset 12px from the selected tab's edges, with only the TOP corners
    // rounded (radius 3px 3px 0 0). wxDC has no per-corner radius, so the pill is
    // drawn at double height and the bitmap's bottom edge clips its lower (rounded)
    // half, leaving square bottom corners flush with the bar baseline. FromDIP is
    // read every paint, so the geometry stays crisp across DPI/density changes with
    // no cached radius.
    const int indicator_inset  = FromDIP(12);
    const int indicator_height = FromDIP(MD3::Metrics::tab_active_indicator);
    const int indicator_width  = std::max(1, (x2 - x1) - 2 * indicator_inset);
    wxRect indicator(x1 + indicator_inset, size.y - indicator_height, indicator_width, indicator_height * 2);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary, m_scheme)));
    dc.DrawRoundedRectangle(indicator, indicator_height);
#endif
}

bool TabCtrl::sendTabCtrlEvent(bool changing)
{
    wxCommandEvent event(changing ? wxEVT_TAB_SEL_CHANGING : wxEVT_TAB_SEL_CHANGED, GetId());
    event.SetEventObject(this);
    event.SetInt(sel);
    GetEventHandler()->ProcessEvent(event);
    return true;
}
