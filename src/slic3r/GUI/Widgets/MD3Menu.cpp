#include "MD3Menu.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../Appearance/AppearanceEditorPopover.hpp"
#include "../Appearance/ElementStyle.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "MD3Motion.hpp"
#include "MD3Tokens.hpp"
#include "SearchField.hpp"
#include "StateColor.hpp"

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/display.h>
#include <wx/evtloop.h>
#include <wx/settings.h>
#include <wx/tooltip.h>

#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

#ifdef __WIN32__
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <memory>

namespace Slic3r { namespace GUI {

namespace {

// Design-pixel geometry (converted with FromDIP at use).
constexpr int kLeadingSlot    = 24; // icon / check / radio slot
constexpr int kLeadingGap     = 12; // slot -> label
constexpr int kTrailingGap    = 24; // label -> shortcut / chevron
constexpr int kGlyphPx        = 20;
constexpr int kSeparatorPad   = 8;  // above and below the 1px rule
constexpr int kContainerPadV  = 8;  // top / bottom inside the frame
constexpr int kSearchHeight   = 44;
constexpr int kSearchGap      = 8;
constexpr int kMinWidth       = 200;
constexpr int kMaxWidth       = 480;
constexpr int kFrame          = 1;

// Alpha-composite `over` onto `under` with opacity a in [0,1].
wxColour blend(const wxColour &over, const wxColour &under, double a)
{
    auto mix = [a](unsigned char o, unsigned char u) {
        return static_cast<unsigned char>(std::lround(o * a + u * (1.0 - a)));
    };
    return wxColour(mix(over.Red(), under.Red()), mix(over.Green(), under.Green()),
                    mix(over.Blue(), under.Blue()));
}

// Type::body_s is 13.5/500. Label ships 13/400 (Body_13) and 13.5/600
// (Head_13); derive the exact style from the body face so the family, font
// scale and language fallback stay in step with the rest of the UI.
wxFont body_s_font()
{
    wxFont f = Label::Body_13;
    if (!f.IsOk())
        return wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    f.SetFractionalPointSize(f.GetFractionalPointSize() * (MD3::Type::body_s.size / 13.0));
    f.SetWeight(wxFONTWEIGHT_MEDIUM);
    // Per-element appearance: "menu.item" is the id every Material menu row
    // resolves through (Appearance/ElementStyle.hpp).
    return ElementStyle::font_for("menu.item", f);
}

wxFont caption_font()
{
    wxFont f = Label::Body_11; // Type::caption 11.5/400
    if (!f.IsOk())
        return wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    return f;
}

// The mnemonic letter of a raw wx label ("&Save" -> 'S'), or 0 when none.
wxUniChar mnemonic_of(const wxString &raw)
{
    for (size_t i = 0; i + 1 < raw.length(); ++i) {
        if (raw[i] == '&') {
            if (raw[i + 1] == '&') {
                ++i;
                continue;
            }
            return raw[i + 1];
        }
    }
    return wxUniChar(0);
}

} // namespace

// ---------------------------------------------------------------------------
// MD3MenuList: the owner-drawn row surface inside the popup.
// ---------------------------------------------------------------------------

class MD3MenuList : public wxWindow
{
public:
    MD3MenuList(MD3MenuPopup *popup, const std::vector<MD3::Menu::Item> &rows);
    ~MD3MenuList() override;

    void SetVisible(std::vector<int> visible);
    const std::vector<int> &Visible() const { return m_visible; }

    int  RowHeight() const { return FromDIP(MD3::Metrics::active().row_height); }
    int  SeparatorHeight() const { return FromDIP(2 * kSeparatorPad) + 1; }
    int  ContentHeight() const;
    // Widest row at the current fonts, including paddings and trailing area.
    int  MeasureWidth();

    // Keyboard selection (index into m_visible, -1 for none).
    int  Selected() const { return m_selected; }
    void Select(int vis_index, bool ensure_visible = true);
    void SelectFirst();
    void SelectLast();
    void MoveSelection(int delta);
    void PageMove(int direction);
    void ActivateSelected();
    bool OpenSelectedSubmenu(bool focus_first);
    bool ActivateMnemonic(wxUniChar ch);

    const MD3::Menu::Item *ItemAt(int vis_index) const;
    wxRect RowRect(int vis_index) const;        // client coordinates
    wxRect RowScreenRect(int vis_index) const;
    int    HitTest(const wxPoint &pt) const;    // vis index or -1

    void ScrollBy(int dy);

    // True when the popup's open submenu belongs to this row.
    bool IsSubmenuOpenFor(int vis_index) const;

#if wxUSE_ACCESSIBILITY
    int  HoverIndex() const { return m_hover; }
    void AccessibilityActivate(int vis_index);
#endif

private:
    void paintEvent(wxPaintEvent &);
    void paintRow(wxDC &dc, int vis_index, const wxRect &r, const wxColour &surface);
    void onMotion(wxMouseEvent &);
    void onLeave(wxMouseEvent &);
    void onLeftUp(wxMouseEvent &);
    void onWheel(wxMouseEvent &);
    void onHoverTimer(wxTimerEvent &);
    void setHover(int vis_index);
    void ensureVisible(int vis_index);
    bool selectable(int vis_index) const;
    void openSubmenuAt(int vis_index, bool focus_first);
    void updateTooltip(int vis_index);

    MD3MenuPopup *                      m_popup;
    const std::vector<MD3::Menu::Item> &m_rows;
    std::vector<int>                    m_visible;
    std::vector<bool>                   m_secondary_inline; // per visible row, set at paint

    int m_hover { -1 };
    int m_selected { -1 };
    int m_offset_y { 0 };

    wxTimer m_hover_timer;
    int     m_pending_submenu { -1 };
};

#if wxUSE_ACCESSIBILITY
// Child ids are 1-based indices into the visible row list (wxACC_SELF is 0).
class MD3MenuAccessible final : public wxWindowAccessible
{
public:
    explicit MD3MenuAccessible(MD3MenuList *list) : wxWindowAccessible(list), m_list(list) {}

