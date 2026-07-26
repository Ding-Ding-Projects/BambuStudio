#include "Button.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/tipwin.h>
#include <algorithm>
#ifdef __APPLE__
#include "libslic3r/MacUtils.hpp"
#endif

namespace {

// Multiply a colour's RGB channels by a factor and clamp to [0,255] — the MD3
// "state layer as brightness multiply" the digest uses for filled/tonal hover
// (filled x1.06, tonal x1.04). Alpha is preserved.
wxColour brightenColor(const wxColour &c, double factor)
{
    auto ch = [factor](unsigned char v) -> unsigned char {
        double n = v * factor;
        if (n < 0.0) n = 0.0;
        if (n > 255.0) n = 255.0;
        return static_cast<unsigned char>(n + 0.5);
    };
    return wxColour(ch(c.Red()), ch(c.Green()), ch(c.Blue()), c.Alpha());
}

} // namespace
BEGIN_EVENT_TABLE(Button, StaticBox)

EVT_LEFT_DOWN(Button::mouseDown)
EVT_LEFT_UP(Button::mouseReleased)
EVT_MOUSE_CAPTURE_LOST(Button::mouseCaptureLost)
EVT_KEY_DOWN(Button::keyDownUp)
EVT_KEY_UP(Button::keyDownUp)

// catch paint events
EVT_PAINT(Button::paintEvent)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

Button::Button()
    : paddingSize(10, 8)
{
    // Legacy (non-variant) pill default: half the active-density row height, so
    // buttons sized to Metrics::active().row_height read as pills at either
    // density. Variant buttons re-derive their radius in applyMD3Style(), and
    // any explicit SetCornerRadius() still overrides this default.
    SetDefaultCornerRadius(MD3::Metrics::active().row_height / 2);
    background_color = StateColor(
        std::make_pair(ThemeColor::Grey300, (int) StateColor::Disabled),
        std::make_pair(ThemeColor::BrandGreenHovered, (int) StateColor::Hovered | StateColor::Checked),
        std::make_pair(ThemeColor::BrandGreen, (int) StateColor::Checked),
        std::make_pair(ThemeColor::Grey200, (int) StateColor::Hovered),
        std::make_pair(ThemeColor::White, (int) StateColor::Normal));
    text_color       = StateColor(
        std::make_pair(ThemeColor::TextDisabled, (int) StateColor::Disabled),
        std::make_pair(ThemeColor::TextPrimary, (int) StateColor::Normal));
}

Button::Button(wxWindow* parent, wxString text, wxString icon, long style, int iconSize, wxWindowID btn_id)
    : Button()
{
    Create(parent, text, icon, style, iconSize, btn_id);
}

bool Button::Create(wxWindow* parent, wxString text, wxString icon, long style, int iconSize, wxWindowID btn_id)
{
    StaticBox::Create(parent, btn_id, wxDefaultPosition, wxDefaultSize, style);
    state_handler.attach({&text_color});
    state_handler.update_binds();
    //BBS set default font
    SetFont(Label::Body_14);
    wxWindow::SetLabel(text);
    if (!icon.IsEmpty()) {
        //BBS set button icon default size to 20
        this->active_icon = ScalableBitmap(this, icon.ToStdString(), iconSize > 0 ? iconSize : 20);
    }
    messureSize();
    return true;
}

void Button::SetLabel(const wxString& label)
{
    if (label != wxWindow::GetLabel()) {
        wxWindow::SetLabel(label);
        messureSize();
        Refresh();
    }
}

bool Button::SetFont(const wxFont& font)
{
    wxWindow::SetFont(font);
    messureSize();
    Refresh();
    return true;
}

void Button::SetIcon(const wxString& icon)
{
    auto tmpBitmap = ScalableBitmap(this, icon.ToStdString(), this->active_icon.px_cnt());
    if (!icon.IsEmpty()) {
        //BBS set button icon default size to 20
        if (!tmpBitmap.bmp().IsSameAs(this->active_icon.bmp())) {
            this->active_icon = tmpBitmap;
            Refresh();
        }
    }
    else
    {
        this->active_icon = ScalableBitmap();
        Refresh();
    }
}

