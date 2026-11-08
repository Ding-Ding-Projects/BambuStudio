#include "../PersonalVocabulary.hpp"
#include "MD3Menu.hpp"

#include "../BilingualRegistry.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../Appearance/AppearanceEditorPopover.hpp"
#include "../Appearance/ElementStyle.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "MD3Motion.hpp"
#include "MenuMotionPaint.hpp"
#include "MD3Tokens.hpp"
#include "SearchField.hpp"
#include "StateColor.hpp"

#include <wx/combobox.h>
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/display.h>
#include <wx/eventfilter.h>
#include <wx/evtloop.h>
#include <wx/settings.h>
#include <wx/textctrl.h>
#include <wx/textentry.h>
#include <wx/tooltip.h>
#include <wx/utils.h>
#include <wx/weakref.h>

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

// ---- live controls drawn inside a row (MD3MenuLive.hpp) -------------------

constexpr int kLiveSwitchW   = 36;
constexpr int kLiveSwitchH   = 20;
constexpr int kLiveSegPad    = 10;
constexpr int kLiveSegH      = 26;
constexpr int kLiveTrack     = 96;
constexpr int kLiveButton    = 24;
constexpr int kLiveGap       = 8;

wxString live_value_text(const MD3::Menu::LiveControl &c, double value)
{
    return wxString::FromUTF8(MD3::Menu::value_text(c, value, into_u8(_L("On")), into_u8(_L("Off"))));
}

// The widest value a slider or stepper can show, so the row does not jump.
int live_value_width(wxDC &dc, const MD3::Menu::LiveControl &c)
{
    dc.SetFont(caption_font());
    int w = 0;
    for (double v : {c.min, c.max, c.value()})
        w = std::max(w, dc.GetTextExtent(live_value_text(c, v)).x);
    return w;
}

int live_width(wxDC &dc, const wxWindow *w, const MD3::Menu::LiveControl &c)
{
    using K = MD3::Menu::LiveControl::Kind;
    switch (c.kind) {
    case K::Switch: return w->FromDIP(kLiveSwitchW);
    case K::Choice: {
        dc.SetFont(caption_font());
        int total = 0;
        for (const std::string &choice : c.choices)
            total += dc.GetTextExtent(wxString::FromUTF8(choice)).x + 2 * w->FromDIP(kLiveSegPad);
        return total;
    }
    case K::Slider: return w->FromDIP(kLiveTrack) + w->FromDIP(kLiveGap) + live_value_width(dc, c);
    case K::Stepper: return 2 * w->FromDIP(kLiveButton) + 2 * w->FromDIP(kLiveGap) + live_value_width(dc, c);
    }
    return 0;
}

// Where the parts of a live control sit in a row: for a choice one rectangle
// per segment, for a stepper the minus and plus buttons, for a slider the track.
struct LiveGeometry
{
    wxRect              area;
    std::vector<wxRect> parts;
    wxRect              track;
    wxRect              value;
};

LiveGeometry live_geometry(wxDC &dc, const wxWindow *w, const MD3::Menu::LiveControl &c, const wxRect &row, int pad)
{
    using K = MD3::Menu::LiveControl::Kind;
    LiveGeometry g;
    const int width = live_width(dc, w, c);
    const int right = row.GetRight() - pad + 1;
    g.area          = wxRect(right - width, row.y, width, row.height);
    switch (c.kind) {
    case K::Switch: {
        const int h = w->FromDIP(kLiveSwitchH);
        g.track     = wxRect(g.area.x, row.y + (row.height - h) / 2, width, h);
        break;
    }
    case K::Choice: {
        dc.SetFont(caption_font());
        const int h = w->FromDIP(kLiveSegH);
        int       x = g.area.x;
        for (const std::string &choice : c.choices) {
            const int seg = dc.GetTextExtent(wxString::FromUTF8(choice)).x + 2 * w->FromDIP(kLiveSegPad);
            g.parts.emplace_back(x, row.y + (row.height - h) / 2, seg, h);
            x += seg;
        }
        break;
    }
    case K::Slider: {
        const int track_w = w->FromDIP(kLiveTrack);
        const int h       = w->FromDIP(18);
        g.track           = wxRect(g.area.x, row.y + (row.height - h) / 2, track_w, h);
        g.value           = wxRect(g.track.GetRight() + 1 + w->FromDIP(kLiveGap), row.y, g.area.GetRight() - g.track.GetRight() - w->FromDIP(kLiveGap), row.height);
        break;
    }
    case K::Stepper: {
        const int b = w->FromDIP(kLiveButton);
        g.parts.emplace_back(g.area.x, row.y + (row.height - b) / 2, b, b);
        g.parts.emplace_back(g.area.GetRight() - b + 1, row.y + (row.height - b) / 2, b, b);
        g.value = wxRect(g.parts[0].GetRight() + 1, row.y, g.parts[1].x - g.parts[0].GetRight() - 1, row.height);
        break;
    }
    }
    return g;
}

