#include "SuperConfirmGate.hpp"

#include <algorithm>
#include <cmath>

#include <wx/access.h>
#include <wx/app.h>
#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/evtloop.h>
#include <wx/panel.h>
#include <wx/sizer.h>

#include "Button.hpp"
#include "Label.hpp"
#include "MD3DialogChrome.hpp"
#include "MD3Tokens.hpp"
#include "MaterialIcon.hpp"
#include "SlideToConfirm.hpp"
#include "StateColor.hpp"
#include "slic3r/GUI/I18N.hpp"

namespace Slic3r { namespace GUI {

namespace {

constexpr int kMinWidthDip    = 320;  // narrow-width floor
constexpr int kMaxWidthDip    = 460;
constexpr int kKeySizeDip     = 56;   // key switch touch target
constexpr int kChargeHeight   = 14;   // hazard charge bar
constexpr int kMaxListedNames = 8;    // affected names shown before "and N more"

// ---------------------------------------------------------------------------
// KeySwitch: a focusable painted rotary key. Click, Space or Enter rotates the
// key slot from vertical (off) to horizontal (on). Reported to assistive
// technology as a check button with its configured name and checked state, so
// the two keys are always announced as two separate, independently operated
// controls.
class KeySwitch final : public wxWindow
{
public:
    KeySwitch(wxWindow *parent, const wxString &accessible_name, std::function<void()> on_toggle)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
        , m_on_toggle(std::move(on_toggle))
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetName(accessible_name);
        SetToolTip(accessible_name);
        const int s = FromDIP(kKeySizeDip);
        SetMinSize(wxSize(s, s));
        Bind(wxEVT_PAINT, &KeySwitch::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) {
            SetFocus();
            toggle();
        });
        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
            const int k = e.GetKeyCode();
            if (k == WXK_SPACE || k == WXK_RETURN || k == WXK_NUMPAD_ENTER) {
                toggle();
                return;
            }
            e.Skip();
        });
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
#if wxUSE_ACCESSIBILITY
        SetAccessible(new Accessible(this));
#endif
    }

    bool IsOn() const { return m_on; }
    void SetOn(bool on)
    {
        if (m_on == on)
            return;
        m_on = on;
        Refresh();
    }

    bool AcceptsFocus() const override { return IsEnabled() && IsShown(); }
    bool AcceptsFocusFromKeyboard() const override { return AcceptsFocus(); }

private:
    void toggle()
    {
        if (!IsEnabled())
            return;
        m_on = !m_on;
        Refresh();
        if (m_on_toggle)
            m_on_toggle();
    }

    void OnPaint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
        dc.Clear();
        const wxSize sz = GetClientSize();
        const int cx = sz.x / 2, cy = sz.y / 2;
        const int r  = std::min(cx, cy) - FromDIP(4);
        const bool enabled = IsEnabled();

        // focus ring: 2px Primary halo outside the key body
        if (HasFocus()) {
            dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Primary), FromDIP(2)));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawCircle(cx, cy, r + FromDIP(3));
        }
        dc.SetPen(wxPen(StateColor::semantic(enabled ? MD3::Role::Outline : MD3::Role::OutlineVariant), 2));
        dc.SetBrush(wxBrush(StateColor::semantic(m_on ? MD3::Role::PrimaryContainer
                                                      : MD3::Role::SurfaceContainerHighest)));
        dc.DrawCircle(cx, cy, r);
        // the key slot: vertical when off, horizontal when on
        const MD3::Role slot = !enabled ? MD3::Role::OutlineVariant
                             : (m_on ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant);
        dc.SetPen(wxPen(StateColor::semantic(slot), FromDIP(6)));
        if (m_on)
            dc.DrawLine(cx - r + FromDIP(9), cy, cx + r - FromDIP(9), cy);
        else
            dc.DrawLine(cx, cy - r + FromDIP(9), cx, cy + r - FromDIP(9));
    }