    wxAccStatus GetChildCount(int *count) override
    {
        if (!count)
            return wxACC_FAIL;
        *count = static_cast<int>(m_list->Visible().size());
        return wxACC_OK;
    }

    wxAccStatus GetChild(int child_id, wxAccessible **child) override
    {
        if (!child)
            return wxACC_FAIL;
        if (child_id == wxACC_SELF) {
            *child = this;
            return wxACC_OK;
        }
        if (!item(child_id))
            return wxACC_FAIL;
        *child = nullptr; // simple element, addressed by id through this object
        return wxACC_OK;
    }

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (!name)
            return wxACC_FAIL;
        if (child_id == wxACC_SELF) {
            *name = _L("Menu");
            return wxACC_OK;
        }
        const MD3::Menu::Item *it = item(child_id);
        if (!it)
            return wxACC_FAIL;
        *name = it->label;
        if (!it->secondary.IsEmpty())
            *name << wxString::FromUTF8(" \xC2\xB7 ") << it->secondary;
        return wxACC_OK;
    }

    wxAccStatus GetDescription(int child_id, wxString *description) override
    {
        if (!description)
            return wxACC_FAIL;
        const MD3::Menu::Item *it = item(child_id);
        if (!it)
            return wxACC_NOT_IMPLEMENTED;
        *description = it->help;
        return wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (!role)
            return wxACC_FAIL;
        if (child_id == wxACC_SELF) {
            *role = wxROLE_SYSTEM_MENUPOPUP;
            return wxACC_OK;
        }
        const MD3::Menu::Item *it = item(child_id);
        if (!it)
            return wxACC_FAIL;
        *role = it->actionable() ? wxROLE_SYSTEM_MENUITEM : wxROLE_SYSTEM_SEPARATOR;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (!state)
            return wxACC_FAIL;
        *state = 0;
        if (child_id == wxACC_SELF) {
            *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
            if (m_list->HasFocus())
                *state |= wxACC_STATE_SYSTEM_FOCUSED;
            return wxACC_OK;
        }
        const MD3::Menu::Item *it = item(child_id);
        if (!it)
            return wxACC_FAIL;
        if (!it->actionable())
            return wxACC_OK;
        *state |= wxACC_STATE_SYSTEM_FOCUSABLE | wxACC_STATE_SYSTEM_SELECTABLE;
        if (m_list->Selected() == child_id - 1)
            *state |= wxACC_STATE_SYSTEM_FOCUSED | wxACC_STATE_SYSTEM_SELECTED;
        if (!it->enabled)
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if ((it->kind == MD3::Menu::Item::Check || it->kind == MD3::Menu::Item::Radio) && it->checked)
            *state |= wxACC_STATE_SYSTEM_CHECKED;
        // wx defines no HASPOPUP state and its MSAA bridge only forwards the
        // wx-defined flags, so a submenu row reports the expandable pair the
        // bridge does map: COLLAPSED while closed, EXPANDED while its
        // submenu is showing.
        if (it->kind == MD3::Menu::Item::Submenu)
            *state |= m_list->IsSubmenuOpenFor(child_id - 1) ? wxACC_STATE_SYSTEM_EXPANDED
                                                              : wxACC_STATE_SYSTEM_COLLAPSED;
        return wxACC_OK;
    }

    wxAccStatus GetKeyboardShortcut(int child_id, wxString *shortcut) override
    {
        if (!shortcut)
            return wxACC_FAIL;
        const MD3::Menu::Item *it = item(child_id);
        if (!it)
            return wxACC_NOT_IMPLEMENTED;
        *shortcut = it->shortcut;
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override
    {
        if (!action_name)
            return wxACC_FAIL;
        const MD3::Menu::Item *it = item(child_id);
        if (!it || !it->actionable())
            return wxACC_NOT_IMPLEMENTED;
        *action_name = _L("Activate");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        const MD3::Menu::Item *it = item(child_id);
        if (!it || !it->actionable() || !it->enabled)
            return wxACC_NOT_IMPLEMENTED;
        m_list->AccessibilityActivate(child_id - 1);
        return wxACC_OK;
    }

    wxAccStatus GetLocation(wxRect &rect, int element_id) override
    {
        if (element_id == wxACC_SELF)
            return wxWindowAccessible::GetLocation(rect, element_id);
        if (!item(element_id))
            return wxACC_FAIL;
        rect = m_list->RowScreenRect(element_id - 1);
        return wxACC_OK;
    }

    wxAccStatus GetFocus(int *child_id, wxAccessible **child) override
    {
        if (!child_id || !child)
            return wxACC_FAIL;
        *child = nullptr;
        *child_id = m_list->Selected() >= 0 ? m_list->Selected() + 1 : wxACC_SELF;
        return wxACC_OK;
    }

    wxAccStatus HitTest(const wxPoint &pt, int *child_id, wxAccessible **child_object) override
    {
        if (!child_id || !child_object)
            return wxACC_FAIL;
        *child_object = nullptr;
        const int hit = m_list->HitTest(m_list->ScreenToClient(pt));
        *child_id = hit >= 0 ? hit + 1 : wxACC_SELF;
        return wxACC_OK;
    }

private:
    const MD3::Menu::Item *item(int child_id) const
    {
        if (child_id <= 0)
            return nullptr;
        return m_list->ItemAt(child_id - 1);
    }

    MD3MenuList *m_list;
};
#endif // wxUSE_ACCESSIBILITY

MD3MenuList::MD3MenuList(MD3MenuPopup *popup, const std::vector<MD3::Menu::Item> &rows)
    : wxWindow(popup, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS)
    , m_popup(popup)
    , m_rows(rows)
    , m_hover_timer(this)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    SetName(_L("Menu"));
    Bind(wxEVT_PAINT, &MD3MenuList::paintEvent, this);
    Bind(wxEVT_MOTION, &MD3MenuList::onMotion, this);
    Bind(wxEVT_LEAVE_WINDOW, &MD3MenuList::onLeave, this);
    Bind(wxEVT_LEFT_UP, &MD3MenuList::onLeftUp, this);
    Bind(wxEVT_LEFT_DOWN, [](wxMouseEvent &) {}); // activation happens on release
    Bind(wxEVT_MOUSEWHEEL, &MD3MenuList::onWheel, this);
    Bind(wxEVT_TIMER, &MD3MenuList::onHoverTimer, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent &) {});
#if wxUSE_ACCESSIBILITY
    new MD3MenuAccessible(this); // wxWindow owns the accessible object.
#endif
}

