#include "BilingualDecorator.hpp"

#include "BilingualRegistry.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/TextInput.hpp"

#include <wx/app.h>
#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/control.h>
#include <wx/dataview.h>
#include <wx/dc.h>
#include <wx/dcclient.h>
#include <wx/dialog.h>
#include <wx/display.h>
#include <wx/eventfilter.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textentry.h>
#include <wx/timer.h>
#include <wx/toplevel.h>
#include <wx/weakref.h>

#include <algorithm>
#include <deque>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Slic3r { namespace GUI { namespace I18N {

namespace {

// Same separators apply_localized_text() uses, so both paths read alike.
const wxString &inline_separator()
{
    static const wxString value = wxString::FromUTF8(" \xC2\xB7 ");
    return value;
}

// "廣東話：" in front of a Cantonese label that had to move into the tooltip.
const wxString &tooltip_prefix()
{
    static const wxString value = wxString::FromUTF8("\xE5\xBB\xA3\xE6\x9D\xB1\xE8\xA9\xB1\xEF\xBC\x9A");
    return value;
}

constexpr int TICK_MS         = 250;  // how often queued windows are decorated
constexpr int SWEEP_EVERY     = 12;   // ticks between passes over every shown window (~3 s)
constexpr int DIALOG_GROWTH_PCT = 40; // how much wider a dialog may grow for compact labels

enum class Kind { None, Text, NativeButton, KitButton, Check, Radio, GroupBox, Header };

// How a window's label changed: wider (compact) or taller (stacked).
enum class Change { None, Wider, Taller };

Kind kind_of(wxWindow *window)
{
    // Typed text and combo values are the user's data, never decorated.
    if (dynamic_cast<wxTextEntry *>(window) != nullptr || dynamic_cast<::TextInput *>(window) != nullptr)
        return Kind::None;
    if (dynamic_cast<::Button *>(window) != nullptr)
        return Kind::KitButton;
    // The upper-case micro-header over a section ("SETTINGS"): custom-drawn, one line.
    if (dynamic_cast<::SectionHeader *>(window) != nullptr)
        return Kind::Header;
    if (wxDynamicCast(window, wxStaticText) != nullptr)
        return Kind::Text;
    if (wxDynamicCast(window, wxCheckBox) != nullptr)
        return Kind::Check;
    if (wxDynamicCast(window, wxRadioButton) != nullptr)
        return Kind::Radio;
    if (wxDynamicCast(window, wxStaticBox) != nullptr)
        return Kind::GroupBox;
    if (wxDynamicCast(window, wxButton) != nullptr)
        return Kind::NativeButton;
    return Kind::None;
}

wxString label_of(wxWindow *window, Kind kind)
{
    if (kind == Kind::Text) {
        if (auto *label = dynamic_cast<::Label *>(window))
            return label->GetUnwrappedLabel();
        return static_cast<wxStaticText *>(window)->GetLabel();
    }
    return window->GetLabel();
}

// Native controls read '&' as a mnemonic marker; the kit Button and the
// section header draw it.
wxString as_label_text(const wxString &text, Kind kind)
{
    if (kind == Kind::KitButton || kind == Kind::Header)
        return text;
    wxString escaped(text);
    escaped.Replace("&", "&&");
    return escaped;
}

bool wraps(wxWindow *window, Kind kind, const wxString &english)
{
    if (english.Contains('\n'))
        return true;
    if (kind != Kind::Text)
        return false;
    const auto *label = dynamic_cast<::Label *>(window);
    return label != nullptr && (label->GetWindowStyle() & LB_AUTO_WRAP) != 0;
}

int text_width(wxWindow *window, const wxString &text)
{
    wxClientDC dc(window);
    dc.SetFont(window->GetFont());
    int width = 0;
    for (const wxString &line : wxSplit(wxControl::RemoveMnemonics(text), '\n', '\0'))
        width = std::max(width, dc.GetTextExtent(line).GetWidth());
    return width;
}

// "English · 廣東話" for a list or table column title, when the pair fits the column.
// A column header has no tooltip of its own, so a title that does not fit stays English.
bool bilingual_title(wxWindow *owner, const wxString &title, int width, wxString &decorated)
{
    if (title.empty() || width <= 0 || title.Contains(inline_separator()))
        return false;
    const wxString cantonese = BilingualRegistry::instance().lookup(title);
    if (cantonese.empty())
        return false;
    decorated = title + inline_separator() + cantonese;
    // The header spends part of the column on its margins and the sort arrow.
    return text_width(owner, decorated) + owner->FromDIP(24) <= width;
}

// Width the layout can give the window without squeezing a neighbour: its own
// row in a vertical sizer, or its own box plus the slack a horizontal row leaves.
int available_width(wxWindow *window, int growth)
{
    const int own   = window->GetSize().GetWidth();
    wxSizer  *sizer = window->GetContainingSizer();
    if (sizer == nullptr || sizer->GetSize().GetWidth() <= 0)
        return own;
    int border = 0;
    if (wxSizerItem *item = sizer->GetItem(window)) {
        if (item->GetFlag() & wxLEFT)
            border += item->GetBorder();
        if (item->GetFlag() & wxRIGHT)
            border += item->GetBorder();
    }
    const auto *box = dynamic_cast<wxBoxSizer *>(sizer);
    if (box != nullptr && box->GetOrientation() == wxVERTICAL)
        return sizer->GetSize().GetWidth() - border + growth;
    return own + std::max(0, sizer->GetSize().GetWidth() - sizer->GetMinSize().GetWidth()) + growth;
}

// Width actually visible from the window's left edge to the nearest client
// edge of its ancestors, up to the top-level window. A scrolled panel's sizer
// can be wider than the panel itself: text laid out past the panel's edge is
// cut, not scrolled into view, and the dialog never learns it needed room
// (Keyboard Shortcuts showed "Objects list · 物件清").
int visible_width(wxWindow *window)
{
    const int left    = window->GetScreenPosition().x;
    int       visible = std::numeric_limits<int>::max();
    for (wxWindow *parent = window->GetParent(); parent != nullptr; parent = parent->GetParent()) {
        const int right = parent->ClientToScreen(wxPoint(parent->GetClientSize().GetWidth(), 0)).x;
        visible = std::min(visible, right - left);
        if (parent->IsTopLevel())
            break;
    }
    return visible;
}

bool fits(wxWindow *window, Kind kind, const wxString &current, const wxString &candidate, int growth)
{
    // An ellipsizing label would cut the Cantonese off instead of growing.
    if (kind == Kind::Text && (window->GetWindowStyle() & wxST_ELLIPSIZE_MASK) != 0)
        return false;
    if (window->GetSize().GetWidth() <= 0)
        return false; // not laid out yet; the next pass decides
    // Buttons, check boxes and group boxes spend part of their width on chrome.
    const int chrome = std::max(0, window->GetBestSize().GetWidth() - text_width(window, current));
    int       room   = std::min(available_width(window, growth), visible_width(window) + growth);
    // A control created with an explicit size keeps it as its minimum, so the
    // sizer never widens it for a longer label: the compact text has to fit
    // the width it already has (Temperature calibration cut "開始溫度" to "開").
    // A kit Button is the exception: its minimum grows with its label, so
    // pinning it left every button English only (md3-v150).
    if (kind != Kind::KitButton && window->GetMinSize().GetWidth() > 0)
        room = std::min(room, window->GetSize().GetWidth());
    return text_width(window, candidate) + chrome <= room;
}

struct Applied
{
    wxString english;       // label the application set
    wxString shown;         // label on screen now
    wxString base_tooltip;  // tooltip the application set
    wxString shown_tooltip; // tooltip on screen now
    wxSize   size;          // window size the decision was made for
};

class Decorator;

class DecoratorTimer : public wxTimer
{
public:
    explicit DecoratorTimer(Decorator &owner) : m_owner(owner) {}
    void Notify() override;

private:
    Decorator &m_owner;
};

class Decorator : public wxEventFilter
{
public:
    Decorator() : m_timer(*this) {}
    ~Decorator() override { stop(); }

