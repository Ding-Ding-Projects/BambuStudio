#include "LockService.hpp"
#include "LockModel.hpp"

#include "../Appearance/ElementStyle.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../GUI_Utils.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/RoundedPopupWindow.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"
#include "../Widgets/TextInput.hpp"

#include <wx/datetime.h>
#include <wx/eventfilter.h>
#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <wx/weakref.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <sstream>

namespace Slic3r { namespace GUI { namespace ElementLocks {

namespace fs = std::filesystem;

namespace {

constexpr int kLockItemId = wxID_HIGHEST + 7311;

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

std::int64_t now_seconds() { return std::int64_t(std::time(nullptr)); }

struct State
{
    fs::path file;
    Store    store;
    bool     ready = false;
};

State &state()
{
    static State s;
    return s;
}

void save()
{
    State &s = state();
    if (s.file.empty()) return;
    std::error_code ec;
    fs::create_directories(s.file.parent_path(), ec);
    const fs::path partial = fs::path(s.file).concat(".partial");
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        out << s.store.save();
    }
    fs::rename(partial, s.file, ec);
    if (ec) {
        fs::copy_file(partial, s.file, fs::copy_options::overwrite_existing, ec);
        fs::remove(partial, ec);
    }
}

std::string random_salt()
{
    std::random_device rd;
    std::string        out;
    char               buf[3];
    for (int i = 0; i < 16; ++i) {
        std::snprintf(buf, sizeof(buf), "%02x", unsigned(rd() & 0xFF));
        out += buf;
    }
    return out;
}

wxString element_name(const std::string &element)
{
    const wxString name = ElementStyle::display_name_of(element);
    return name.IsEmpty() ? wxString::FromUTF8(element) : name;
}

wxString wait_text(std::int64_t seconds)
{
    if (seconds >= 120) return wxString::Format(_L("%d min"), int((seconds + 59) / 60));
    return wxString::Format(_L("%d s"), int(seconds));
}

Label *label(wxWindow *parent, const wxFont &font, const wxString &text, MD3::Role colour = MD3::Role::OnSurface, bool wrap = false)
{
    auto *l = new Label(parent, font, text, wrap ? (LB_AUTO_WRAP | wxST_NO_AUTORESIZE) : 0);
    if (wrap) l->SetMinSize(wxSize(parent->FromDIP(380), -1));
    l->SetBackgroundColour(parent->GetBackgroundColour());
    l->SetForegroundColour(role(colour));
    return l;
}

Button *button(wxWindow *parent, const wxString &text, Button::Variant variant)
{
    auto *b = new Button(parent, text);
    b->SetVariant(variant);
    b->SetButtonSize(Button::Size::Small);
    return b;
}

TextInput *field(wxWindow *parent, bool secret)
{
    auto *t = new TextInput(parent, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(parent->FromDIP(380), -1),
                            secret ? wxTE_PASSWORD : 0);
    return t;
}

class Dialog : public DPIDialog
{
public:
    Dialog(wxWindow *parent, const wxString &title) : DPIDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    {
        SetBackgroundColour(role(MD3::Role::Surface));
        m_root = new wxBoxSizer(wxVERTICAL);
        m_root->Add(new MD3DialogCaption(this, title), 0, wxEXPAND);
        m_body = new wxBoxSizer(wxVERTICAL);
        m_root->Add(m_body, 1, wxEXPAND | wxALL, FromDIP(20));
        m_buttons = new wxBoxSizer(wxHORIZONTAL);
        m_buttons->AddStretchSpacer(1);
        m_root->Add(m_buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    }
    wxBoxSizer *body() { return m_body; }
    wxBoxSizer *buttons() { return m_buttons; }
    void finish()
    {
        SetSizer(m_root);
        Fit();
        MD3DialogCaption::FinishChrome(this);
        CenterOnParent();
        wxGetApp().UpdateDlgDarkUI(this);
    }

protected:
    void        on_dpi_changed(const wxRect &) override { Layout(); }
    wxBoxSizer *m_root = nullptr, *m_body = nullptr, *m_buttons = nullptr;
};

// ---------------------------------------------------------------- lock

class LockDialog : public Dialog
{
public:
    LockDialog(wxWindow *parent, const std::string &element)
        : Dialog(parent, wxString::Format(_L("Lock %s"), element_name(element))), m_element(element)
    {
        m_body->Add(label(this, Label::Body_13,
                          wxString::Format(_L("%s stops answering clicks, the mouse wheel and keys until it is unlocked. Its right-click menu "
                                              "still opens, with Unlock."),
                                           element_name(element)),
                          MD3::Role::OnSurfaceVariant, true),
                    0, wxEXPAND | wxBOTTOM, FromDIP(14));
        m_body->Add(label(this, Label::Head_13, _L("Why (shown to whoever meets the lock, optional)")), 0, wxBOTTOM, FromDIP(4));
        m_reason = field(this, false);
        m_body->Add(m_reason, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
        m_body->Add(label(this, Label::Head_13, _L("Code, 4 to 32 characters")), 0, wxBOTTOM, FromDIP(4));
        m_code = field(this, true);
        m_body->Add(m_code, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
        m_body->Add(label(this, Label::Head_13, _L("The code again")), 0, wxBOTTOM, FromDIP(4));
        m_again = field(this, true);
        m_body->Add(m_again, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
        m_problem = label(this, Label::Body_13, wxString(), MD3::Role::Error, true);
        m_body->Add(m_problem, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
        m_body->Add(label(this, Label::Body_12,
                          _L("Forgot the code? The unlock ladder opens it after a wait: 1 minute, then typing its name, then 5 more minutes. "
                             "A lock guards against slips and impulses, not against someone who can edit your settings files."),
                          MD3::Role::OnSurfaceVariant, true),
                    0, wxEXPAND);
        auto *cancel = button(this, _L("Cancel"), Button::Variant::Outlined);
        cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        auto *lock = button(this, _L("Lock"), Button::Variant::Filled);
        lock->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { confirm(); });
        m_buttons->Add(cancel, 0, wxLEFT, FromDIP(8));
        m_buttons->Add(lock, 0, wxLEFT, FromDIP(8));
        finish();
        m_code->GetTextCtrl()->SetFocus();
    }

private:
    void confirm()
    {
        const std::string code  = into_u8(m_code->GetTextCtrl()->GetValue());
        const std::string again = into_u8(m_again->GetTextCtrl()->GetValue());
        std::string       problem;
        if (!valid_code(code, problem)) {
            m_problem->SetLabel(problem == "short"  ? _L("The code needs at least 4 characters.")
                                : problem == "long" ? _L("The code can have at most 32 characters.")
                                                    : _L("The code cannot be only spaces or contain a line break."));
            Layout();
            return;
        }
        if (code != again) {
            m_problem->SetLabel(_L("The two codes are different."));
            Layout();
            return;
        }
        const Lock lock = make_lock(m_element, into_u8(element_name(m_element)), into_u8(m_reason->GetTextCtrl()->GetValue()),
                                    into_u8(wxGetUserId()), code, now_seconds(), random_salt());
        state().store.put(lock);
        save();
        EndModal(wxID_OK);
    }

    std::string m_element;
    TextInput  *m_reason = nullptr, *m_code = nullptr, *m_again = nullptr;
    Label      *m_problem = nullptr;
};

// ---------------------------------------------------------------- unlock and the ladder

class UnlockDialog : public Dialog
{
public:
    UnlockDialog(wxWindow *parent, const std::string &element)
        : Dialog(parent, wxString::Format(_L("Unlock %s"), element_name(element))), m_element(element), m_timer(this)
    {
        const Lock *lock = state().store.find(element);
        wxString    who;
        if (lock) {
            const wxDateTime when(static_cast<time_t>(lock->created));
            who = wxString::Format(_L("Locked by %s on %s."), wxString::FromUTF8(lock->who), when.Format("%Y-%m-%d %H:%M"));
            if (!lock->reason.empty()) who << " " << wxString::Format(_L("Why: %s"), wxString::FromUTF8(lock->reason));
        }
        m_body->Add(label(this, Label::Body_13, who, MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND | wxBOTTOM, FromDIP(14));
        m_body->Add(label(this, Label::Head_13, _L("Code")), 0, wxBOTTOM, FromDIP(4));
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        m_code    = field(this, true);
        m_code->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { try_unlock(); });
        row->Add(m_code, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        m_unlock = button(this, _L("Unlock"), Button::Variant::Filled);
        m_unlock->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { try_unlock(); });
        row->Add(m_unlock, 0, wxALIGN_CENTER_VERTICAL);
        m_body->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
        m_status = label(this, Label::Body_13, wxString(), MD3::Role::Error, true);
        m_body->Add(m_status, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

        m_ladder_box = new StaticBox(this);
        m_ladder_box->SetCornerRadius(FromDIP(14));
        m_ladder_box->SetBorderWidth(0);
        m_ladder_box->SetBackgroundColor(role(MD3::Role::SurfaceContainerLow));
        m_ladder_box->SetBackgroundColour(role(MD3::Role::SurfaceContainerLow));
        auto *ls = new wxBoxSizer(wxVERTICAL);
        ls->Add(label(m_ladder_box, Label::Head_14, _L("Unlock ladder")), 0, wxBOTTOM, FromDIP(4));
        ls->Add(label(m_ladder_box, Label::Body_12,
                      _L("For a forgotten code: wait 1 minute, type the element's name, wait 5 more minutes, and the lock comes off. "
                         "You can close this window; the ladder keeps its place."),
                      MD3::Role::OnSurfaceVariant, true),
                0, wxEXPAND | wxBOTTOM, FromDIP(8));
        m_ladder_text = label(m_ladder_box, Label::Body_13, wxString(), MD3::Role::OnSurface, true);
        ls->Add(m_ladder_text, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
        m_name = field(m_ladder_box, false);
        m_name->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { ladder_next(); });
        ls->Add(m_name, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
        m_ladder = button(m_ladder_box, _L("Start the ladder"), Button::Variant::Tonal);
        m_ladder->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { ladder_next(); });
        ls->Add(m_ladder, 0);
        auto *lp = new wxBoxSizer(wxVERTICAL);
        lp->Add(ls, 1, wxEXPAND | wxALL, FromDIP(14));
        m_ladder_box->SetSizer(lp);
        m_body->Add(m_ladder_box, 0, wxEXPAND);

        m_forgot = button(this, _L("Forgot the code?"), Button::Variant::Text);
        m_forgot->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            m_show_ladder = true;
            refresh();
        });
        m_buttons->Insert(0, m_forgot, 0);
        auto *close = button(this, _L("Close"), Button::Variant::Outlined);
        close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        m_buttons->Add(close, 0, wxLEFT, FromDIP(8));

        Bind(wxEVT_TIMER, [this](wxTimerEvent &) { refresh(); }, m_timer.GetId());
        m_timer.Start(1000);
        if (lock && (lock->failures >= kLadderAfter || lock->ladder_started != 0)) m_show_ladder = true;
        refresh();
        finish();
        m_code->GetTextCtrl()->SetFocus();
    }
    ~UnlockDialog() override { m_timer.Stop(); }

private:
    void opened()
    {
        state().store.remove(m_element);
        save();
        m_timer.Stop();
        EndModal(wxID_OK);
    }

    void try_unlock()
    {
        Lock *lock = state().store.find(m_element);
        if (!lock) {
            EndModal(wxID_OK);
            return;
        }
        const Attempt a = try_code(*lock, into_u8(m_code->GetTextCtrl()->GetValue()), now_seconds());
        save();
        if (a == Attempt::Opened) {
            opened();
            return;
        }
        m_code->GetTextCtrl()->Clear();
        if (lock->failures >= kLadderAfter) m_show_ladder = true;
        refresh();
    }

    void ladder_next()
    {
        Lock *lock = state().store.find(m_element);
        if (!lock) return;
        std::int64_t left = 0;
        switch (ladder_state(*lock, now_seconds(), left)) {
        case LadderState::NotStarted: ladder_start(*lock, now_seconds()); break;
        case LadderState::Waiting: ladder_advance(*lock, now_seconds()); break;
        case LadderState::NeedsName:
            if (!ladder_advance(*lock, now_seconds(), into_u8(m_name->GetTextCtrl()->GetValue()))) m_name_wrong = true;
            else m_name_wrong = false;
            break;
        case LadderState::Done: break;
        }
        save();
        if (ladder_state(*lock, now_seconds(), left) == LadderState::Done) {
            opened();
            return;
        }
        refresh();
    }

    void refresh()
    {
        Lock *lock = state().store.find(m_element);
        if (!lock) return;
        const std::int64_t now = now_seconds();
        if (now < lock->blocked_until) {
            m_status->SetLabel(wxString::Format(_L("%d wrong codes in a row. Try again in %s."), lock->failures, wait_text(lock->blocked_until - now)));
            m_unlock->Enable(false);
        } else {
            m_status->SetLabel(lock->failures > 0 ? wxString::Format(_L("%d wrong codes in a row."), lock->failures) : wxString());
            m_unlock->Enable(true);
        }
        std::int64_t left  = 0;
        const auto   stage = ladder_state(*lock, now, left);
        // A finished wait moves the ladder on by itself.
        if (stage == LadderState::Waiting && left == 0) {
            ladder_advance(*lock, now);
            save();
            if (ladder_state(*lock, now, left) == LadderState::Done) {
                opened();
                return;
            }
        }
        const auto current = ladder_state(*lock, now, left);
        m_name->Show(current == LadderState::NeedsName);
        m_ladder->Show(current != LadderState::Waiting);
        switch (current) {
        case LadderState::NotStarted:
            m_ladder_text->SetLabel(_L("Step 1 of 3: wait 1 minute."));
            m_ladder->SetLabel(_L("Start the ladder"));
            break;
        case LadderState::Waiting:
            m_ladder_text->SetLabel(wxString::Format(_L("Step %d of 3: wait. %s left."), lock->ladder_step + 1, wait_text(left)));
            break;
        case LadderState::NeedsName:
            m_ladder_text->SetLabel(wxString::Format(_L("Step 2 of 3: type the name of what is locked, %s."), element_name(m_element)) +
                                    (m_name_wrong ? "\n" + _L("That is not the name.") : wxString()));
            m_ladder->SetLabel(_L("Continue"));
            break;
        case LadderState::Done: break;
        }
        m_ladder_box->Show(m_show_ladder);
        m_forgot->Show(!m_show_ladder);
        Layout();
        Fit();
    }

    std::string m_element;
    wxTimer     m_timer;
    TextInput  *m_code = nullptr, *m_name = nullptr;
    Button     *m_unlock = nullptr, *m_ladder = nullptr, *m_forgot = nullptr;
    Label      *m_status = nullptr, *m_ladder_text = nullptr;
    StaticBox  *m_ladder_box = nullptr;
    bool        m_show_ladder = false, m_name_wrong = false;
};

// ---------------------------------------------------------------- the filter

class LockFilter : public wxEventFilter
{
public:
    int FilterEvent(wxEvent &event) override
    {
        if (state().store.empty()) return Event_Skip;
        const wxEventType t = event.GetEventType();
        const bool mouse = t == wxEVT_LEFT_DOWN || t == wxEVT_LEFT_UP || t == wxEVT_LEFT_DCLICK || t == wxEVT_MIDDLE_DOWN || t == wxEVT_MOUSEWHEEL;
        const bool keys  = t == wxEVT_KEY_DOWN || t == wxEVT_CHAR;
        if (!mouse && !keys) return Event_Skip;
        auto *window = wxDynamicCast(event.GetEventObject(), wxWindow);
        if (!window) return Event_Skip;
        // The element itself or any element around it, up to its window.
        std::string id;
        for (wxWindow *w = window; w; w = w->GetParent()) {
            const std::string own = ElementStyle::own_id_of(w);
            if (!own.empty() && state().store.find(own)) {
                id = own;
                break;
            }
            if (w->IsTopLevel()) break;
        }
        if (id.empty()) return Event_Skip;
        // The keys that open the right-click menu still work.
        if (keys) {
            const auto &k = static_cast<wxKeyEvent &>(event);
            if (k.GetKeyCode() == WXK_WINDOWS_MENU || k.GetKeyCode() == WXK_MENU || (k.ShiftDown() && k.GetKeyCode() == WXK_F10) ||
                k.GetKeyCode() == WXK_TAB)
                return Event_Skip;
        }
        if (t == wxEVT_LEFT_DOWN || t == wxEVT_KEY_DOWN) hint(window, id);
        return Event_Processed;
    }

private:
    void hint(wxWindow *window, const std::string &id)
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - m_last < std::chrono::milliseconds(1500)) return;
        m_last           = now;
        const Lock *lock = state().store.find(id);
        if (!lock) return;
        wxString text = wxString::Format(_L("%s is locked by %s."), element_name(id), wxString::FromUTF8(lock->who));
        if (!lock->reason.empty()) text << " " << wxString::FromUTF8(lock->reason);
        text << "\n" << _L("Right-click it to unlock.");
        auto *popup = new RoundedPopupWindow(window, window->FromDIP(10));
        popup->SetBackgroundColour(role(MD3::Role::InverseSurface));
        auto *s = new wxBoxSizer(wxVERTICAL);
        auto *l = new Label(popup, Label::Body_13, text);
        l->SetBackgroundColour(role(MD3::Role::InverseSurface));
        l->SetForegroundColour(role(MD3::Role::InverseOn));
        s->Add(l, 0, wxALL, window->FromDIP(10));
        popup->SetSizer(s);
        popup->Finalize();
        popup->Position(wxGetMousePosition() + window->FromDIP(wxPoint(12, 16)), wxSize(0, 0));
        popup->Popup();
    }