MD3MenuList::~MD3MenuList()
{
    m_hover_timer.Stop();
}

void MD3MenuList::SetVisible(std::vector<int> visible)
{
    m_visible = std::move(visible);
    m_secondary_inline.assign(m_visible.size(), false);
    m_hover    = -1;
    m_selected = -1;
    m_offset_y = 0;
    m_hover_timer.Stop();
    m_pending_submenu = -1;
    Refresh();
}

const MD3::Menu::Item *MD3MenuList::ItemAt(int vis_index) const
{
    if (vis_index < 0 || vis_index >= static_cast<int>(m_visible.size()))
        return nullptr;
    const int row = m_visible[vis_index];
    if (row < 0 || row >= static_cast<int>(m_rows.size()))
        return nullptr;
    return &m_rows[row];
}

int MD3MenuList::ContentHeight() const
{
    int h = 0;
    for (int vis = 0; vis < static_cast<int>(m_visible.size()); ++vis) {
        const MD3::Menu::Item *it = ItemAt(vis);
        h += (it && it->actionable()) ? RowHeight() : SeparatorHeight();
    }
    return h;
}

int MD3MenuList::MeasureWidth()
{
    wxClientDC dc(this);
    const int  pad   = FromDIP(MD3::Metrics::active().padding);
    const int  fixed = 2 * pad + FromDIP(kLeadingSlot) + FromDIP(kLeadingGap) + FromDIP(kTrailingGap);
    int        widest = 0;
    for (const MD3::Menu::Item &it : m_rows) {
        if (!it.actionable())
            continue;
        dc.SetFont(body_s_font());
        int label_w = dc.GetTextExtent(it.label).x;
        if (!it.secondary.IsEmpty())
            label_w = dc.GetTextExtent(it.label + wxString::FromUTF8(" \xC2\xB7 ") + it.secondary).x;
        int trailing = 0;
        if (it.kind == MD3::Menu::Item::Submenu) {
            trailing = FromDIP(kGlyphPx);
        } else if (!it.shortcut.IsEmpty()) {
            dc.SetFont(caption_font());
            trailing = dc.GetTextExtent(it.shortcut).x;
        }
        widest = std::max(widest, fixed + label_w + trailing);
    }
    return std::max(FromDIP(kMinWidth), std::min(widest, FromDIP(kMaxWidth)));
}

wxRect MD3MenuList::RowRect(int vis_index) const
{
    int y = -m_offset_y;
    for (int vis = 0; vis < static_cast<int>(m_visible.size()); ++vis) {
        const MD3::Menu::Item *it = ItemAt(vis);
        const int h = (it && it->actionable()) ? RowHeight() : SeparatorHeight();
        if (vis == vis_index)
            return wxRect(0, y, GetClientSize().x, h);
        y += h;
    }
    return wxRect();
}

wxRect MD3MenuList::RowScreenRect(int vis_index) const
{
    wxRect r = RowRect(vis_index);
    r.SetPosition(ClientToScreen(r.GetPosition()));
    return r;
}

int MD3MenuList::HitTest(const wxPoint &pt) const
{
    if (pt.x < 0 || pt.x >= GetClientSize().x)
        return -1;
    int y = -m_offset_y;
    for (int vis = 0; vis < static_cast<int>(m_visible.size()); ++vis) {
        const MD3::Menu::Item *it = ItemAt(vis);
        const int h = (it && it->actionable()) ? RowHeight() : SeparatorHeight();
        if (pt.y >= y && pt.y < y + h)
            return vis;
        y += h;
    }
    return -1;
}

bool MD3MenuList::IsSubmenuOpenFor(int vis_index) const
{
    const MD3::Menu::Item *it = ItemAt(vis_index);
    return it && it->kind == MD3::Menu::Item::Submenu && m_popup->IsSubmenuShown() &&
           m_popup->OpenSubmenuMenu() == it->submenu;
}

bool MD3MenuList::selectable(int vis_index) const
{
    const MD3::Menu::Item *it = ItemAt(vis_index);
    return it && it->actionable() && it->enabled;
}

void MD3MenuList::ensureVisible(int vis_index)
{
    const wxRect r    = RowRect(vis_index);
    const int    view = GetClientSize().y;
    if (r.IsEmpty() || view <= 0)
        return;
    if (r.y < 0)
        m_offset_y += r.y;
    else if (r.GetBottom() >= view)
        m_offset_y += r.GetBottom() - view + 1;
    m_offset_y = std::max(0, std::min(m_offset_y, std::max(0, ContentHeight() - view)));
}

void MD3MenuList::Select(int vis_index, bool ensure_visible)
{
    if (vis_index < -1 || vis_index >= static_cast<int>(m_visible.size()))
        return;
    if (m_selected == vis_index)
        return;
    m_selected = vis_index;
    if (ensure_visible && vis_index >= 0)
        ensureVisible(vis_index);
    Refresh();
#if wxUSE_ACCESSIBILITY
    if (vis_index >= 0)
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_FOCUS, this, wxOBJID_CLIENT, vis_index + 1);
#endif
}

void MD3MenuList::SelectFirst()
{
    for (int vis = 0; vis < static_cast<int>(m_visible.size()); ++vis)
        if (selectable(vis)) {
            Select(vis);
            return;
        }
}

