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
    // MD3 filled-field geometry. Radius 10 flows through the StaticBox
    // default-radius path so it is FromDIP-scaled at Create and recomputed on
    // every monitor/DPI change (no stale cached radius). Fill is
    // SurfaceContainerHighest; the resting border is Outline (not the lighter
    // OutlineVariant), hover promotes to Primary, disabled falls to
    // OutlineVariant. Every colour is stored as its MD3 *light* role value, and
    // each of those hexes is a key in StateColor.cpp's gDarkColors table, so
    // colorForStates() live-remaps them on a runtime dark-mode toggle -- this is
    // why the old Grey400 / BrandGreen / White / Grey300 literals are dropped in
    // favour of the role tones rather than semantic() snapshots.
    SetDefaultCornerRadius(10);
    border_width = 1;
    border_color = StateColor(std::make_pair(MD3::Light::outlineVariant, (int) StateColor::Disabled),
                              std::make_pair(MD3::Light::primary, (int) StateColor::Hovered),
                              std::make_pair(MD3::Light::outline, (int) StateColor::Normal));
    background_color = StateColor(std::make_pair(MD3::Light::scHigh, (int) StateColor::Disabled),
                                  std::make_pair(MD3::Light::scHighest, (int) StateColor::Normal));
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
    
    int prefix_space = 0;
    if (!m_prefix.IsEmpty()) {
        wxClientDC dc(this);
        wxSize     prefix_size = dc.GetTextExtent(m_prefix);
        prefix_space           = prefix_size.x + 8;
    }

    text_ctrl = new TextCtrl(this, wxID_ANY, text, {4 + prefix_space, 4}, wxDefaultSize, style | wxBORDER_NONE | wxTE_PROCESS_ENTER);
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
    this->radius = radius;
    Refresh();
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

