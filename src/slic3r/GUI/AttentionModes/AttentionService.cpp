#include "AttentionService.hpp"
#include "AttentionModel.hpp"

#include "../GLCanvas3D.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../GUI_Utils.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../NotificationManager.hpp"
#include "../Plater.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/TextInput.hpp"

#include "libslic3r/AppConfig.hpp"

#include <wx/dcclient.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/weakref.h>

#include <atomic>
#include <ctime>

namespace Slic3r { namespace GUI { namespace AttentionModes {

namespace {

enum : std::uint32_t { GlyphTime = 0xE8B5, GlyphFlag = 0xE153, GlyphTune = 0xE429, GlyphFocus = 0xE3B4, GlyphDone = 0xE876, GlyphEdit = 0xE3C9 };

std::int64_t now_seconds() { return std::int64_t(std::time(nullptr)); }

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

Settings current()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (!cfg) return Settings{};
    return read([cfg](const std::string &k) { return cfg->get(k); });
}

void store(const Settings &s)
{
    AppConfig *cfg = wxGetApp().app_config;
    if (!cfg) return;
    write(s, [cfg](const std::string &k, const std::string &v) { cfg->set(k, v); });
    cfg->save();
}

wxString duration_text(std::int64_t seconds)
{
    const Parts p = split(seconds);
    if (p.hours > 0) return wxString::Format(_L("%d h %d min"), p.hours, p.minutes);
    if (p.minutes > 0) return wxString::Format(_L("%d min"), p.minutes);
    return _L("under a minute");
}

wxWindow *glyph(wxWindow *parent, std::uint32_t code, MD3::Role colour)
{
    auto *w = new wxWindow(parent, wxID_ANY, wxDefaultPosition, parent->FromDIP(wxSize(22, 22)));
    w->SetBackgroundColour(parent->GetBackgroundColour());
    w->Bind(wxEVT_PAINT, [w, code, colour](wxPaintEvent &) {
        wxPaintDC dc(w);
        if (!MaterialIcon::available()) return;
        const int    px = w->FromDIP(18);
        const wxSize gs = MaterialIcon::measure(dc, code, px);
        MaterialIcon::draw(dc, code, px, role(colour), wxPoint((w->GetSize().x - gs.x) / 2, (w->GetSize().y - gs.y) / 2));
    });
    return w;
}

Button *text_button(wxWindow *parent, const wxString &text, std::uint32_t code)
{
    auto *b = new Button(parent, text);
    b->SetVariant(Button::Variant::Text);
    b->SetButtonSize(Button::Size::Small);
    if (code) b->SetGlyph(code);
    return b;
}

class NextStepDialog : public DPIDialog
{
public:
    explicit NextStepDialog(wxWindow *parent) : DPIDialog(parent, wxID_ANY, _L("Your next step"), wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    {
        SetBackgroundColour(role(MD3::Role::Surface));
        root = new wxBoxSizer(wxVERTICAL);
        root->Add(new MD3DialogCaption(this, _L("Your next step")), 0, wxEXPAND);
        SetSizer(root);
    }
    wxBoxSizer *root = nullptr; // the caption is in; the body goes under it

protected:
    void on_dpi_changed(const wxRect &) override { Layout(); }
};

// The next step, typed by the person.
bool edit_next_action(wxWindow *parent, std::string &text)
{
    NextStepDialog dialog(parent);
    wxBoxSizer    *root = dialog.root;
    auto *body = new wxBoxSizer(wxVERTICAL);
    auto *lead = new Label(&dialog, Label::Body_13, _L("One thing, in your words. It stays in the strip until you mark it done."),
                           LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    lead->SetMinSize(wxSize(dialog.FromDIP(380), -1));
    lead->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    body->Add(lead, 0, wxEXPAND | wxBOTTOM, dialog.FromDIP(10));
    auto *input = new TextInput(&dialog, wxString::FromUTF8(text), wxEmptyString, wxEmptyString, wxDefaultPosition,
                                wxSize(dialog.FromDIP(380), -1), wxTE_PROCESS_ENTER);
    input->GetTextCtrl()->SetMaxLength(120);
    input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [&dialog](wxCommandEvent &) { dialog.EndModal(wxID_OK); });
    body->Add(input, 0, wxEXPAND);
    root->Add(body, 1, wxEXPAND | wxALL, dialog.FromDIP(20));
    auto *buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer(1);
    auto *cancel = new Button(&dialog, _L("Cancel"));
    cancel->SetVariant(Button::Variant::Outlined);
    cancel->SetButtonSize(Button::Size::Small);
    cancel->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent &) { dialog.EndModal(wxID_CANCEL); });
    auto *save = new Button(&dialog, _L("Save"));
    save->SetVariant(Button::Variant::Filled);
    save->SetButtonSize(Button::Size::Small);
    save->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent &) { dialog.EndModal(wxID_OK); });
    buttons->Add(cancel, 0, wxLEFT, dialog.FromDIP(8));
    buttons->Add(save, 0, wxLEFT, dialog.FromDIP(8));
    root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, dialog.FromDIP(20));
    dialog.Fit();
    MD3DialogCaption::FinishChrome(&dialog);
    dialog.CenterOnParent();
    wxGetApp().UpdateDlgDarkUI(&dialog);
    input->GetTextCtrl()->SetFocus();
    input->GetTextCtrl()->SelectAll();
    if (dialog.ShowModal() != wxID_OK) return false;
    text = clean_next_action(into_u8(input->GetTextCtrl()->GetValue()));
    return true;
}

