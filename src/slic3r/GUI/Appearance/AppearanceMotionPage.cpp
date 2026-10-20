#include "AppearanceMotionPage.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

#include <wx/dcbuffer.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>

#include "ElementStyle.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/PreferencesHistory.hpp"
#include "slic3r/GUI/Presets/BlankEditorPresets.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/MD3Motion.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"
#include "slic3r/GUI/Widgets/MaterialIcon.hpp"
#include "slic3r/GUI/Widgets/SearchField.hpp"
#include "slic3r/GUI/Widgets/SpinInput.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/GUI/Widgets/StateMotionMath.hpp"
#include "slic3r/GUI/Widgets/TextInput.hpp"

namespace Slic3r { namespace GUI {

namespace {

constexpr int kWrapDip   = 320; // card width less padding and the scroll bar
constexpr int kFieldGap  = 4;
constexpr int kGroupGap  = 14;

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

Label *body(wxWindow *parent, const wxString &text, const wxFont &font = Label::Body_13)
{
    auto *l = new Label(parent, font, text, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    l->SetMinSize(wxSize(0, -1));
    l->Wrap(parent->FromDIP(kWrapDip));
    ElementStyle::apply(l, "appearance-editor.label", _L("Appearance editor label"));
    return l;
}

Label *heading(wxWindow *parent, const wxString &text)
{
    auto *l = new Label(parent, Label::Head_14, text, wxST_NO_AUTORESIZE);
    ElementStyle::apply(l, "appearance-editor.heading", _L("Appearance editor heading"));
    return l;
}

Label *note(wxWindow *parent, const wxString &text)
{
    auto *l = new Label(parent, Label::Body_12, text, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    l->SetMinSize(wxSize(0, -1));
    l->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    l->Wrap(parent->FromDIP(kWrapDip));
    return l;
}

Button *action(wxWindow *parent, const wxString &label, Button::Variant variant)
{
    auto *b = new Button(parent, label);
    b->SetVariant(variant);
    b->SetButtonSize(Button::Size::Small);
    b->SetName(label);
    // The editor's own buttons are elements too: their hover, focus and press
    // transitions follow motion set for this id.
    ElementStyle::apply(b, "appearance-editor.action", _L("Appearance editor button"));
    return b;
}

Button *reset_button(wxWindow *parent, const wxString &what)
{
    auto *b = new Button(parent, "", "", 0, 0, wxID_ANY);
    if (MaterialIcon::available()) {
        b->SetIconButton(Button::IconShape::Circle, 28);
        b->SetGlyph(MaterialIcon::Undo, 16);
    } else {
        b->SetLabel(wxString(wxUniChar(0x21BA)));
        b->SetVariant(Button::Variant::Text);
        b->SetButtonSize(Button::Size::Small);
    }
    b->SetName(wxString::Format(_L("Reset %s"), what));
    b->SetToolTip(wxString::Format(_L("Reset %s to the inherited or Material default value"), what));
    return b;
}

wxString field_label(ElementMotion::Field field)
{
    switch (field) {
    case ElementMotion::Field::Duration: return _L("duration");
    case ElementMotion::Field::Delay: return _L("delay");
    case ElementMotion::Field::Easing: return _L("easing");
    }
    return {};
}

wxString number_text(double value)
{
    return std::floor(value) == value ? wxString::Format("%lld", static_cast<long long>(value))
                                      : wxString::FromCDouble(value, 2);
}

// The registry's own value for (id, key) without any layer: null when unset.
nlohmann::json user_value(const std::string &id, const std::string &key)
{
    const StyleBag *bag = ElementStyle::registry().user_bag(id);
    if (!bag || !bag->is_object())
        return nlohmann::json();
    const auto it = bag->find(key);
    return it == bag->end() ? nlohmann::json() : *it;
}

} // namespace

// ---------------------------------------------------------------------------
// One timing (duration, delay, easing) for one element and trigger.
// ---------------------------------------------------------------------------
class MotionTimingEditor : public wxPanel
{
public:
    using Writer = std::function<void(ElementMotion::Field, const nlohmann::json &)>;

    MotionTimingEditor(wxWindow *parent, const wxString &context, Writer writer)
        : wxPanel(parent, wxID_ANY), m_context(context), m_write(std::move(writer))
    {
        SetBackgroundColour(parent->GetBackgroundColour());
        SetName(context);
        auto *s = new wxBoxSizer(wxVERTICAL);

        auto add_row = [&](const wxString &label, wxWindow *control, ElementMotion::Field field) {
            s->Add(body(this, label), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
            auto *row = new wxBoxSizer(wxHORIZONTAL);
            row->Add(control, 1, wxALIGN_CENTER_VERTICAL);
            Button *reset = reset_button(this, field_label(field));
            reset->Bind(wxEVT_BUTTON, [this, field](wxCommandEvent &) { m_write(field, nlohmann::json()); });
            m_reset[static_cast<size_t>(field)] = reset;
            row->Add(reset, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(kFieldGap));
            s->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(MD3::Metrics::active().gap));
        };

        m_duration = new SpinInput(this, "0", "", wxDefaultPosition, wxSize(FromDIP(110), -1), wxTE_PROCESS_ENTER, 0,
                                   static_cast<int>(ElementMotion::kMaxDurationMs), 0);
        m_duration->SetName(wxString::Format(_L("%s duration in milliseconds"), context));
        m_duration->SetToolTip(_L("How long the transition runs, from 0 to 10000 milliseconds."));
        m_duration->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) {
            if (!m_loading)
                m_write(ElementMotion::Field::Duration, m_duration->GetValue());
        });
        add_row(_L("Duration (ms)"), m_duration, ElementMotion::Field::Duration);

        m_delay = new SpinInput(this, "0", "", wxDefaultPosition, wxSize(FromDIP(110), -1), wxTE_PROCESS_ENTER,
                                static_cast<int>(ElementMotion::kMinDelayMs), static_cast<int>(ElementMotion::kMaxDelayMs), 0);
        m_delay->SetName(wxString::Format(_L("%s delay in milliseconds"), context));
        m_delay->SetToolTip(_L("Wait before the transition starts. A negative delay starts it part of the way through."));
        m_delay->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) {
            if (!m_loading)
                m_write(ElementMotion::Field::Delay, m_delay->GetValue());
        });
        add_row(_L("Delay (ms)"), m_delay, ElementMotion::Field::Delay);