void Button::SetInactiveIcon(const wxString &icon)
{
    if (!icon.IsEmpty()) {
        // BBS set button icon default size to 20
        this->inactive_icon = ScalableBitmap(this, icon.ToStdString(), this->active_icon.px_cnt());
    } else {
        this->inactive_icon = ScalableBitmap();
    }
    Refresh();
}

void Button::SetMinSize(const wxSize& size)
{
    minSize = size;
    messureSize();
}

void Button::SetMaxSize(const wxSize& size)
{
    wxWindow::SetMaxSize(size);
    messureSize();
}

void Button::SetPaddingSize(const wxSize& size)
{
    paddingSize = size;
    messureSize();
}

void Button::SetAllowShrink(bool allow)
{
    if (m_allow_shrink == allow)
        return;
    m_allow_shrink = allow;
    messureSize();
}

void Button::SetTextColor(StateColor const& color)
{
    text_color = color;
    state_handler.update_binds();
    Refresh();
}

void Button::SetTextColorNormal(wxColor const &color)
{
    text_color.setColorForStates(color, 0);
    Refresh();
}

void Button::SetVariant(Variant variant)
{
    m_variant     = variant;
    m_md3_variant = true;
    applyMD3Style();
}

void Button::SetButtonSize(Size size)
{
    m_button_size = size;
    if (m_md3_variant)
        applyMD3Style();
}

void Button::SetColorScheme(MD3::ColorScheme scheme)
{
    m_scheme = scheme;
    if (m_md3_variant)
        applyMD3Style();
}

void Button::SetGlyph(uint32_t codepoint, int px)
{
    m_glyph_cp  = codepoint;
    m_has_glyph = codepoint != 0;
    m_glyph_px  = px > 0 ? px : 0;
    if (m_md3_variant) {
        // Re-derive geometry (an IconButton's glyph size feeds its fallback
        // raster rebuild) and repaint.
        applyMD3Style();
    } else {
        messureSize();
        Refresh();
    }
}

void Button::SetGlyphColor(StateColor const &color)
{
    glyph_color = color;
    state_handler.update_binds();
    Refresh();
}

void Button::SetIconButton(IconShape shape, int container_px, bool filled, bool danger)
{
    m_variant     = Variant::IconButton;
    m_md3_variant = true;
    m_icon_shape  = shape;
    if (container_px > 0)
        m_icon_container_px = container_px;
    m_icon_filled = filled;
    m_icon_danger = danger;
    applyMD3Style();
}

int Button::effectiveGlyphPx() const
{
    if (m_glyph_px > 0)
        return m_glyph_px;
    if (m_md3_variant && m_variant == Variant::IconButton) {
        // Kit derivation: 22 (>=40) / 19 (>=34) / 17 by container edge.
        const int c = m_icon_container_px > 0 ? m_icon_container_px : 36;
        return c >= 40 ? 22 : (c >= 34 ? 19 : 17);
    }
    // Pill variant / legacy: match the size-tier icon glyph (18/20/20).
    return m_button_size == Size::Small ? 18 : 20;
}

void Button::rebuildIcons(int px)
{
    if (active_icon.bmp().IsOk() && !active_icon.name().empty() && active_icon.px_cnt() != px)
        active_icon = ScalableBitmap(this, active_icon.name(), px);
    if (inactive_icon.bmp().IsOk() && !inactive_icon.name().empty() && inactive_icon.px_cnt() != px)
        inactive_icon = ScalableBitmap(this, inactive_icon.name(), px);
}