    void start()
    {
        if (m_running)
            return;
        m_running = true;
        wxEvtHandler::AddFilter(this);
        m_timer.Start(TICK_MS);
        queue_shown_top_levels();
    }

    void stop()
    {
        if (!m_running)
            return;
        m_running = false;
        m_timer.Stop();
        wxEvtHandler::RemoveFilter(this);
    }

    int FilterEvent(wxEvent &event) override
    {
        const wxEventType type = event.GetEventType();
        if (type == wxEVT_SHOW) {
            if (static_cast<wxShowEvent &>(event).IsShown())
                if (auto *window = wxDynamicCast(event.GetEventObject(), wxWindow))
                    schedule(window);
        } else if (type == wxEVT_MENU_OPEN) {
            if (wxMenu *menu = static_cast<wxMenuEvent &>(event).GetMenu())
                decorate_menu(*menu);
        } else if (type == wxEVT_DESTROY) {
            if (auto *window = wxDynamicCast(event.GetEventObject(), wxWindow))
                forget(window);
        }
        return Event_Skip;
    }

    void schedule(wxWindow *window)
    {
        wxWindow *top = window == nullptr ? nullptr : wxGetTopLevelParent(window);
        if (top == nullptr || top->IsBeingDeleted())
            return;
        for (const wxWeakRef<wxWindow> &queued : m_queue)
            if (queued.get() == top)
                return;
        m_queue.emplace_back(top);
    }