        m_curve = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(200), -1), 0, nullptr,
                               wxCB_READONLY);
        for (const ElementMotion::NamedCurve &curve : ElementMotion::named_curves())
            m_curve->Append(_L(curve.label));
        m_curve->Append(_L("Custom (type it below)"));
        m_curve->SetName(wxString::Format(_L("%s easing curve"), context));
        m_curve->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) {
            const int sel = m_curve->GetSelection();
            const auto &curves = ElementMotion::named_curves();
            if (m_loading || sel < 0)
                return;
            if (sel < static_cast<int>(curves.size()))
                m_write(ElementMotion::Field::Easing, std::string(curves[static_cast<size_t>(sel)].name));
            else
                m_text->GetTextCtrl()->SetFocus(); // custom: type cubic-bezier(), steps() or linear()
        });
        add_row(_L("Easing"), m_curve, ElementMotion::Field::Easing);

        s->Add(body(this, _L("Easing function")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
        m_text = new ::TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition,
                                 wxSize(FromDIP(200), -1), wxTE_PROCESS_ENTER);
        m_text->SetName(wxString::Format(_L("%s easing function"), context));
        m_text->GetTextCtrl()->SetName(m_text->GetName());
        m_text->SetToolTip(_L("A named curve, cubic-bezier(x1, y1, x2, y2), steps(count, jump-end) or a linear() stop list. Press Enter to apply."));
        m_text->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { commit_text(); });
        m_text->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
            commit_text();
            e.Skip();
        });
        s->Add(m_text, 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));

        m_issue = note(this, wxEmptyString);
        m_issue->SetForegroundColour(role(MD3::Role::Error));
        m_issue->SetName(_L("Motion value problem"));
        s->Add(m_issue, 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
        m_summary = note(this, wxEmptyString);
        m_summary->SetName(wxString::Format(_L("%s timing summary"), context));
        s->Add(m_summary, 0, wxEXPAND);
        SetSizer(s);
    }

    // Show what element `id` plays for `trigger`, whose consumer default is
    // `default_ms`. An empty id disables every field.
    void show(const std::string &id, ElementMotion::Trigger trigger, int default_ms)
    {
        m_loading = true;
        const ElementMotion::Resolved state = ElementStyle::motion_state(id, trigger, default_ms);
        m_timing = state.timing;
        m_duration->SetValue(static_cast<int>(std::lround(state.timing.duration_ms)));
        m_delay->SetValue(static_cast<int>(std::lround(state.timing.delay_ms)));
        const std::string easing = state.timing.easing.to_string();
        const auto &curves = ElementMotion::named_curves();
        int selection = static_cast<int>(curves.size()); // custom
        for (size_t i = 0; i < curves.size(); ++i)
            if (easing == curves[i].name)
                selection = static_cast<int>(i);
        m_curve->SetSelection(selection);
        m_text->GetTextCtrl()->ChangeValue(wxString::FromUTF8(easing));

        // Where each value comes from, and any stored value that was refused.
        wxString sources, issues;
        const StyleRegistry &reg = ElementStyle::registry();
        for (const ElementMotion::FieldState &field : state.fields) {
            const std::string key = ElementMotion::property_key(trigger, field.field);
            const bool own = !id.empty() && reg.has_override(id, key);
            m_reset[static_cast<size_t>(field.field)]->Enable(own);
            wxString value;
            switch (field.field) {
            case ElementMotion::Field::Duration: value = wxString::Format(_L("%s ms"), number_text(state.timing.duration_ms)); break;
            case ElementMotion::Field::Delay: value = wxString::Format(_L("%s ms"), number_text(state.timing.delay_ms)); break;
            case ElementMotion::Field::Easing: value = wxString::FromUTF8(easing); break;
            }
            const wxString source = !field.stored || !field.valid ? _L("Material default")
                                    : own                         ? _L("set for this element")
                                                                  : _L("inherited");
            if (!sources.empty())
                sources << "\n";
            sources << wxString::Format(_L("%s: %s (%s)"), field_label(field.field), value, source);
            if (field.stored && !field.valid)
                issues << wxString::Format(_L("The stored %s %s is not used: %s"), field_label(field.field),
                                           wxString::FromUTF8(field.value.dump()), _L(field.issue))
                       << "\n";
        }
        sources << "\n"
                << wxString::Format(_L("Total active time: %d ms. It plays once and ends on the new state."),
                                    state.timing.total_ms());
        m_summary->SetLabel(sources);
        m_summary->Wrap(FromDIP(kWrapDip));
        m_issue->SetLabel(issues.Trim());
        m_issue->Wrap(FromDIP(kWrapDip));
        m_issue->Show(!issues.empty());
        Enable(!id.empty());
        m_loading = false;
    }

    const ElementMotion::Timing &timing() const { return m_timing; }

