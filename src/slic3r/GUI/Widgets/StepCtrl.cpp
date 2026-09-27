#include <wx/dc.h>
#include <wx/pen.h>

#include "StepCtrl.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "MD3Tokens.hpp"
#include "StateColor.hpp"
#include "../I18N.hpp"

wxDEFINE_EVENT( EVT_STEP_CHANGING, wxCommandEvent );
wxDEFINE_EVENT( EVT_STEP_CHANGED, wxCommandEvent );

namespace {

// Logical px of the completed-step tick. The retired step_ok raster stroked its
// checkmark edge to edge across the dot, whereas a Material Symbols glyph only
// inks about 70% of its em box -- so the glyph is sized up past the dot to land
// on the same visual weight. Only the transparent margin overhangs the dot; the
// mark itself stays inside it.
constexpr int kStepOkGlyphPx = 16;

} // namespace

BEGIN_EVENT_TABLE(StepCtrl, StepCtrlBase)
EVT_LEFT_DOWN(StepCtrl::mouseDown)
EVT_MOTION(StepCtrl::mouseMove)
EVT_LEFT_UP(StepCtrl::mouseUp)
EVT_MOUSE_CAPTURE_LOST(StepCtrl::mouseCaptureLost)
END_EVENT_TABLE()

StepCtrlBase::StepCtrlBase(wxWindow *      parent,
                   wxWindowID      id,
                   const wxPoint & pos,
                   const wxSize &  size,
                   long            style)
    : StaticBox(parent, id, pos, size, style)
    , font_tip(Label::Body_14)
{
    SetFont(Label::Body_14);
    // Qualified on purpose: a virtual call here would still resolve to the base
    // during base construction. Each subclass re-applies its own mapping at the
    // end of its own constructor.
    StepCtrlBase::applyColorScheme();
    // Set once here, not in applyColorScheme(): the border is a neutral role, so
    // a scheme change must not stamp over a SetBorderColor() from a caller.
    border_color      = StateColor(StateColor::semantic(MD3::Role::OutlineVariant));
    StaticBox::radius = 0;
    //wxString reason;
    //IsTransparentBackgroundSupported(&reason);
}

StepCtrlBase::~StepCtrlBase()
{
}

void StepCtrlBase::applyColorScheme()
{
    // The connecting track reads as a divider, and the dots are plain position
    // markers -- the accent belongs to the thumb painted over the current one.
    clr_bar  = StateColor::semantic(MD3::Role::OutlineVariant);
    clr_step = StateColor::semantic(MD3::Role::Outline);
    // Captions and tips sit on the surface beside the dots, not inside them.
    clr_text = StateColor(std::make_pair(StateColor::semantic(MD3::Role::OnSurface), (int) StateColor::Checked),
            std::make_pair(StateColor::semantic(MD3::Role::OnSurfaceVariant), (int) StateColor::Normal));
    clr_tip  = StateColor::semantic(MD3::Role::OnSurfaceVariant);
}

void StepCtrlBase::applyIndicatorColorScheme()
{
    // Qualified: the indicators' applyColorScheme() overrides delegate straight
    // back here, so a virtual call would recurse forever.
    StepCtrlBase::applyColorScheme();
    // StateColor::Disabled means two different things on the indicator rails.
    // On clr_step it is the whole control being disabled, so the dots drop to
    // the neutral outline; on clr_text it is a step already completed, which
    // the rail de-emphasises the same way.
    clr_step = StateColor(
            std::make_pair(StateColor::semantic(MD3::Role::Outline), (int) StateColor::Disabled),
            std::make_pair(StateColor::semantic(MD3::Role::Primary, m_scheme), 0));
    clr_text = StateColor(
            std::make_pair(StateColor::semantic(MD3::Role::Outline), (int) StateColor::Disabled),
            std::make_pair(StateColor::semantic(MD3::Role::OnSurface), (int) StateColor::Checked),
            std::make_pair(StateColor::semantic(MD3::Role::OnSurfaceVariant), 0));
    // The step numeral and the completed tick are painted on top of the accent
    // dot, so they take the accent's own on-colour rather than a text role.
    clr_tip = StateColor::semantic(MD3::Role::OnPrimary, m_scheme);
}

void StepCtrlBase::SetColorScheme(MD3::ColorScheme scheme)
{
    if (m_scheme == scheme)
        return;
    SetSchemeAccent(scheme); // keeps StaticBox's own accent in step with ours
    applyColorScheme();
    Refresh();
}