    std::chrono::steady_clock::time_point m_last{};
};

std::unique_ptr<LockFilter> &filter()
{
    static std::unique_ptr<LockFilter> f;
    return f;
}

struct MenuBinding
{
    std::string         element;
    wxWeakRef<wxWindow> anchor;
};
std::map<wxMenu *, MenuBinding> &bindings()
{
    static std::map<wxMenu *, MenuBinding> b;
    return b;
}

} // namespace

void init(const std::string &data_dir)
{
    State &s = state();
    s.file   = fs::u8path(data_dir) / "element-locks.json";
    std::ifstream in(s.file, std::ios::binary);
    if (in) {
        std::stringstream text;
        text << in.rdbuf();
        s.store.load(text.str());
    }
    s.ready = true;
    if (!filter()) {
        filter() = std::make_unique<LockFilter>();
        wxEvtHandler::AddFilter(filter().get());
    }
}

void shutdown()
{
    if (filter()) {
        wxEvtHandler::RemoveFilter(filter().get());
        filter().reset();
    }
}

bool is_locked(const std::string &element) { return state().store.find(element) != nullptr; }

int lock_item_id() { return kLockItemId; }

void append_lock_item(wxMenu &menu, const std::string &element_id, wxWindow *anchor)
{
    // Only where a window stands behind the element: a row of a list has none to lock.
    if (element_id.empty() || !anchor || !state().ready) return;
    bindings()[&menu] = MenuBinding{element_id, anchor};
    if (menu.FindItem(kLockItemId)) return;
    const bool locked = is_locked(element_id);
    menu.Append(kLockItemId, locked ? _L("Unlock this element...") : _L("Lock this element..."),
                locked ? _L("Open it with its code or the unlock ladder") : _L("Stop clicks and keys from changing it until it is unlocked"));
    menu.Bind(wxEVT_MENU, [menu_ptr = &menu](wxCommandEvent &) {
        const auto it = bindings().find(menu_ptr);
        if (it == bindings().end()) return;
        const std::string id = it->second.element;
        wxWindow         *a  = it->second.anchor.get();
        if (!a) a = menu_ptr->GetInvokingWindow();
        if (!a) a = wxGetApp().mainframe;
        if (!a) return;
        wxWeakRef<wxWindow> ref(a);
        a->CallAfter([ref, id]() {
            wxWindow *w = ref.get() ? ref.get() : wxGetApp().mainframe;
            if (is_locked(id)) unlock_element(w, id);
            else lock_element(w, id);
        });
    }, kLockItemId);
}

void remove_lock_item(wxMenu &menu)
{
    if (wxMenuItem *item = menu.FindItem(kLockItemId)) menu.Destroy(item);
    bindings().erase(&menu);
}

bool lock_element(wxWindow *anchor, const std::string &element_id)
{
    if (element_id.empty()) return false;
    LockDialog dialog(anchor ? wxGetTopLevelParent(anchor) : nullptr, element_id);
    return dialog.ShowModal() == wxID_OK;
}

bool unlock_element(wxWindow *anchor, const std::string &element_id)
{
    if (!is_locked(element_id)) return true;
    UnlockDialog dialog(anchor ? wxGetTopLevelParent(anchor) : nullptr, element_id);
    return dialog.ShowModal() == wxID_OK;
}

void open_manager(wxWindow *parent)
{
    Dialog dialog(parent, _L("Element locks"));
    dialog.body()->Add(label(&dialog, Label::Body_13,
                             _L("Every element you locked. Unlock opens it with its code or the unlock ladder."), MD3::Role::OnSurfaceVariant, true),
                       0, wxEXPAND | wxBOTTOM, dialog.FromDIP(12));
    auto *panel = new wxPanel(&dialog);
    panel->SetBackgroundColour(dialog.GetBackgroundColour());
    auto *list = new wxBoxSizer(wxVERTICAL);
    panel->SetSizer(list);
    dialog.body()->Add(panel, 1, wxEXPAND);
    auto                                   rebuild = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>>   weak    = rebuild;
    Dialog                                *d       = &dialog;
    *rebuild = [d, panel, list, weak]() {
        list->Clear(true);
        if (state().store.empty())
            list->Add(label(panel, Label::Body_13, _L("Nothing is locked. Right-click an element and choose Lock this element."),
                            MD3::Role::OnSurfaceVariant, true),
                      0, wxEXPAND | wxALL, panel->FromDIP(4));
        for (const auto &[id, lock] : state().store.locks()) {
            auto *row  = new wxBoxSizer(wxHORIZONTAL);
            auto *text = new wxBoxSizer(wxVERTICAL);
            text->Add(label(panel, Label::Head_14, element_name(id)), 0);
            wxString detail = wxString::Format(_L("Locked by %s on %s."), wxString::FromUTF8(lock.who),
                                               wxDateTime(static_cast<time_t>(lock.created)).Format("%Y-%m-%d %H:%M"));
            if (!lock.reason.empty()) detail << " " << wxString::FromUTF8(lock.reason);
            text->Add(label(panel, Label::Body_12, detail, MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND | wxTOP, panel->FromDIP(2));
            row->Add(text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, panel->FromDIP(12));
            auto             *unlock  = button(panel, _L("Unlock..."), Button::Variant::Tonal);
            const std::string element = id;
            unlock->Bind(wxEVT_BUTTON, [d, element, weak](wxCommandEvent &) {
                unlock_element(d, element);
                d->CallAfter([weak]() {
                    if (auto again = weak.lock()) (*again)();
                });
            });
            row->Add(unlock, 0, wxALIGN_CENTER_VERTICAL);
            list->Add(row, 0, wxEXPAND | wxBOTTOM, panel->FromDIP(12));
        }
        panel->Layout();
        d->Layout();
        d->Fit();
    };
    auto *close = button(&dialog, _L("Close"), Button::Variant::Outlined);
    close->Bind(wxEVT_BUTTON, [d](wxCommandEvent &) { d->EndModal(wxID_CANCEL); });
    dialog.buttons()->Add(close, 0, wxLEFT, dialog.FromDIP(8));
    (*rebuild)();
    dialog.finish();
    dialog.ShowModal();
}

}}} // namespace Slic3r::GUI::ElementLocks