private:
    void commit_text()
    {
        if (m_loading)
            return;
        const std::string text(m_text->GetTextCtrl()->GetValue().ToUTF8().data());
        const char *issue = nullptr;
        const auto easing = ElementMotion::parse_easing(text, &issue);
        if (!easing) {
            // Refused: the element keeps playing its current easing.
            m_issue->SetLabel(wxString::Format(_L("Not applied: %s"), _L(issue)));
            m_issue->Wrap(FromDIP(kWrapDip));
            m_issue->Show();
            GetParent()->Layout();
            return;
        }
        if (easing->to_string() != m_timing.easing.to_string())
            m_write(ElementMotion::Field::Easing, easing->to_string());
        else
            m_text->GetTextCtrl()->ChangeValue(wxString::FromUTF8(easing->to_string()));
    }

    wxString                m_context;
    Writer                  m_write;
    bool                    m_loading { false };
    ElementMotion::Timing   m_timing;
    SpinInput *             m_duration { nullptr };
    SpinInput *             m_delay { nullptr };
    ComboBox *              m_curve { nullptr };
    ::TextInput *           m_text { nullptr };
    Label *                 m_issue { nullptr };
    Label *                 m_summary { nullptr };
    std::array<Button *, 3> m_reset { {nullptr, nullptr, nullptr} };
};

// ---------------------------------------------------------------------------
// Curve, time ruler and playhead, played with the element's own arithmetic.
// ---------------------------------------------------------------------------
class MotionCurvePreview : public wxPanel
{
public:
    explicit MotionCurvePreview(wxWindow *parent) : wxPanel(parent, wxID_ANY)
    {
        SetName(_L("Motion curve preview"));
        SetMinSize(wxSize(FromDIP(260), FromDIP(150)));
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, [this](wxPaintEvent &) { paint(); });
    }
    ~MotionCurvePreview() override { m_anim.Stop(); }

    void set_timing(const ElementMotion::Timing &authored, const ElementMotion::Timing &defaults)
    {
        m_authored = authored;
        m_default  = defaults;
        Refresh(false);
    }

    int total_ms() const { return m_authored.total_ms(); }
    double head_ms() const { return m_head * m_authored.total_ms(); }

    void seek_ms(double ms)
    {
        m_anim.Stop();
        const int total = m_authored.total_ms();
        m_head = total > 0 ? std::clamp(ms / total, 0.0, 1.0) : 1.0;
        Refresh(false);
    }

    // Plays the selected transition at `speed` (1, 0.5 or 0.25). Under reduced
    // motion the animator settles at once, so the final state shows directly.
    void play(bool reversed, double speed)
    {
        m_reversed = reversed;
        const int length = static_cast<int>(std::lround(m_authored.total_ms() / std::max(0.25, speed)));
        m_anim.Play(length, [this](double t) {
            m_head = t;
            Refresh(false);
        }, [this]() { if (on_finished) on_finished(); }, &MD3::Motion::easeLinear, this);
    }

    void stop() { m_anim.Stop(); }

    std::function<void()> on_finished;