// Paints the control and returns where it starts, so the label stops short of it.
int paint_live(wxDC &dc, const wxWindow *w, const MD3::Menu::LiveControl &c, const wxRect &row, int pad, const wxColour &fg,
               const wxColour &fg_muted, const wxColour &under, bool enabled)
{
    using K             = MD3::Menu::LiveControl::Kind;
    const LiveGeometry g = live_geometry(dc, w, c, row, pad);
    const double       v = MD3::Menu::clamp_to(c, c.value());
    const auto tone = [&](MD3::Role role) {
        const wxColour col = StateColor::semantic(role);
        return enabled ? col : blend(col, under, 0.38);
    };
    switch (c.kind) {
    case K::Switch: {
        const bool on = v >= 0.5;
        const int  r  = g.track.height / 2;
        dc.SetPen(on ? *wxTRANSPARENT_PEN : wxPen(tone(MD3::Role::Outline), std::max(1, w->FromDIP(2))));
        dc.SetBrush(wxBrush(on ? tone(MD3::Role::Primary) : tone(MD3::Role::SurfaceContainerHighest)));
        dc.DrawRoundedRectangle(g.track, r);
        const int thumb = on ? g.track.height - w->FromDIP(6) : g.track.height - w->FromDIP(10);
        const int cx    = on ? g.track.GetRight() - r : g.track.x + r;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(on ? tone(MD3::Role::OnPrimary) : tone(MD3::Role::Outline)));
        dc.DrawCircle(cx, g.track.y + g.track.height / 2, thumb / 2);
        break;
    }
    case K::Choice: {
        dc.SetFont(caption_font());
        const int radius = g.parts.empty() ? 0 : g.parts.front().height / 2;
        if (!g.parts.empty()) {
            wxRect all = g.parts.front();
            all.SetRight(g.parts.back().GetRight());
            dc.SetPen(wxPen(tone(MD3::Role::Outline)));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRoundedRectangle(all, radius);
        }
        for (std::size_t i = 0; i < g.parts.size(); ++i) {
            const bool chosen = std::size_t(v) == i;
            if (chosen) {
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(tone(MD3::Role::SecondaryContainer)));
                dc.DrawRoundedRectangle(wxRect(g.parts[i]).Deflate(1), radius);
            }
            dc.SetTextForeground(chosen ? tone(MD3::Role::OnSecondaryContainer) : (enabled ? fg : fg_muted));
            dc.DrawLabel(wxString::FromUTF8(c.choices[i]), g.parts[i], wxALIGN_CENTER);
        }
        break;
    }
    case K::Slider: {
        const int    thick = std::max(2, w->FromDIP(4));
        const int    y     = g.track.y + g.track.height / 2;
        const double f     = MD3::Menu::fraction_of(c, v);
        const int    knob  = g.track.x + int(std::lround(f * g.track.width));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(tone(MD3::Role::SurfaceContainerHighest)));
        dc.DrawRoundedRectangle(g.track.x, y - thick / 2, g.track.width, thick, thick / 2);
        dc.SetBrush(wxBrush(tone(MD3::Role::Primary)));
        dc.DrawRoundedRectangle(g.track.x, y - thick / 2, std::max(thick, knob - g.track.x), thick, thick / 2);
        dc.DrawCircle(knob, y, w->FromDIP(7));
        dc.SetFont(caption_font());
        dc.SetTextForeground(enabled ? fg : fg_muted);
        dc.DrawLabel(live_value_text(c, v), g.value, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
        break;
    }
    case K::Stepper: {
        const bool can[2] = {v > std::min(c.min, c.max), v < std::max(c.min, c.max)};
        const wxString signs[2] = {wxString(wxUniChar(0x2212)), wxString("+")};
        dc.SetFont(body_s_font());
        for (int i = 0; i < 2; ++i) {
            const bool active = enabled && can[i];
            dc.SetPen(wxPen(active ? tone(MD3::Role::Outline) : blend(StateColor::semantic(MD3::Role::Outline), under, 0.38)));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRoundedRectangle(g.parts[i], g.parts[i].height / 2);
            dc.SetTextForeground(active ? fg : blend(fg, under, 0.38));
            dc.DrawLabel(signs[i], g.parts[i], wxALIGN_CENTER);
        }
        dc.SetFont(caption_font());
        dc.SetTextForeground(enabled ? fg : fg_muted);
        dc.DrawLabel(live_value_text(c, v), g.value, wxALIGN_CENTER);
        break;
    }
    }
    return g.area.x;
}

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

    // Rows with a live control change their value in place; the menu stays open.
    bool IsLive(int vis_index) const;
    bool AdjustLive(int vis_index, int direction); // Left and Right
    bool ToggleLive(int vis_index);                // Space, or a click on a switch

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
    void onLeftDown(wxMouseEvent &);
    void onWheel(wxMouseEvent &);
    // A click inside a live row: the control part under the pointer decides.
    void liveClick(int vis_index, const wxPoint &pt);
    void applyLive(int vis_index, double value);
    void dragSlider(const wxPoint &pt);
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

    MD3::Motion::Anim m_hover_motion;
    MD3::Motion::Anim m_filter_motion;
    std::vector<double> m_hover_weights;
    double m_filter_progress = 1.0;
    wxTimer m_hover_timer;
    int     m_pending_submenu { -1 };
    int     m_drag_slider { -1 }; // visible row whose slider is being dragged
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
            *name = m_list->Visible().empty() ? _L("No matches.") : _L("Menu");
            return wxACC_OK;
        }
        const MD3::Menu::Item *it = item(child_id);
        if (!it)
            return wxACC_FAIL;
        *name = PersonalVocabulary::display(it->label);
        if (!it->secondary.IsEmpty())
            *name << wxString::FromUTF8(" \xC2\xB7 ") << PersonalVocabulary::display(it->secondary);
        return wxACC_OK;
    }

    wxAccStatus GetDescription(int child_id, wxString *description) override
    {
        if (!description)
            return wxACC_FAIL;
        if (child_id == wxACC_SELF) {
            int results = 0;
            for (size_t row = 0; row < m_list->Visible().size(); ++row) {
                const auto *entry = m_list->ItemAt(static_cast<int>(row));
                if (entry && entry->actionable()) ++results;
            }
            *description = wxString::Format(_L("%d results"), results);
            return wxACC_OK;
        }
        const MD3::Menu::Item *it = item(child_id);
        if (!it)
            return wxACC_NOT_IMPLEMENTED;
        *description = it->help;
        if (it->live) {
            if (!description->IsEmpty())
                *description << ' ';
            *description << (it->live->kind == MD3::Menu::LiveControl::Kind::Switch
                                 ? _L("Space turns it on or off; the menu stays open.")
                                 : _L("Left and Right change it; the menu stays open."));
        }
        return wxACC_OK;
    }

    wxAccStatus GetValue(int child_id, wxString *value) override
    {
        if (!value)
            return wxACC_FAIL;
        const MD3::Menu::Item *it = item(child_id);
        if (!it || !it->live)
            return wxACC_NOT_IMPLEMENTED;
        *value = live_value_text(*it->live, it->live->value());
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
        if (it->live) {
            switch (it->live->kind) {
            case MD3::Menu::LiveControl::Kind::Switch: *role = wxROLE_SYSTEM_CHECKBUTTON; break;
            case MD3::Menu::LiveControl::Kind::Choice: *role = wxROLE_SYSTEM_COMBOBOX; break;
            case MD3::Menu::LiveControl::Kind::Slider: *role = wxROLE_SYSTEM_SLIDER; break;
            case MD3::Menu::LiveControl::Kind::Stepper: *role = wxROLE_SYSTEM_SPINBUTTON; break;
            }
        }
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
        if (it->live) {
            if (it->live->kind == MD3::Menu::LiveControl::Kind::Switch && it->live->value() >= 0.5)
                *state |= wxACC_STATE_SYSTEM_CHECKED;
        } else if ((it->kind == MD3::Menu::Item::Check || it->kind == MD3::Menu::Item::Radio) && it->checked)
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
        *action_name = it->live ? (it->live->kind == MD3::Menu::LiveControl::Kind::Switch ? _L("Toggle") : _L("Next value"))
                                : _L("Activate");
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
    Bind(wxEVT_LEFT_DOWN, &MD3MenuList::onLeftDown, this); // activation happens on release; a slider drags from here
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { m_drag_slider = -1; });
    Bind(wxEVT_MOUSEWHEEL, &MD3MenuList::onWheel, this);
    Bind(wxEVT_TIMER, &MD3MenuList::onHoverTimer, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent &) {});