int StepCtrlBase::GetSelection() const { return step; }

void StepCtrlBase::SelectItem(int item)
{
    if (item == step || item < -1 || item >= steps.size() || !sendStepCtrlEvent(true))
        return;
    step = item;
    sendStepCtrlEvent();
    Refresh();
}

void StepCtrlBase::Idle()
{
    if (step != -1) {
        step = -1;
        sendStepCtrlEvent();
        Refresh();
    }
}

bool StepCtrlBase::SetTipFont(wxFont const& font)
{
    font_tip = font;
    return true;
}

void StepCtrlBase::SetHint(wxString hint) {
    this->hint = hint;
}

int StepCtrlBase::AppendItem(const wxString &item, wxString const & tip)
{
    steps.push_back(item);
    tips.push_back(tip);
    return steps.size() - 1;
}

void StepCtrlBase::DeleteAllItems()
{
    steps.clear();
    tips.clear();
    if (step >= 0) {
        step = -1;
        sendStepCtrlEvent();
    }
}

unsigned int StepCtrlBase::GetCount() const { return steps.size(); }

wxString StepCtrlBase::GetItemText(unsigned int item) const
{
    return item < steps.size() ? steps[item] : wxString{};
}

int StepCtrlBase::GetItemUseText(wxString txt) const
{
    for(int i = 0; i < steps.size(); i++){
        if (steps[i] == txt) {
            return i;
        }
        else {
            continue;
        }
    }
    return 0;
}

void StepCtrlBase::SetItemText(unsigned int item, wxString const &value)
{
    if (item >= steps.size()) return;
    steps[item] = value;
}

bool StepCtrlBase::sendStepCtrlEvent(bool changing)
{
    wxCommandEvent event(changing ? EVT_STEP_CHANGING : EVT_STEP_CHANGED, GetId());
    event.SetEventObject(this);
    event.SetInt(step);
    GetEventHandler()->ProcessEvent(event);
    return true;
}

/* StepCtrl */

StepCtrl::StepCtrl(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style)
    : StepCtrlBase(parent, id, pos, size, style)
    , bmp_thumb(this, "step_thumb", 36)
{
    StaticBox::border_width = 3;
    radius = radius * bmp_thumb.GetBmpHeight() / 36;
    bar_width = bar_width * bmp_thumb.GetBmpHeight() / 36;
}

void StepCtrl::Rescale()
{
    bmp_thumb.msw_rescale();
    radius    = radius * bmp_thumb.GetBmpHeight() / 36;
    bar_width = bar_width * bmp_thumb.GetBmpHeight() / 36;
}

void StepCtrl::mouseDown(wxMouseEvent &event)
{
    wxPoint pt;
    event.GetPosition(&pt.x, &pt.y);
    wxSize size      = GetSize();
    int    itemWidth = size.x / steps.size();
    wxRect rcBar     = {0, (size.y - 60) / 2, size.x, 60};
    int    circleX   = itemWidth / 2 + itemWidth * step;
    wxRect rcThumb   = {{circleX, size.y / 2}, bmp_thumb.GetBmpSize()};
    rcThumb.x -= rcThumb.width / 2;
    rcThumb.y -= rcThumb.height / 2;
    if (rcThumb.Contains(pt)) {
        pos_thumb   = wxPoint{circleX, size.y / 2};
        drag_offset = pos_thumb - pt;
        if (!HasCapture())
            CaptureMouse();
    } else if (rcBar.Contains(pt)) {
        if (pt.x < circleX) {
            if (step > 0) SelectItem(step - 1);
        } else {
            if (step < steps.size() - 1) SelectItem(step + 1);
        }
    }
}

void StepCtrl::mouseMove(wxMouseEvent &event)
{
    if (pos_thumb == wxPoint{0, 0}) return;
    wxPoint pt;
    event.GetPosition(&pt.x, &pt.y);
    pos_thumb.x = pt.x + drag_offset.x;
    wxSize size      = GetSize();
    int    itemWidth = size.x / steps.size();
    int    index     = pos_thumb.x / itemWidth;
    if (index < 0)
        index = 0;
    else if (index >= steps.size())
        index = steps.size() - 1;
    if (index != pos_thumb.y) {
        pos_thumb.y = index;
        Refresh();
    }
}