#if wxUSE_ACCESSIBILITY
    class Accessible final : public wxWindowAccessible
    {
    public:
        explicit Accessible(KeySwitch *key) : wxWindowAccessible(key), m_key(key) {}

        wxAccStatus GetName(int child_id, wxString *name) override
        {
            if (child_id != wxACC_SELF || !name)
                return wxACC_NOT_IMPLEMENTED;
            *name = m_key->GetName();
            return wxACC_OK;
        }
        wxAccStatus GetRole(int child_id, wxAccRole *role) override
        {
            if (child_id != wxACC_SELF || !role)
                return wxACC_NOT_IMPLEMENTED;
            *role = wxROLE_SYSTEM_CHECKBUTTON;
            return wxACC_OK;
        }
        wxAccStatus GetState(int child_id, long *state) override
        {
            if (child_id != wxACC_SELF || !state)
                return wxACC_NOT_IMPLEMENTED;
            *state = wxACC_STATE_SYSTEM_FOCUSABLE;
            if (m_key->HasFocus())
                *state |= wxACC_STATE_SYSTEM_FOCUSED;
            if (m_key->IsOn())
                *state |= wxACC_STATE_SYSTEM_CHECKED;
            if (!m_key->IsEnabled())
                *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
            return wxACC_OK;
        }
        wxAccStatus GetDefaultAction(int child_id, wxString *action) override
        {
            if (child_id != wxACC_SELF || !action)
                return wxACC_NOT_IMPLEMENTED;
            *action = m_key->IsOn() ? _L("Turn key off") : _L("Turn key on");
            return wxACC_OK;
        }
        wxAccStatus DoDefaultAction(int child_id) override
        {
            if (child_id != wxACC_SELF)
                return wxACC_NOT_IMPLEMENTED;
            m_key->toggle();
            return wxACC_OK;
        }
        wxAccStatus GetChildCount(int *count) override
        {
            if (!count)
                return wxACC_NOT_IMPLEMENTED;
            *count = 0;
            return wxACC_OK;
        }

    private:
        KeySwitch *m_key;
    };
#endif

    bool m_on { false };
    std::function<void()> m_on_toggle;
};

wxWindow *resolve_parent(wxWindow *anchor)
{
    if (anchor != nullptr) {
        if (wxWindow *top = wxGetTopLevelParent(anchor))
            return top;
        return anchor;
    }
    return wxTheApp != nullptr ? wxTheApp->GetTopWindow() : nullptr;
}

} // namespace

// ---------------------------------------------------------------------------

bool SuperConfirmGate::Run(wxWindow *anchor, const Spec &spec)
{
    wxWindow *parent = resolve_parent(anchor);
    auto *gate = new SuperConfirmGate(anchor, parent, spec);
    bool authorized = false;
    if (anchor != nullptr) {
        // Anchored: non-modal surface, blocking on a nested loop. Losing
        // activation (clicking elsewhere) dismisses it as a cancel.
        gate->m_nested_loop = true;
        gate->Bind(wxEVT_ACTIVATE, [gate](wxActivateEvent &e) {
            e.Skip();
            if (!e.GetActive() && !gate->m_finished)
                gate->finish(false);
        });
        gate->wxDialog::Show(true);
        gate->Raise();
        {
            wxGUIEventLoop loop;
            wxEventLoopActivator activate(&loop);
            gate->m_loop = &loop;
            loop.Run();
            gate->m_loop = nullptr;
        }
        authorized = gate->m_state.may_fire();
    } else {
        // No anchor: honest modal fallback, centred on the parent.
        gate->CenterOnParent();
        authorized = gate->ShowModal() == wxID_OK && gate->m_state.may_fire();
    }
    gate->restore_focus();
    gate->Destroy();
    return authorized;
}

void SuperConfirmGate::Show(wxWindow *anchor, const Spec &spec,
                            std::function<void()> on_confirm,
                            std::function<void()> on_cancel)
{
    wxWindow *parent = resolve_parent(anchor);
    auto *gate = new SuperConfirmGate(anchor, parent, spec);
    gate->m_on_confirm = std::move(on_confirm);
    gate->m_on_cancel  = std::move(on_cancel);
    if (anchor != nullptr) {
        gate->Bind(wxEVT_ACTIVATE, [gate](wxActivateEvent &e) {
            e.Skip();
            if (!e.GetActive() && !gate->m_finished)
                gate->finish(false);
        });
    } else {
        gate->CenterOnParent();
    }
    gate->wxDialog::Show(true);
    gate->Raise();
}

SuperConfirmGate::SuperConfirmGate(wxWindow *anchor, wxWindow *parent, const Spec &spec)
    : wxDialog(parent, wxID_ANY, spec.action, wxDefaultPosition, wxDefaultSize,
               wxBORDER_NONE | (anchor != nullptr ? wxFRAME_FLOAT_ON_PARENT : 0))
    , m_anchor(anchor)
    , m_return_focus(wxWindow::FindFocus() != nullptr ? wxWindow::FindFocus() : anchor)
{
    build(spec);
    if (anchor != nullptr)
        place_beside_anchor();
}

SuperConfirmGate::~SuperConfirmGate()
{
    m_charge_anim.Stop();
    m_burst_anim.Stop();
    m_pulse_anim.Stop();
}