#if wxUSE_ACCESSIBILITY
    new MD3MenuAccessible(this); // wxWindow owns the accessible object.
#endif
}

MD3MenuList::~MD3MenuList()
{
    if (HasCapture())
        ReleaseMouse();
    m_hover_motion.Stop();
    m_filter_motion.Stop();
    m_hover_timer.Stop();
}

void MD3MenuList::SetVisible(std::vector<int> visible)
{
    const bool changed = m_visible != visible;
    m_hover_motion.Stop();
    m_hover_weights.assign(visible.size(), 0.0);
    m_visible = std::move(visible);
    if (changed) {
        m_filter_motion.Play(MD3::Motion::short2, [this](double t) {
            m_filter_progress = t;
            Refresh(false);
        }, nullptr, &MD3::Motion::easeStandard, this);
    }
    m_secondary_inline.assign(m_visible.size(), false);
    m_hover    = -1;
    m_selected = -1;
    m_offset_y = 0;
    m_hover_timer.Stop();
    m_pending_submenu = -1;
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_REORDER, this, wxOBJID_CLIENT, wxACC_SELF);
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_DESCRIPTIONCHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
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
    if (m_visible.empty())
        return RowHeight();
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
        int label_w = dc.GetTextExtent(PersonalVocabulary::display(it.label)).x;
        if (!it.secondary.IsEmpty())
            label_w = dc.GetTextExtent(PersonalVocabulary::display(it.label) + wxString::FromUTF8(" \xC2\xB7 ") + PersonalVocabulary::display(it.secondary)).x;
        int trailing = 0;
        if (it.live) {
            trailing = live_width(dc, this, *it.live);
        } else if (it.kind == MD3::Menu::Item::Submenu) {
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
    if (it->live) {
        ToggleLive(m_selected);
        return;
    }
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
    const auto starts = m_hover_weights;
    m_hover = vis_index;
    m_hover_motion.Play(MD3::Motion::short2, [this, starts, vis_index](double t) {
        for (size_t i = 0; i < m_hover_weights.size(); ++i) {
            const double from = i < starts.size() ? starts[i] : 0.0;
            m_hover_weights[i] = from + ((int(i) == vis_index ? 1.0 : 0.0) - from) * t;
        }
        Refresh(false);
    }, nullptr, &MD3::Motion::easeStandard, this);
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
    if (m_drag_slider >= 0 && evt.LeftIsDown()) {
        dragSlider(evt.GetPosition());
        return;
    }
    setHover(HitTest(evt.GetPosition()));
}

bool MD3MenuList::IsLive(int vis_index) const
{
    const MD3::Menu::Item *it = ItemAt(vis_index);
    return it && it->live && selectable(vis_index);
}

