#include "SpinInput.hpp"
#include "Label.hpp"
#include "Button.hpp"
#include "StateColor.hpp"
#include "MaterialIcon.hpp"
#include "TextCtrl.h"
#include "../BilingualDecorator.hpp"

#include <wx/dcgraph.h>
#include <algorithm>

namespace {
struct AtlasSpinLayout {
    int width, height, minimum_width, entry_x, entry_width, label_x, step_width, step_height;
};

// All arguments are measured device pixels. Keep the editor and steppers disjoint
// even when a caller requests less room than the current font and unit require.
AtlasSpinLayout atlasSpinLayout(int width, int height, int text_height, int digit_width,
                               int label_width, int label_height, int padding, int step_width)
{
    const int label_gap = label_width > 0 ? padding : 0;
    const int minimum = 3 * padding + step_width + digit_width + label_gap + label_width;
    width = std::max(width, minimum);
    height = std::max(height, std::max(text_height, label_height) + 2 * padding);
    const int entry_x = 2 * padding + step_width;
    const int label_x = width - padding - label_width;
    return {width, height, minimum, entry_x, label_x - label_gap - entry_x,
            label_x, step_width, std::max(1, (height - 2 * padding) / 2)};
}
} // namespace

wxDEFINE_EVENT(EVT_SPINCTRL_TEXT, wxCommandEvent);

BEGIN_EVENT_TABLE(SpinInput, wxPanel)

EVT_KEY_DOWN(SpinInput::keyPressed)
//EVT_MOUSEWHEEL(SpinInput::mouseWheelMoved)

EVT_PAINT(SpinInput::paintEvent)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

SpinInput::SpinInput()
    : label_color(std::make_pair(ThemeColor::TextDisabled, (int) StateColor::Disabled), std::make_pair(ThemeColor::TextMuted, (int) StateColor::Normal))
    , text_color(std::make_pair(ThemeColor::TextDisabled, (int) StateColor::Disabled), std::make_pair(ThemeColor::TextPrimary, (int) StateColor::Normal))
    , text_updating(false)
{
    // Studio Atlas value field: density radius through the StaticBox rescale
    // path, SurfaceContainerLow fill and resting Outline border (focus/hover
    // Primary, disabled OutlineVariant). Every
    // colour is stored as its MD3 light role value -- all keys in StateColor.cpp's
    // gDarkColors table -- so colorForStates() live-remaps them on a dark-mode
    // toggle, dropping the White / Grey300 / Grey400 / BrandGreen literals.
    SetDefaultCornerRadius(MD3::Metrics::active().small_radius);
    border_width     = 1;
    border_color     = StateColor(std::make_pair(MD3::Light::outlineVariant, (int) StateColor::Disabled),
                              std::make_pair(MD3::Light::primary, (int) StateColor::Focused),
                              std::make_pair(MD3::Light::primary, (int) StateColor::Hovered),
                              std::make_pair(MD3::Light::outline, (int) StateColor::Normal));
    background_color = StateColor(std::make_pair(MD3::Light::scHigh, (int) StateColor::Disabled),
                              std::make_pair(MD3::Light::scLow, (int) StateColor::Normal));
}


SpinInput::SpinInput(wxWindow *parent,
                     wxString       text,
                     wxString       label,
                     const wxPoint &pos,
                     const wxSize & size,
                     long           style,
                     int min, int max, int initial)
    : SpinInput()
{
    Create(parent, text, label, pos, size, style, min, max, initial);
}