void StepCtrl::mouseUp(wxMouseEvent &event)
{
    if (pos_thumb == wxPoint{0, 0}) return;
    wxSize size      = GetSize();
    int    itemWidth = size.x / steps.size();
    int    index     = pos_thumb.x / itemWidth;
    if (index < 0)
        index = 0;
    else if (index >= steps.size())
        index = steps.size() - 1;
    pos_thumb = {0, 0};
    SelectItem(index);
    if (HasCapture())
        ReleaseMouse();
}

void StepCtrl::mouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    wxMouseEvent evt;
    mouseUp(evt);
}

void StepCtrl::doRender(wxDC &dc)
{
    if (steps.empty()) return;
    StaticBox::doRender(dc);

    wxSize size   = GetSize();
    int    states = state_handler.states();

    int    itemWidth = size.x / steps.size();
    wxRect rcBar     = {itemWidth / 2, (size.y - bar_width) / 2, size.x - itemWidth, bar_width};

    dc.SetPen(wxPen(clr_bar.colorForStates(states)));
    dc.SetBrush(wxBrush(clr_bar.colorForStates(states)));
    dc.DrawRectangle(rcBar);
    int circleX = itemWidth / 2;
    int circleY = size.y / 2;
    dc.SetPen(wxPen(clr_step.colorForStates(states)));
    dc.SetBrush(wxBrush(clr_step.colorForStates(states)));
    if (!hint.empty()) {
        dc.SetFont(font_tip);
        dc.SetTextForeground(clr_tip.colorForStates(states));
        wxSize sz = dc.GetTextExtent(hint);
        dc.DrawText(hint, dc.GetCharWidth(), circleY - FromDIP(20) - sz.y);
    }
    for (int i = 0; i < steps.size(); ++i) {
        bool check = (pos_thumb == wxPoint{0, 0} ? step : pos_thumb.y) == i;
        dc.DrawEllipse(circleX - radius, circleY - radius, radius * 2, radius * 2);
        dc.SetFont(GetFont());
        dc.SetTextForeground(clr_text.colorForStates(states | (check ? StateColor::Checked : 0)));
        wxSize sz = dc.GetTextExtent(steps[i]);
        dc.DrawText(steps[i], circleX - sz.x / 2, circleY + 20);
        if (check) {
            dc.SetFont(font_tip);
            dc.SetTextForeground(clr_tip.colorForStates(states));
            wxSize sz = dc.GetTextExtent(tips[i]);
            dc.DrawText(tips[i], circleX - sz.x / 2, circleY - 20 - sz.y);
            sz = bmp_thumb.GetBmpSize();
            dc.DrawBitmap(bmp_thumb.bmp(), circleX - sz.x / 2, circleY - sz.y / 2);
        }
        circleX += itemWidth;
    }
}

/* StepIndicator */

StepIndicator::StepIndicator(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, long style)
    : StepCtrlBase(parent, id, pos, size, style)
    , bmp_ok(this, "step_ok", 12)
{
    SetFont(Label::Body_12);
    font_tip = Label::Body_10;
    StepIndicator::applyColorScheme();
    StaticBox::border_width = 0;
    radius    = bmp_ok.GetBmpHeight() / 2;
    bar_width = bmp_ok.GetBmpHeight() / 20;
    if (bar_width < 2) bar_width = 2;
}

void StepIndicator::applyColorScheme() { applyIndicatorColorScheme(); }

void StepIndicator::Rescale()
{
    bmp_ok.msw_rescale();
    radius    = bmp_ok.GetBmpHeight() / 2;
    bar_width = bmp_ok.GetBmpHeight() / 20;
    if (bar_width < 2) bar_width = 2;
}

void StepIndicator::SelectNext() { SelectItem(step + 1); }