    void tick()
    {
        if (++m_ticks >= SWEEP_EVERY) {
            m_ticks = 0;
            queue_shown_top_levels();
        }
        // A few windows per tick keeps each pass short; the queue drains in turn.
        for (int budget = 4; budget > 0 && !m_queue.empty(); --budget) {
            wxWindow *top = m_queue.front().get();
            m_queue.pop_front();
            if (top != nullptr)
                decorate_top(top, true);
        }
    }

private:
    void queue_shown_top_levels()
    {
        for (wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext())
            if (wxWindow *top = node->GetData(); top != nullptr && top->IsShown())
                schedule(top);
    }

    void forget(wxWindow *window)
    {
        m_applied.erase(window);
        m_no_compact.erase(window);
        m_compact_refused.erase(window);
        BilingualRegistry::instance().set_managed(window, false);
    }

    void decorate_top(wxWindow *top, bool allow_retry)
    {
        if (top->IsBeingDeleted() || !top->IsShown())
            return;
        const bool dialog        = wxDynamicCast(top, wxDialog) != nullptr;
        const bool allow_compact = m_no_compact.count(top) == 0;
        int        growth        = 0;
        if (dialog && allow_compact) {
            const int display = wxDisplay::GetFromWindow(top);
            const wxRect area = wxDisplay(display == wxNOT_FOUND ? 0u : static_cast<unsigned>(display)).GetClientArea();
            growth = std::max(0, std::min(area.GetWidth() - top->GetSize().GetWidth(),
                                          top->GetSize().GetWidth() * DIALOG_GROWTH_PCT / 100));
        }
        bool                           taller = false;
        std::unordered_set<wxWindow *> parents;
        walk(top, [&](wxWindow *window) {
            const Change change = decorate_window(window, allow_compact, growth);
            if (change == Change::None)
                return;
            taller |= change == Change::Taller;
            if (wxWindow *parent = window->GetParent())
                parents.insert(parent);
        });
        if (parents.empty())
            return;
        if (dialog) {
            settle(top, allow_retry);
        } else {
            // A frame re-lays out only what changed: compact labels were only
            // chosen where their row had room, so their parent is enough.
            for (wxWindow *parent : parents)
                parent->Layout();
            if (taller)
                top->Layout();
        }
        if (recheck_compact(top))
            top->Layout();
    }