void SpinInput::Create(wxWindow *parent, 
                     wxString       text,
                     wxString       label,
                     const wxPoint &pos,
                     const wxSize & size,
                     long           style,
                     int min, int max, int initial)
{
    StaticBox::Create(parent, wxID_ANY, pos, size);
    SetFont(Label::Body_12);
    wxWindow::SetLabel(label);
    state_handler.attach({&label_color, &text_color});
    state_handler.update_binds();
    text_ctrl = new TextCtrl(this, wxID_ANY, text, {20, 4}, wxDefaultSize, style | wxBORDER_NONE | wxTE_PROCESS_ENTER, wxTextValidator(wxFILTER_DIGITS));
    // ValueField value in the native wxDC Roboto Mono face at 12.5/500 (Label::Mono_12
    // is 12.5/400; bump to Medium for the 500 weight) -- not the ImGui atlas mono.
    wxFont mono_value = Label::Mono_12;
    mono_value.SetWeight(wxFONTWEIGHT_MEDIUM);
    text_ctrl->SetFont(mono_value);
    text_ctrl->SetBackgroundColour(background_color.colorForStates(state_handler.states()));
    text_ctrl->SetForegroundColour(text_color.colorForStates(state_handler.states()));
    text_ctrl->SetInitialSize(text_ctrl->GetBestSize());
    state_handler.attach_child(text_ctrl);
    text_ctrl->Bind(wxEVT_TEXT, &SpinInput::onTextChanged, this);
    text_ctrl->Bind(wxEVT_KILL_FOCUS, &SpinInput::onTextLostFocus, this);
    text_ctrl->Bind(wxEVT_TEXT_ENTER, &SpinInput::onTextEnter, this);
    text_ctrl->Bind(wxEVT_KEY_DOWN, &SpinInput::keyPressed, this);
    // A right-click reaches the Material text menu (MD3::EnableTextContextMenus), not the native one.
    text_ctrl->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) {
        e.Skip();
        CallAfter([this]() {
            if (text_ctrl->HasFocus())
                text_ctrl->SelectAll();
        });
    });
    button_inc = createButton(true);
    button_dec = createButton(false);
    delta      = 0;
    timer.Bind(wxEVT_TIMER, &SpinInput::onTimer, this);

    long initialFromText;
    if (text.ToLong(&initialFromText)) initial = initialFromText;
    SetRange(min, max);
    SetValue(initial);
    messureSize();
}

void SpinInput::SetCornerRadius(double radius)
{
    StaticBox::SetCornerRadius(radius);
    Refresh();
}

void SpinInput::SetLabel(const wxString &label)
{
    wxWindow::SetLabel(label);
    messureSize();
    Refresh();
}

void SpinInput::SetLabelColor(StateColor const &color)
{
    label_color = color;
    state_handler.update_binds();
}

void SpinInput::SetTextColor(StateColor const &color)
{
    text_color = color;
    state_handler.update_binds();
}

void SpinInput::SetSize(wxSize const &size)
{
    StaticBox::SetSize(size);
    Rescale();
}

void SpinInput::SetValue(const wxString &text)
{
    long value;
    if ( text.ToLong(&value) )
        SetValue(value);
}

void SpinInput::SetValue(int value)
{
    if (value < min) value = min;
    else if (value > max) value = max;
    this->val = value;
    text_updating = true;
    text_ctrl->SetValue(wxString::FromDouble(value));
    text_updating = false;
}

int SpinInput::GetValue()const
{
    return val;
}

void SpinInput::SetRange(int min, int max)
{
    this->min = min;
    this->max = max;
    if (min < 0)
        text_ctrl->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
}

void SpinInput::DoSetToolTipText(wxString const &tip)
{
    if (tip != bilingual_base_tooltip)
        bilingual_note.Clear(); // that note was for the old text; the next paint decides afresh
    bilingual_base_tooltip = tip;
    const wxString merged = bilingual_note.empty() ? tip : (tip.empty() ? bilingual_note : tip + "\n\n" + bilingual_note);
    wxWindow::DoSetToolTipText(merged);
    text_ctrl->SetToolTip(merged);
}

void SpinInput::Rescale()
{
    SetDefaultCornerRadius(MD3::Metrics::active().small_radius);
    RescaleDefaultCornerRadius();
    button_inc->Rescale();
    button_dec->Rescale();
    messureSize();
}

