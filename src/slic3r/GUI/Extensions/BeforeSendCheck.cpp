#include "BeforeSendCheck.hpp"
#include "ExtensionRuntime.hpp"

#include "../GUI_App.hpp"
#include "../GUI_Utils.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/ProgressBar.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"

#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/sizer.h>
#include <wx/timer.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>

namespace Slic3r { namespace GUI { namespace Extensions {

namespace {

enum Glyphs : std::uint32_t {
    GlyphChecklist = 0xE6B1, GlyphFactCheck = 0xF0C5, GlyphCheckCircle = 0xE86C, GlyphWarning = 0xE002, GlyphCancel = 0xE5C9,
    GlyphError = 0xE000, GlyphInfo = 0xE88E, GlyphBlock = 0xE14B, GlyphSchedule = 0xE8B5, GlyphHourglass = 0xEA5B,
    GlyphExtension = 0xE87B, GlyphSend = 0xE163,
};

using Clock = std::chrono::steady_clock;

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

// The kit has no warning role; this is the amber the design boards use, darker on light surfaces.
wxColour warning_colour() { return GUI_App::dark_mode() ? wxColour(0xFF, 0xB8, 0x70) : wxColour(0x8B, 0x50, 0x00); }

// " · " built from its code point.
wxString dot() { return wxString(" ") + wxUniChar(0x00B7) + " "; }

// A Material Symbols glyph that can change after it is made.
class GlyphView : public wxWindow
{
public:
    GlyphView(wxWindow *parent, std::uint32_t glyph, const wxColour &colour, int size_dip = 20)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, parent->FromDIP(wxSize(size_dip + 4, size_dip + 4)), wxBORDER_NONE)
        , m_glyph(glyph), m_colour(colour), m_size(size_dip)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
            dc.Clear();
            if (MaterialIcon::available())
                MaterialIcon::drawCentered(dc, m_glyph, FromDIP(m_size), m_colour, wxRect(wxPoint(0, 0), GetClientSize()));
        });
    }
    void set(std::uint32_t glyph, const wxColour &colour)
    {
        if (glyph == m_glyph && colour == m_colour) return;
        m_glyph  = glyph;
        m_colour = colour;
        Refresh();
    }

private:
    std::uint32_t m_glyph;
    wxColour      m_colour;
    int           m_size;
};

Label *label(wxWindow *parent, const wxFont &font, const wxString &text, const wxColour &colour, int wrap_dip = 0)
{
    auto *l = new Label(parent, font, text, wrap_dip > 0 ? (LB_AUTO_WRAP | wxST_NO_AUTORESIZE) : 0);
    l->SetBackgroundColour(parent->GetBackgroundColour());
    l->SetForegroundColour(colour);
    if (wrap_dip > 0) {
        l->SetMinSize(wxSize(parent->FromDIP(wrap_dip), -1));
        l->Wrap(parent->FromDIP(wrap_dip));
    }
    return l;
}

StaticBox *card(wxWindow *parent, const wxColour &background)
{
    auto *box = new StaticBox(parent);
    box->SetCornerRadius(parent->FromDIP(16));
    box->SetBorderWidth(0);
    box->SetBackgroundColor(background);
    box->SetBackgroundColour(background);
    return box;
}

wxString what_is_sent(const SendFacts &send)
{
    return send.all_plates ? _L("every plate") : wxString::Format(_L("Plate %d"), send.plate);
}

// ---------------------------------------------------------------- the window