void MD3MenuList::SelectLast()
{
    for (int vis = static_cast<int>(m_visible.size()) - 1; vis >= 0; --vis)
        if (selectable(vis)) {
            Select(vis);
            return;
        }
}

void MD3MenuList::MoveSelection(int delta)
{
    const int n = static_cast<int>(m_visible.size());
    if (n == 0)
        return;
    if (m_selected < 0) {
        delta > 0 ? SelectFirst() : SelectLast();
        return;
    }
    int index = m_selected;
    for (int step = 0; step < n; ++step) {
        index = (index + delta + n) % n;
        if (selectable(index)) {
            Select(index);
            return;
        }
    }
}

void MD3MenuList::PageMove(int direction)
{
    const int n = static_cast<int>(m_visible.size());
    if (n == 0 || RowHeight() <= 0)
        return;
    const int per_page = std::max(1, GetClientSize().y / RowHeight());
    int target = m_selected < 0 ? (direction > 0 ? 0 : n - 1) : m_selected + direction * per_page;
    target = std::max(0, std::min(target, n - 1));
    // Walk toward the travel direction, then back, to land on a selectable row.
    for (int i = target; i >= 0 && i < n; i += direction)
        if (selectable(i)) {
            Select(i);
            return;
        }
    for (int i = target; i >= 0 && i < n; i -= direction)
        if (selectable(i)) {
            Select(i);
            return;
        }
}

void MD3MenuList::openSubmenuAt(int vis_index, bool focus_first)
{
    const MD3::Menu::Item *it = ItemAt(vis_index);
    if (!it || it->kind != MD3::Menu::Item::Submenu || !it->enabled || !it->submenu)
        return;
    m_popup->OpenSubmenu(*it, RowScreenRect(vis_index), focus_first);
}

void MD3MenuList::ActivateSelected()
{
    const MD3::Menu::Item *it = ItemAt(m_selected);
    if (!it || !selectable(m_selected))
        return;
    if (it->kind == MD3::Menu::Item::Submenu) {
        openSubmenuAt(m_selected, true);
        return;
    }
    m_popup->ActivateItem(*it);
}

bool MD3MenuList::OpenSelectedSubmenu(bool focus_first)
{
    const MD3::Menu::Item *it = ItemAt(m_selected);
    if (!it || it->kind != MD3::Menu::Item::Submenu || !it->enabled)
        return false;
    openSubmenuAt(m_selected, focus_first);
    return true;
}

bool MD3MenuList::ActivateMnemonic(wxUniChar ch)
{
    if (ch == wxUniChar(0))
        return false;
    const wxString want = wxString(ch).Lower();
    for (int vis = 0; vis < static_cast<int>(m_visible.size()); ++vis) {
        const MD3::Menu::Item *it = ItemAt(vis);
        if (!it || !selectable(vis) || !it->source)
            continue;
        const wxUniChar m = mnemonic_of(it->source->GetItemLabel());
        if (m == wxUniChar(0) || wxString(m).Lower() != want)
            continue;
        Select(vis);
        if (it->kind == MD3::Menu::Item::Submenu)
            openSubmenuAt(vis, true);
        else
            m_popup->ActivateItem(*it);
        return true;
    }
    return false;
}

#if wxUSE_ACCESSIBILITY
void MD3MenuList::AccessibilityActivate(int vis_index)
{
    if (!selectable(vis_index))
        return;
    Select(vis_index);
    ActivateSelected();
}
#endif

void MD3MenuList::ScrollBy(int dy)
{
    const int max_off = std::max(0, ContentHeight() - GetClientSize().y);
    const int next    = std::max(0, std::min(m_offset_y + dy, max_off));
    if (next == m_offset_y)
        return;
    m_offset_y = next;
    Refresh();
}

void MD3MenuList::updateTooltip(int vis_index)
{
    const MD3::Menu::Item *it = ItemAt(vis_index);
    wxString tip;
    if (it && it->actionable()) {
        if (!it->secondary.IsEmpty() && vis_index < static_cast<int>(m_secondary_inline.size()) &&
            !m_secondary_inline[vis_index])
            tip = it->secondary;
        if (!it->help.IsEmpty()) {
            if (!tip.IsEmpty())
                tip << "\n";
            tip << it->help;
        }
    }
    if (tip.IsEmpty())
        UnsetToolTip();
    else if (GetToolTipText() != tip)
        SetToolTip(tip);
}

void MD3MenuList::setHover(int vis_index)
{
    if (m_hover == vis_index)
        return;
    m_hover = vis_index;
    updateTooltip(vis_index);
    Refresh();

    m_hover_timer.Stop();
    m_pending_submenu = -1;
    const MD3::Menu::Item *it = ItemAt(vis_index);
    if (it && it->kind == MD3::Menu::Item::Submenu && it->enabled) {
        if (MD3::Motion::reduced()) {
            openSubmenuAt(vis_index, false);
        } else {
            m_pending_submenu = vis_index;
            m_hover_timer.StartOnce(MD3MenuPopup::kSubmenuHoverDelayMs);
        }
    } else if (it && it->actionable() && m_popup->IsSubmenuShown()) {
        // Hovering a sibling row closes a submenu opened for another row.
        m_popup->CloseSubmenu();
    }
}

void MD3MenuList::onHoverTimer(wxTimerEvent &)
{
    if (m_pending_submenu >= 0 && m_pending_submenu == m_hover)
        openSubmenuAt(m_pending_submenu, false);
    m_pending_submenu = -1;
}

void MD3MenuList::onMotion(wxMouseEvent &evt)
{
    setHover(HitTest(evt.GetPosition()));
}

void MD3MenuList::onLeave(wxMouseEvent &)
{
    // Keep the hover highlight while the pointer travels into a child submenu.
    if (m_popup->IsSubmenuShown())
        return;
    setHover(-1);
}