void TextInput::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    const wxSize oldSize = GetSize();
    wxWindow::DoSetSize(x, y, width, height, sizeFlags);
    if (sizeFlags & wxSIZE_USE_EXISTING) return;
    wxSize size = GetSize();
    // The border/background/dropdown arrow are custom-painted from the full bounds; a partial
    // erase on grow would leave the old right edge stale, so force a full repaint on any resize.
    if (size != oldSize) Refresh();
    wxPoint textPos = {5, 0};
    if (this->icon.bmp().IsOk()) {
        wxSize szIcon = this->icon.GetBmpSize();
        textPos.x += szIcon.x;
    }
    if (this->icon_1.bmp().IsOk()) {
        wxSize szIcon = this->icon_1.GetBmpSize();
        textPos.x += (szIcon.x);
    }
    bool align_right = GetWindowStyle() & wxALIGN_RIGHT;
    if (align_right)
        textPos.x += labelSize.x;

    int prefix_space = 0;
    if (!m_prefix.IsEmpty()) {
        wxClientDC dc(this);
        wxSize     prefix_size = dc.GetTextExtent(m_prefix);
        prefix_space           = prefix_size.x + 8;
    }
    if (text_ctrl) {
        wxClientDC dc(this);
        wxSize unitSize = dc.GetTextExtent(m_unit);
        // Reserve room for the trailing unit suffix (gap + glyph + right pad) so
        // render()'s unit draw (at the entry's right edge + 4) stays inside the
        // field instead of clipping. 0 when there is no unit, so unit-less fields
        // keep their previous width exactly.
        int unit_space = m_unit.IsEmpty() ? 0 : (unitSize.x + 5 + 10);
        wxSize textSize = text_ctrl->GetSize();
        textSize.x = size.x - textPos.x - labelSize.x - 10 - prefix_space - unit_space;
        if(textSize.x < -1) textSize.x = -1;
        text_ctrl->SetSize(textSize);
        text_ctrl->SetPosition({textPos.x + prefix_space, (size.y - textSize.y) / 2});
    }
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
    int states = state_handler.states();
    wxSize size = GetSize();
    bool   align_center = GetWindowStyle() & wxALIGN_CENTER_HORIZONTAL;
    bool   align_right = GetWindowStyle() & wxALIGN_RIGHT;
    // start draw
    wxPoint pt = {5, 0};
    if (icon.bmp().IsOk()) {
        wxSize szIcon = icon.GetBmpSize();
        pt.y = (size.y - szIcon.y) / 2;
        if (align_center) {
            if (pt.x * 2 + szIcon.x + 0 + labelSize.x < size.x)
                pt.x = (size.x - (szIcon.x + 0 + labelSize.x)) / 2;
        }
        dc.DrawBitmap(icon.bmp(), pt);
        pt.x += (szIcon.x + szIcon.x * 0.2);
    }
    if (icon_1.bmp().IsOk()) {
        wxSize szIcon = icon_1.GetBmpSize();
        pt.y          = (size.y - szIcon.y) / 2;
        if (align_center) {
            if (pt.x * 2 + szIcon.x + 0 + labelSize.x < size.x)
                pt.x = (size.x - (szIcon.x + 0 + labelSize.x)) / 2;
        }
        pt.x += szIcon.x / 4.f;
        dc.DrawBitmap(icon_1.bmp(), pt);
        pt.x += szIcon.x + 0;
    }
    auto text = wxWindow::GetLabel();
    if (!text.IsEmpty()) {
        if (static_tips.IsEmpty()) {
            wxSize textSize = text_ctrl->GetSize();
            int    prefix_space = 0;
            if (!m_prefix.IsEmpty()) {
                wxClientDC dc(this);
                wxSize     prefix_size = dc.GetTextExtent(m_prefix);
                prefix_space           = prefix_size.x + 8;
            }
            // Only a right-aligned field moves its entry right to make room for the label on
            // the left (DoSetSize). A centred one keeps the entry at the left edge, so its label
            // goes after the entry like a left-aligned one; drawn at the left edge it sat under
            // the entry, which hid the start of the unit ("0.1/mm" for "0.1 mm/mm").
            if (align_right)
            {
                if (pt.x + labelSize.x + 5 > size.x)
                    text = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END, size.x - pt.x - 5);
                pt.y = (size.y - labelSize.y) / 2;
            }
            else
            {
                pt.x += textSize.x + prefix_space;
                pt.y = (size.y + textSize.y) / 2 - labelSize.y;
            }
            dc.SetTextForeground(label_color.colorForStates(states));
            dc.SetFont(GetFont());
            dc.DrawText(text, pt);
        } else {
            wxSize textSize = text_ctrl->GetSize();
            if (align_right) {
                if (pt.x + labelSize.x + 5 > size.x)
                    text = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END, size.x - pt.x - 5);
                pt.y = (size.y - labelSize.y - static_tips_size.y - 8) / 2;
            } else {
                pt.x += textSize.x;
                pt.y = (size.y - labelSize.y - static_tips_size.y - 8) / 2;
            }
            dc.SetTextForeground(label_color.colorForStates(states));
            dc.SetFont(GetFont());
            dc.DrawText(text, pt);

            if (align_right) {
                if (pt.x + static_tips_size.x + 5 > size.x) {
                    text = wxControl::Ellipsize(static_tips, dc, wxELLIPSIZE_END, size.x - pt.x - 5);
                }

                pt.y += (labelSize.y + 8);
            } else {
                pt.x += static_tips_size.x;
                pt.y += (labelSize.y + 8);
            }

            dc.SetTextForeground(ThemeColor::TextDisabled);

            wxFont font = GetFont();
            font.SetPointSize(font.GetPointSize() - 1);// use smaller font
            dc.SetFont(font);
            dc.DrawText(static_tips, pt);
        }
    }
    if (!m_prefix.IsEmpty() && text_ctrl) {
        wxPoint ctrl_pos    = text_ctrl->GetPosition();
        wxSize  ctrl_size   = text_ctrl->GetSize();
        wxSize  prefix_size = dc.GetTextExtent(m_prefix);
        int     prefix_space = prefix_size.x + 8;

        int x = ctrl_pos.x - prefix_space - 2;
        int y = ctrl_pos.y + (ctrl_size.y - prefix_size.y) / 2 - 2;

        wxFont prefix_font = text_ctrl->GetFont();
        dc.SetFont(prefix_font);
        dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        dc.DrawText(m_prefix, wxPoint(x, y));
    }
    if (!m_unit.IsEmpty() && text_ctrl) {
        wxPoint ctrl_pos  = text_ctrl->GetPosition();
        wxSize  ctrl_size = text_ctrl->GetSize();

        // Measure with the actual (smaller) unit font, then clamp+ellipsize the
        // draw against the field's right edge so a too-narrow field never paints
        // the unit past the border.
        wxFont unit_font = text_ctrl->GetFont();
        unit_font.SetPointSize(unit_font.GetPointSize() - 1);
        dc.SetFont(unit_font);
        wxSize unit_size = dc.GetTextExtent(m_unit);

        int x = ctrl_pos.x + ctrl_size.x + 4;
        int y = ctrl_pos.y + (ctrl_size.y - unit_size.y) / 2;
        int avail = size.x - x - 5; // room to the field's right edge
        if (avail > 0) {
            wxString unit = unit_size.x > avail
                                ? wxControl::Ellipsize(m_unit, dc, wxELLIPSIZE_END, avail)
                                : m_unit;
            dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurfaceVariant));
            dc.DrawText(unit, wxPoint(x, y));
        }
    }
}