class BeforeSendDialog : public DPIDialog
{
public:
    BeforeSendDialog(wxWindow *parent, const SendFacts &send, std::vector<PlannedRun> runs, nlohmann::json context, std::string subject)
        : DPIDialog(parent, wxID_ANY, _L("Check before sending"), wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
        , m_send(send), m_context(std::move(context)), m_subject(std::move(subject))
    {
        Host &host = Runtime::instance().host();
        m_limits   = host.limits();
        for (PlannedRun &run : runs) {
            Entry e;
            e.name = wxString::FromUTF8(run.manifest.name.empty() ? run.id : run.manifest.name);
            if (!run.manifest.version.empty()) e.name << " " << wxString::FromUTF8(run.manifest.version);
            e.plan = std::move(run);
            m_entries.push_back(std::move(e));
        }

        SetBackgroundColour(role(MD3::Role::Surface));
        m_root = new wxBoxSizer(wxVERTICAL);
        m_root->Add(new MD3DialogCaption(this, _L("Check before sending")), 0, wxEXPAND);
        m_body = new wxBoxSizer(wxVERTICAL);
        m_root->Add(m_body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));
        m_buttons = new wxBoxSizer(wxHORIZONTAL);
        m_root->Add(m_buttons, 0, wxEXPAND | wxALL, FromDIP(20));

        auto *head = new wxBoxSizer(wxHORIZONTAL);
        m_head_glyph = new GlyphView(this, GlyphChecklist, role(MD3::Role::Primary), 26);
        head->Add(m_head_glyph, 0, wxALIGN_TOP | wxRIGHT, FromDIP(10));
        auto *lead = new wxBoxSizer(wxVERTICAL);
        m_title    = label(this, Label::Head_16, wxEmptyString, role(MD3::Role::OnSurface), 440);
        m_subtitle = label(this, Label::Body_13, wxEmptyString, role(MD3::Role::OnSurfaceVariant), 440);
        lead->Add(m_title, 0, wxEXPAND | wxBOTTOM, FromDIP(2));
        lead->Add(m_subtitle, 0, wxEXPAND);
        head->Add(lead, 1, wxEXPAND);
        m_body->Add(head, 0, wxEXPAND | wxBOTTOM, FromDIP(14));

        m_progress = new ProgressBar(this, wxID_ANY, std::max<int>(1, int(m_entries.size())), wxDefaultPosition, wxSize(-1, FromDIP(8)), false);
        m_body->Add(m_progress, 0, wxEXPAND | wxBOTTOM, FromDIP(14));

        m_list = card(this, role(MD3::Role::SurfaceContainerLow));
        auto *ls = new wxBoxSizer(wxVERTICAL);
        for (Entry &e : m_entries) {
            auto *row = new wxBoxSizer(wxHORIZONTAL);
            e.glyph  = new GlyphView(m_list, GlyphSchedule, role(MD3::Role::OnSurfaceVariant));
            e.status = label(m_list, Label::Body_12, _L("Waiting"), role(MD3::Role::OnSurfaceVariant));
            row->Add(e.glyph, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
            row->Add(label(m_list, Label::Body_14, e.name, role(MD3::Role::OnSurface)), 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
            row->Add(e.status, 0, wxALIGN_CENTER_VERTICAL);
            ls->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
        }
        ls->AddSpacer(FromDIP(12));
        m_list->SetSizer(ls);
        m_body->Add(m_list, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

        m_note = label(this, Label::Body_12,
                       wxString::Format(_L("Each one has the time limit set in Extensions (%d s). Skipping stops the run now; it is recorded as "
                                           "stopped by you and never counts as a failure."),
                                        m_limits.timeout_ms / 1000),
                       role(MD3::Role::OnSurfaceVariant), 480);
        m_body->Add(m_note, 0, wxEXPAND);

        set_header(GlyphChecklist, role(MD3::Role::Primary),
                   m_send.all_plates ? _L("Checking every plate before it is sent")
                                     : wxString::Format(_L("Checking Plate %d before it is sent"), m_send.plate),
                   wxString::Format(_L("Extensions: %d, one at a time"), int(m_entries.size())));
        set_buttons({{_L("Back"), Button::Variant::Text, [this]() { back(); }},
                     {_L("Send without the checks"), Button::Variant::Outlined, [this]() { skip(); }}},
                    0);

        m_ticker.SetOwner(this);
        m_closer.SetOwner(this);
        Bind(wxEVT_TIMER, [this](wxTimerEvent &e) {
            if (e.GetTimer().GetId() == m_ticker.GetId()) tick();
            else if (e.GetTimer().GetId() == m_closer.GetId()) finish(SendDecision::Send);
        });
        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
            if (e.GetKeyCode() == WXK_ESCAPE) back();
            else e.Skip();
        });
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &) { back(); });
    }

    ~BeforeSendDialog() override
    {
        *m_alive = false;
        m_ticker.Stop();
        m_closer.Stop();
        if (!m_runs_done) Runtime::instance().stop(m_cancel);
    }

    SendCheckOutcome run()
    {
        SetSizer(m_root);
        SetMinSize(wxSize(FromDIP(520), -1));
        SetMaxSize(wxSize(FromDIP(640), -1));
        Fit();
        MD3DialogCaption::FinishChrome(this);
        CenterOnParent();
        wxGetApp().UpdateDlgDarkUI(this);

        std::vector<PlannedRun> runs;
        for (const Entry &e : m_entries) runs.push_back(e.plan);
        std::shared_ptr<bool> alive = m_alive;
        m_cancel = Runtime::instance().run_before_send(
            std::move(runs), m_context, m_subject,
            [this, alive](const RunResult &result) {
                if (*alive) on_result(result);
            },
            [this, alive]() {
                if (*alive) on_done();
            });
        if (!m_cancel) return SendCheckOutcome{SendDecision::Send, {}};
        m_ticker.Start(100);
        ShowModal();
        return m_outcome;
    }