    // The fit counts on the dialog growing for a compact label, but a fixed-width
    // panel or a scrolling page does not grow with it (md3-v150's Keyboard Shortcuts
    // drew "Objects list · 物件清" and an import description running off the
    // dialog). Once the layout has settled, a compact label cut short in its own box
    // or reaching past what its parents show goes back to English with the Cantonese
    // in its tooltip, and stays that way.
    bool recheck_compact(wxWindow *top)
    {
        std::vector<wxWindow *> cut;
        for (const auto &[window, applied] : m_applied) {
            if (window == nullptr || window->IsBeingDeleted() || wxGetTopLevelParent(window) != top)
                continue;
            if (applied.shown == applied.english || applied.shown.Contains('\n') || !window->IsShownOnScreen())
                continue;
            const Kind kind     = kind_of(window);
            const int  width    = window->GetSize().GetWidth();
            const bool squeezed = kind == Kind::Text ? text_width(window, applied.shown) > width
                                                     : width < window->GetBestSize().GetWidth();
            if (squeezed || width > visible_width(window))
                cut.push_back(window);
        }
        BilingualRegistry &registry = BilingualRegistry::instance();
        for (wxWindow *window : cut) {
            Applied &applied = m_applied[window];
            const Kind kind = kind_of(window);
            m_compact_refused.insert(window);
            window->SetLabel(applied.english);
            wxString tooltip = applied.base_tooltip;
            if (!tooltip.empty()) {
                const wxString cantonese = registry.lookup(tooltip);
                if (!cantonese.empty())
                    tooltip += "\n" + cantonese;
            }
            const wxString cantonese = registry.lookup(applied.english);
            if (!cantonese.empty()) {
                const wxString note = tooltip_prefix() + cantonese;
                tooltip = tooltip.empty() ? note : tooltip + "\n\n" + note;
            }
            if (tooltip.empty())
                window->UnsetToolTip();
            else
                window->SetToolTip(tooltip);
            applied.shown         = label_of(window, kind);
            applied.shown_tooltip = window->GetToolTipText();
            applied.size          = window->GetSize();
        }
        return !cut.empty();
    }

    template<class Visit> void walk(wxWindow *window, Visit &&visit)
    {
        if (window->IsBeingDeleted() || BilingualRegistry::instance().is_managed(window))
            return;
        visit(window);
        for (wxWindow *child : window->GetChildren())
            if (child != nullptr && !child->IsTopLevel() && child->IsShown())
                walk(child, visit);
    }

    // Lays a dialog out again after labels changed. It may grow inside the
    // screen for its new labels; if its content still does not fit, its compact
    // labels go back to English with the Cantonese in the tooltip.
    void settle(wxWindow *top, bool allow_retry)
    {
        top->Layout();
        if (top->GetSizer() == nullptr)
            return;
        const wxSize need = top->ClientToWindowSize(top->GetSizer()->GetMinSize());
        const wxSize now  = top->GetSize();
        if (need.GetWidth() <= now.GetWidth() && need.GetHeight() <= now.GetHeight())
            return;
        const int    display = wxDisplay::GetFromWindow(top);
        const wxRect area    = wxDisplay(display == wxNOT_FOUND ? 0u : static_cast<unsigned>(display)).GetClientArea();
        const wxSize grown(std::min(std::max(need.GetWidth(), now.GetWidth()), area.GetWidth()),
                           std::min(std::max(need.GetHeight(), now.GetHeight()), area.GetHeight()));
        if (grown != now) {
            top->SetSize(grown);
            top->Layout();
        }
        if (need.GetWidth() > area.GetWidth() && allow_retry && m_no_compact.insert(top).second)
            decorate_top(top, false);
    }