void MD3MenuList::applyLive(int vis, double value)
{
    const MD3::Menu::Item *it = ItemAt(vis);
    if (!it || !it->live || !it->enabled || !it->live->set)
        return;
    const MD3::Menu::LiveControl &c = *it->live;
    const double next = MD3::Menu::clamp_to(c, value);
    if (c.get && MD3::Menu::clamp_to(c, c.value()) == next)
        return;
    c.set(next);
    if (c.kind == MD3::Menu::LiveControl::Kind::Switch && it->source && it->source->IsCheckable())
        it->source->Check(next >= 0.5);
    Refresh();
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(c.kind == MD3::Menu::LiveControl::Kind::Switch ? wxACC_EVENT_OBJECT_STATECHANGE
                                                                            : wxACC_EVENT_OBJECT_VALUECHANGE,
                              this, wxOBJID_CLIENT, vis + 1);
#endif
}

bool MD3MenuList::AdjustLive(int vis, int direction)
{
    if (!IsLive(vis))
        return false;
    const MD3::Menu::LiveControl &c = *ItemAt(vis)->live;
    if (c.kind == MD3::Menu::LiveControl::Kind::Switch)
        applyLive(vis, direction > 0 ? 1.0 : 0.0); // Right turns it on, Left off
    else
        applyLive(vis, MD3::Menu::nudge(c, c.value(), direction));
    return true;
}

bool MD3MenuList::ToggleLive(int vis)
{
    if (!IsLive(vis))
        return false;
    const MD3::Menu::LiveControl &c = *ItemAt(vis)->live;
    // A choice moves on and wraps, so Space visits every option; a slider or
    // stepper moves one step up.
    double next = MD3::Menu::nudge(c, c.value(), +1);
    if (c.kind == MD3::Menu::LiveControl::Kind::Choice && next == MD3::Menu::clamp_to(c, c.value()))
        next = 0;
    applyLive(vis, next);
    return true;
}

void MD3MenuList::liveClick(int vis, const wxPoint &pt)
{
    const MD3::Menu::Item *it = ItemAt(vis);
    if (!it || !it->live || !it->enabled)
        return;
    const MD3::Menu::LiveControl &c = *it->live;
    wxClientDC          dc(this);
    const LiveGeometry  g = live_geometry(dc, this, c, RowRect(vis), FromDIP(MD3::Metrics::active().padding));
    using K = MD3::Menu::LiveControl::Kind;
    switch (c.kind) {
    case K::Switch: ToggleLive(vis); break; // the label and the switch both flip it
    case K::Choice:
        for (std::size_t i = 0; i < g.parts.size(); ++i)
            if (g.parts[i].Contains(pt)) {
                applyLive(vis, double(i));
                return;
            }
        ToggleLive(vis); // the label moves to the next choice
        break;
    case K::Stepper:
        if (g.parts.size() == 2 && g.parts[0].Contains(pt)) AdjustLive(vis, -1);
        else if (g.parts.size() == 2 && g.parts[1].Contains(pt)) AdjustLive(vis, +1);
        break;
    case K::Slider: break; // handled from the press so it can drag
    }
}

void MD3MenuList::dragSlider(const wxPoint &pt)
{
    const MD3::Menu::Item *it = ItemAt(m_drag_slider);
    if (!it || !it->live)
        return;
    wxClientDC         dc(this);
    const LiveGeometry g = live_geometry(dc, this, *it->live, RowRect(m_drag_slider), FromDIP(MD3::Metrics::active().padding));
    if (g.track.width <= 0)
        return;
    applyLive(m_drag_slider, MD3::Menu::value_at(*it->live, double(pt.x - g.track.x) / double(g.track.width)));
}