void StepIndicator::doRender(wxDC &dc)
{
    if (steps.empty()) return;

    StaticBox::doRender(dc);

    wxSize size   = GetSize();

    int    states = state_handler.states();
    if (!IsEnabled()) {
        states = clr_step.Disabled;
    }

    int textWidth = size.x - radius * 5;
    dc.SetFont(GetFont());
    wxString firstLine;
    if (step == 0) dc.SetFont(GetFont().Bold());
    wxSize   firstLineSize = Label::split_lines(dc, textWidth, steps.front(), firstLine);
    wxString lastLine;
    if (step == steps.size() - 1) dc.SetFont(GetFont().Bold());
    wxSize   lastLineSize = Label::split_lines(dc, textWidth, steps.back(), lastLine);
    int      firstPadding = std::max(0, firstLineSize.y / 2 - radius);
    int      lastPadding  = std::max(0, lastLineSize.y / 2 - radius);

    wxRect rcBar = {radius * 2 - bar_width / 2, radius * 2 + firstPadding, bar_width, size.y - radius * 6 - firstPadding - lastPadding};
    int    itemWidth = steps.size() == 1 ? size.y : rcBar.height / (steps.size() - 1);

    // Draw thin bar stick
    dc.SetPen(wxPen(clr_bar.colorForStates(states)));
    dc.SetBrush(wxBrush(clr_bar.colorForStates(states)));
    dc.DrawRectangle(rcBar);

    int circleX = radius * 2;
    int circleY = radius * 3 + firstPadding;
    dc.SetPen(wxPen(clr_step.colorForStates(states)));
    dc.SetBrush(wxBrush(clr_step.colorForStates(states)));
    for (int i = 0; i < steps.size(); ++i) {
        bool disabled = step > i;
        bool checked = step == i;
        // Draw circle point & texts in it
        dc.DrawEllipse(circleX - radius, circleY - radius, radius * 2, radius * 2);
        // Draw content ( icon or text ) in circle
        if (disabled) {
            // Completed step. The tick is a Material Symbols glyph tinted with
            // the same on-accent role as the numerals, so it follows the theme
            // and the owning scheme instead of freezing the step_ok raster's
            // white; the raster remains the fallback when the font is missing.
            const wxRect rcDot(circleX - radius, circleY - radius, radius * 2, radius * 2);
            if (MaterialIcon::available())
                MaterialIcon::drawCentered(dc, MaterialIcon::Check, kStepOkGlyphPx,
                        clr_tip.colorForStates(states), rcDot);
            else
                dc.DrawBitmap(bmp_ok.bmp(), rcDot.x, rcDot.y);
        } else {
            dc.SetFont(font_tip);
            dc.SetTextForeground(clr_tip.colorForStates(states));
            auto tip = tips[i];
            if (tip.IsEmpty()) tip.append(1, wchar_t(L'0' + i + 1));
            wxSize sz = dc.GetTextExtent(tip);
            dc.DrawText(tip, circleX - sz.x / 2, circleY - sz.y / 2 + 1);
        }
        // Draw step text
        dc.SetTextForeground(clr_text.colorForStates(states
                | (disabled ? StateColor::Disabled : checked ? StateColor::Checked : 0)));
        dc.SetFont(checked ? GetFont().Bold() : GetFont());
        wxString text;
        wxSize textSize;
        if (i == 0) {
            text = firstLine;
            textSize = firstLineSize;
        } else if (i == steps.size() - 1) {
            text = lastLine;
            textSize = lastLineSize;
        } else {
            textSize = Label::split_lines(dc, textWidth, steps[i], text);
        }
        dc.DrawText(text, circleX + radius * 3, circleY - (textSize.y / 2));
        circleY += itemWidth;
    }
}


/* FilamentStepIndicator */