protected:
    void on_dpi_changed(const wxRect &) override
    {
        Layout();
        Fit();
    }

private:
    struct Entry
    {
        PlannedRun        plan;
        wxString          name; // name and version
        bool              started = false, done = false;
        Clock::time_point since;
        RunResult         result;
        GlyphView        *glyph  = nullptr;
        Label            *status = nullptr;
    };

    struct ButtonSpec
    {
        wxString              text;
        Button::Variant       variant;
        std::function<void()> act;
    };

    void set_header(std::uint32_t glyph, const wxColour &colour, const wxString &title, const wxString &subtitle)
    {
        m_head_glyph->set(glyph, colour);
        m_title->SetLabel(title);
        m_title->Wrap(FromDIP(440));
        m_subtitle->SetLabel(subtitle);
        m_subtitle->Wrap(FromDIP(440));
    }

    // focus: the button Enter presses. Back, wherever reading comes first, so a
    // key still held from pressing Send never sends past what is shown.
    void set_buttons(const std::vector<ButtonSpec> &specs, std::size_t focus)
    {
        m_buttons->Clear(true);
        m_buttons->AddStretchSpacer(1);
        for (std::size_t i = 0; i < specs.size(); ++i) {
            const ButtonSpec &spec = specs[i];
            auto             *b    = new Button(this, spec.text);
            b->SetVariant(spec.variant);
            b->SetButtonSize(Button::Size::Small);
            auto act = spec.act;
            b->Bind(wxEVT_BUTTON, [act](wxCommandEvent &) { act(); });
            m_buttons->Add(b, 0, wxLEFT, FromDIP(8));
            if (i == focus) b->SetFocus();
        }
    }

    // ---- while the runs go
    void tick()
    {
        if (m_runs_done) return;
        const Runtime::Activity activity = Runtime::instance().activity();
        const bool              ours     = activity.hook == info(Hook::PrintBeforeSend).id;
        Entry                  *running  = nullptr;
        for (Entry &e : m_entries)
            if (!e.done && ours && activity.running == e.plan.id) running = &e;
        if (running) {
            if (!running->started) {
                running->started = true;
                running->since   = Clock::now();
                running->glyph->set(GlyphHourglass, role(MD3::Role::Primary));
                set_header(GlyphChecklist, role(MD3::Role::Primary),
                           m_send.all_plates ? _L("Checking every plate before it is sent")
                                             : wxString::Format(_L("Checking Plate %d before it is sent"), m_send.plate),
                           wxString::Format(_L("Extensions: %d, one at a time"), int(m_entries.size())));
                m_progress->SetValue(done_count());
            }
            const double seconds = std::chrono::duration<double>(Clock::now() - running->since).count();
            running->status->SetLabel(wxString::Format(_L("Checking... %.1f s"), seconds));
            running->status->GetParent()->Layout();
        } else if (!activity.running.empty() && !ours) {
            // Another extension is finishing a run it started before Send was pressed.
            const Installed *x    = Runtime::instance().host().find(activity.running);
            const wxString   name = wxString::FromUTF8(x && !x->manifest.name.empty() ? x->manifest.name : activity.running);
            m_subtitle->SetLabel(wxString::Format(_L("Waiting for %s to finish another run"), name));
            m_subtitle->Wrap(FromDIP(440));
            m_progress->Pulse();
        }
    }

    int done_count() const
    {
        return int(std::count_if(m_entries.begin(), m_entries.end(), [](const Entry &e) { return e.done; }));
    }

    void on_result(const RunResult &result)
    {
        auto it = std::find_if(m_entries.begin(), m_entries.end(), [&](const Entry &e) { return !e.done && e.plan.id == result.plan.id; });
        if (it == m_entries.end()) return;
        Entry &e = *it;
        e.done   = true;
        e.result = result;
        std::uint32_t glyph  = GlyphCheckCircle;
        wxColour      colour = role(MD3::Role::Primary);
        wxString      status = _L("Answered");
        if (result.outcome == Outcome::Answered) {
            const Answer &a    = result.answer;
            const bool    fail = std::any_of(a.checklist.begin(), a.checklist.end(), [](const ChecklistRow &r) { return r.state == "fail"; });
            const bool    warn = std::any_of(a.checklist.begin(), a.checklist.end(), [](const ChecklistRow &r) { return r.state == "warn"; });
            if (a.block) {
                glyph  = GlyphBlock;
                colour = role(MD3::Role::Error);
                status = _L("Stopped the print");
            } else if (fail) {
                glyph  = GlyphCancel;
                colour = role(MD3::Role::Error);
            } else if (warn) {
                glyph  = GlyphWarning;
                colour = warning_colour();
            }
        } else if (result.outcome == Outcome::Stopped) {
            glyph  = GlyphSchedule;
            colour = role(MD3::Role::OnSurfaceVariant);
            status = outcome_text(result.outcome, result.problem, result.plan.limits);
        } else {
            glyph  = GlyphError;
            colour = role(MD3::Role::Error);
            status = outcome_text(result.outcome, result.problem, result.plan.limits);
        }
        e.glyph->set(glyph, colour);
        e.status->SetLabel(status);
        e.status->GetParent()->Layout();
        m_progress->SetValue(done_count());
    }

    void on_done()
    {
        m_runs_done = true;
        m_ticker.Stop();
        if (m_closing) return;
        m_progress->SetValue(int(m_entries.size()));
        std::vector<SendCheckRun> runs;
        for (const Entry &e : m_entries)
            runs.push_back(SendCheckRun{e.plan.id, e.done ? e.result.outcome : Outcome::NotStarted, e.done ? e.result.problem : std::string(),
                                        e.done ? e.result.answer : Answer()});
        const SendSummary summary = summarise_send_check(runs);
        if (summary.verdict == SendVerdict::Continue) {
            // Nothing to look at: say so for a moment, then carry on sending.
            set_header(GlyphCheckCircle, role(MD3::Role::Primary), _L("All checks passed"),
                       wxString::Format(_L("Sending %s"), what_is_sent(m_send)));
            m_note->Hide();
            set_buttons({{_L("Back"), Button::Variant::Text, [this]() { back(); }},
                         {_L("Send"), Button::Variant::Filled, [this]() { finish(SendDecision::Send); }}},
                        1);
            relayout();
            m_closer.StartOnce(700);
            return;
        }
        show_results(summary);
    }

    // ---- what they answered
    void add_line(wxWindow *parent, wxBoxSizer *sizer, std::uint32_t glyph, const wxColour &colour, const wxString &text, bool muted = false)
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new GlyphView(parent, glyph, colour, 18), 0, wxALIGN_TOP | wxRIGHT, FromDIP(8));
        row->Add(label(parent, Label::Body_13, text, role(muted ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface), 400), 1,
                 wxALIGN_CENTER_VERTICAL);
        sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    }

    void show_results(const SendSummary &summary)
    {
        const bool stop = summary.verdict == SendVerdict::Stop;
        if (stop) {
            const Entry   &first = m_entries[summary.blockers.front()];
            const wxString who   = wxString::FromUTF8(first.plan.manifest.name.empty() ? first.plan.id : first.plan.manifest.name);
            set_header(GlyphBlock, role(MD3::Role::Error),
                       summary.blockers.size() == 1 ? wxString::Format(_L("%s stopped this print"), who)
                                                    : wxString::Format(_L("Stopped by %d extensions"), int(summary.blockers.size())),
                       m_send.all_plates ? _L("Nothing is sent") : wxString::Format(_L("Plate %d is not sent"), m_send.plate));
        } else {
            wxString parts;
            auto     add = [&parts](const wxString &part) {
                if (!parts.IsEmpty()) parts << dot();
                parts << part;
            };
            if (summary.passed > 0) add(wxString::Format(_L("Passed: %d"), summary.passed));
            if (summary.warned + summary.failed > 0) add(wxString::Format(_L("To look at: %d"), summary.warned + summary.failed));
            if (summary.notes > 0) add(wxString::Format(_L("Notes: %d"), summary.notes));
            if (summary.not_answered > 0) add(wxString::Format(_L("Did not answer: %d"), summary.not_answered));
            set_header(GlyphFactCheck, warning_colour(),
                       m_send.all_plates ? _L("Look before you send every plate")
                                         : wxString::Format(_L("Look before you send Plate %d"), m_send.plate),
                       parts);
        }

        m_list->Hide();
        m_progress->Hide();
        m_note->Hide();
        auto *scroll = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
        scroll->SetBackgroundColour(role(MD3::Role::Surface));
        scroll->SetScrollRate(0, FromDIP(12));
        auto *ss = new wxBoxSizer(wxVERTICAL);

        // The reasons first: they decide what happens.
        for (std::size_t i : summary.blockers) {
            const Entry &e      = m_entries[i];
            StaticBox   *banner = card(scroll, role(MD3::Role::ErrorContainer));
            auto        *bs     = new wxBoxSizer(wxVERTICAL);
            const wxString reason = e.result.answer.block_reason.empty() ? _L("It gave no reason.")
                                                                          : wxString::FromUTF8(e.result.answer.block_reason);
            add_line(banner, bs, GlyphBlock, role(MD3::Role::OnErrorContainer), reason);
            if (summary.blockers.size() > 1) {
                bs->AddSpacer(FromDIP(4));
                bs->Add(label(banner, Label::Body_12, e.name, role(MD3::Role::OnErrorContainer)), 0, wxLEFT, FromDIP(36));
            }
            bs->AddSpacer(FromDIP(10));
            banner->SetSizer(bs);
            ss->Add(banner, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
        }

        Host &host = Runtime::instance().host();
        for (const Entry &e : m_entries) {
            if (e.done && e.result.outcome == Outcome::Stopped) continue;
            StaticBox *box = card(scroll, role(MD3::Role::SurfaceContainerLow));
            auto      *bs  = new wxBoxSizer(wxVERTICAL);
            bs->Add(label(box, Label::Head_12, e.name, role(MD3::Role::OnSurfaceVariant)), 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
            if (!e.done || e.result.outcome != Outcome::Answered) {
                wxString text = wxString::Format(_L("%s did not answer: %s."), e.name,
                                                 e.done ? outcome_text(e.result.outcome, e.result.problem, e.result.plan.limits) : _L("Not started"));
                if (const Installed *x = host.find(e.plan.id); x && host.state(*x) == State::OffAfterFailures)
                    text << " " << wxString::Format(_L("It is off after %d failed runs in a row."), host.limits().off_after_failures);
                add_line(box, bs, GlyphError, role(MD3::Role::Error), text);
            } else {
                const Answer &a = e.result.answer;
                for (const ChecklistRow &row : a.checklist) {
                    const bool pass = row.state == "pass", warn = row.state == "warn";
                    add_line(box, bs, pass ? GlyphCheckCircle : warn ? GlyphWarning : GlyphCancel,
                             pass ? role(MD3::Role::Primary) : warn ? warning_colour() : role(MD3::Role::Error), wxString::FromUTF8(row.text));
                }
                for (const std::string &note : a.notes)
                    add_line(box, bs, GlyphInfo, role(MD3::Role::OnSurfaceVariant), wxString::FromUTF8(note), true);
                if (a.checklist.empty() && a.notes.empty() && !a.block)
                    add_line(box, bs, GlyphCheckCircle, role(MD3::Role::Primary), _L("Nothing to point out."), true);
            }
            bs->AddSpacer(FromDIP(12));
            box->SetSizer(bs);
            ss->Add(box, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
        }
        if (stop)
            ss->Add(label(scroll, Label::Body_12,
                          _L("You allowed this extension to stop prints (send.block). To send anyway, change the plate or the mapping, "
                             "or turn the extension off in Extensions and send again."),
                          role(MD3::Role::OnSurfaceVariant), 480),
                    0, wxEXPAND | wxTOP, FromDIP(2));
        scroll->SetSizer(ss);
        ss->Fit(scroll);
        const int display = wxDisplay::GetFromWindow(this);
        const int most    = wxDisplay(display == wxNOT_FOUND ? 0u : unsigned(display)).GetClientArea().GetHeight() * 3 / 5;
        const int want    = ss->GetMinSize().y;
        scroll->SetMinSize(wxSize(-1, std::min(want, std::max(FromDIP(160), most))));
        m_body->Add(scroll, 1, wxEXPAND);
        wxGetApp().UpdateDlgDarkUI(this);

        const std::string look = first_to_look_at(summary);
        if (stop)
            set_buttons({{_L("Open Extensions"), Button::Variant::Text, [this, look]() { finish(SendDecision::OpenExtensions, look); }},
                         {_L("Back"), Button::Variant::Filled, [this]() { back(); }}},
                        1);
        else
            set_buttons({{_L("Open Extensions"), Button::Variant::Text, [this, look]() { finish(SendDecision::OpenExtensions, look); }},
                         {_L("Back"), Button::Variant::Outlined, [this]() { back(); }},
                         {_L("Send"), Button::Variant::Filled, [this]() { finish(SendDecision::Send); }}},
                        1);
        relayout();
    }

    std::string first_to_look_at(const SendSummary &summary) const
    {
        if (!summary.blockers.empty()) return m_entries[summary.blockers.front()].plan.id;
        for (const Entry &e : m_entries)
            if (!e.done || (e.result.outcome != Outcome::Answered && e.result.outcome != Outcome::Stopped)) return e.plan.id;
        for (const Entry &e : m_entries)
            for (const ChecklistRow &row : e.result.answer.checklist)
                if (row.state != "pass") return e.plan.id;
        return m_entries.empty() ? std::string() : m_entries.front().plan.id;
    }

    void relayout()
    {
        Layout();
        Fit();
        CenterOnParent();
        Refresh();
    }

    // ---- leaving
    void back() { finish(SendDecision::Back); }

    void skip() { finish(SendDecision::Send); }

    void finish(SendDecision decision, const std::string &extension = std::string())
    {
        if (m_closing) return;
        m_closing = true;
        m_ticker.Stop();
        m_closer.Stop();
        if (!m_runs_done) Runtime::instance().stop(m_cancel);
        m_outcome = SendCheckOutcome{decision, extension};
        EndModal(decision == SendDecision::Send ? wxID_OK : wxID_CANCEL);
    }

    SendFacts                          m_send;
    nlohmann::json                     m_context;
    std::string                        m_subject;
    Limits                             m_limits;
    std::vector<Entry>                 m_entries;
    std::shared_ptr<std::atomic<bool>> m_cancel;
    std::shared_ptr<bool>              m_alive = std::make_shared<bool>(true);
    bool                               m_runs_done = false, m_closing = false;
    SendCheckOutcome                   m_outcome{SendDecision::Back, {}};
    wxTimer                            m_ticker, m_closer;

    wxBoxSizer  *m_root = nullptr, *m_body = nullptr, *m_buttons = nullptr;
    GlyphView   *m_head_glyph = nullptr;
    Label       *m_title = nullptr, *m_subtitle = nullptr, *m_note = nullptr;
    ProgressBar *m_progress = nullptr;
    StaticBox   *m_list = nullptr;
};

} // namespace

SendCheckOutcome check_before_send(wxWindow *parent, const SendFacts &send)
{
    Runtime                &runtime = Runtime::instance();
    std::vector<PlannedRun> runs    = runtime.plan_before_send();
    if (runs.empty()) return SendCheckOutcome{SendDecision::Send, {}};
    std::string      subject;
    nlohmann::json   context = runtime.before_send_context(send, subject);
    BeforeSendDialog dialog(parent, send, std::move(runs), std::move(context), std::move(subject));
    return dialog.run();
}

}}} // namespace Slic3r::GUI::Extensions