bool SpinInput::Enable(bool enable)
{
    bool result = text_ctrl->Enable(enable) && wxWindow::Enable(enable);
    if (result) {
        wxCommandEvent e(EVT_ENABLE_CHANGED);
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(e);
        text_ctrl->SetBackgroundColour(background_color.colorForStates(state_handler.states()));
        text_ctrl->SetForegroundColour(text_color.colorForStates(state_handler.states()));
        button_inc->Enable(enable);
        button_dec->Enable(enable);
    }
    return result;
}

void SpinInput::paintEvent(wxPaintEvent& evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxPaintDC dc(this);
    render(dc);
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void SpinInput::render(wxDC& dc)
{
    StaticBox::render(dc);
    int    states = state_handler.states();
    wxSize size = GetSize();
    // A quiet divider separates the stepper well from the editable value.
    const int padding = FromDIP(MD3::Metrics::isCompact() ? 3 : 4);
    const int divider_x = button_inc->GetPosition().x + button_inc->GetSize().x + padding / 2;
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), 1));
    dc.DrawLine(divider_x, padding, divider_x, size.y - padding);
    auto label = GetLabel();
    if (!label.IsEmpty()) {
        wxPoint pt;
        pt.x = size.x - labelSize.x - padding;
        pt.y = (size.y - labelSize.y) / 2;
        dc.SetFont(GetFont());
        dc.SetTextForeground(label_color.colorForStates(states));
        // Bilingual mode: the compact English-plus-Cantonese form when it fits
        // the labelSize slot messureSize() already reserved (English-sized, so
        // the value field never moves); otherwise the label stays English and
        // the note joins this control's tooltip (already forwarded to text_ctrl).
        wxString note;
        const wxString shown_label = Slic3r::GUI::I18N::fit_bilingual(dc, label, labelSize.x, &note);
        if (note != bilingual_note) {
            bilingual_note = note;
            const wxString merged = note.empty() ? bilingual_base_tooltip
                                                  : (bilingual_base_tooltip.empty() ? note : bilingual_base_tooltip + "\n\n" + note);
            wxWindow::DoSetToolTipText(merged);
            text_ctrl->SetToolTip(merged);
        }
        dc.DrawText(shown_label, pt);
    }
}

void SpinInput::messureSize()
{
    wxClientDC dc(this);
    dc.SetFont(GetFont());
    labelSize = dc.GetMultiLineTextExtent(GetLabel());
    dc.SetFont(text_ctrl->GetFont());
    const int padding = FromDIP(MD3::Metrics::isCompact() ? 3 : 4);
    const wxSize textSize = text_ctrl->GetBestSize();
    const auto layout = atlasSpinLayout(GetSize().x, GetSize().y, textSize.y,
        dc.GetTextExtent("0").x + FromDIP(2), labelSize.x, labelSize.y,
        padding, FromDIP(MD3::Metrics::isCompact() ? 18 : 20));
    StaticBox::SetSize({layout.width, layout.height});
    SetMinSize({layout.minimum_width, std::max(textSize.y, labelSize.y) + 2 * padding});
    text_ctrl->SetSize({layout.entry_width, textSize.y});
    text_ctrl->SetPosition({layout.entry_x, (layout.height - textSize.y) / 2});
    const wxSize btnSize{layout.step_width, layout.step_height};
    button_inc->SetSize(btnSize);
    button_dec->SetSize(btnSize);
    button_inc->SetPosition({padding, padding});
    button_dec->SetPosition({padding, layout.height - padding - layout.step_height});
}