class Strip;

struct Service
{
    Clock               clock;
    std::atomic<bool>   focus{false};
    wxWeakRef<wxWindow> strip;
    wxTimer            *timer   = nullptr;
    wxEvtHandler       *handler = nullptr;
    bool                started = false;
};

Service &service()
{
    static Service s;
    return s;
}

// ---------------------------------------------------------------- the strip

class Strip : public wxPanel
{
public:
    explicit Strip(wxWindow *parent) : wxPanel(parent, wxID_ANY)
    {
        SetName("attention-strip");
        SetBackgroundColour(role(MD3::Role::SurfaceContainerLow));
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        const int gap = FromDIP(6);

        m_time_glyph = glyph(this, GlyphTime, MD3::Role::OnSurfaceVariant);
        m_time       = new Label(this, Label::Body_13, wxString());
        m_time->SetForegroundColour(role(MD3::Role::OnSurface));
        m_time->SetName("attention-time");
        row->Add(m_time_glyph, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(12));
        row->Add(m_time, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);

        m_next_glyph = glyph(this, GlyphFlag, MD3::Role::Primary);
        m_next       = new Label(this, Label::Head_13, wxString());
        m_next->SetForegroundColour(role(MD3::Role::OnSurface));
        m_next->SetName("attention-next");
        m_edit = text_button(this, _L("Set the next step"), GlyphEdit);
        m_edit->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { edit(); });
        m_done = text_button(this, _L("Done"), GlyphDone);
        m_done->SetToolTip(_L("Clear the next step"));
        m_done->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            Settings s    = current();
            s.next_action.clear();
            store(s);
            refresh();
        });
        row->Add(m_next_glyph, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(20));
        row->Add(m_next, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);
        row->Add(m_edit, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);
        row->Add(m_done, 0, wxALIGN_CENTER_VERTICAL);

        m_focus_glyph = glyph(this, GlyphFocus, MD3::Role::Primary);
        m_focus       = new Label(this, Label::Body_13, _L("Focus is on: with something selected, the rest of the plate steps back."));
        m_focus->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
        m_focus_off = text_button(this, _L("Turn Focus off"), 0);
        m_focus_off->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            Settings s = current();
            s.focus    = false;
            store(s);
            tick();
        });
        row->Add(m_focus_glyph, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(20));
        row->Add(m_focus, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);
        row->Add(m_focus_off, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, gap);

        row->AddStretchSpacer(1);
        auto *tune = new Button(this, wxString());
        tune->SetIconButton(Button::IconShape::Circle, FromDIP(30));
        tune->SetGlyph(GlyphTune);
        tune->SetToolTip(_L("Attention modes in Preferences"));
        tune->SetName(_L("Attention modes in Preferences"));
        tune->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { open_preferences(); });
        row->Add(tune, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        row->SetMinSize(wxSize(-1, FromDIP(36)));
        SetSizer(row);
        refresh();
    }

    void refresh()
    {
        const Settings s = current();
        const bool     any = s.time_awareness || s.one_thing || s.focus;
        if (s.time_awareness) {
            const Elapsed e = elapsed(service().clock, now_seconds());
            wxString      text = wxString::Format(_L("Open %s"), duration_text(e.open));
            text << "  " << wxUniChar(0x00B7) << "  ";
            text << (e.changed ? wxString::Format(_L("last change %s ago"), duration_text(e.since_change)) : _L("no changes yet"));
            m_time->SetLabel(text);
        }
        m_time_glyph->Show(s.time_awareness);
        m_time->Show(s.time_awareness);

        const bool has_next = !s.next_action.empty();
        m_next->SetLabel(has_next ? _L("Next:") + " " + wxString::FromUTF8(s.next_action) : wxString());
        m_next_glyph->Show(s.one_thing);
        m_next->Show(s.one_thing && has_next);
        m_edit->SetLabel(has_next ? _L("Edit") : _L("Set the next step"));
        m_edit->Show(s.one_thing);
        m_done->Show(s.one_thing && has_next);

        m_focus_glyph->Show(s.focus);
        m_focus->Show(s.focus);
        m_focus_off->Show(s.focus);

        if (IsShown() != any) {
            Show(any);
            if (GetParent()) GetParent()->Layout();
        }
        Layout();
    }