    // Column titles of data views and report lists are not windows of their
    // own, so the label pass below never reaches them.
    void decorate_columns(wxWindow *window)
    {
        if (auto *view = dynamic_cast<wxDataViewCtrl *>(window)) {
            for (unsigned int i = 0; i < view->GetColumnCount(); ++i) {
                wxDataViewColumn *column = view->GetColumn(i);
                // Icon columns have no title, and an auto-sized column measures its
                // rows to report a width, so only a title that could change asks for it.
                if (column == nullptr || column->GetTitle().empty() || column->GetTitle().Contains(inline_separator()))
                    continue;
                wxString decorated;
                if (bilingual_title(view, column->GetTitle(), column->GetWidth(), decorated))
                    column->SetTitle(decorated);
            }
        } else if (auto *list = dynamic_cast<wxListCtrl *>(window)) {
            if (!list->InReportView())
                return;
            for (int i = 0; i < list->GetColumnCount(); ++i) {
                wxListItem item;
                item.SetMask(wxLIST_MASK_TEXT);
                wxString decorated;
                if (list->GetColumn(i, item) && bilingual_title(list, item.GetText(), list->GetColumnWidth(i), decorated)) {
                    item.SetText(decorated);
                    list->SetColumn(i, item);
                }
            }
        }
    }

    // A placeholder hint is catalogue text, not the user's: it reads "English ·
    // 廣東話" when the pair fits the field, otherwise it stays English (a hint
    // has no tooltip). What the user typed is never read or changed.
    void decorate_hint(wxWindow *window)
    {
        auto *entry = dynamic_cast<wxTextEntry *>(window);
        if (entry == nullptr)
            return;
        const wxString hint = entry->GetHint();
        if (hint.empty() || hint.Contains(inline_separator()))
            return;
        const wxString cantonese = BilingualRegistry::instance().lookup(hint);
        if (cantonese.empty())
            return;
        const wxString decorated = hint + inline_separator() + cantonese;
        if (text_width(window, decorated) + window->FromDIP(12) <= window->GetClientSize().GetWidth())
            entry->SetHint(decorated);
    }

    Change decorate_window(wxWindow *window, bool allow_compact, int growth)
    {
        decorate_columns(window);
        decorate_hint(window);
        const Kind     kind  = kind_of(window);
        const wxString label = kind == Kind::None ? wxString() : label_of(window, kind);
        const wxString tip   = window->GetToolTipText();

        const auto previous = m_applied.find(window);
        if (label.empty() && tip.empty()) {
            if (previous != m_applied.end())
                m_applied.erase(previous);
            return Change::None;
        }
        // Unchanged since the last decision: nothing to do. (A pass that no
        // longer allows compact labels always decides again.)
        if (allow_compact && previous != m_applied.end() && label == previous->second.shown &&
            tip == previous->second.shown_tooltip && window->GetSize() == previous->second.size)
            return Change::None;

        // What the application itself set. Our own text is never read back as
        // English, so decorations never pile up.
        Applied next;
        next.english      = label;
        next.base_tooltip = tip;
        if (previous != m_applied.end()) {
            if (label == previous->second.shown)
                next.english = previous->second.english;
            if (tip == previous->second.shown_tooltip)
                next.base_tooltip = previous->second.base_tooltip;
        }
        next.shown = next.english;

        BilingualRegistry &registry = BilingualRegistry::instance();
        wxString           label_note; // Cantonese of a label that had to stay English
        if (kind != Kind::None && !next.english.empty()) {
            const wxString cantonese = registry.lookup(next.english);
            if (!cantonese.empty()) {
                const wxString second  = as_label_text(cantonese, kind);
                const wxString compact = next.english + inline_separator() + second;
                if (wraps(window, kind, next.english))
                    next.shown = next.english + "\n" + second;
                else if (allow_compact && m_compact_refused.count(window) == 0 && fits(window, kind, label, compact, growth))
                    next.shown = compact;
                else
                    label_note = tooltip_prefix() + cantonese;
            }
        }

        wxString tooltip = next.base_tooltip;
        if (!tooltip.empty()) {
            const wxString cantonese = registry.lookup(tooltip);
            if (!cantonese.empty())
                tooltip += "\n" + cantonese;
        }
        if (!label_note.empty())
            tooltip = tooltip.empty() ? label_note : tooltip + "\n\n" + label_note;

        Change change = Change::None;
        if (next.shown != label) {
            const int width = window->GetSize().GetWidth();
            window->SetLabel(next.shown);
            // A plain wxStaticText that was wrapped by its owner keeps its width.
            if (kind == Kind::Text && dynamic_cast<::Label *>(window) == nullptr && next.shown.Contains('\n') && width > 0)
                static_cast<wxStaticText *>(window)->Wrap(width);
            change     = next.shown.Freq('\n') > label.Freq('\n') ? Change::Taller : Change::Wider;
            next.shown = label_of(window, kind);
        }
        if (tooltip != tip) {
            if (tooltip.empty())
                window->UnsetToolTip();
            else
                window->SetToolTip(tooltip);
        }
        next.shown_tooltip = window->GetToolTipText();
        next.size          = window->GetSize();
        if (next.shown == next.english && next.shown_tooltip == next.base_tooltip)
            m_applied.erase(window);
        else
            m_applied[window] = next;
        return change;
    }