void Button::applyMD3Style()
{
    if (!m_md3_variant)
        return;

    using R = MD3::Role;
    const MD3::ColorScheme s = m_scheme;

    // Borderless IconButton mode: a circle (radius = half the container) or
    // square (r8) ghost touch target that draws a single centered glyph. Its
    // geometry and neutral surface colours differ from the pill variants, so it
    // is configured here and returns before the pill layout runs.
    if (m_variant == Variant::IconButton) {
        const wxColour disabledFg = ThemeColor::TextDisabled;
        const wxColour parentBg   = StaticBox::GetParentBackgroundColor(GetParent());
        const int      container  = m_icon_container_px > 0 ? m_icon_container_px : 36;
        const wxColour rest       = m_icon_filled ? StateColor::semantic(R::SurfaceContainerHighest)
                                                  : parentBg;

        // Refresh the StaticBox window background (used to clear the area behind
        // the rounded shape in StaticBox::render). The Create-time snapshot goes
        // stale when the parent is themed AFTER the button is constructed — in
        // dark mode that left a light system-#F0F0F0 square behind the circular
        // icon button (Prepare action bar). Rescale()/theme rebuild re-runs this.
        SetBackgroundColour(parentBg);

        StateColor bg, fg;
        if (m_icon_danger) {
            // Window-close idiom: hover fills Error and flips the glyph to OnError.
            bg = StateColor(std::make_pair(StateColor::semantic(R::Error), (int) StateColor::Hovered),
                            std::make_pair(rest, (int) StateColor::Normal));
            fg = StateColor(std::make_pair(disabledFg, (int) StateColor::Disabled),
                            std::make_pair(StateColor::semantic(R::OnError), (int) StateColor::Hovered),
                            std::make_pair(StateColor::semantic(R::OnSurfaceVariant), (int) StateColor::Normal));
        } else {
            bg = StateColor(std::make_pair(StateColor::semantic(R::SurfaceContainerHigh), (int) StateColor::Hovered),
                            std::make_pair(rest, (int) StateColor::Normal));
            fg = StateColor(std::make_pair(disabledFg, (int) StateColor::Disabled),
                            std::make_pair(StateColor::semantic(R::OnSurfaceVariant), (int) StateColor::Normal));
        }

        background_color = bg;
        text_color       = fg;
        SetBorderWidth(0);
        // Circle => pill radius (half the DPI-scaled edge); square => r8.
        if (m_icon_shape == IconShape::Circle)
            SetCornerRadius(FromDIP(container) / 2.0);
        else
            SetCornerRadius(FromDIP(MD3::Metrics::radius_tiny));

        // Square adds 4px of width (matches the kit window-control shape); the
        // glyph stays centered via render()'s isCenter path.
        const int wpx = container + (m_icon_shape == IconShape::Square ? 4 : 0);
        paddingSize    = wxSize(0, 0);
        minSize        = wxSize(FromDIP(wpx), FromDIP(container));

        // Keep any fallback raster icon sized to the glyph so it lines up when
        // the Material Symbols face is unavailable.
        rebuildIcons(effectiveGlyphPx());

        state_handler.update_binds();
        messureSize();
        Refresh();
        return;
    }

    // Size tier geometry + label size + icon glyph size.
    int   height = 42, hpad = 18, icon_px = 20;
    switch (m_button_size) {
    case Size::Small: height = 36; hpad = 16; icon_px = 18; break;
    case Size::Large: height = 44; hpad = 22; icon_px = 20; break;
    case Size::Medium:
    default:          height = 42; hpad = 18; icon_px = 20; break;
    }

    // Per-size label font. The 12.5/13.5/14 weight-600 tokens already exist as
    // Head_12/Head_13/Head_14 (see Label::initSysFont). Outlined uses weight
    // 500, obtained by cloning the size-matched face and lowering the weight so
    // the Roboto/CJK face and design px are preserved.
    wxFont font = m_button_size == Size::Small ? Label::Head_12
                : m_button_size == Size::Large ? Label::Head_14
                                               : Label::Head_13;
    if (m_variant == Variant::Outlined) {
        font.SetWeight(wxFONTWEIGHT_MEDIUM);
        font.SetNumericWeight(500);
    }

    const wxColour disabledBg  = StateColor::semantic(R::SurfaceContainerHigh);
    const wxColour disabledTxt = ThemeColor::TextDisabled;
    const wxColour parentBg    = StaticBox::GetParentBackgroundColor(GetParent());

    // Same stale-snapshot fix as the IconButton branch above: keep the pill's
    // rounded-corner backing in sync with the parent's CURRENT background.
    SetBackgroundColour(parentBg);

    StateColor bg, fg, bd;
    int        bw = 0;

    switch (m_variant) {
    case Variant::IconButton: break; // configured and returned above; unreachable here
    case Variant::Filled: {
        const wxColour fill = StateColor::semantic(R::Primary, s);
        bg = StateColor(std::make_pair(disabledBg, (int) StateColor::Disabled),
                        std::make_pair(brightenColor(fill, 1.06), (int) StateColor::Hovered),
                        std::make_pair(fill, (int) StateColor::Normal));
        fg = StateColor(std::make_pair(disabledTxt, (int) StateColor::Disabled),
                        std::make_pair(StateColor::semantic(R::OnPrimary, s), (int) StateColor::Normal));
        bw = 0;
        break;
    }
    case Variant::Tonal: {
        const wxColour fill = StateColor::semantic(R::SecondaryContainer, s);
        bg = StateColor(std::make_pair(disabledBg, (int) StateColor::Disabled),
                        std::make_pair(brightenColor(fill, 1.04), (int) StateColor::Hovered),
                        std::make_pair(fill, (int) StateColor::Normal));
        fg = StateColor(std::make_pair(disabledTxt, (int) StateColor::Disabled),
                        std::make_pair(StateColor::semantic(R::OnSecondaryContainer, s), (int) StateColor::Normal));
        bw = 0;
        break;
    }
    case Variant::Outlined: {
        // Transparent interior (parent bg for the rest fill) + Outline ring;
        // hover adds a SurfaceContainerHigh wash while the ring/label hold.
        bg = StateColor(std::make_pair(StateColor::semantic(R::SurfaceContainerHigh), (int) StateColor::Hovered),
                        std::make_pair(parentBg, (int) StateColor::Normal));
        fg = StateColor(std::make_pair(disabledTxt, (int) StateColor::Disabled),
                        std::make_pair(StateColor::semantic(R::OnSurface), (int) StateColor::Normal));
        bd = StateColor(std::make_pair(StateColor::semantic(R::OutlineVariant), (int) StateColor::Disabled),
                        std::make_pair(StateColor::semantic(R::Outline), (int) StateColor::Normal));
        bw = 1;
        break;
    }
    case Variant::Text: {
        // No border, transparent at rest; hover adds a SecondaryContainer wash.
        bg = StateColor(std::make_pair(StateColor::semantic(R::SecondaryContainer, s), (int) StateColor::Hovered),
                        std::make_pair(parentBg, (int) StateColor::Normal));
        fg = StateColor(std::make_pair(disabledTxt, (int) StateColor::Disabled),
                        std::make_pair(StateColor::semantic(R::Primary, s), (int) StateColor::Normal));
        bw = 0;
        break;
    }
    case Variant::Danger: {
        // Transparent + Error ring/label; hover adds a SurfaceContainerHigh wash
        // while the Error ring and Error label hold.
        bg = StateColor(std::make_pair(StateColor::semantic(R::SurfaceContainerHigh), (int) StateColor::Hovered),
                        std::make_pair(parentBg, (int) StateColor::Normal));
        fg = StateColor(std::make_pair(disabledTxt, (int) StateColor::Disabled),
                        std::make_pair(StateColor::semantic(R::Error), (int) StateColor::Normal));
        bd = StateColor(std::make_pair(StateColor::semantic(R::OutlineVariant), (int) StateColor::Disabled),
                        std::make_pair(StateColor::semantic(R::Error), (int) StateColor::Normal));
        bw = 1;
        break;
    }
    }

    background_color = bg;
    text_color       = fg;
    if (bw > 0)
        border_color = bd;

    SetBorderWidth(bw);
    // Pill radius = button height / 2 (18 / 21 / 22 for sm / md / lg).
    SetCornerRadius(FromDIP(height) / 2.0);

    paddingSize = wxSize(FromDIP(hpad), paddingSize.y);
    minSize.SetHeight(FromDIP(height));

    wxWindow::SetFont(font);
    rebuildIcons(icon_px);

    state_handler.update_binds();
    messureSize();
    Refresh();
}

