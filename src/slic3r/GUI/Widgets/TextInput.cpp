#include "TextInput.hpp"
#include "Label.hpp"
#include "StateColor.hpp"
#include "TextCtrl.h"
#include "MaterialIcon.hpp"

#include "slic3r/GUI/I18N.hpp"

#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include "../MsgDialog.hpp"

BEGIN_EVENT_TABLE(TextInput, wxPanel)

EVT_PAINT(TextInput::paintEvent)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

TextInput::TextInput()
    : label_color(std::make_pair(ThemeColor::TextDisabled, (int) StateColor::Disabled),
                 std::make_pair(ThemeColor::TextMuted, (int) StateColor::Normal))
    , text_color(std::make_pair(ThemeColor::TextDisabled, (int) StateColor::Disabled),
                 std::make_pair(ThemeColor::TextPrimary, (int) StateColor::Normal))
{
    // The default radius follows density; explicit caller radii remain authoritative.
    SetDefaultCornerRadius(MD3::Metrics::active().small_radius);
    border_width = 1;
    border_color = StateColor(std::make_pair(MD3::Light::outlineVariant, (int) StateColor::Disabled),
                              std::make_pair(MD3::Light::primary, (int) StateColor::Hovered),
                              std::make_pair(MD3::Light::outline, (int) StateColor::Normal));
    background_color = StateColor(std::make_pair(MD3::Light::scHigh, (int) StateColor::Disabled),
                                  std::make_pair(MD3::Light::scLow, (int) StateColor::Normal));
    SetFont(Label::Body_12);
}

TextInput::TextInput(wxWindow *     parent,
                     wxString       text,
                     wxString       label,
                     wxString       icon,
                     const wxPoint &pos,
                     const wxSize & size,
                     long           style,
                     wxString       unit,
                     wxString       prefix)
    : TextInput()
{
    Create(parent, text, label, icon, pos, size, style, unit, prefix);
}

void TextInput::Create(wxWindow *     parent,
                       wxString       text,
                       wxString       label,
                       wxString       icon,
                       const wxPoint &pos,
                       const wxSize & size,
                       long           style,
                       wxString       unit,
                       wxString       prefix)
{
    m_unit    = unit;
    m_prefix  = prefix;
    text_ctrl = nullptr;
    StaticBox::Create(parent, wxID_ANY, pos, size, style);
    wxWindow::SetLabel(label);
    assert((style & wxRIGHT) == 0);
    style &= ~wxALIGN_MASK;
    state_handler.attach({&label_color, & text_color});
    state_handler.update_binds();
    
    text_ctrl = new TextCtrl(this, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, style | wxBORDER_NONE | wxTE_PROCESS_ENTER);
    text_ctrl->SetFont(Label::Body_14);
    text_ctrl->SetInitialSize(text_ctrl->GetBestSize());
    text_ctrl->SetBackgroundColour(background_color.colorForStates(state_handler.states()));
    text_ctrl->SetForegroundColour(text_color.colorForStates(state_handler.states()));
    state_handler.attach_child(text_ctrl);
    text_ctrl->Bind(wxEVT_KILL_FOCUS, [this](auto &e) {
        OnEdit();
        e.SetId(GetId());
        ProcessEventLocally(e);
        e.Skip();
    });
    text_ctrl->Bind(wxEVT_TEXT_ENTER, [this](auto &e) {
        OnEdit();
        e.SetId(GetId());
        ProcessEventLocally(e);
    });
    // A right-click reaches the Material text menu (MD3::EnableTextContextMenus), not the native one.

    text_ctrl->Bind(wxEVT_TEXT, [this](auto &e)
        {
            if (m_checkers.empty()) { return e.Skip(); }

            CheckValid(false);
            e.Skip();
        });

    if (!icon.IsEmpty()) {
        this->icon = ScalableBitmap(this, icon.ToStdString(), 16);
    }
    messureSize();
}

void TextInput::SetCornerRadius(double radius)
{
    StaticBox::SetCornerRadius(radius);
}

bool TextInput::SetFont(const wxFont& font)
{
    const bool changed = wxWindow::SetFont(font);
    if (text_ctrl) {
        messureSize();
        Refresh();
    }
    return changed;
}

void TextInput::SetLabel(const wxString& label)
{
    wxWindow::SetLabel(label);
    messureSize();
    Refresh();
}

void TextInput::SetPrefix(const wxString& prefix)
{
    m_prefix = prefix;
    messureSize();
    Refresh();
}

void TextInput::SetStaticTips(const wxString& tips, const wxBitmap& bitmap)
{
    static_tips = tips;
    static_tips_icon = bitmap;
    messureSize();
    Refresh();
}