Button *SpinInput::createButton(bool inc)
{
    // MD3 stepper: a borderless glyph icon-button. The raster spin_inc / spin_dec
    // PNG stays as the graceful fallback; SetGlyph promotes it to a Material Symbols
    // chevron (ExpandLess to increment, ExpandMore to decrement) in OnSurfaceVariant.
    auto btn = new Button(this, "", inc ? "spin_inc" : "spin_dec", wxBORDER_NONE, 6);
    btn->SetTextColor(StateColor(std::make_pair(MD3::Light::onSurfaceVariant, (int) StateColor::Normal)));
    btn->SetGlyph(inc ? MaterialIcon::ExpandLess : MaterialIcon::ExpandMore, 12);
    btn->SetCornerRadius(FromDIP(3));
    btn->DisableFocusFromKeyboard();
    btn->Bind(wxEVT_LEFT_DOWN, [=](auto &e) {
        delta = inc ? 1 : -1;
        SetValue(val + delta);
        text_ctrl->SetFocus();
        if (!btn->HasCapture())
            btn->CaptureMouse();
        delta *= 8;
        timer.Start(100);
        sendSpinEvent();
    });
    btn->Bind(wxEVT_LEFT_DCLICK, [=](auto &e) {
        delta = inc ? 1 : -1;
        if (!btn->HasCapture())
            btn->CaptureMouse();
        SetValue(val + delta);
        sendSpinEvent();
    });
    btn->Bind(wxEVT_LEFT_UP, [=](auto &e) {
        if (btn->HasCapture())
            btn->ReleaseMouse();
        timer.Stop();
        text_ctrl->SelectAll();
        delta = 0;
    });
    return btn;
}

void SpinInput::onTimer(wxTimerEvent &evnet) {
    if (delta < -1 || delta > 1) {
        delta /= 2;
        return;
    }
    SetValue(val + delta);
    sendSpinEvent();
}

void SpinInput::onTextChanged(wxCommandEvent &event)
{
    if (!text_updating) {
        long value;
        if (text_ctrl->GetValue().ToLong(&value)) {
            wxCommandEvent e(EVT_SPINCTRL_TEXT, GetId());
            e.SetEventObject(this);
            e.SetInt((int)value);
            e.SetString(text_ctrl->GetValue());
            GetEventHandler()->ProcessEvent(e);
        }
    }
    event.Skip();
}

void SpinInput::onTextLostFocus(wxEvent &event)
{
    timer.Stop();
    for (auto * child : GetChildren())
        if (auto btn = dynamic_cast<Button*>(child))
            if (btn->HasCapture())
                btn->ReleaseMouse();
    wxCommandEvent e;
    onTextEnter(e);
    // pass to outer
    event.SetId(GetId());
    ProcessEventLocally(event);
    event.Skip();
}

void SpinInput::onTextEnter(wxCommandEvent &event)
{
    long value;
    if (!text_ctrl->GetValue().ToLong(&value)) { value = val; }
    int normalized = (int)value;
    if (normalized < min) normalized = min;
    else if (normalized > max) normalized = max;
    bool value_changed = normalized != val;
    if (normalized != val || text_ctrl->GetValue() != wxString::FromDouble(normalized)) {
        SetValue(value);
        if (value_changed)
            sendSpinEvent();
    }
    event.SetId(GetId());
    ProcessEventLocally(event);
}

void SpinInput::mouseWheelMoved(wxMouseEvent &event)
{
    auto delta = event.GetWheelRotation() < 0 ? 1 : -1;
    SetValue(val + delta);
    sendSpinEvent();
    text_ctrl->SetFocus();
}

void SpinInput::keyPressed(wxKeyEvent &event)
{
    switch (event.GetKeyCode()) {
    case WXK_UP:
    case WXK_DOWN:
        long value;
        if (!text_ctrl->GetValue().ToLong(&value)) { value = val; }
        if (event.GetKeyCode() == WXK_DOWN && value > min) {
            --value;
        } else if (event.GetKeyCode() == WXK_UP && value + 1 < max) {
            ++value;
        }
        if (value != val) {
            SetValue(value);
            sendSpinEvent();
        }
        break;
    default: event.Skip(); break;
    }
}

void SpinInput::sendSpinEvent()
{
    wxCommandEvent event(wxEVT_SPINCTRL, GetId());
    event.SetEventObject(this);
    GetEventHandler()->ProcessEvent(event); 
}