bool Button::Enable(bool enable)
{
    bool result = wxWindow::Enable(enable);
    if (result) {
        wxCommandEvent e(EVT_ENABLE_CHANGED);
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(e);
    }
    return result;
}

void Button::SetCanFocus(bool canFocus) { this->canFocus = canFocus; }

void Button::SetValue(bool state)
{
    if (GetValue() == state) return;
    state_handler.set_state(state ? StateHandler::Checked : 0, StateHandler::Checked);
}

bool Button::GetValue() const { return state_handler.states() & StateHandler::Checked; }

void Button::SetCenter(bool isCenter)
{
    this->isCenter = isCenter; }

void Button::SetVertical(bool vertical)
{
    this->vertical = vertical;
    messureSize();
}

void Button::Rescale()
{
    RescaleDefaultCornerRadius();

    if (this->active_icon.bmp().IsOk())
        this->active_icon.msw_rescale();

    if (this->inactive_icon.bmp().IsOk())
        this->inactive_icon.msw_rescale();

    // Re-derive the DPI-scaled radius / padding / height / icon size for a
    // variant Button so it survives monitor DPI changes.
    if (m_md3_variant) {
        applyMD3Style();
        return;
    }

    messureSize();
    Refresh();
}

void Button::paintEvent(wxPaintEvent& evt)
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
void Button::render(wxDC& dc)
{
    StaticBox::render(dc);
    if (m_left_corner_white || m_right_corner_white) {
        renderWhiteCorners(dc);
    }
    int states = state_handler.states();
    wxSize size = GetSize();
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    // calc content size
    wxSize szIcon;
    wxSize textSize = this->textSize.GetSize();

    ScalableBitmap icon;
    if (m_selected || ((states & (int)StateColor::State::Hovered) != 0))
        icon = active_icon;
    else
        icon = inactive_icon;
    wxSize padding = this->paddingSize;
    // MD3 icon->label gap is 8px (was a hardcoded 5). DIP-scaled so it holds on
    // HiDPI; must stay in sync with the value used by messureSize().
    int spacing = FromDIP(8);
    // Wrap text
    auto text = GetLabel();
    if (vertical && textSize.x + padding.x * 2 > size.x) {
        Label::split_lines(dc, size.x - padding.x * 2, text, text, 2);
        textSize = dc.GetMultiLineTextExtent(text);
        if (padding.x * 2 + textSize.x > size.x) {
            text = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END, size.x - padding.x * 2);
            textSize = dc.GetMultiLineTextExtent(text);
        }
    }
    // Glyph content (part b): when a Material Symbols glyph is set and the icon
    // face resolves, it stands in for the raster icon and is drawn live in the
    // state-resolved text colour. A raster icon set alongside it is the fallback
    // used when the Material Symbols face is unavailable.
    const bool drawGlyph = m_has_glyph && MaterialIcon::available();
    const int  glyph_px  = drawGlyph ? effectiveGlyphPx() : 0;
    const bool hasIcon   = drawGlyph || icon.bmp().IsOk();
    // Don't reserve the icon->label gap when there is no label, so a glyph-only
    // IconButton stays exactly centered; legacy raster call sites keep their gap.
    const bool tightCenter = drawGlyph || (m_md3_variant && m_variant == Variant::IconButton);

    auto szContent = textSize;
    if (hasIcon) {
        if (szContent.y > 0 && !(tightCenter && text.IsEmpty())) {
            //BBS norrow size between text and icon
            if (vertical)
                szContent.y += spacing;
            else
                szContent.x += spacing;
        }
        szIcon = drawGlyph ? MaterialIcon::measure(dc, m_glyph_cp, glyph_px) : icon.GetBmpSize();
        if (vertical) {
            szContent.y += szIcon.y;
            if (szIcon.x > szContent.x) szContent.x = szIcon.x;
        } else {
            szContent.x += szIcon.x;
            if (szIcon.y > szContent.y) szContent.y = szIcon.y;
        }
        if (szContent.x > size.x) {
            int d = std::min(padding.x, (szContent.x - size.x) / 2);
            padding.x -= d;
            szContent.x -= d;
        }
    }
    // move to center
    wxRect rcContent = { {0, 0}, size };
    if (isCenter) {
        wxSize offset = (size - szContent) / 2;
        if (offset.x < 0) offset.x = 0;
        rcContent.Deflate(offset.x, offset.y);
    }
    // start draw
    wxPoint pt = rcContent.GetLeftTop();
    if (hasIcon) {
        if (vertical)
            pt.x += (rcContent.width - szIcon.x) / 2;
        else
            pt.y += (rcContent.height - szIcon.y) / 2;
        if (drawGlyph)
            MaterialIcon::draw(dc, m_glyph_cp, glyph_px,
                               (glyph_color.count() > 0 ? glyph_color : text_color).colorForStates(states), pt);
        else
            dc.DrawBitmap(icon.bmp(), pt);
        //BBS norrow size between text and icon
        if (vertical) {
            pt.y += szIcon.y + spacing;
            pt.x = rcContent.x;
        } else {
            pt.x += szIcon.x + spacing;
            pt.y = rcContent.y;
        }
    }
    if (!text.IsEmpty()) {
        if (vertical) {
            pt.x += (rcContent.width - textSize.x) / 2;
        } else {
            if (pt.x + textSize.x > size.x)
                text = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END, size.x - pt.x);
            pt.y += (rcContent.height - textSize.y) / 2;
        }
        dc.SetTextForeground(text_color.colorForStates(states));