void TextInput::messureSize()
{
    wxSize size = GetSize();
    wxClientDC dc(this);
    labelSize = dc.GetTextExtent(wxWindow::GetLabel());
    wxSize textSize = text_ctrl->GetSize();

    if (!static_tips.empty()) {
        static_tips_size = dc.GetTextExtent(static_tips);
        textSize.x = std::max(labelSize.GetWidth(), static_tips_size.GetWidth());
        textSize.y += static_tips_size.y;
        textSize.y += 8;
    }

    int h = textSize.y + 8;
    if (size.y < h) {
        size.y = h;
    }

    wxSize minSize = size;
    minSize.x = GetMinWidth();
    // The label is drawn after the entry (the unit in "0.1 mm/mm") and DoSetSize() takes its
    // width out of the entry. A field created at a fixed width with a wide unit left the number
    // a sliver: the retraction step drew "0.1" in 22 px and cut anything longer. Keep room for
    // a short number beside the label, whatever width the caller asked for.
    if (text_ctrl && labelSize.x > 0 && static_tips.empty()) {
        int needed = 5 + labelSize.x + 10 + text_ctrl->GetTextExtent(wxS("0.000")).x + FromDIP(6);
        if (this->icon.bmp().IsOk())
            needed += this->icon.GetBmpSize().x;
        if (this->icon_1.bmp().IsOk())
            needed += this->icon_1.GetBmpSize().x;
        if (!m_prefix.IsEmpty())
            needed += dc.GetTextExtent(m_prefix).x + 8;
        if (!m_unit.IsEmpty())
            needed += dc.GetTextExtent(m_unit).x + 5 + 10;
        minSize.x = std::max(minSize.x, needed);
        size.x = std::max(size.x, needed);
    }
    /*if (!m_unit.IsEmpty()) {
        wxClientDC dc(this);
        wxSize     unitSize = dc.GetTextExtent(m_unit);
        minSize.x += unitSize.x + 20;
    }*/
    SetMinSize(minSize);
    SetSize(size);
}

bool TextInput::CheckValid(bool pop_dlg) const
{
    for (auto checker : m_checkers)
    {
        wxString error_msg = checker->CheckValid(text_ctrl->GetValue());
        if (!error_msg.IsEmpty())
        {
            text_ctrl->SetBackgroundColour(StateColor::semantic(MD3::Role::ErrorContainer));
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

    // Reset-on-valid restores the field's fill ROLE (SurfaceContainerHighest),
    // not a raw White literal, so the interior matches the MD3 filled field in
    // both light and dark themes.
    text_ctrl->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHighest));
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