    // Native menus: "English · 廣東話" on every item, the mnemonic on the
    // English half and the accelerator once, at the end.
    void decorate_menu(wxMenu &menu)
    {
        BilingualRegistry &registry = BilingualRegistry::instance();
        for (wxMenuItemList::compatibility_iterator node = menu.GetMenuItems().GetFirst(); node; node = node->GetNext()) {
            wxMenuItem *item = node->GetData();
            if (item == nullptr || item->IsSeparator())
                continue;
            if (wxMenu *sub = item->GetSubMenu())
                decorate_menu(*sub);
            const wxString full        = item->GetItemLabel();
            const wxString text        = full.BeforeFirst('\t');
            const wxString accelerator = full.Contains('\t') ? full.AfterFirst('\t') : wxString();
            if (text.Contains(inline_separator()))
                continue; // decorated when the menu last opened
            const wxString cantonese = registry.lookup(text);
            if (cantonese.empty())
                continue;
            wxString decorated = text + inline_separator() + as_label_text(cantonese, Kind::NativeButton);
            if (!accelerator.empty())
                decorated += "\t" + accelerator;
            item->SetItemLabel(decorated);
        }
    }

    DecoratorTimer                           m_timer;
    bool                                     m_running = false;
    int                                      m_ticks   = 0;
    std::deque<wxWeakRef<wxWindow>>          m_queue;
    std::unordered_map<wxWindow *, Applied>  m_applied;
    std::unordered_set<wxWindow *>           m_no_compact;
    // Labels whose compact form the settled layout did not fully show (recheck_compact).
    std::unordered_set<wxWindow *>           m_compact_refused;
};

void DecoratorTimer::Notify() { m_owner.tick(); }

std::unique_ptr<Decorator> &decorator()
{
    static std::unique_ptr<Decorator> instance;
    return instance;
}

} // namespace

void enable_bilingual_decorator(bool enable)
{
    std::unique_ptr<Decorator> &instance = decorator();
    if (!enable) {
        instance.reset();
        return;
    }
    if (!instance)
        instance = std::make_unique<Decorator>();
    instance->start();
}

void refresh_bilingual_decoration(wxWindow *window)
{
    if (const std::unique_ptr<Decorator> &instance = decorator())
        instance->schedule(window);
}

wxString fit_bilingual(wxDC &dc, const wxString &english, int width, wxString *tooltip_note)
{
    if (tooltip_note != nullptr)
        tooltip_note->clear();
    const wxString cantonese = BilingualRegistry::instance().lookup(english);
    if (cantonese.empty())
        return english;
    const wxString compact = english + inline_separator() + cantonese;
    if (width > 0 && !english.Contains('\n') && dc.GetTextExtent(compact).GetWidth() <= width)
        return compact;
    if (tooltip_note != nullptr)
        *tooltip_note = tooltip_prefix() + cantonese;
    return english;
}

wxString bilingual_secondary(const wxString &english)
{
    return BilingualRegistry::instance().lookup(english);
}

}}} // namespace Slic3r::GUI::I18N