void MD3MenuList::onLeftUp(wxMouseEvent &evt)
{
    const int vis = HitTest(evt.GetPosition());
    if (!selectable(vis))
        return;
    const MD3::Menu::Item *it = ItemAt(vis);
    if (it->kind == MD3::Menu::Item::Submenu) {
        Select(vis);
        openSubmenuAt(vis, false);
        return;
    }
    m_popup->ActivateItem(*it);
}

void MD3MenuList::onWheel(wxMouseEvent &evt)
{
    const int delta = evt.GetWheelDelta() > 0 ? evt.GetWheelDelta() : 120;
    ScrollBy(-evt.GetWheelRotation() * RowHeight() / delta);
    setHover(HitTest(evt.GetPosition()));
}

void MD3MenuList::paintEvent(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainer);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(surface));
    dc.DrawRectangle(GetClientRect());

    const int view = GetClientSize().y;
    for (int vis = 0; vis < static_cast<int>(m_visible.size()); ++vis) {
        const wxRect r = RowRect(vis);
        if (r.GetBottom() < 0 || r.y >= view)
            continue;
        paintRow(dc, vis, r, surface);
    }
}

void MD3MenuList::paintRow(wxDC &dc, int vis, const wxRect &r, const wxColour &surface)
{
    const MD3::Menu::Item *it = ItemAt(vis);
    if (!it)
        return;
    const int pad = FromDIP(MD3::Metrics::active().padding);

    if (!it->actionable()) {
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant)));
        const int y = r.y + FromDIP(kSeparatorPad);
        dc.DrawLine(r.x + pad, y, r.GetRight() - pad + 1, y);
        return;
    }

    const bool selected = vis == m_selected;
    const bool hovered  = vis == m_hover && it->enabled;

    const wxColour on_surface = ElementStyle::colour_for("menu.item", StyleProp::foreground,
                                                         StateColor::semantic(MD3::Role::OnSurface));
    wxColour fg        = on_surface;
    wxColour fg_muted  = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    if (selected) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SecondaryContainer)));
        dc.DrawRectangle(r);
        fg = fg_muted = StateColor::semantic(MD3::Role::OnSecondaryContainer);
    } else if (hovered) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(blend(on_surface, surface, 0.08)));
        dc.DrawRectangle(r);
    }
    if (!it->enabled) {
        const wxColour under = selected ? StateColor::semantic(MD3::Role::SecondaryContainer) : surface;
        fg       = blend(fg, under, 0.38);
        fg_muted = blend(fg_muted, under, 0.38);
    }

    // Leading slot: bitmap, check mark or radio glyph.
    const int    slot = FromDIP(kLeadingSlot);
    const wxRect slot_rect(r.x + pad, r.y + (r.height - slot) / 2, slot, slot);
    const int    glyph = FromDIP(kGlyphPx);
    switch (it->kind) {
    case MD3::Menu::Item::Check:
        if (it->checked)
            MaterialIcon::drawCentered(dc, MaterialIcon::Check, glyph, fg, slot_rect);
        break;
    case MD3::Menu::Item::Radio:
        MaterialIcon::drawCentered(dc, it->checked ? MaterialIcon::RadioButtonChecked : MaterialIcon::RadioButtonUnchecked,
                                   glyph, fg, slot_rect);
        break;
    default:
        if (it->icon.IsOk()) {
            const wxSize bs(it->icon.GetScaledWidth(), it->icon.GetScaledHeight());
            dc.DrawBitmap(it->icon, slot_rect.x + (slot - bs.x) / 2, slot_rect.y + (slot - bs.y) / 2, true);
        }
        break;
    }

    // Trailing area: chevron for submenus, else the shortcut in caption type.
    int trailing_left = r.GetRight() - pad + 1;
    if (it->kind == MD3::Menu::Item::Submenu) {
        const wxRect chevron(r.GetRight() - pad - glyph + 1, r.y + (r.height - glyph) / 2, glyph, glyph);
        MaterialIcon::drawCentered(dc, MaterialIcon::ChevronRight, glyph, fg_muted, chevron);
        trailing_left = chevron.x;
    } else if (!it->shortcut.IsEmpty()) {
        dc.SetFont(caption_font());
        dc.SetTextForeground(fg_muted);
        const wxSize ext = dc.GetTextExtent(it->shortcut);
        trailing_left    = r.GetRight() - pad - ext.x + 1;
        dc.DrawText(it->shortcut, trailing_left, r.y + (r.height - ext.y) / 2);
    }

    // Label, with the bilingual secondary appended when it fits.
    const int label_x  = slot_rect.GetRight() + 1 + FromDIP(kLeadingGap);
    const int avail    = trailing_left - FromDIP(kTrailingGap) - label_x;
    dc.SetFont(body_s_font());
    dc.SetTextForeground(fg);
    wxString text = it->label;
    bool     inline_secondary = false;
    if (!it->secondary.IsEmpty()) {
        const wxString combined = it->label + wxString::FromUTF8(" \xC2\xB7 ") + it->secondary;
        if (dc.GetTextExtent(combined).x <= avail) {
            text             = combined;
            inline_secondary = true;
        }
    }
    if (vis < static_cast<int>(m_secondary_inline.size()))
        m_secondary_inline[vis] = inline_secondary;
    wxSize ext = dc.GetTextExtent(text);
    if (ext.x > avail && avail > 0) {
        // Ellipsize from the end so the leading words stay readable.
        const wxString ellipsis = wxString::FromUTF8("\xE2\x80\xA6");
        while (!text.IsEmpty() && dc.GetTextExtent(text + ellipsis).x > avail)
            text.RemoveLast();
        text += ellipsis;
        ext = dc.GetTextExtent(text);
    }
    dc.DrawText(text, label_x, r.y + (r.height - ext.y) / 2);
}

// ---------------------------------------------------------------------------
// MD3MenuPopup
// ---------------------------------------------------------------------------