void TextInput::SetIcon(const wxBitmap &icon)
{
    this->icon = ScalableBitmap();
    this->icon.bmp() = icon;
    Rescale();
}

void TextInput::SetIcon(const wxString &icon)
{
    if (this->icon.name() == icon.ToStdString())
        return;
    // MD3 leading icon as a Material Symbols glyph (kit ValueField/SelectField
    // anatomy) for the handful of well-known raster names still routed through
    // this string-keyed setter (e.g. ComboBox's trailing 'drop_down' chevron,
    // inherited from TextInput -- see AMSMaterialsSetting.cpp). Falls back to
    // the legacy raster for any name not in this table, or when the icon face
    // is unavailable.
    if (MaterialIcon::available() && icon == "drop_down") {
        this->icon = ScalableBitmap();
        this->icon.bmp() = MaterialIcon::bitmap(this, MaterialIcon::ExpandMore, 16, StateColor::semantic(MD3::Role::OnSurfaceVariant));
        Rescale();
        return;
    }
    this->icon = ScalableBitmap(this, icon.ToStdString(), 16);
    Rescale();
}

void TextInput::SetIcon_1(const wxString &icon) {
    if (this->icon_1.name() == icon.ToStdString())
        return;
    if (icon.empty()) {
        this->icon_1 = ScalableBitmap();
        Rescale();
        return;
    }
    this->icon_1 = ScalableBitmap(this, icon.ToStdString(), 14);
    Rescale();
}

// Set icon_1 from a raw bitmap. Note: won't auto-rescale on DPI change
// since ScalableBitmap::name() will be empty. Caller should re-set after DPI change.
void TextInput::SetIcon_1(const wxBitmap &icon) {
    this->icon_1 = ScalableBitmap();
    if (icon.IsOk())
        this->icon_1.bmp() = icon;
    Rescale();
}

void TextInput::SetLabelColor(StateColor const &color)
{
    label_color = color;
    state_handler.update_binds();
}

void TextInput::SetTextColor(StateColor const& color)
{
    text_color= color;
    state_handler.update_binds();
}

void TextInput::Rescale()
{
    SetDefaultCornerRadius(MD3::Metrics::active().small_radius);
    RescaleDefaultCornerRadius();
    if (!this->icon.name().empty())
        this->icon.msw_rescale();
    if (!this->icon_1.name().empty())
        this->icon_1.msw_rescale();
    messureSize();
    Refresh();
}

bool TextInput::Enable(bool enable)
{
    bool result = text_ctrl->Enable(enable) && wxWindow::Enable(enable);
    if (result) {
        wxCommandEvent e(EVT_ENABLE_CHANGED);
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(e);
        text_ctrl->SetBackgroundColour(background_color.colorForStates(state_handler.states()));
        text_ctrl->SetForegroundColour(text_color.colorForStates(state_handler.states()));
    }
    return result;
}

void TextInput::SetMinSize(const wxSize& size)
{
    wxSize size2 = size;
    if (size2.y < 0) {
#ifdef __WXMAC__
        if (GetPeer()) // peer is not ready in Create on mac
#endif
        size2.y = GetSize().y;
    }
    wxWindow::SetMinSize(size2);
}

namespace {
// All inputs are measured device pixels. No DPI conversion belongs in this calculation.
struct AtlasFieldLayout {
    int entry_x, entry_width, label_x, prefix_x, unit_x, minimum_width;
};

AtlasFieldLayout atlasFieldLayout(int width, int padding, int gap, int icons,
                                  int label, int prefix, int unit, int entry_min,
                                  bool editable, bool label_left)
{
    const int label_slot = label > 0 ? label + (editable ? gap : 0) : 0;
    const int prefix_slot = prefix > 0 ? prefix + gap : 0;
    const int unit_slot = unit > 0 ? unit + gap : 0;
    const int left = padding + icons;
    const int entry_x = left + (label_left ? label_slot : 0) + prefix_slot;
    const int entry_width = editable ? std::max(0, width - 2 * padding - icons - label_slot - prefix_slot - unit_slot) : 0;
    const int unit_x = entry_x + entry_width + (unit > 0 ? gap : 0);
    const int label_x = label_left ? left : entry_x + entry_width + unit_slot + (label > 0 ? gap : 0);
    return {entry_x, entry_width, label_x, entry_x - prefix_slot, unit_x,
            2 * padding + icons + label_slot + prefix_slot + unit_slot + (editable ? entry_min : 0)};
}
} // namespace