#if 0
        dc.SetBrush(*wxLIGHT_GREY);
        dc.SetPen(wxPen(*wxLIGHT_GREY));
        dc.DrawRectangle(pt, textSize.GetSize());
#endif
#ifdef __WXOSX__
        pt.y -= this->textSize.x / 2;
#endif
#ifdef __APPLE__
        if (Slic3r::is_mac_version_15()) {
        pt.y -= FromDIP(1);
    }
#endif
        dc.DrawText(text, pt);
    }
}

void Button::renderWhiteCorners(wxDC& dc)
{
    wxSize size = GetSize();
    int r = static_cast<int>(radius);
    wxColor parent_bg_color = StaticBox::GetParentBackgroundColor(GetParent());
    int states = state_handler.states();
    wxColor bg_color = background_color.colorForStates(states);

    auto drawWhiteCorners = [&](wxDC &dc) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(parent_bg_color));

        if (m_left_corner_white) {
            dc.DrawRectangle(0, 0, r, r);
            dc.DrawRectangle(0, size.y - r, r, r);

            dc.SetBrush(wxBrush(bg_color));
            dc.DrawRoundedRectangle(0, 0, r * 2, r * 2, r);
            dc.DrawRoundedRectangle(0, size.y - r * 2, r * 2, r * 2, r);
        }

        if (m_right_corner_white) {
            dc.DrawRectangle(size.x - r, 0, r, r);
            dc.DrawRectangle(size.x - r, size.y - r, r, r);

            dc.SetBrush(wxBrush(bg_color));
            dc.DrawRoundedRectangle(size.x - r * 2, 0, r * 2, r * 2, r);
            dc.DrawRoundedRectangle(size.x - r * 2, size.y - r * 2, r * 2, r * 2, r);
        }
    };