MD3MenuPopup::MD3MenuPopup(wxWindow *owner, wxMenu *menu, MD3MenuPopup *parent_popup)
    : PopupWindow(parent_popup ? static_cast<wxWindow *>(parent_popup) : owner,
                  wxBORDER_NONE | wxPU_CONTAINS_CONTROLS)
    , m_owner(owner)
    , m_menu(menu)
    , m_parent(parent_popup)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    m_radius = FromDIP(MD3::Metrics::radius_rail);
    Bind(wxEVT_PAINT, &MD3MenuPopup::paintEvent, this);
    Bind(wxEVT_CHAR_HOOK, &MD3MenuPopup::onCharHook, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent &) {});

    if (I18N::language_mode_profile().is_bilingual()) {
        m_secondary = [](const wxString &label) {
            return I18N::language_mode_service().translate(label).secondary;
        };
    }
}

MD3MenuPopup::~MD3MenuPopup()
{
    // A root destroyed without ever closing (owner torn down under it) must
    // still release its blocking caller.
    if (!m_parent && !m_finalized) {
        m_finalized = true;
        if (m_close_cb) {
            auto cb = std::move(m_close_cb);
            cb();
        }
    }
}

MD3MenuPopup *MD3MenuPopup::RootPopup()
{
    MD3MenuPopup *p = this;
    while (p->m_parent)
        p = p->m_parent;
    return p;
}

bool MD3MenuPopup::IsSubmenuShown() const
{
    return m_child && m_child->IsShown();
}

wxRect MD3MenuPopup::displayArea() const
{
    int idx = wxDisplay::GetFromWindow(m_owner ? m_owner : static_cast<const wxWindow *>(this));
    if (idx == wxNOT_FOUND)
        idx = 0;
    return wxDisplay(static_cast<unsigned>(idx)).GetClientArea();
}

void MD3MenuPopup::build()
{
    if (m_list)
        return;
    m_rows = MD3::Menu::snapshot(*m_menu, m_secondary);

    int actionable = 0;
    for (const MD3::Menu::Item &it : m_rows)
        if (it.actionable())
            ++actionable;

    if (actionable >= kSearchThreshold) {
        m_search = new SearchField(this, _L("Search menu"));
        m_search->SetOnQuery([this](const wxString &) { ApplyFilter(); });
        m_search->SetOnRegexToggle([this](bool) { ApplyFilter(); });
    }
    m_list = new MD3MenuList(this, m_rows);
    ApplyFilter();
}

wxSize MD3MenuPopup::wantedSize() const
{
    const int inset = FromDIP(kFrame) + FromDIP(kContainerPadV);
    int       w     = m_list->MeasureWidth() + 2 * FromDIP(kFrame);
    int       h     = m_list->ContentHeight() + 2 * inset;
    if (m_search) {
        h += FromDIP(kSearchHeight) + FromDIP(kSearchGap);
        w = std::max(w, FromDIP(MD3::Metrics::popover_width));
    }
    return wxSize(w, h);
}

void MD3MenuPopup::layout(const wxRect &target)
{
    SetSize(target);
    const int frame = FromDIP(kFrame);
    const int padv  = FromDIP(kContainerPadV);
    const int padh  = FromDIP(kContainerPadV);
    int       y     = frame + padv;
    if (m_search) {
        m_search->SetSize(frame + padh, y, target.width - 2 * (frame + padh), FromDIP(kSearchHeight));
        y += FromDIP(kSearchHeight) + FromDIP(kSearchGap);
    }
    const int list_h = std::max(0, target.height - y - frame - padv);
    m_list->SetSize(frame, y, target.width - 2 * frame, list_h);
}

void MD3MenuPopup::PopupAt(const wxRect &anchor_screen)
{
    // Capture the invoker before any child takes focus (SideMenuPopup pattern).
    if (!m_parent) {
        wxWindow *focused = wxWindow::FindFocus();
        for (wxWindow *w = focused; w; w = w->GetParent())
            if (w == this) {
                focused = nullptr;
                break;
            }
        m_invoker = focused ? focused : m_owner;
    }
    build();
    m_anchor_rect   = anchor_screen;
    m_anchor_is_row = false;
    const MD3::Menu::Placement p = MD3::Menu::place_root(anchor_screen, wantedSize(), displayArea());
    layout(p.rect);
    Popup();
}

void MD3MenuPopup::PopupBesideRow(const wxRect &row_screen)
{
    build();
    m_anchor_rect   = row_screen;
    m_anchor_is_row = true;
    const MD3::Menu::Placement p = MD3::Menu::place_submenu(row_screen, wantedSize(), displayArea());
    layout(p.rect);
    Popup();
}

void MD3MenuPopup::Popup(wxWindow *focus)
{
    if (!m_parent)
        wxGetApp().set_side_menu_popup_status(true);
    wxWindow *target = focus ? focus : (m_search ? static_cast<wxWindow *>(m_search->GetTextCtrl()) : m_list);
    PopupWindow::Popup(target);
    MD3::Motion::FadeIn(this, MD3::Motion::short2);
    if (target)
        target->SetFocus();
}

void MD3MenuPopup::Dismiss()
{
    // A parent stays up while its submenu is showing (DropDown pattern): the
    // focus loss caused by the child opening must not close us.
    if (IsSubmenuShown())
        return;
    PopupWindow::Dismiss();
}

void MD3MenuPopup::OnDismiss()
{
    if (IsSubmenuShown())
        return;
    if (m_closed)
        return;
    m_closed = true;
    if (m_parent) {
        MD3MenuPopup *parent = m_parent;
        PopupWindow::OnDismiss();
        parent->onChildDismissed(this, m_reason);
        return;
    }
    finalizeClose();
}

bool MD3MenuPopup::ProcessLeftDown(wxMouseEvent &event)
{
    return PopupWindow::ProcessLeftDown(event);
}

void MD3MenuPopup::restoreInvokerFocus()
{
    if (m_restoring_focus)
        return;
    m_restoring_focus = true;
    if (m_invoker && m_invoker->IsShown() && m_invoker->IsEnabled())
        m_invoker->SetFocus();
    m_restoring_focus = false;
}