void MD3MenuList::onLeftDown(wxMouseEvent &evt)
{
    const int vis = HitTest(evt.GetPosition());
    if (!IsLive(vis) || ItemAt(vis)->live->kind != MD3::Menu::LiveControl::Kind::Slider)
        return;
    wxClientDC         dc(this);
    const LiveGeometry g = live_geometry(dc, this, *ItemAt(vis)->live, RowRect(vis), FromDIP(MD3::Metrics::active().padding));
    wxRect             grab = g.track;
    grab.Inflate(FromDIP(8), 0);
    if (!grab.Contains(evt.GetPosition()))
        return;
    Select(vis, false);
    m_drag_slider = vis;
    if (!HasCapture())
        CaptureMouse();
    dragSlider(evt.GetPosition());
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
    if (m_drag_slider >= 0) {
        m_drag_slider = -1;
        if (HasCapture())
            ReleaseMouse();
        return;
    }
    const int vis = HitTest(evt.GetPosition());
    if (!selectable(vis))
        return;
    const MD3::Menu::Item *it = ItemAt(vis);
    if (it->live) {
        Select(vis, false);
        liveClick(vis, evt.GetPosition());
        return;
    }
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

    if (m_visible.empty()) {
        dc.SetFont(body_s_font());
        dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        dc.DrawLabel(_L("No matches."), GetClientRect().Deflate(FromDIP(12), 0), wxALIGN_CENTER);
        return;
    }

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
    // Inset only the state paint. RowRect remains the full accessible hit target.
    wxRect state_rect = r;
    state_rect.Deflate(FromDIP(4), FromDIP(2));
    const int state_radius = FromDIP(MD3::Metrics::active().small_radius);

    const wxColour on_surface = ElementStyle::colour_for("menu.item", StyleProp::foreground,
                                                         StateColor::semantic(MD3::Role::OnSurface));
    wxColour fg        = on_surface;
    wxColour fg_muted  = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    if (selected) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SecondaryContainer)));
        dc.DrawRoundedRectangle(state_rect, state_radius);
        fg = fg_muted = StateColor::semantic(MD3::Role::OnSecondaryContainer);
        // Retain a non-colour selection cue without reducing label space.
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary)));
        const int marker = std::max(1, FromDIP(2));
        dc.DrawRoundedRectangle(state_rect.x + FromDIP(2), r.y + FromDIP(8),
            marker, std::max(0, r.height - FromDIP(16)), marker);
    } else if (it->enabled && (hovered || (vis < int(m_hover_weights.size()) && m_hover_weights[vis] > 0.0))) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(blend(StateColor::semantic(MD3::Role::SurfaceContainerHigh), surface,
            MD3::Motion::reduced() ? (hovered ? 1.0 : 0.0) : (vis < int(m_hover_weights.size()) ? m_hover_weights[vis] : 0.0))));
        dc.DrawRoundedRectangle(state_rect, state_radius);
    }
    if (!it->enabled) {
        const wxColour under = selected ? StateColor::semantic(MD3::Role::SecondaryContainer) : surface;
        fg       = blend(fg, under, 0.38);
        fg_muted = blend(fg_muted, under, 0.38);
    }

    const auto filter_paint = MD3::Motion::menu_filter_paint(fg, fg_muted, m_filter_progress, MD3::Motion::reduced());
    fg = filter_paint.foreground;
    fg_muted = filter_paint.secondary;
    if (filter_paint.edge_opacity > 0.0) {
        const wxColour under = selected ? StateColor::semantic(MD3::Role::SecondaryContainer) : surface;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(blend(StateColor::semantic(MD3::Role::Primary), under, filter_paint.edge_opacity)));
        dc.DrawRectangle(r.x, r.y, std::max(1, FromDIP(2)), r.height);
    }
    // Leading slot: bitmap, check mark or radio glyph.
    const int    slot = FromDIP(kLeadingSlot);
    const wxRect slot_rect(r.x + pad, r.y + (r.height - slot) / 2, slot, slot);
    const int    glyph = FromDIP(kGlyphPx);
    switch (it->kind) {
    case MD3::Menu::Item::Check:
        if (it->checked && !it->live)
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
    if (it->live) {
        trailing_left = paint_live(dc, this, *it->live, r, pad, fg, fg_muted,
                                   selected ? StateColor::semantic(MD3::Role::SecondaryContainer) : surface, it->enabled);
    } else if (it->kind == MD3::Menu::Item::Submenu) {
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
    wxString text = PersonalVocabulary::display(it->label);
    bool     inline_secondary = false;
    if (!it->secondary.IsEmpty()) {
        const wxString combined = PersonalVocabulary::display(it->label) + wxString::FromUTF8(" \xC2\xB7 ") + PersonalVocabulary::display(it->secondary);
        if (dc.GetTextExtent(combined).x <= avail) {
            text             = combined;
            inline_secondary = true;
        }
    }
    if (vis < static_cast<int>(m_secondary_inline.size()))
        m_secondary_inline[vis] = inline_secondary;
    wxSize ext = dc.GetTextExtent(text);
    if (avail <= 0)
        return;
    wxDCClipper label_clip(dc, wxRect(label_x, r.y, avail, r.height));
    if (ext.x > avail) {
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

MD3MenuPopup::MD3MenuPopup(wxWindow *owner, wxMenu *menu, MD3MenuPopup *parent_popup, bool show_search)
    : PopupWindow(parent_popup ? static_cast<wxWindow *>(parent_popup) : owner,
                  wxBORDER_NONE | wxPU_CONTAINS_CONTROLS)
    , m_owner(owner)
    , m_menu(menu)
    , m_parent(parent_popup)
    , m_show_search(show_search)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    m_radius = FromDIP(MD3::Metrics::active().radius);
    Bind(wxEVT_PAINT, &MD3MenuPopup::paintEvent, this);
    Bind(wxEVT_CHAR_HOOK, &MD3MenuPopup::onCharHook, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent &) {});

    if (I18N::language_mode_profile().is_bilingual()) {
        m_secondary = [](const wxString &label) {
            // The registry knows the Cantonese of the label exactly as shown
            // (ink wording, mnemonics, accelerators, formatted numbers); the
            // catalogue lookup by msgid covers labels built another way.
            wxString found = I18N::BilingualRegistry::instance().lookup(label);
            if (found.empty())
                found = I18N::language_mode_service().translate(label).secondary;
            return found;
        };
    }
}

MD3MenuPopup::~MD3MenuPopup()
{
    m_entrance.Stop();
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
    for (const auto &row : m_rows)
        if (row.source)
            PersonalVocabulary::remember_menu_label(row.source->GetItemLabel(), row.label);

    // Every menu, including one-item and nested menus, owns its query and builder.
    m_search = new SearchField(this, _L("Search menu"));
    m_search->SetOnQuery([this](const wxString &) { ApplyFilter(); });
    m_search->SetOnRegexToggle([this](bool) { ApplyFilter(); });
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
    m_entrance.Show(this, MD3::Motion::short2);
    if (target)
        target->SetFocus();
}

void MD3MenuPopup::Dismiss()
{
    // A parent stays up while its submenu is showing (DropDown pattern): the
    // focus loss caused by the child opening must not close us.
    if (IsSubmenuShown() || (m_search && m_search->IsBuilderShown()))
        return;
    m_entrance.Stop();
    PopupWindow::Dismiss();
}

void MD3MenuPopup::OnDismiss()
{
    if (IsSubmenuShown() || (m_search && m_search->IsBuilderShown()))
        return;
    if (m_closed)
        return;
    m_entrance.Stop();
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
    wxWeakRef<MD3MenuPopup> self(this);
    if (m_invoker && m_invoker->IsShown() && m_invoker->IsEnabled())
        m_invoker->SetFocus();
    if (self)
        self->m_restoring_focus = false;
}

void MD3MenuPopup::finalizeClose()
{
    if (m_finalized)
        return;
    m_finalized = true;
    // Focus and platform dismissal may synchronously destroy the surface.
    // Capture all delivery state before either path invokes external handlers.
    const int result = m_result;
    const int checked = m_result_kind_checkable ? (m_result_checked ? 1 : 0) : -1;
    const bool send = m_send_events;
    wxWeakRef<wxMenu> source = m_result_menu;
    auto close = std::move(m_close_cb);
    wxWeakRef<MD3MenuPopup> self(this);
    wxGetApp().set_side_menu_popup_status(false);
    // Restore focus while the transient window is still alive; the base
    // implementation can destroy it synchronously on some backends.
    restoreInvokerFocus();
    if (self)
        self->PopupWindow::OnDismiss();
    if (self)
        self->restoreInvokerFocus();
    if (close) close();
    if (result != wxID_NONE && send && source)
        source->SendEvent(result, checked);
    // No member access after command delivery.
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

void MD3MenuPopup::CloseChain()
{
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
    if (!m_list || (m_search && m_search->IsBuilderShown())) {
        evt.Skip();
        return;
    }
    wxWindow *focused = wxWindow::FindFocus();
    const bool search_focused = m_search && m_search->GetTextCtrl() &&
                                focused == m_search->GetTextCtrl();
    const bool search_control_focused = m_search && focused &&
                                        (focused == m_search || m_search->IsDescendant(focused));
    const bool search_has_text = m_search && !m_search->GetValue().IsEmpty();

    switch (evt.GetKeyCode()) {
    case WXK_ESCAPE:
        if (search_has_text) m_search->Clear();
        else RequestEscape();
        return;
    case WXK_DOWN: m_list->MoveSelection(1); return;
    case WXK_UP: m_list->MoveSelection(-1); return;
    case WXK_TAB:
        if (m_search && m_search->GetTextCtrl()) {
            // Include the search field's visible regex, builder and clear
            // controls. Skipping them would strand keyboard users at the query.
            std::vector<wxWindow *> stops{m_search->GetTextCtrl()};
            for (wxWindow *child : m_search->GetChildren()) {
                if (child != m_search->GetTextCtrl() && child->AcceptsFocusFromKeyboard())
                    stops.push_back(child);
            }
            stops.push_back(m_list);
            const auto current = std::find(stops.begin(), stops.end(), focused);
            const std::size_t index = current == stops.end() ? stops.size() - 1 :
                                      static_cast<std::size_t>(std::distance(stops.begin(), current));
            const std::size_t next = evt.ShiftDown() ? (index + stops.size() - 1) % stops.size() :
                                                         (index + 1) % stops.size();
            stops[next]->SetFocus();
            return;
        }
        evt.Skip();
        return;
    case WXK_HOME:
        if (search_control_focused && !search_focused) break;
        if (search_focused && search_has_text) break;
        m_list->SelectFirst();
        return;
    case WXK_END:
        if (search_control_focused && !search_focused) break;
        if (search_focused && search_has_text) break;
        m_list->SelectLast();
        return;
    case WXK_PAGEDOWN: m_list->PageMove(1); return;
    case WXK_PAGEUP: m_list->PageMove(-1); return;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
        if (search_control_focused && !search_focused) break;
        if (m_list->IsLive(m_list->Selected())) {
            // A live value is applied as it changes: Enter flips a switch, then closes.
            const MD3::Menu::Item *it = m_list->ItemAt(m_list->Selected());
            if (it && it->live->kind == MD3::Menu::LiveControl::Kind::Switch)
                m_list->ToggleLive(m_list->Selected());
            CloseChain();
            return;
        }
        m_list->ActivateSelected(); return;
    case WXK_SPACE:
        if (search_control_focused) break; // text entry or a focused search action
        m_list->ActivateSelected();
        return;
    case WXK_RIGHT:
        if (search_control_focused) break;
        if (m_list->AdjustLive(m_list->Selected(), +1)) return;
        if (m_list->OpenSelectedSubmenu(true)) return;
        return;
    case WXK_LEFT:
        if (search_control_focused) break;
        if (m_list->AdjustLive(m_list->Selected(), -1)) return;
        if (m_parent) { ReturnToParent(); return; }
        return;
    default: {
        // Mnemonics only while there is no query to type into.
        if (!search_control_focused && !search_has_text && !evt.HasAnyModifiers()) {
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

    // SurfaceContainer fill inside a density-shaped OutlineVariant frame.
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

int run_blocking(wxWindow *owner, wxMenu *menu, const wxRect &anchor, bool send_events,
                 bool show_search = false)
{
    if (!owner || !menu)
        return wxID_NONE;

    wxWeakRef<wxMenu> menu_ref(menu);
    wxWeakRef<wxWindow> previous_invoker(menu->GetInvokingWindow());
    wxWeakRef<wxWindow> dispatch_owner(wxGetTopLevelParent(owner));
    menu->SetInvokingWindow(dispatch_owner.get());

    // Every context menu offers "Edit appearance..." for the element it was
    // opened on: when the owner (or an ancestor) was adopted through
    // ElementStyle::apply and the menu does not already carry the item, it is
    // appended for this popup and removed again once the menu has closed.
    bool auto_edit_item = false;
    std::string appearance_element;
    wxWeakRef<wxWindow> appearance_anchor(owner);
    {
        const std::string element_id = Slic3r::GUI::ElementStyle::element_id_of(owner);
        if (!element_id.empty() && !menu->FindItem(Slic3r::GUI::AppearanceEditor::edit_appearance_item_id())) {
            Slic3r::GUI::AppearanceEditor::append_edit_appearance_item(*menu, element_id, owner);
            auto_edit_item = true;
            appearance_element = element_id;
        }
    }

    auto *popup = new Slic3r::GUI::MD3MenuPopup(owner, menu, nullptr, show_search);
    // Destructive handlers run only after this popup stack and loop unwind.
    popup->SetSendEvents(false);
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

    // Owner destruction can exit the loop while surfaces are still shown.
    if (popup_ref)
        popup_ref->RequestEscape();

    int result = wxID_NONE;
    bool checkable = false;
    bool checked = false;
    wxWeakRef<wxMenu> source;
    if (popup_ref) {
        result = popup_ref->Result();
        checkable = popup_ref->ResultCheckable();
        checked = popup_ref->ResultChecked();
        source = popup_ref->ResultMenu();
        popup_ref->SetCloseCallback({});
        popup_ref->Destroy();
    }
    if (auto_edit_item && menu_ref)
        Slic3r::GUI::AppearanceEditor::remove_edit_appearance_item(*menu_ref);
    if (send_events && result != wxID_NONE && dispatch_owner) {
        if (auto_edit_item && result == Slic3r::GUI::AppearanceEditor::edit_appearance_item_id()) {
            if (appearance_anchor)
                Slic3r::GUI::AppearanceEditor::open_for(appearance_anchor.get(), appearance_element);
        } else if (source && menu_ref) {
            wxWeakRef<wxWindow> previous_source_invoker(source->GetInvokingWindow());
            source->SetInvokingWindow(dispatch_owner.get());
            source->SendEvent(result, checkable ? (checked ? 1 : 0) : -1);
            if (source)
                source->SetInvokingWindow(previous_source_invoker.get());
        }
    }
    if (menu_ref)
        menu_ref->SetInvokingWindow(previous_invoker.get());
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

bool PopupMenuBelow(wxWindow *anchor, wxMenu *menu, bool show_search)
{
    if (!anchor || !menu)
        return false;
    run_blocking(anchor, menu, anchor->GetScreenRect(), true, show_search);
    return true;
}

// ---------------------------------------------------------------------------
// Text entry context menus
// ---------------------------------------------------------------------------

namespace {

// The text entry a context menu request came from, or null. A read-only combo
// box is a list rather than a field, so it keeps whatever its widget offers.
wxTextEntryBase *text_entry_of(wxWindow *window)
{
    if (window == nullptr)
        return nullptr;
    if (auto *combo = dynamic_cast<wxComboBox *>(window); combo != nullptr && combo->HasFlag(wxCB_READONLY))
        return nullptr;
    return dynamic_cast<wxTextEntryBase *>(window);
}

// A password field never offers its text for copying: wxTE_PASSWORD, or an
// EDIT control masked after creation with EM_SETPASSWORDCHAR (the Smart home
// token is masked that way).
bool is_masked(wxWindow *window)
{
    if (window->HasFlag(wxTE_PASSWORD))
        return true;
#ifdef __WIN32__
    if (HWND hwnd = static_cast<HWND>(window->GetHWND()))
        return ::SendMessage(hwnd, EM_GETPASSWORDCHAR, 0, 0) != 0;
#endif
    return false;
}

std::vector<MD3::TextMenuSection> &text_menu_sections()
{
    static std::vector<MD3::TextMenuSection> sections;
    return sections;
}

void show_text_menu(wxWindow *window, wxPoint screen_pos)
{
    wxTextEntryBase *entry = text_entry_of(window);
    if (entry == nullptr)
        return;
    const bool editable = entry->IsEditable();
    const bool masked   = is_masked(window);
    long       from = 0, to = 0;
    entry->GetSelection(&from, &to);

    wxMenu menu;
    menu.Append(wxID_UNDO, _L("Undo"))->Enable(editable && entry->CanUndo());
    menu.AppendSeparator();
    menu.Append(wxID_CUT, _L("Cut"))->Enable(editable && !masked && entry->CanCut());
    menu.Append(wxID_COPY, _L("Copy"))->Enable(!masked && entry->CanCopy());
    menu.Append(wxID_PASTE, _L("Paste"))->Enable(editable && entry->CanPaste());
    menu.Append(wxID_DELETE, _L("Delete"))->Enable(editable && from != to);
    menu.AppendSeparator();
    menu.Append(wxID_SELECTALL, _L("Select all"))->Enable(!entry->IsEmpty());
    menu.Bind(wxEVT_MENU, [entry](wxCommandEvent &) { entry->Undo(); }, wxID_UNDO);
    menu.Bind(wxEVT_MENU, [entry](wxCommandEvent &) { entry->Cut(); }, wxID_CUT);
    menu.Bind(wxEVT_MENU, [entry](wxCommandEvent &) { entry->Copy(); }, wxID_COPY);
    menu.Bind(wxEVT_MENU, [entry](wxCommandEvent &) { entry->Paste(); }, wxID_PASTE);
    menu.Bind(wxEVT_MENU, [entry, from, to](wxCommandEvent &) { entry->Remove(from, to); }, wxID_DELETE);
    menu.Bind(wxEVT_MENU, [entry](wxCommandEvent &) { entry->SelectAll(); }, wxID_SELECTALL);
    for (const MD3::TextMenuSection &section : text_menu_sections())
        if (section)
            section(window, menu);

    // The Menu key and Shift+F10 carry no position: open under the field.
    if (screen_pos == wxDefaultPosition)
        screen_pos = window->ClientToScreen(wxPoint(0, window->GetSize().GetHeight()));
    // Sent events, so the "Edit appearance..." item run_blocking() adds for a
    // styled field works here as in every other menu.
    MD3::PopupMenu(window, &menu, screen_pos);
}

class TextContextMenus : public wxEventFilter
{
public:
    TextContextMenus() { wxEvtHandler::AddFilter(this); }
    ~TextContextMenus() override { wxEvtHandler::RemoveFilter(this); }

    int FilterEvent(wxEvent &event) override
    {
        if (event.GetEventType() != wxEVT_CONTEXT_MENU)
            return Event_Skip;
        auto *window = dynamic_cast<wxWindow *>(event.GetEventObject());
        if (text_entry_of(window) == nullptr)
            return Event_Skip;
        // Shift+right-click on a styled field still opens its appearance editor.
        if (wxGetKeyState(WXK_SHIFT) && !Slic3r::GUI::ElementStyle::element_id_of(window).empty())
            return Event_Skip;
        // Handled here, so the native menu never opens. The Material menu runs its
        // own event loop, which starts after this message instead of inside it.
        const wxPoint       at = static_cast<wxContextMenuEvent &>(event).GetPosition();
        wxWeakRef<wxWindow> ref(window);
        window->CallAfter([ref, at]() {
            if (ref)
                show_text_menu(ref.get(), at);
        });
        return Event_Processed;
    }
};

std::unique_ptr<TextContextMenus> &text_context_menus()
{
    static std::unique_ptr<TextContextMenus> instance;
    return instance;
}

} // namespace

void EnableTextContextMenus(bool enable)
{
    std::unique_ptr<TextContextMenus> &instance = text_context_menus();
    if (!enable)
        instance.reset();
    else if (!instance)
        instance = std::make_unique<TextContextMenus>();
}

void AddTextMenuSection(TextMenuSection section)
{
    text_menu_sections().push_back(std::move(section));
}

namespace {

wxMenuItem *append_live(wxMenu *menu, const wxString &label, const wxString &help, MD3::Menu::LiveControl control)
{
    if (!menu)
        return nullptr;
    auto *item = new MD3::Menu::LiveMenuItem(menu, wxID_ANY, label, help, std::move(control));
    menu->Append(item);
    if (item->IsCheckable())
        item->Check(item->control().value() >= 0.5);
    // Outside the Material menu a switch is a check item: keep it working there too.
    if (item->control().kind == MD3::Menu::LiveControl::Kind::Switch)
        menu->Bind(wxEVT_MENU, [item](wxCommandEvent &e) {
            const MD3::Menu::LiveControl &c = item->control();
            if (c.set)
                c.set(e.IsChecked() ? 1.0 : 0.0);
        }, item->GetId());
    return item;
}

} // namespace

wxMenuItem *AppendSwitch(wxMenu *menu, const wxString &label, std::function<bool()> get, std::function<void(bool)> set, const wxString &help)
{
    MD3::Menu::LiveControl c;
    c.kind = MD3::Menu::LiveControl::Kind::Switch;
    c.get  = [get]() { return get && get() ? 1.0 : 0.0; };
    c.set  = [set](double v) { if (set) set(v >= 0.5); };
    return append_live(menu, label, help, std::move(c));
}

wxMenuItem *AppendChoice(wxMenu *menu, const wxString &label, const std::vector<wxString> &choices, std::function<int()> get,
                         std::function<void(int)> set, const wxString &help)
{
    MD3::Menu::LiveControl c;
    c.kind = MD3::Menu::LiveControl::Kind::Choice;
    for (const wxString &choice : choices)
        c.choices.push_back(Slic3r::GUI::into_u8(choice));
    c.get = [get]() { return get ? double(get()) : 0.0; };
    c.set = [set](double v) { if (set) set(int(std::lround(v))); };
    return append_live(menu, label, help, std::move(c));
}

wxMenuItem *AppendSlider(wxMenu *menu, const wxString &label, double min, double max, double step, const wxString &unit,
                         std::function<double()> get, std::function<void(double)> set, const wxString &help)
{
    MD3::Menu::LiveControl c;
    c.kind     = MD3::Menu::LiveControl::Kind::Slider;
    c.min      = min;
    c.max      = max;
    c.step     = step;
    c.decimals = MD3::Menu::decimals_for(step);
    c.unit     = Slic3r::GUI::into_u8(unit);
    c.get      = std::move(get);
    c.set      = std::move(set);
    return append_live(menu, label, help, std::move(c));
}

wxMenuItem *AppendStepper(wxMenu *menu, const wxString &label, double min, double max, double step, int decimals, const wxString &unit,
                          std::function<double()> get, std::function<void(double)> set, const wxString &help)
{
    MD3::Menu::LiveControl c;
    c.kind     = MD3::Menu::LiveControl::Kind::Stepper;
    c.min      = min;
    c.max      = max;
    c.step     = step;
    c.decimals = decimals;
    c.unit     = Slic3r::GUI::into_u8(unit);
    c.get      = std::move(get);
    c.set      = std::move(set);
    return append_live(menu, label, help, std::move(c));
}

} // namespace MD3