#ifdef __WXMSW__
    auto renderWithAntialiasing = [&]() -> bool {
        wxMemoryDC memdc(&dc);
        if (!memdc.IsOk()) return false;

        wxBitmap bmp(size.x, size.y);
        memdc.SelectObject(bmp);
        memdc.Blit({0, 0}, size, &dc, {0, 0});

        wxGCDC gcdc(memdc);
        if (!gcdc.IsOk()) return false;

        drawWhiteCorners(gcdc);

        memdc.SelectObject(wxNullBitmap);
        dc.DrawBitmap(bmp, 0, 0);
        return true;
    };

    if (!renderWithAntialiasing()) {
        drawWhiteCorners(dc);
    }
#else
    drawWhiteCorners(dc);
#endif
}

void Button::messureSize()
{
    wxClientDC dc(this);
    dc.GetTextExtent(GetLabel(), &textSize.width, &textSize.height, &textSize.x, &textSize.y);
    wxSize szContent = textSize.GetSize();
    // Mirror render(): a Material Symbols glyph stands in for the raster icon
    // for sizing when the icon face resolves, else the raster icon is measured.
    const bool drawGlyph   = m_has_glyph && MaterialIcon::available();
    const bool tightCenter = drawGlyph || (m_md3_variant && m_variant == Variant::IconButton);
    wxSize     szIcon;
    bool       hasIcon = false;
    if (drawGlyph) {
        szIcon  = MaterialIcon::measure(dc, m_glyph_cp, effectiveGlyphPx());
        hasIcon = true;
    } else if (this->active_icon.bmp().IsOk()) {
        szIcon  = this->active_icon.GetBmpSize();
        hasIcon = true;
    }
    if (hasIcon) {
        if (szContent.y > 0 && !(tightCenter && GetLabel().IsEmpty())) {
            // MD3 icon->label gap is 8px; keep in sync with render()'s spacing.
            if (vertical)
                szContent.y += FromDIP(8);
            else
                szContent.x += FromDIP(8);
        }
        if (vertical) {
            szContent.y += szIcon.y;
            if (szIcon.x > szContent.x) szContent.x = szIcon.x;
        } else {
            szContent.x += szIcon.x;
            if (szIcon.y > szContent.y) szContent.y = szIcon.y;
        }
    }
    wxSize size = szContent + paddingSize * 2;
    if (minSize.GetHeight() > 0)
        size.SetHeight(minSize.GetHeight());

    if (auto w = GetMaxWidth(); w > 0 && size.GetWidth() > w) {
        size.SetWidth(GetMaxWidth());

        const wxString& tip_str = GetToolTipText();
        if (tip_str.IsEmpty()) {
            SetToolTip(GetLabel());
        }
    }

    // BBS: when shrinking is allowed, honor the explicit min width as the window's
    // min size even though the content is wider. The sizer may then compress the
    // button, and render() truncates the label with an ellipsis. The content width
    // is still reported as the best size so the button prefers its full width.
    if (m_allow_shrink && minSize.GetWidth() > 0) {
        wxSize minWnd = size;
        minWnd.SetWidth(minSize.GetWidth());
        wxWindow::SetMinSize(minWnd);
        // Keep the content size as the best-size hint so the sizer prefers the full
        // width (up to the max) when there is room, and only compresses when crowded.
        CacheBestSize(size);
        return;
    }

    if (minSize.GetWidth() > size.GetWidth())
        wxWindow::SetMinSize(minSize);
    else
        wxWindow::SetMinSize(size);
}