private:
    void paint()
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
        dc.Clear();
        const wxRect all = GetClientRect();
        const int pad = FromDIP(8), ruler = FromDIP(16), strip = FromDIP(14);
        wxRect plot(all.x + pad, all.y + pad, all.width - 2 * pad, all.height - 2 * pad - ruler - strip - pad);
        if (plot.width < 20 || plot.height < 20)
            return;
        dc.SetPen(wxPen(role(MD3::Role::OutlineVariant), 1));
        dc.SetBrush(wxBrush(role(MD3::Role::SurfaceContainerHighest)));
        dc.DrawRoundedRectangle(plot, FromDIP(6));

        const int total   = std::max(1, std::max(m_authored.total_ms(), m_default.total_ms()));
        // Room for overshoot: the vertical range covers every sampled value.
        double lo = 0.0, hi = 1.0;
        const int samples = std::max(32, plot.width / 2);
        for (int i = 0; i <= samples; ++i) {
            const double v = sample(m_authored, total, i * double(total) / samples);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        auto x_of = [&](double ms) { return plot.x + static_cast<int>(std::lround(ms / total * plot.width)); };
        auto y_of = [&](double v) {
            return plot.GetBottom() - static_cast<int>(std::lround((v - lo) / (hi - lo) * plot.height));
        };

        // A positive delay holds the start: shaded.
        if (m_authored.delay_ms > 0) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(role(MD3::Role::SurfaceContainerHigh)));
            dc.DrawRectangle(wxRect(plot.x + 1, plot.y + 1, std::max(0, x_of(m_authored.delay_ms) - plot.x - 1), plot.height - 2));
        }
        // 0 and 1 guides.
        dc.SetPen(wxPen(role(MD3::Role::OutlineVariant), 1, wxPENSTYLE_DOT));
        dc.DrawLine(plot.x, y_of(0), plot.GetRight(), y_of(0));
        dc.DrawLine(plot.x, y_of(1), plot.GetRight(), y_of(1));

        auto draw_curve = [&](const ElementMotion::Timing &timing, const wxPen &pen) {
            dc.SetPen(pen);
            wxPoint previous;
            for (int i = 0; i <= samples; ++i) {
                const double ms = i * double(total) / samples;
                const wxPoint p(x_of(ms), y_of(sample(timing, total, ms)));
                if (i)
                    dc.DrawLine(previous, p);
                previous = p;
            }
        };
        // Before (the Material default) dashed, after (this element) solid.
        draw_curve(m_default, wxPen(role(MD3::Role::Outline), 1, wxPENSTYLE_SHORT_DASH));
        draw_curve(m_authored, wxPen(role(MD3::Role::Primary), FromDIP(2)));

        // Playhead.
        const double head_ms = m_head * m_authored.total_ms();
        const double value   = sample(m_authored, total, head_ms);
        dc.SetPen(wxPen(role(MD3::Role::OnSurface), 1));
        dc.DrawLine(x_of(head_ms), plot.y, x_of(head_ms), plot.GetBottom());
        dc.SetBrush(wxBrush(role(MD3::Role::OnSurface)));
        dc.DrawCircle(x_of(head_ms), y_of(value), FromDIP(4));

        // Time ruler.
        dc.SetFont(Label::Body_10);
        dc.SetTextForeground(role(MD3::Role::OnSurfaceVariant));
        static const int steps[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000};
        int step = steps[0];
        for (int candidate : steps) {
            step = candidate;
            if (total / candidate <= 5)
                break;
        }
        dc.SetPen(wxPen(role(MD3::Role::OutlineVariant), 1));
        for (int ms = 0; ms <= total; ms += step) {
            const int x = x_of(ms);
            dc.DrawLine(x, plot.GetBottom() + 1, x, plot.GetBottom() + FromDIP(4));
            const wxString text = wxString::Format(_L("%d ms"), ms);
            const int w = dc.GetTextExtent(text).x;
            dc.DrawText(text, std::max(plot.x, std::min(x - w / 2, plot.GetRight() - w)), plot.GetBottom() + FromDIP(4));
        }

        // The colour the element shows at the playhead (reversed: the way back).
        const wxColour start = role(MD3::Role::SurfaceContainerHighest), end = role(MD3::Role::Primary);
        const wxColour &from = m_reversed ? end : start;
        const wxColour &to   = m_reversed ? start : end;
        const double    p    = m_authored.progress(m_head);
        const wxColour  shown(MD3::Motion::color_channel(from.Red(), to.Red(), p),
                              MD3::Motion::color_channel(from.Green(), to.Green(), p),
                              MD3::Motion::color_channel(from.Blue(), to.Blue(), p));
        const wxRect swatch(plot.x, all.GetBottom() - pad - strip + 1, plot.width, strip);
        dc.SetPen(wxPen(role(MD3::Role::Outline), 1));
        dc.SetBrush(wxBrush(shown));
        dc.DrawRoundedRectangle(swatch, FromDIP(4));
    }

    // Progress of `timing` at absolute time `ms` on a shared axis.
    static double sample(const ElementMotion::Timing &timing, int /*axis*/, double ms)
    {
        const int total = timing.total_ms();
        if (total <= 0)
            return 1.0;
        return ms >= total ? 1.0 : timing.progress(ms / total);
    }

    ElementMotion::Timing m_authored;
    ElementMotion::Timing m_default;
    double                m_head { 1.0 };
    bool                  m_reversed { false };
    MD3::Motion::Anim     m_anim;
};

// ---------------------------------------------------------------------------
// The page
// ---------------------------------------------------------------------------
const char *AppearanceMotionPage::editor_card_id() { return "appearance-editor"; }