void MD3MenuPopup::finalizeClose()
{
    if (m_finalized)
        return;
    m_finalized = true;
    wxGetApp().set_side_menu_popup_status(false);
    // Restore focus while the transient window is still alive; the base
    // implementation can destroy it synchronously on some backends.
    restoreInvokerFocus();
    PopupWindow::OnDismiss();
    restoreInvokerFocus();

    if (m_result != wxID_NONE && m_send_events && m_menu) {
        // The activated item may live in a submenu; its own wxMenu owns the
        // wxEVT_MENU bindings (append_menu_item binds on the submenu object),
        // and wxMenuBase::SendEvent walks up to the invoking window afterwards.
        wxMenu *source = m_result_menu ? m_result_menu : m_menu;
        source->SendEvent(m_result, m_result_kind_checkable ? (m_result_checked ? 1 : 0) : -1);
    }
    if (m_close_cb) {
        auto cb = std::move(m_close_cb);
        cb();
    }
}

void MD3MenuPopup::ActivateItem(const MD3::Menu::Item &item)
{
    if (!item.actionable() || !item.enabled || item.kind == MD3::Menu::Item::Submenu)
        return;
    MD3MenuPopup *root = RootPopup();
    bool          checked = item.checked;
    if (item.kind == MD3::Menu::Item::Check) {
        checked = !item.checked;
        if (item.source)
            item.source->Check(checked);
    } else if (item.kind == MD3::Menu::Item::Radio) {
        checked = true;
        if (item.source && !item.source->IsChecked())
            item.source->Check(true);
    }
    root->m_result                = item.id;
    root->m_result_checked        = checked;
    root->m_result_kind_checkable = item.kind == MD3::Menu::Item::Check || item.kind == MD3::Menu::Item::Radio;
    root->m_result_menu           = item.source ? item.source->GetMenu() : m_menu;

    // Close from the deepest surface upward. Each parent's m_child is cleared
    // first so its Dismiss() no longer refuses.
    for (MD3MenuPopup *p = this; p;) {
        MD3MenuPopup *parent = p->m_parent;
        p->m_reason          = DismissReason::Chain;
        if (parent)
            parent->m_child = nullptr;
        p->DismissAndNotify();
        p = parent;
    }
}

void MD3MenuPopup::OpenSubmenu(const MD3::Menu::Item &item, const wxRect &row_screen, bool focus_first)
{
    if (!item.submenu)
        return;
    if (m_child) {
        if (m_child->m_menu == item.submenu && m_child->IsShown()) {
            if (focus_first) {
                m_child->FocusList();
                m_child->m_list->SelectFirst();
            }
            return;
        }
        CloseSubmenu();
    }
    m_child = new MD3MenuPopup(m_owner, item.submenu, this);
    m_child->PopupBesideRow(row_screen);
    if (focus_first)
        m_child->m_list->SelectFirst();
}

void MD3MenuPopup::CloseSubmenu()
{
    if (!m_child)
        return;
    MD3MenuPopup *child = m_child;
    m_child             = nullptr;
    child->CloseSubmenu();
    child->m_reason = DismissReason::Chain;
    child->DismissAndNotify();
    child->Destroy();
}

void MD3MenuPopup::ReturnToParent()
{
    if (!m_parent)
        return;
    CloseSubmenu();
    m_reason = DismissReason::ReturnToParent;
    DismissAndNotify();
}

void MD3MenuPopup::RequestEscape()
{
    if (m_parent) {
        ReturnToParent();
        return;
    }
    CloseSubmenu();
    m_reason = DismissReason::Escape;
    DismissAndNotify();
}

void MD3MenuPopup::FocusList()
{
#ifdef __WIN32__
    ::SetActiveWindow(static_cast<HWND>(GetHandle()));
#endif
    if (m_list)
        m_list->SetFocus();
}

void MD3MenuPopup::onChildDismissed(MD3MenuPopup *child, DismissReason reason)
{
    if (m_child == child)
        m_child = nullptr;
    child->Destroy(); // deferred for top-level windows

    switch (reason) {
    case DismissReason::Outside:
        // Pointer over this surface: the user is browsing back into the parent.
        // Anywhere else: the whole chain was clicked away.
        if (GetScreenRect().Contains(wxGetMousePosition()))
            FocusList();
        else
            DismissAndNotify();
        break;
    case DismissReason::ReturnToParent:
    case DismissReason::Escape:
        FocusList();
        break;
    case DismissReason::Chain:
        break;
    }
}

void MD3MenuPopup::ApplyFilter()
{
    if (!m_list)
        return;
    std::function<bool(const wxString &)> matches;
    if (m_search && !m_search->GetValue().IsEmpty()) {
        auto pass = std::make_shared<SearchField::MatchPass>(m_search->GetValue(), m_search->IsRegexEnabled(),
                                                             m_search->IsCaseSensitive(), m_search->IsWholeWord(),
                                                             m_search->IsMultiline());
        matches   = [pass](const wxString &s) { return pass->matches(s); };
    }
    m_list->SetVisible(MD3::Menu::filter(m_rows, matches, m_secondary));
    if (matches)
        m_list->SelectFirst();
    // Re-place the card for the new row count so a narrowed list does not
    // leave a tall empty surface below the last visible row.
    if (IsShown() && m_anchor_rect.width > 0) {
        const MD3::Menu::Placement p = m_anchor_is_row
            ? MD3::Menu::place_submenu(m_anchor_rect, wantedSize(), displayArea())
            : MD3::Menu::place_root(m_anchor_rect, wantedSize(), displayArea());
        layout(p.rect);
        Refresh();
    }
}