void Button::mouseDown(wxMouseEvent& event)
{
    event.Skip();
    pressedDown = true;
    if (canFocus)
        SetFocus();
    if (!HasCapture())
        CaptureMouse();
}

void Button::mouseReleased(wxMouseEvent& event)
{
    event.Skip();
    if (pressedDown) {
        pressedDown = false;
        if (HasCapture())
            ReleaseMouse();
        if (wxRect({0, 0}, GetSize()).Contains(event.GetPosition()))
            sendButtonEvent();
    }
}

void Button::mouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    wxMouseEvent evt;
    mouseReleased(evt);
}

void Button::keyDownUp(wxKeyEvent &event)
{
    if (event.GetKeyCode() == WXK_SPACE || event.GetKeyCode() == WXK_RETURN) {
        wxMouseEvent evt(event.GetEventType() == wxEVT_KEY_UP ? wxEVT_LEFT_UP : wxEVT_LEFT_DOWN);
        event.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
        return;
    }
    if (event.GetEventType() == wxEVT_KEY_DOWN &&
        (event.GetKeyCode() == WXK_TAB || event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT
        || event.GetKeyCode() == WXK_UP || event.GetKeyCode() == WXK_DOWN))
        HandleAsNavigationKey(event);
    else
        event.Skip();
}