AppearanceMotionPage::AppearanceMotionPage(wxWindow *parent, const std::string &element_id,
                                           std::function<wxWindow *()> anchor)
    : wxPanel(parent, wxID_ANY), m_id(element_id), m_anchor(std::move(anchor))
{
    SetBackgroundColour(parent->GetBackgroundColour());
    SetName(_L("Animation and transition"));
    build();
    m_registry_token = ElementStyle::registry().subscribe([this](const std::string &changed) {
        if (changed == StyleRegistry::kEveryElement || changed == m_id || changed == m_entrance_id ||
            (m_id.size() > changed.size() && m_id.compare(0, changed.size(), changed) == 0 && m_id[changed.size()] == '/'))
            schedule_refresh();
    });
    // The global motion setting can change while the editor is open.
    m_reduced_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
        if (MD3::Motion::reduced() != m_last_reduced)
            refresh_reduced();
    });
    m_reduced_timer.Start(1000);
    refresh();
}

AppearanceMotionPage::~AppearanceMotionPage()
{
    m_reduced_timer.Stop();
    ElementStyle::registry().unsubscribe(m_registry_token);
}

void AppearanceMotionPage::retarget() { refresh(); }

void AppearanceMotionPage::build()
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    const int gap = FromDIP(MD3::Metrics::active().gap);

    // The global authority first: nothing below can turn motion back on.
    m_reduced_note = body(this, wxEmptyString);
    m_reduced_note->SetName(_L("Interface motion status"));
    s->Add(m_reduced_note, 0, wxEXPAND | wxBOTTOM, FromDIP(kGroupGap));

    // State transitions of this element.
    s->Add(heading(this, _L("State transitions")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_state_target = note(this, wxEmptyString);
    s->Add(m_state_target, 0, wxEXPAND | wxBOTTOM, gap);
    s->Add(body(this, _L("Trigger")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_trigger = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(200), -1), 0, nullptr,
                             wxCB_READONLY);
    for (ElementMotion::Trigger trigger : ElementMotion::state_triggers())
        m_trigger->Append(_L(ElementMotion::trigger_label(trigger)));
    m_trigger->SetSelection(0);
    m_trigger->SetName(_L("State change to edit"));
    m_trigger->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { refresh(); });
    s->Add(m_trigger, 0, wxEXPAND | wxBOTTOM, gap);
    m_state_editor = new MotionTimingEditor(this, _L("State transition"),
        [this](ElementMotion::Field field, const nlohmann::json &value) { write(m_id, state_trigger(), field, value); });
    s->Add(m_state_editor, 0, wxEXPAND | wxBOTTOM, FromDIP(kGroupGap));

    // Preview with the same arithmetic.
    s->Add(heading(this, _L("Preview")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_preview = new MotionCurvePreview(this);
    m_preview->on_finished = [this]() {
        if (m_preview_position)
            m_preview_position->SetValue(m_preview->total_ms());
    };
    s->Add(m_preview, 0, wxEXPAND | wxBOTTOM, gap);
    auto *controls = new wxWrapSizer(wxHORIZONTAL);
    auto speed = [this]() {
        static const double speeds[] = {1.0, 0.5, 0.25};
        const int sel = m_preview_speed ? m_preview_speed->GetSelection() : 0;
        return speeds[std::clamp(sel, 0, 2)];
    };
    Button *play = action(this, _L("Play"), Button::Variant::Tonal);
    play->SetToolTip(_L("Play this transition in the preview. Playing again restarts it."));
    play->Bind(wxEVT_BUTTON, [this, speed](wxCommandEvent &) {
        refresh_reduced();
        m_preview->play(false, speed());
    });
    Button *back = action(this, _L("Play reversed"), Button::Variant::Outlined);
    back->SetToolTip(_L("Play the way back toward the starting colour, as leaving the state does."));
    back->Bind(wxEVT_BUTTON, [this, speed](wxCommandEvent &) {
        refresh_reduced();
        m_preview->play(true, speed());
    });
    Button *stop = action(this, _L("Stop"), Button::Variant::Text);
    stop->SetToolTip(_L("Stop the preview where it is."));
    stop->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_preview->stop();
        if (m_preview_position)
            m_preview_position->SetValue(static_cast<int>(std::lround(m_preview->head_ms())));
    });
    for (Button *b : {play, back, stop})
        controls->Add(b, 0, wxRIGHT | wxBOTTOM, FromDIP(6));
    s->Add(controls, 0, wxEXPAND);
    s->Add(body(this, _L("Preview speed")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_preview_speed = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(200), -1), 0,
                                   nullptr, wxCB_READONLY);
    m_preview_speed->Append(_L("Full speed"));
    m_preview_speed->Append(_L("Half speed"));
    m_preview_speed->Append(_L("Quarter speed"));
    m_preview_speed->SetSelection(0);
    m_preview_speed->SetName(_L("Preview speed"));
    m_preview_speed->SetToolTip(_L("Slows the preview only. The element always plays its own timing."));
    s->Add(m_preview_speed, 0, wxEXPAND | wxBOTTOM, gap);
    s->Add(body(this, _L("Preview position (ms)")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_preview_position = new SpinInput(this, "0", "", wxDefaultPosition, wxSize(FromDIP(110), -1), wxTE_PROCESS_ENTER,
                                       0, static_cast<int>(ElementMotion::kMaxDurationMs + ElementMotion::kMaxDelayMs), 0);
    m_preview_position->SetName(_L("Preview position in milliseconds"));
    m_preview_position->SetToolTip(_L("Seek the preview to this moment of the transition."));
    m_preview_position->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) { m_preview->seek_ms(m_preview_position->GetValue()); });
    s->Add(m_preview_position, 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_preview_note = note(this, _L("Solid: this element. Dashed: the Material default. The shaded part is the delay; the strip shows the colour at the playhead."));
    s->Add(m_preview_note, 0, wxEXPAND | wxBOTTOM, FromDIP(kGroupGap));

    // The opening fade of the window that contains the element.
    s->Add(heading(this, _L("Window entrance")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_entrance_note = note(this, wxEmptyString);
    s->Add(m_entrance_note, 0, wxEXPAND | wxBOTTOM, gap);
    m_entrance_editor = new MotionTimingEditor(this, _L("Window entrance"),
        [this](ElementMotion::Field field, const nlohmann::json &value) {
            write(m_entrance_id, ElementMotion::Trigger::Entrance, field, value);
        });
    s->Add(m_entrance_editor, 0, wxEXPAND | wxBOTTOM, FromDIP(kGroupGap));

    // Undo, redo and reset.
    s->Add(heading(this, _L("Changes")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    auto *changes = new wxWrapSizer(wxHORIZONTAL);
    m_undo = action(this, _L("Undo"), Button::Variant::Outlined);
    m_undo->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (auto edit = m_history.undo())
            apply_edit(*edit, false);
    });
    m_redo = action(this, _L("Redo"), Button::Variant::Outlined);
    m_redo->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (auto edit = m_history.redo())
            apply_edit(*edit, true);
    });
    m_reset_motion = action(this, _L("Reset element motion"), Button::Variant::Text);
    m_reset_motion->SetToolTip(_L("Drop every motion value of this element; inherited and Material defaults apply again"));
    m_reset_motion->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { reset_element_motion(); });
    for (Button *b : {m_undo, m_redo, m_reset_motion})
        changes->Add(b, 0, wxRIGHT | wxBOTTOM, FromDIP(6));
    s->Add(changes, 0, wxEXPAND);
    s->Add(note(this, _L("Every change is saved at once and kept in Version history, where any earlier appearance can be restored.")),
           0, wxEXPAND | wxBOTTOM, FromDIP(kGroupGap));

    // Values kept but not applied.
    s->Add(heading(this, _L("Kept but not applied")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_kept = note(this, wxEmptyString);
    m_kept->SetName(_L("Motion values kept but not applied"));
    s->Add(m_kept, 0, wxEXPAND | wxBOTTOM, FromDIP(kGroupGap));

    build_capabilities(this, s);
    SetSizer(s);
}

void AppearanceMotionPage::build_capabilities(wxWindow *parent, wxBoxSizer *s)
{
    s->Add(heading(parent, _L("Renderer capabilities")), 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    int wired = 0, unavailable = 0;
    for (const ElementMotion::Capability &row : ElementMotion::capabilities())
        (row.support == ElementMotion::Support::Wired ? wired : unavailable) += 1;
    m_capability_count = note(parent, wxString::Format(_L("%d motion capabilities work on this element's renderer; %d are unavailable and listed with the reason."),
                                                       wired, unavailable));
    s->Add(m_capability_count, 0, wxEXPAND | wxBOTTOM, FromDIP(kFieldGap));
    m_capability_search = new SearchField(parent, _L("Search motion capabilities"));
    m_capability_search->SetOnQuery([this](const wxString &) { filter_capabilities(); });
    m_capability_search->SetOnRegexToggle([this](bool) { filter_capabilities(); });
    s->Add(m_capability_search, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    const char *area = nullptr;
    for (const ElementMotion::Capability &row : ElementMotion::capabilities()) {
        if (!area || std::string(area) != row.area) {
            area = row.area;
            auto *h = new Label(parent, Label::Head_13, _L(ElementMotion::area_label(row.area)), wxST_NO_AUTORESIZE);
            s->Add(h, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(kFieldGap));
            m_capability_areas.emplace_back(row.area, h);
        }
        const bool wired_row = row.support == ElementMotion::Support::Wired;
        const wxString title = wxString::Format(wired_row ? _L("Available: %s") : _L("Unavailable: %s"), _L(row.title));
        auto *t = new Label(parent, Label::Body_13, title, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
        t->SetMinSize(wxSize(0, -1));
        t->Wrap(FromDIP(kWrapDip));
        // An unavailable capability is a visible, disabled entry with its reason.
        if (!wired_row)
            t->Disable();
        Label *d = note(parent, _L(row.detail));
        s->Add(t, 0, wxEXPAND);
        s->Add(d, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
        m_capability_rows.push_back({row.area, t, d, title + "\n" + _L(row.detail)});
    }
}

void AppearanceMotionPage::filter_capabilities()
{
    const wxString query = m_capability_search->GetValue();
    SearchField::MatchPass pass(query, m_capability_search->IsRegexEnabled(), m_capability_search->IsCaseSensitive(),
                                m_capability_search->IsWholeWord(), m_capability_search->IsMultiline());
    std::map<std::string, bool> any_in_area;
    for (CapabilityRow &row : m_capability_rows) {
        const bool shown = query.IsEmpty() || pass.matches(row.text);
        row.title->Show(shown);
        row.detail->Show(shown);
        any_in_area[row.area] = any_in_area[row.area] || shown;
    }
    for (auto &area : m_capability_areas)
        area.second->Show(any_in_area[area.first]);
    relayout();
}

void AppearanceMotionPage::relayout()
{
    Layout();
    if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(GetParent())) {
        scrolled->Layout();
        scrolled->FitInside();
    }
}

ElementMotion::Trigger AppearanceMotionPage::state_trigger() const
{
    const auto &states = ElementMotion::state_triggers();
    const int sel = m_trigger ? m_trigger->GetSelection() : 0;
    return states[static_cast<size_t>(std::clamp(sel, 0, static_cast<int>(states.size()) - 1))];
}

void AppearanceMotionPage::schedule_refresh()
{
    if (m_refresh_pending)
        return;
    m_refresh_pending = true;
    CallAfter([this]() {
        m_refresh_pending = false;
        refresh();
    });
}

void AppearanceMotionPage::refresh()
{
    refresh_reduced();
    if (m_state_target)
        m_state_target->SetLabel(wxString::Format(_L("For %s (%s). Buttons, fields, lists and cards play these when this state changes."),
                                                  ElementStyle::display_name_of(m_id), wxString::FromUTF8(m_id)));
    if (m_state_target)
        m_state_target->Wrap(FromDIP(kWrapDip));
    m_state_editor->show(m_id, state_trigger(), MD3::Motion::short2);
    refresh_entrance();
    refresh_preview();
    refresh_kept();
    refresh_history_buttons();
    relayout();
}

void AppearanceMotionPage::refresh_reduced()
{
    m_last_reduced = MD3::Motion::reduced();
    m_reduced_note->SetLabel(m_last_reduced
        ? _L("Reduced motion is on (Preferences > Interface motion or the Windows animation setting). Every transition shows its final state at once, the preview included. The values below are kept for when motion is allowed again.")
        : _L("Motion plays as set below. Preferences > Interface motion and the Windows animation setting decide first: when either reduces motion, every transition shows its final state at once."));
    m_reduced_note->Wrap(FromDIP(kWrapDip));
}

void AppearanceMotionPage::refresh_entrance()
{
    wxWindow *anchor = m_anchor ? m_anchor() : nullptr;
    wxWindow *top    = anchor ? wxGetTopLevelParent(anchor) : nullptr;
    const ElementStyle::EntranceInfo info = ElementStyle::entrance_of(top);
    m_entrance_id      = info.has_entrance ? info.id : std::string();
    m_entrance_default = info.default_duration_ms;
    wxString text;
    if (!info.has_entrance)
        text = _L("The window that contains this element opens without an entrance animation, so there is nothing to edit here. Dialogs with the Material caption, the command palette, the regex builder, menus and this editor fade in.");
    else if (info.id.empty())
        text = wxString::Format(_L("The window that contains this element fades in over %d ms, but it is not an adopted element, so its entrance cannot be edited yet."),
                                info.default_duration_ms);
    else
        text = wxString::Format(_L("Entrance of %s (%s): the window fades in from a quarter of full opacity when it opens."),
                                ElementStyle::display_name_of(info.id), wxString::FromUTF8(info.id));
    m_entrance_note->SetLabel(text);
    m_entrance_note->Wrap(FromDIP(kWrapDip));
    m_entrance_editor->show(m_entrance_id, ElementMotion::Trigger::Entrance,
                            info.has_entrance ? info.default_duration_ms : MD3::Motion::medium1);
    m_entrance_editor->Show(!m_entrance_id.empty());
}

void AppearanceMotionPage::refresh_preview()
{
    m_preview->stop();
    m_preview->set_timing(m_state_editor->timing(), ElementMotion::material_default(MD3::Motion::short2));
    m_preview->seek_ms(m_preview->total_ms());
    m_preview_position->SetRange(0, std::max(0, m_preview->total_ms()));
    m_preview_position->SetValue(m_preview->total_ms());
}

void AppearanceMotionPage::refresh_kept()
{
    wxString text;
    const StyleBag *bag = ElementStyle::registry().user_bag(m_id);
    if (bag && bag->is_object()) {
        for (const std::string &key : ElementMotion::unknown_motion_keys(*bag))
            text << wxString::Format(_L("%s = %s: this version does not use this motion setting. It stays in the file for export and import."),
                                     wxString::FromUTF8(key), wxString::FromUTF8(bag->at(key).dump()))
                 << "\n";
        for (auto it = bag->begin(); it != bag->end(); ++it) {
            const ElementMotion::Check check = ElementMotion::validate_key(it.key(), it.value());
            if (!check.ok)
                text << wxString::Format(_L("%s = %s: %s The Material default plays instead."), wxString::FromUTF8(it.key()),
                                         wxString::FromUTF8(it.value().dump()), _L(check.issue))
                     << "\n";
        }
    }
    text.Trim();
    m_kept->SetLabel(text.IsEmpty() ? _L("Nothing is waiting here: every stored motion value of this element is in use.") : text);
    m_kept->Wrap(FromDIP(kWrapDip));
}

wxString AppearanceMotionPage::describe(const ElementMotion::Edit &edit) const
{
    if (edit.changes.empty())
        return {};
    if (edit.changes.size() > 1 || edit.label == "reset")
        return wxString::Format(_L("reset motion of %s"), ElementStyle::display_name_of(edit.changes.front().id));
    const ElementMotion::Change &change = edit.changes.front();
    const auto parts = ElementMotion::parse_key(change.key);
    if (!parts)
        return wxString::FromUTF8(change.key);
    return wxString::Format(_L("%s %s of %s"), _L(ElementMotion::trigger_label(parts->trigger)), field_label(parts->field),
                            ElementStyle::display_name_of(change.id));
}

void AppearanceMotionPage::refresh_history_buttons()
{
    m_undo->Enable(m_history.can_undo());
    m_redo->Enable(m_history.can_redo());
    m_undo->SetToolTip(m_history.can_undo() ? wxString::Format(_L("Undo: %s"), describe(*m_history.next_undo()))
                                            : _L("Nothing to undo"));
    m_redo->SetToolTip(m_history.can_redo() ? wxString::Format(_L("Redo: %s"), describe(*m_history.next_redo()))
                                            : _L("Nothing to redo"));
    bool any = false;
    if (const StyleBag *bag = ElementStyle::registry().user_bag(m_id))
        for (auto it = bag->begin(); it != bag->end(); ++it)
            any = any || ElementMotion::is_motion_namespace(it.key());
    m_reset_motion->Enable(any);
}

void AppearanceMotionPage::write(const std::string &id, ElementMotion::Trigger trigger, ElementMotion::Field field,
                                 const nlohmann::json &value)
{
    if (id.empty())
        return;
    const std::string    key    = ElementMotion::property_key(trigger, field);
    const nlohmann::json before = user_value(id, key);
    if (before == value)
        return;
    // Typed values are validated by their fields; this is the last guard.
    if (!value.is_null() && !ElementMotion::validate(field, value).ok)
        return;
    m_history.record({key, {{id, key, before, value}}});
    StyleRegistry &reg = ElementStyle::registry();
    if (value.is_null())
        reg.reset_property(id, key);
    else
        reg.set(id, key, value);
    persist();
}

void AppearanceMotionPage::apply_edit(const ElementMotion::Edit &edit, bool forward)
{
    if (edit.changes.empty())
        return;
    PreferencesHistory::begin_appearance_action(BlankEditorPresets::history_label(
        forward ? "Redo element motion" : "Undo element motion", edit.changes.front().id));
    StyleRegistry &reg = ElementStyle::registry();
    for (const ElementMotion::Change &change : edit.changes) {
        const nlohmann::json &value = forward ? change.after : change.before;
        if (value.is_null())
            reg.reset_property(change.id, change.key);
        else
            reg.set(change.id, change.key, value);
    }
    persist();
    refresh_history_buttons();
}

void AppearanceMotionPage::reset_element_motion()
{
    const StyleBag *bag = ElementStyle::registry().user_bag(m_id);
    if (!bag || !bag->is_object())
        return;
    ElementMotion::Edit edit;
    edit.label = "reset";
    // Every motion value, understood or not: a reset leaves nothing behind.
    for (auto it = bag->begin(); it != bag->end(); ++it)
        if (ElementMotion::is_motion_namespace(it.key()))
            edit.changes.push_back({m_id, it.key(), it.value(), nlohmann::json()});
    if (edit.changes.empty())
        return;
    PreferencesHistory::begin_appearance_action(BlankEditorPresets::history_label("Reset element motion", m_id));
    m_history.record(edit);
    StyleRegistry &reg = ElementStyle::registry();
    for (const ElementMotion::Change &change : edit.changes)
        reg.reset_property(change.id, change.key);
    persist();
}

void AppearanceMotionPage::persist()
{
    refresh_history_buttons();
    // One save per event-loop turn, like the rest of the editor.
    if (m_save_pending)
        return;
    m_save_pending = true;
    CallAfter([this]() {
        m_save_pending = false;
        ElementStyle::save();
    });
}

}} // namespace Slic3r::GUI
