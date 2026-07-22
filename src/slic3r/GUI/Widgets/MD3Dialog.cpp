#include "MD3Dialog.hpp"

#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/region.h>

#include "Button.hpp"
#include "Label.hpp"
#include "StateColor.hpp"
#include "../GUI_App.hpp"

namespace Slic3r { namespace GUI {

// ----------------------------------------------------------------------------
// MD3HeaderTile — the 44x44 rounded PrimaryContainer icon tile that leads the
// dialog header. Owner-drawn so it stays theme- and scheme-adaptive (the roles
// are resolved live in OnPaint, so a light/dark or Brand/Preview/Device change
// just needs a Refresh()).
// ----------------------------------------------------------------------------
class MD3HeaderTile : public wxWindow
{
public:
    MD3HeaderTile(wxWindow *parent, uint32_t glyph)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
        , m_glyph(glyph)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        const wxSize edge(FromDIP(44), FromDIP(44));
        SetMinSize(edge);
        SetMaxSize(edge);
        Bind(wxEVT_PAINT, &MD3HeaderTile::OnPaint, this);
    }

    void SetGlyph(uint32_t glyph)
    {
        m_glyph = glyph;
        Refresh();
    }
    void SetAccent(MD3::Role container, MD3::Role on)
    {
        m_container = container;
        m_on        = on;
        Refresh();
    }
    void SetScheme(MD3::ColorScheme scheme)
    {
        m_scheme = scheme;
        Refresh();
    }

private:
    void OnPaint(wxPaintEvent &)
    {
        wxPaintDC    dc(this);
        const wxSize size = GetSize();
        if (size.x <= 0 || size.y <= 0)
            return;

        // Clear with the dialog surface so the tile's rounded corners reveal it,
        // then draw the rounded fill + glyph on an antialiased wxGCDC (the same
        // memdc + wxGCDC double-buffered pattern the shared widgets use).
        const wxColour parentBg = GetParent() ? GetParent()->GetBackgroundColour()
                                              : StateColor::semantic(MD3::Role::SurfaceContainer);
        wxMemoryDC memdc(&dc);
        if (!memdc.IsOk())
            return;
        wxBitmap bmp(size.x, size.y);
        memdc.SelectObject(bmp);
        memdc.SetBackground(wxBrush(parentBg));
        memdc.Clear();
        {
            wxGCDC gc(memdc);
            gc.SetPen(*wxTRANSPARENT_PEN);
            gc.SetBrush(wxBrush(StateColor::semantic(m_container, m_scheme)));
            gc.DrawRoundedRectangle(0, 0, size.x, size.y, FromDIP(MD3::Metrics::radius_icon_tile));
            if (m_glyph != 0 && MaterialIcon::available())
                MaterialIcon::drawCentered(gc, m_glyph, FromDIP(24),
                                           StateColor::semantic(m_on, m_scheme),
                                           wxRect(0, 0, size.x, size.y));
        }
        memdc.SelectObject(wxNullBitmap);
        dc.DrawBitmap(bmp, 0, 0);
    }

    uint32_t         m_glyph;
    MD3::Role        m_container = MD3::Role::PrimaryContainer;
    MD3::Role        m_on        = MD3::Role::OnPrimaryContainer;
    MD3::ColorScheme m_scheme    = MD3::ColorScheme::Brand;
};

// ----------------------------------------------------------------------------
// MD3Dialog
// ----------------------------------------------------------------------------
MD3Dialog::MD3Dialog(wxWindow *          parent,
                     const wxString &    title,
                     const wxString &    subtitle,
                     MaterialIcon::Glyph header_glyph)
    // A null parent yields an ownerless top-level dialog; callers that want the
    // main frame as owner (e.g. MsgDialog) resolve it before constructing.
    : DPIDialog(parent,
                wxID_ANY,
                title,
                wxDefaultPosition,
                wxDefaultSize,
                wxBORDER_NONE | wxFRAME_SHAPED)
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    SetFont(wxGetApp().normal_font());

    build_shell(title, subtitle, header_glyph);

    // A shaped, borderless window must re-apply its rounded region whenever its
    // size changes (initial fit, extended-message re-fit, details expand, ...).
    Bind(wxEVT_SIZE, [this](wxSizeEvent &e) {
        UpdateShape();
        e.Skip();
    });
    // Restore the movability lost with the native title bar: the dialog surface
    // and the (non-interactive) header widgets act as drag handles.
    bind_drag(this);
}

MD3Dialog::~MD3Dialog() {}