private:
    void edit()
    {
        Settings s = current();
        if (!edit_next_action(wxGetTopLevelParent(this), s.next_action)) return;
        store(s);
        refresh();
    }
    static void tick();

    wxWindow *m_time_glyph = nullptr, *m_next_glyph = nullptr, *m_focus_glyph = nullptr;
    Label    *m_time = nullptr, *m_next = nullptr, *m_focus = nullptr;
    Button   *m_edit = nullptr, *m_done = nullptr, *m_focus_off = nullptr;
};

void redraw_canvas()
{
    if (Plater *plater = wxGetApp().plater())
        if (GLCanvas3D *canvas = plater->get_current_canvas3D()) {
            canvas->set_as_dirty();
            canvas->request_extra_frame();
        }
}

void tick_all()
{
    Service       &svc = service();
    Settings       s   = current();
    const bool     focus = s.focus;
    if (svc.focus.exchange(focus) != focus) redraw_canvas();
    if (auto *strip = dynamic_cast<Strip *>(svc.strip.get())) strip->refresh();

    const std::int64_t now = now_seconds();
    if (momentum_due(s, svc.clock, now)) {
        prompted(svc.clock, now);
        if (Plater *plater = wxGetApp().plater()) {
            const Elapsed e = elapsed(svc.clock, now);
            const wxString text = e.changed ? wxString::Format(_L("Nothing has changed here for %s."), duration_text(e.since_change))
                                            : wxString::Format(_L("Nothing has changed since Bambu Studio opened, %s ago."), duration_text(e.open));
            const wxString not_now = wxString::Format(_L("Not now (%s)"), duration_text(std::int64_t(s.not_now_for) * 60));
            plater->get_notification_manager()->push_notification(NotificationType::CustomNotification,
                                                                   NotificationManager::NotificationLevel::RegularNotificationLevel, into_u8(text),
                                                                   into_u8(not_now), [](wxEvtHandler *) {
                                                                       Settings now_settings = current();
                                                                       snooze(now_settings, now_seconds());
                                                                       store(now_settings);
                                                                       return true;
                                                                   });
        }
    }
}

void Strip::tick() { tick_all(); }

} // namespace

void init()
{
    Service &svc = service();
    if (svc.started) return;
    svc.started             = true;
    svc.clock.session_start = now_seconds();
    svc.focus               = current().focus;
    svc.handler             = new wxEvtHandler();
    svc.timer               = new wxTimer(svc.handler);
    svc.handler->Bind(wxEVT_TIMER, [](wxTimerEvent &) { tick_all(); });
    svc.timer->Start(2000);
}

void shutdown()
{
    Service &svc = service();
    if (svc.timer) {
        svc.timer->Stop();
        delete svc.timer;
        svc.timer = nullptr;
    }
    delete svc.handler;
    svc.handler = nullptr;
}

void note_change() { service().clock.last_change = now_seconds(); }

bool focus_on() { return service().focus.load(); }

void set_focus(bool on)
{
    Settings s = current();
    if (s.focus == on) return;
    s.focus = on;
    store(s);
    tick_all();
}

wxWindow *create_strip(wxWindow *parent)
{
    auto *strip       = new Strip(parent);
    service().strip   = strip;
    return strip;
}

void open_preferences() { wxGetApp().open_preferences(kTimeAwareness); }

}}} // namespace Slic3r::GUI::AttentionModes