FilamentStepIndicator::FilamentStepIndicator(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : StepCtrlBase(parent, id, pos, size, style)
    , bmp_ok(this, "step_ok", 12)
{
    static Slic3r::GUI::BitmapCache cache;
    //bmp_extruder = *cache.load_png("filament_load_extruder", FromDIP(300), FromDIP(200), false, false);
    SetFont(Label::Body_12);
    font_tip = Label::Body_12;
    // This rail only ever lives in the Device workspace's AMS filament panel
    // (FilamentLoad), so it starts on the teal Device accent rather than making
    // every caller remember to switch it away from brand green.
    SetSchemeAccent(MD3::ColorScheme::Device);
    FilamentStepIndicator::applyColorScheme();
    StaticBox::border_width = 0;
    radius = 9;
    bar_width = 0;
}

void FilamentStepIndicator::applyColorScheme() { applyIndicatorColorScheme(); }

void FilamentStepIndicator::Rescale()
{
    bmp_ok.msw_rescale();
    radius = bmp_ok.GetBmpHeight() / 2;
    bar_width = bmp_ok.GetBmpHeight() / 20;
    if (bar_width < 2) bar_width = 2;
}

void FilamentStepIndicator::SelectNext() { SelectItem(step + 1); }


void FilamentStepIndicator::doRender(wxDC& dc)
{


    if (steps.empty()) return;

    StaticBox::doRender(dc);

    wxSize size = GetSize();

    int    states = state_handler.states();
    if (!IsEnabled()) {
        states = clr_step.Disabled;
    }

    // Rail heading. Neutral OnSurface, not the accent: the kit reserves Primary
    // here for the step dots below, and the origin goes through FromDIP so the
    // heading (and every dot measured from it) tracks the display scale.
    dc.SetFont(::Label::Head_16);
    dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurface));
    const wxString heading = _L("Loading");
    int circleX = FromDIP(20);
    int circleY = FromDIP(20);
    wxSize sz = dc.GetTextExtent(heading);
    dc.DrawText(heading, circleX, circleY);

    dc.SetFont(::Label::Body_13);

    //dc.DrawBitmap(bmp_extruder, FromDIP(250), circleY);
    circleY += sz.y;

    // The step text column starts at the heading origin, so the wrap budget has to
    // subtract it. It never did - the origin used to be a bare 20, which merely made
    // the budget 20px too generous. Now that the origin scales with the display, the
    // same omission would push every wrapped line past the right edge by exactly the
    // amount circleX grew: 20px at 150%, 40px at 200%.
    int textWidth = size.x - circleX - radius * 5;
    dc.SetFont(GetFont());
    wxString firstLine;
    if (step == 0) dc.SetFont(GetFont().Bold());
    wxSize   firstLineSize = Label::split_lines(dc, textWidth, steps.front(), firstLine);
    wxString lastLine;
    if (step == steps.size() - 1) dc.SetFont(GetFont().Bold());
    wxSize   lastLineSize = Label::split_lines(dc, textWidth, steps.back(), lastLine);
    int      firstPadding = std::max(0, firstLineSize.y / 2 - radius);
    int      lastPadding = std::max(0, lastLineSize.y / 2 - radius);

    int    itemWidth = radius * 3;

    // Draw thin bar stick
    dc.SetPen(wxPen(clr_bar.colorForStates(states)));
    dc.SetBrush(wxBrush(clr_bar.colorForStates(states)));
    //dc.DrawRectangle(rcBar);

    circleX += radius;
    circleY += radius * 3 + firstPadding;
    dc.SetPen(wxPen(clr_step.colorForStates(states)));
    dc.SetBrush(wxBrush(clr_step.colorForStates(states)));
    for (int i = 0; i < steps.size(); ++i) {
        bool disabled = step > i;
        bool checked = step == i;
        // Draw circle point & texts in it
        dc.DrawEllipse(circleX - radius, circleY - radius, radius * 2, radius * 2);
        // Draw content ( icon or text ) in circle
        if (disabled) {
            // Completed step -- same theme-following tick as StepIndicator, with
            // the step_ok raster kept as the fallback.
            const wxRect rcDot(circleX - radius, circleY - radius, radius * 2, radius * 2);
            if (MaterialIcon::available()) {
                MaterialIcon::drawCentered(dc, MaterialIcon::Check, kStepOkGlyphPx,
                    clr_tip.colorForStates(states), rcDot);
            } else {
                wxSize szOk = bmp_ok.GetBmpSize();
                dc.DrawBitmap(bmp_ok.bmp(), circleX - szOk.x / 2, circleY - szOk.y / 2);
            }
        }
        else {
            dc.SetFont(font_tip);
            dc.SetTextForeground(clr_tip.colorForStates(states));
            auto tip = tips[i];
            if (tip.IsEmpty()) tip.append(1, wchar_t(L'0' + i + 1));
            wxSize sz = dc.GetTextExtent(tip);
            dc.DrawText(tip, circleX - sz.x / 2, circleY - sz.y / 2 + 1);
        }
        // Draw step text
        dc.SetTextForeground(clr_text.colorForStates(states
            | (disabled ? StateColor::Disabled : checked ? StateColor::Checked : 0)));
        dc.SetFont(checked ? GetFont().Bold() : GetFont());
        wxString text;
        wxSize textSize;
        if (i == 0) {
            text = firstLine;
            textSize = firstLineSize;
        }
        else if (i == steps.size() - 1) {
            text = lastLine;
            textSize = lastLineSize;
        }
        else {
            textSize = Label::split_lines(dc, textWidth, steps[i], text);
        }
        dc.DrawText(text, circleX + radius * 1.5, circleY - (textSize.y / 2));
        circleY += itemWidth;
    }
}

void FilamentStepIndicator::SetSlotInformation(wxString slot) {
    this->m_slot_information = slot;
}