void MD3MenuPopup::onCharHook(wxKeyEvent &evt)
{
    if (!m_list) {
        evt.Skip();
        return;
    }
    const bool search_focused = m_search && m_search->GetTextCtrl() &&
                                wxWindow::FindFocus() == m_search->GetTextCtrl();
    const bool search_has_text = m_search && !m_search->GetValue().IsEmpty();

    switch (evt.GetKeyCode()) {
    case WXK_ESCAPE: RequestEscape(); return;
    case WXK_DOWN: m_list->MoveSelection(1); return;
    case WXK_UP: m_list->MoveSelection(-1); return;
    case WXK_TAB: m_list->MoveSelection(evt.ShiftDown() ? -1 : 1); return;
    case WXK_HOME:
        if (search_focused && search_has_text) break;
        m_list->SelectFirst();
        return;
    case WXK_END:
        if (search_focused && search_has_text) break;
        m_list->SelectLast();
        return;
    case WXK_PAGEDOWN: m_list->PageMove(1); return;
    case WXK_PAGEUP: m_list->PageMove(-1); return;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER: m_list->ActivateSelected(); return;
    case WXK_SPACE:
        if (search_focused && search_has_text) break; // typing a space into the query
        m_list->ActivateSelected();
        return;
    case WXK_RIGHT:
        if (m_list->OpenSelectedSubmenu(true)) return;
        if (search_focused) break;
        return;
    case WXK_LEFT:
        if (m_parent) { ReturnToParent(); return; }
        if (search_focused) break;
        return;
    default: {
        // Mnemonics only while there is no query to type into.
        if (!search_has_text && !evt.HasAnyModifiers()) {
            const wxUniChar ch = evt.GetUnicodeKey();
            if (ch.GetValue() >= 32 && m_list->ActivateMnemonic(ch))
                return;
        }
        break;
    }
    }
    evt.Skip();
}

void MD3MenuPopup::paintEvent(wxPaintEvent &)
{
    wxPaintDC dc(this);
#ifdef __WXMSW__
    wxGCDC dc2(dc); // antialiased rounded frame
#else
    wxDC &dc2(dc);
#endif
    const wxSize   size    = GetSize();
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainer);

    // The popup HWND has square corners: fill the whole client area first so
    // the corner triangles outside the rounded frame are not left undefined.
    dc2.SetPen(*wxTRANSPARENT_PEN);
    dc2.SetBrush(wxBrush(surface));
    dc2.DrawRectangle(0, 0, size.x, size.y);

    // SurfaceContainer fill inside a 1px OutlineVariant frame at radius 12.
    // kDrawShadow stays false: the elevation is expressed by the frame alone.
    dc2.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant)));
    dc2.SetBrush(wxBrush(surface));
    dc2.DrawRoundedRectangle(0, 0, size.x - 1, size.y - 1, m_radius);
}

}} // namespace Slic3r::GUI

// ---------------------------------------------------------------------------
// Blocking entry points
// ---------------------------------------------------------------------------

namespace MD3 {

namespace {

int run_blocking(wxWindow *owner, wxMenu *menu, const wxRect &anchor, bool send_events)
{
    if (!owner || !menu)
        return wxID_NONE;

    wxMenuInvokingWindowSetter invoking(*menu, owner);

    // Every context menu offers "Edit appearance..." for the element it was
    // opened on: when the owner (or an ancestor) was adopted through
    // ElementStyle::apply and the menu does not already carry the item, it is
    // appended for this popup and removed again once the menu has closed.
    bool auto_edit_item = false;
    {
        const std::string element_id = Slic3r::GUI::ElementStyle::element_id_of(owner);
        if (!element_id.empty() && !menu->FindItem(Slic3r::GUI::AppearanceEditor::edit_appearance_item_id())) {
            Slic3r::GUI::AppearanceEditor::append_edit_appearance_item(*menu, element_id, owner);
            auto_edit_item = true;
        }
    }

    auto *popup = new Slic3r::GUI::MD3MenuPopup(owner, menu);
    popup->SetSendEvents(send_events);
    wxWeakRef<Slic3r::GUI::MD3MenuPopup> popup_ref(popup);

    wxGUIEventLoop loop;
    bool           closed  = false;
    bool           running = false;
    // Every close path (activate, Escape, outside click, app deactivation,
    // owner destruction) funnels through here; Exit() runs exactly once.
    auto close = [&]() {
        if (closed)
            return;
        closed = true;
        if (running)
            loop.Exit();
    };
    popup->SetCloseCallback(close);

    wxWeakRef<wxWindow> owner_ref(owner);
    auto on_destroy = [&](wxWindowDestroyEvent &e) {
        e.Skip();
        if (e.GetWindow() == owner)
            close();
    };
    owner->Bind(wxEVT_DESTROY, on_destroy);

    popup->PopupAt(anchor);

    if (!closed) {
        wxEventLoopActivator activate(&loop);
        running = true;
        loop.Run();
        running = false;
    }

    if (owner_ref)
        owner_ref->Unbind(wxEVT_DESTROY, on_destroy);

    int result = wxID_NONE;
    if (popup_ref) {
        result = popup_ref->Result();
        popup_ref->Destroy();
    }
    if (auto_edit_item)
        Slic3r::GUI::AppearanceEditor::remove_edit_appearance_item(*menu);
    return result;
}

wxRect point_anchor(wxPoint screen_pos)
{
    if (screen_pos == wxDefaultPosition)
        screen_pos = wxGetMousePosition();
    return wxRect(screen_pos, wxSize(1, 1));
}

} // namespace

bool PopupMenu(wxWindow *owner, wxMenu *menu, wxPoint screen_pos)
{
    if (!owner || !menu)
        return false;
    run_blocking(owner, menu, point_anchor(screen_pos), true);
    return true;
}

int PopupMenuSelection(wxWindow *owner, wxMenu &menu, wxPoint screen_pos)
{
    return run_blocking(owner, &menu, point_anchor(screen_pos), false);
}

bool PopupMenuBelow(wxWindow *anchor, wxMenu *menu)
{
    if (!anchor || !menu)
        return false;
    run_blocking(anchor, menu, anchor->GetScreenRect(), true);
    return true;
}

} // namespace MD3