void SuperConfirmGate::build(const Spec &spec)
{
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainerHigh);
    SetBackgroundColour(surface);
    const int content_w = FromDIP(kMaxWidthDip) - 2 * FromDIP(20);

    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(new MD3DialogCaption(this, spec.action), 0, wxEXPAND);

    auto make_label = [&](const wxFont &font, const wxString &text, MD3::Role colour) {
        auto *l = new Label(this, font, text);
        l->SetBackgroundColour(surface);
        l->SetForegroundColour(StateColor::semantic(colour));
        l->Wrap(content_w);
        return l;
    };

    // Safety facts first, in the same words at every language mode and funny
    // level: the action, then exactly what it affects.
    root->Add(make_label(Label::Head_16, spec.consequence, MD3::Role::OnSurface), 0,
              wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    const int count = spec.affected_count >= 0 ? spec.affected_count : int(spec.affected.size());
    wxString listed;
    const int shown = std::min<int>(kMaxListedNames, int(spec.affected.size()));
    for (int i = 0; i < shown; ++i) {
        if (!listed.empty())
            listed += "\n";
        listed += wxString::FromUTF8("\xE2\x80\xA2 ") + spec.affected[i];
    }
    if (count > shown) {
        if (!listed.empty())
            listed += "\n";
        // TRN %d is how many more affected items are not listed individually.
        listed += wxString::Format(_L("... and %d more"), count - shown);
    }
    if (!listed.empty())
        root->Add(make_label(Label::Body_13, listed, MD3::Role::OnSurfaceVariant), 0,
                  wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    // TRN %d is the total number of items the destructive action affects.
    root->Add(make_label(Label::Body_13,
                         count == 1 ? _L("1 item will be affected. This cannot be undone.")
                                    : wxString::Format(_L("%d items will be affected. This cannot be undone."), count),
                         MD3::Role::Error),
              0, wxLEFT | wxRIGHT | wxTOP, FromDIP(8));

    // Stage narration
    m_stage = make_label(Label::Body_13, wxEmptyString, MD3::Role::OnSurfaceVariant);
    root->Add(m_stage, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    // Two independent keys, each with a caption under it.
    auto *keys_row = new wxBoxSizer(wxHORIZONTAL);
    const wxString key_names[2] = { _L("Key 1 of 2: turn to arm"), _L("Key 2 of 2: turn to arm") };
    const wxString key_caps[2]  = { _L("Key 1"), _L("Key 2") };
    for (int k = 0; k < 2; ++k) {
        auto *col = new wxBoxSizer(wxVERTICAL);
        auto *key = new KeySwitch(this, key_names[k], [this, k]() { on_key_toggled(k); });
        m_keys[k] = key;
        col->Add(key, 0, wxALIGN_CENTER_HORIZONTAL);
        col->Add(make_label(Label::Body_12, key_caps[k], MD3::Role::OnSurfaceVariant), 0,
                 wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(4));
        keys_row->Add(col, 0, wxRIGHT, FromDIP(24));
    }
    root->Add(keys_row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    // Charge bar: hazard stripes fill as the ritual progresses; the burst
    // plays here once the slide lands.
    m_charge = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(kChargeHeight)));
    m_charge->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_charge->SetMinSize(wxSize(FromDIP(kMinWidthDip) - 2 * FromDIP(20), FromDIP(kChargeHeight)));
    m_charge->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_charge);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        const wxSize sz = m_charge->GetClientSize();
        const int radius = sz.y / 2;
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Outline), 1));
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHighest)));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, radius);
        const int fill_w = int(std::lround(sz.x * m_charge_level));
        if (fill_w > 0 && m_burst <= 0.0) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::ErrorContainer)));
            dc.DrawRoundedRectangle(0, 0, std::max(fill_w, sz.y), sz.y, radius);
            // sliding hazard stripes clipped to the filled part
            dc.SetClippingRegion(0, 0, fill_w, sz.y);
            dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Error), FromDIP(3)));
            const int period = FromDIP(14);
            const int offset = int(std::lround(m_pulse * period));
            for (int x = -sz.y - period + offset; x < fill_w + sz.y; x += period)
                dc.DrawLine(x, sz.y, x + sz.y, 0);
            dc.DestroyClippingRegion();
        }
        if (m_burst > 0.0) {
            // completion: PrimaryContainer floods the bar, a Primary disc with a
            // check glyph grows from the centre.
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::PrimaryContainer)));
            dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, radius);
            const int cx = sz.x / 2, cy = sz.y / 2;
            const int r  = int(std::lround((sz.x / 2) * m_burst));
            dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary)));
            dc.SetClippingRegion(0, 0, sz.x, sz.y);
            dc.DrawEllipse(cx - r, cy - r / 4, 2 * r, std::max(1, r / 2));
            dc.DestroyClippingRegion();
            if (MaterialIcon::available())
                MaterialIcon::drawCentered(dc, MaterialIcon::Check, sz.y - 2,
                                           StateColor::semantic(MD3::Role::OnPrimary), wxRect(0, 0, sz.x, sz.y));
        }
    });
    root->Add(m_charge, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    // Full-range slider, enabled only once both keys are on.
    // TRN %s is the destructive action label, e.g. "Delete preset".
    const wxString instruction = wxString::Format(_L("Slide to %s"), spec.action.Lower());
    const wxString done_label  = spec.completed_label.empty() ? _L("Authorized") : spec.completed_label;
    m_slider = new SlideToConfirm(this, instruction, done_label);
    m_slider->SetDangerStyle(true);
    m_slider->Enable(false);
    m_slider->SetOnProgress([this](double f) { on_slide_progress(f); });
    m_slider->SetOnConfirm([this]() { on_slide_complete(); });
    root->Add(m_slider, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(8));

    // Emergency exit: always available, always the default focus so Enter
    // can never confirm.
    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_exit = new Button(this, _L("Emergency exit"), "", 0, 0, wxID_CANCEL);
    m_exit->SetVariant(Button::Variant::Tonal);
    m_exit->SetMinSize(wxSize(FromDIP(160), FromDIP(40)));
    m_exit->SetToolTip(_L("Close without doing anything (Esc)"));
    m_exit->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { finish(false); });
    actions->AddStretchSpacer();
    actions->Add(m_exit, 0);
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(20));

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE) {
            finish(false);
            return;
        }
        e.Skip();
    });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &) { finish(false); });

    SetSizerAndFit(root);
    SetMinSize(wxSize(FromDIP(kMinWidthDip), -1));
    refresh_stage();
    m_exit->SetFocus();
    MD3DialogCaption::FinishChrome(this);
}