void Button::sendButtonEvent()
{
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    event.SetEventObject(this);
    GetEventHandler()->ProcessEvent(event);
}

#ifdef __WIN32__

WXLRESULT Button::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if (nMsg == WM_GETDLGCODE) { return DLGC_WANTMESSAGE; }
    if (nMsg == WM_KEYDOWN) {
        wxKeyEvent event(CreateKeyEvent(wxEVT_KEY_DOWN, wParam, lParam));
        switch (wParam) {
        case WXK_RETURN: { // WXK_RETURN key is handled by default button
            GetEventHandler()->ProcessEvent(event);
            return 0;
        }
        }
    }
    return wxWindow::MSWWindowProc(nMsg, wParam, lParam);
}

#endif

bool Button::AcceptsFocus() const { return canFocus; }

void Button::EnableTooltipEvenDisabled()
{
#if defined(_MSC_VER) || defined(_WIN32)
    auto parent = this->GetParent();
    if (parent)
    {
        parent->Bind(wxEVT_MOTION, &Button::OnParentMotion, this);
        parent->Bind(wxEVT_LEAVE_WINDOW, &Button::OnParentLeave, this);
    };
#endif
};

void Button::OnParentMotion(wxMouseEvent& event)
{
    auto parent = this->GetParent();
    if (!parent) return event.Skip();

    wxPoint pos = parent->ClientToScreen(event.GetPosition());
    wxRect screen_rect = this->GetScreenRect();
    wxString tip = this->GetToolTipText();
    if (!tip.IsEmpty() && !this->IsEnabled() && screen_rect.Contains(pos))
    {
        if (!tipWindow)
        {
            tipWindow = new wxTipWindow(this, tip);
            tipWindow->Bind(wxEVT_DESTROY, [this](wxEvent& event) { this->tipWindow = nullptr;});
            tipWindow->Enable(false);
        }

        if (tipWindow->GetLabel() != tip)
        {
            tipWindow->SetLabel(tip);
        }

        tipWindow->Position(wxGetMousePosition(), wxSize(0, 0));
        tipWindow->Popup();
    }
    else
    {
        if (tipWindow)
        {
            delete tipWindow;
            tipWindow = nullptr;
        }
    }

    event.Skip();
}

void Button::OnParentLeave(wxMouseEvent& event)
{
    auto parent = this->GetParent();
    if (!parent) return event.Skip();

    if (tipWindow)
    {
        wxPoint pos = parent->ClientToScreen(event.GetPosition());
        wxRect screen_rect = this->GetScreenRect();
        wxString tip = this->GetToolTipText();
        if (!screen_rect.Contains(pos))
        {
            tipWindow->Dismiss();
            delete tipWindow;
            tipWindow = nullptr;
        }
    }

    event.Skip();
}