TextInput::ContentMetrics TextInput::measureContent()
{
    ContentMetrics m;
    m.padding = FromDIP(MD3::Metrics::active().padding / 2);
    m.gap = FromDIP(8);
    m.editable = text_ctrl && text_ctrl->IsShown();
    wxClientDC dc(this);
    dc.SetFont(GetFont());
    m.label = dc.GetTextExtent(wxWindow::GetLabel());
    dc.SetFont(Label::Body_12);
    m.support = dc.GetTextExtent(static_tips);
    dc.SetFont(text_ctrl->GetFont());
    m.prefix = dc.GetTextExtent(m_prefix);
    m.unit = dc.GetTextExtent(m_unit);
    m.entry = {dc.GetTextExtent(wxS("0.000")).x, text_ctrl->GetBestSize().y};
    if (icon.bmp().IsOk()) m.icons += icon.GetBmpSize().x + m.gap;
    if (icon_1.bmp().IsOk()) m.icons += icon_1.GetBmpSize().x + m.gap;
    return m;
}

void TextInput::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    const wxSize oldSize = GetSize();
    wxWindow::DoSetSize(x, y, width, height, sizeFlags);
    if (sizeFlags & wxSIZE_USE_EXISTING || !text_ctrl) return;
    const wxSize size = GetSize();
    if (size != oldSize) Refresh();
    const auto m = measureContent();
    const auto layout = atlasFieldLayout(size.x, m.padding, m.gap, m.icons,
        std::max(m.label.x, m.support.x), m.prefix.x, m.unit.x, m.entry.x,
        m.editable, !m.editable || (GetWindowStyle() & wxALIGN_RIGHT));
    const int entry_height = std::min(m.entry.y, std::max(0, size.y - 2 * m.padding));
    text_ctrl->SetSize(layout.entry_width, entry_height);
    text_ctrl->SetPosition({layout.entry_x, (size.y - entry_height) / 2});
}

void TextInput::DoSetToolTipText(wxString const &tip)
{
    wxWindow::DoSetToolTipText(tip);
    text_ctrl->SetToolTip(tip);
}

void TextInput::paintEvent(wxPaintEvent &evt)
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
void TextInput::render(wxDC& dc)
{
    StaticBox::render(dc);
    if (!text_ctrl) return;
    const int states = state_handler.states();
    const wxSize size = GetSize();
    if (IsEnabled() && (states & StateColor::Focused)) {
        wxRect focus = GetClientRect();
        const int inset = FromDIP(2);
        focus.Deflate(inset);
        if (focus.width > 0 && focus.height > 0) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(border_color.colorForStates(states), FromDIP(2)));
            dc.DrawRoundedRectangle(focus, std::max(0.0, std::min(radius - inset,
                std::min(focus.width, focus.height) / 2.0)));
        }
    }
    const auto m = measureContent();
    const auto layout = atlasFieldLayout(size.x, m.padding, m.gap, m.icons,
        std::max(m.label.x, m.support.x), m.prefix.x, m.unit.x, m.entry.x,
        m.editable, !m.editable || (GetWindowStyle() & wxALIGN_RIGHT));
    // Parent layouts must honour the measured minimum. A forced smaller allocation must
    // still never paint into neighbouring controls; the complete label remains accessible.
    wxDCClipper clip(dc, GetClientRect());
    int icon_x = m.padding;
    for (auto* bitmap : {&icon, &icon_1}) {
        if (!bitmap->bmp().IsOk()) continue;
        const wxSize dimensions = bitmap->GetBmpSize();
        dc.DrawBitmap(bitmap->bmp(), icon_x, (size.y - dimensions.y) / 2);
        icon_x += dimensions.x + m.gap;
    }
    const int support_gap = static_tips.empty() ? 0 : FromDIP(4);
    const int stack_height = m.label.y + (static_tips.empty() ? 0 : m.support.y + support_gap);
    int label_x = layout.label_x;
    if (!m.editable && (GetWindowStyle() & wxALIGN_CENTER_HORIZONTAL))
        label_x = std::max(label_x, (size.x - std::max(m.label.x, m.support.x)) / 2);
    const int label_y = (size.y - stack_height) / 2;
    dc.SetFont(GetFont());
    dc.SetTextForeground(label_color.colorForStates(states));
    dc.DrawText(wxWindow::GetLabel(), label_x, label_y);
    if (!static_tips.empty()) {
        dc.SetFont(Label::Body_12);
        dc.SetTextForeground(label_color.colorForStates(states));
        dc.DrawText(static_tips, label_x, label_y + m.label.y + support_gap);
    }
    dc.SetFont(text_ctrl->GetFont());
    dc.SetTextForeground(label_color.colorForStates(states));
    if (!m_prefix.empty())
        dc.DrawText(m_prefix, layout.prefix_x, (size.y - m.prefix.y) / 2);
    if (!m_unit.empty())
        dc.DrawText(m_unit, layout.unit_x, (size.y - m.unit.y) / 2);
}