void SuperConfirmGate::place_beside_anchor()
{
    wxWindow *anchor = m_anchor.get();
    if (anchor == nullptr)
        return;
    const wxRect a(anchor->GetScreenPosition(), anchor->GetSize());
    const wxSize me = GetSize();
    const int idx = wxDisplay::GetFromWindow(anchor);
    const wxRect area = wxDisplay(idx == wxNOT_FOUND ? 0u : unsigned(idx)).GetClientArea();
    const int gap = FromDIP(8);

    wxPoint pos;
    if (a.height > me.y) {
        // Anchor is a large surface (a whole panel): centre over it instead
        // of hanging off its edge.
        pos = wxPoint(a.x + (a.width - me.x) / 2, a.y + (a.height - me.y) / 2);
    } else if (a.GetBottom() + gap + me.y <= area.GetBottom()) {
        pos = wxPoint(a.x, a.GetBottom() + gap);            // below
    } else if (a.y - gap - me.y >= area.y) {
        pos = wxPoint(a.x, a.y - gap - me.y);          // above
    } else if (a.GetRight() + gap + me.x <= area.GetRight()) {
        pos = wxPoint(a.GetRight() + gap, a.y);             // right
    } else {
        pos = wxPoint(a.x - gap - me.x, a.y);           // left
    }
    // Never leave the display; never cover the anchor when hanging off it.
    pos.x = std::clamp(pos.x, area.x, std::max(area.x, area.GetRight() - me.x + 1));
    pos.y = std::clamp(pos.y, area.y, std::max(area.y, area.GetBottom() - me.y + 1));
    SetPosition(pos);
}

wxString SuperConfirmGate::stage_text() const
{
    switch (m_state.phase()) {
    case SuperConfirm::Phase::Untouched:  return _L("Step 1 of 2: turn both keys.");
    case SuperConfirm::Phase::OneKey:     return _L("Step 1 of 2: turn the other key too.");
    case SuperConfirm::Phase::Armed:      return _L("Step 2 of 2: slide all the way to the end.");
    case SuperConfirm::Phase::Sliding:    return _L("Keep sliding... release early to back out.");
    case SuperConfirm::Phase::Authorized: return _L("Authorized. Carrying out the action.");
    case SuperConfirm::Phase::Cancelled:  return _L("Cancelled. Nothing was changed.");
    }
    return wxEmptyString;
}