void MD3Dialog::build_shell(const wxString &title, const wxString &subtitle, MaterialIcon::Glyph glyph)
{
    auto *main = new wxBoxSizer(wxVERTICAL);

    // --- Header: tile + title/subtitle + circular close (pad 22/24/14) --------
    auto *header = new wxBoxSizer(wxHORIZONTAL);

    m_tile = new MD3HeaderTile(this, glyph);
    header->Add(m_tile, 0, wxALIGN_CENTER_VERTICAL);
    header->AddSpacer(FromDIP(12));

    auto *titles = new wxBoxSizer(wxVERTICAL);
    m_title_txt  = new wxStaticText(this, wxID_ANY, title);
    m_title_txt->SetFont(::Label::Head_18);
    m_title_txt->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    titles->Add(m_title_txt, 0, wxEXPAND);

    m_subtitle_txt = new wxStaticText(this, wxID_ANY, subtitle);
    m_subtitle_txt->SetFont(::Label::Body_12);
    m_subtitle_txt->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    titles->Add(m_subtitle_txt, 0, wxEXPAND | wxTOP, FromDIP(2));
    if (subtitle.IsEmpty())
        m_subtitle_txt->Hide();

    header->Add(titles, 1, wxALIGN_CENTER_VERTICAL);
    header->AddSpacer(FromDIP(8));

    // Circular 36px close IconButton (glyph), with a text fallback when the
    // Material Symbols face is unavailable so the affordance is never blank.
    if (MaterialIcon::available()) {
        m_close_btn = new Button(this, "", "", 0, 0, wxID_ANY);
        m_close_btn->SetIconButton(Button::IconShape::Circle, 36);
        m_close_btn->SetGlyph(MaterialIcon::Close, 20);
    } else {
        m_close_btn                = new Button(this, wxString(wxUniChar(0x2715)));
        const wxColour parentBg    = GetBackgroundColour();
        m_close_btn->SetBackgroundColor(StateColor(parentBg));
        m_close_btn->SetBorderColor(StateColor(parentBg));
        m_close_btn->SetTextColor(StateColor(StateColor::semantic(MD3::Role::OnSurfaceVariant)));
        m_close_btn->SetMinSize(wxSize(FromDIP(36), FromDIP(36)));
        m_close_btn->SetCornerRadius(FromDIP(18));
    }
    m_close_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { OnHeaderClose(); });
    header->Add(m_close_btn, 0, wxALIGN_CENTER_VERTICAL);

    main->AddSpacer(FromDIP(22));
    main->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(24));
    main->AddSpacer(FromDIP(14));

    // --- Body: content sizer (pad 0/24), children parented to the dialog ------
    m_content_sizer = new wxBoxSizer(wxVERTICAL);
    main->Add(m_content_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(24));
    main->AddSpacer(FromDIP(16));

    // --- Footer: 1px OutlineVariant top border + right-aligned button row -----
    m_footer_line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(1)), wxTAB_TRAVERSAL);
    m_footer_line->SetBackgroundColour(StateColor::semantic(MD3::Role::OutlineVariant));
    main->Add(m_footer_line, 0, wxEXPAND);

    m_footer_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_footer_sizer->AddStretchSpacer(); // leading stretch => AddFooterButton right-aligns
    auto *footer_wrap = new wxBoxSizer(wxHORIZONTAL);
    footer_wrap->Add(m_footer_sizer, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(24));
    main->Add(footer_wrap, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(14));

    SetSizer(main);

    bind_drag(m_tile);
    bind_drag(m_title_txt);
    bind_drag(m_subtitle_txt);
}

Button *MD3Dialog::AddFooterButton(Button *btn)
{
    if (btn && m_footer_sizer)
        m_footer_sizer->Add(btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));
    return btn;
}

void MD3Dialog::SetHeaderGlyph(MaterialIcon::Glyph glyph)
{
    if (m_tile)
        m_tile->SetGlyph(glyph);
}

void MD3Dialog::SetHeaderTitle(const wxString &title)
{
    if (m_title_txt) {
        m_title_txt->SetLabel(title);
        Layout();
    }
}

void MD3Dialog::SetHeaderSubtitle(const wxString &subtitle)
{
    if (!m_subtitle_txt)
        return;
    m_subtitle_txt->SetLabel(subtitle);
    m_subtitle_txt->Show(!subtitle.IsEmpty());
    Layout();
}

void MD3Dialog::SetHeaderAccent(MD3::Role container_role, MD3::Role on_role)
{
    if (m_tile)
        m_tile->SetAccent(container_role, on_role);
}

void MD3Dialog::SetColorScheme(MD3::ColorScheme scheme)
{
    m_scheme = scheme;
    if (m_tile)
        m_tile->SetScheme(scheme);
    Refresh();
}

void MD3Dialog::OnHeaderClose() { EndModal(wxID_CANCEL); }

void MD3Dialog::UpdateShape()
{
    const wxSize size = GetSize();
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
        return;

    // Build a black/white mask (white == opaque rounded rect) and derive the
    // window region from it, mirroring the established shaped-dialog pattern.
    wxBitmap mask(size.GetWidth(), size.GetHeight(), 32);
    {
        wxMemoryDC dc;
        dc.SelectObject(mask);
        dc.SetBackground(wxBrush(wxColour(0, 0, 0)));
        dc.Clear();
        dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRoundedRectangle(0, 0, size.GetWidth(), size.GetHeight(), FromDIP(m_corner_radius));
        dc.SelectObject(wxNullBitmap);
    }

    wxRegion region(mask, wxColour(0, 0, 0));
    if (region.IsOk())
        SetShape(region);
}

void MD3Dialog::on_dpi_changed(const wxRect & /*suggested_rect*/) { UpdateShape(); }

void MD3Dialog::bind_drag(wxWindow *w)
{
    if (!w)
        return;
    w->Bind(wxEVT_LEFT_DOWN, [this, w](wxMouseEvent &e) {
        m_dragging    = true;
        m_drag_offset = w->ClientToScreen(e.GetPosition()) - GetPosition();
        if (!w->HasCapture())
            w->CaptureMouse();
        e.Skip();
    });
    w->Bind(wxEVT_MOTION, [this, w](wxMouseEvent &e) {
        if (m_dragging && e.Dragging() && e.LeftIsDown())
            Move(w->ClientToScreen(e.GetPosition()) - m_drag_offset);
        e.Skip();
    });
    w->Bind(wxEVT_LEFT_UP, [this, w](wxMouseEvent &e) {
        if (m_dragging) {
            m_dragging = false;
            if (w->HasCapture())
                w->ReleaseMouse();
        }
        e.Skip();
    });
    w->Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { m_dragging = false; });
}

}} // namespace Slic3r::GUI