void TextInput::messureSize()
{
    if (!text_ctrl) return;
    const auto m = measureContent();
    labelSize = m.label;
    static_tips_size = m.support;
    const auto layout = atlasFieldLayout(GetSize().x, m.padding, m.gap, m.icons,
        std::max(m.label.x, m.support.x), m.prefix.x, m.unit.x, m.entry.x,
        m.editable, !m.editable || (GetWindowStyle() & wxALIGN_RIGHT));
    const int label_height = m.label.y + (static_tips.empty() ? 0 : m.support.y + FromDIP(4));
    const int content_height = std::max(m.editable ? m.entry.y : 0, label_height);
    int icon_height = 0;
    if (icon.bmp().IsOk()) icon_height = icon.GetBmpSize().y;
    if (icon_1.bmp().IsOk()) icon_height = std::max(icon_height, icon_1.GetBmpSize().y);
    const int minimum_height = std::max(FromDIP(MD3::Metrics::active().row_height),
        std::max(content_height, icon_height) + 2 * m.padding);
    const wxSize minimum(std::max(GetMinWidth(), layout.minimum_width), minimum_height);
    SetMinSize(minimum);
    const wxSize current = GetSize();
    SetSize(std::max(current.x, minimum.x), std::max(current.y, minimum.y));
    // SetSize can be a no-op after a label, prefix or icon change. Reposition the
    // native entry even when the outside size stays identical.
    DoSetSize(wxDefaultCoord, wxDefaultCoord, GetSize().x, GetSize().y, wxSIZE_AUTO);
}

bool TextInput::CheckValid(bool pop_dlg) const
{
    for (auto checker : m_checkers)
    {
        wxString error_msg = checker->CheckValid(text_ctrl->GetValue());
        if (!error_msg.IsEmpty())
        {
            text_ctrl->SetBackgroundColour(StateColor::semantic(MD3::Role::ErrorContainer));
            text_ctrl->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
            text_ctrl->SetToolTip(error_msg);
            text_ctrl->Refresh();

            if (pop_dlg)
            {
                Slic3r::GUI::MessageDialog dlg(nullptr, error_msg, _L("Error"), wxOK |wxICON_WARNING);
                dlg.ShowModal();
            }

            return false;
        }
    }

    // Restore the caller's current state palette after validation succeeds.
    text_ctrl->SetBackgroundColour(background_color.colorForStates(state_handler.states()));
    text_ctrl->SetForegroundColour(text_color.colorForStates(state_handler.states()));
    text_ctrl->SetToolTip(wxEmptyString);
    text_ctrl->Refresh();
    return true;
}

std::shared_ptr<TextInputValChecker> TextInputValChecker::CreateIntMinChecker(int val)
{
    return std::make_shared<TextInputValIntMinChecker>(val);
}

wxString TextInputValIntMinChecker::CheckValid(const wxString& value) const
{
    long num;
    if (value.ToLong(&num) && num >= m_min_value) { return wxEmptyString; }
    return  wxString::Format(_L("Please input a number greater than or equal to %d"), m_min_value);
}

std::shared_ptr<TextInputValChecker> TextInputValChecker::CreateIntRangeChecker(int min, int max)
{
    return std::make_shared<TextInputValIntRangeChecker>(min, max);
}

std::shared_ptr<TextInputValChecker> TextInputValChecker::CreateDoubleMinChecker(double min)
{
    return std::make_shared<TextInputValDoubleMinChecker>(min);
}

std::shared_ptr<TextInputValChecker> TextInputValChecker::CreateDoubleRangeChecker(double min, double max, bool enable)
{
    return std::make_shared<TextInputValDoubleRangeChecker> (min, max, enable);
}

wxString TextInputValIntRangeChecker::CheckValid(const wxString& value) const
{
    long num;
    if (value.ToLong(&num) && num >= m_min_value && num <= m_max_value) { return wxEmptyString; }
    return  wxString::Format(_L("Please enter a number between %d and %d."), m_min_value, m_max_value);
}

wxString TextInputValDoubleMinChecker::CheckValid(const wxString& value) const
{
    double num;
    if (value.ToDouble(&num) && num >= m_min_value) { return wxEmptyString; }
    return  wxString::Format(_L("Please enter a float greater than or equal to %f."), m_min_value);
}

wxString TextInputValDoubleRangeChecker::CheckValid(const wxString& value) const
{
    if (m_enable_empty && value.empty())
    {
        return wxEmptyString;
    }

    double num;
    if (value.ToDouble(&num) && num >= m_min_value && num <= m_max_value) { return wxEmptyString; }
    return  wxString::Format(_L("Please enter a float between %f and %f."), m_min_value, m_max_value);
}