void SuperConfirmGate::refresh_stage()
{
    if (m_stage)
        m_stage->SetLabel(stage_text());
    if (m_slider) {
        const bool enable = m_state.slider_enabled();
        if (m_slider->IsEnabled() != enable)
            m_slider->Enable(enable);
        if (!enable && !m_state.authorized && m_slider->Progress() > 0.0)
            m_slider->Reset();
    }
    for (auto *k : m_keys)
        if (k) k->Enable(!m_state.authorized && !m_state.cancelled);

    // Animate the charge bar to the ritual progress (jumps under reduced
    // motion inside Anim::Play).
    const double target = m_state.ritual_progress();
    const double from   = m_charge_level;
    if (std::abs(target - from) > 1e-6)
        m_charge_anim.Play(MD3::Motion::short2, [this, from, target](double t) {
            m_charge_level = from + (target - from) * t;
            if (m_charge) m_charge->Refresh();
        });
    // Hazard stripes crawl only while the knob is travelling. Never loop
    // under reduced motion: Anim::Play would call done() synchronously and
    // recurse forever.
    if (m_state.phase() == SuperConfirm::Phase::Sliding && !MD3::Motion::reduced()) {
        // done() re-enters refresh_stage(), which restarts the crawl while
        // the phase is still Sliding (the timer is already stopped by then).
        if (!m_pulse_anim.IsRunning())
            m_pulse_anim.Play(MD3::Motion::long2, [this](double t) {
                m_pulse = t;
                if (m_charge) m_charge->Refresh();
            }, [this]() { refresh_stage(); }, [](double t) { return t; });
    } else {
        m_pulse_anim.Stop();
    }
    Layout();
}

void SuperConfirmGate::on_key_toggled(int index)
{
    if (m_finished)
        return;
    const bool now_on = m_state.toggle_key(index);
    if (auto *k = static_cast<KeySwitch *>(m_keys[index]))
        k->SetOn(now_on);
    refresh_stage();
    // Arming hands focus to the slider so the keyboard path continues
    // without a Tab hunt; disarming leaves focus where it is.
    if (m_state.phase() == SuperConfirm::Phase::Armed && m_slider)
        m_slider->SetFocus();
}

void SuperConfirmGate::on_slide_progress(double fraction)
{
    if (m_finished || m_state.authorized)
        return;
    // Travel never authorizes on its own: the widget's own completion (LeftUp
    // past the threshold or End) is the only route to 100.
    const int pct = std::min(99, int(std::lround(fraction * 100.0)));
    if (!m_state.set_slider(pct))
        return;
    refresh_stage();
}

void SuperConfirmGate::on_slide_complete()
{
    if (m_finished)
        return;
    if (!m_state.set_slider(100) || !m_state.may_fire()) {
        // Slider completed while the state machine disagrees (a key flipped
        // in the same instant): treat as an incomplete slide.
        if (m_slider) m_slider->Reset();
        refresh_stage();
        return;
    }
    m_pulse_anim.Stop();
    refresh_stage();
    // Distinct completion burst, then hand over. Under reduced motion the
    // burst jumps to its end and done() runs synchronously.
    m_burst_anim.Play(MD3::Motion::medium2, [this](double t) {
        m_burst = t;
        if (m_charge) m_charge->Refresh();
    }, [this]() { finish(true); }, &MD3::Motion::easeEmphasized);
}

void SuperConfirmGate::finish(bool authorized)
{
    if (m_finished)
        return;
    if (!authorized)
        m_state.cancel();
    // Only the state machine can say yes; a caller asking for `true` without
    // the ritual complete is refused.
    const bool fire = authorized && m_state.may_fire();
    m_finished = true;
    m_charge_anim.Stop();
    m_burst_anim.Stop();
    m_pulse_anim.Stop();
    refresh_stage();

    if (m_loop != nullptr) {
        // Run() (anchored) — it restores focus and destroys us.
        Hide();
        m_loop->Exit();
        return;
    }
    if (IsModal()) {
        EndModal(fire ? wxID_OK : wxID_CANCEL);
        return;
    }
    // Callback form.
    Hide();
    restore_focus();
    if (fire) {
        if (m_on_confirm) m_on_confirm();
    } else if (m_on_cancel) {
        m_on_cancel();
    }
    m_on_confirm = nullptr;
    m_on_cancel  = nullptr;
    Destroy();
}

void SuperConfirmGate::restore_focus()
{
    wxWindow *target = m_return_focus.get();
    if (target == nullptr)
        target = m_anchor.get();
    if (target != nullptr && target->IsShownOnScreen() && target->IsEnabled())
        target->SetFocus();
}

} } // namespace Slic3r::GUI
