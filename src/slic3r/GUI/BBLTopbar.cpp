#include "BBLTopbar.hpp"
#include "wx/artprov.h"
#include "wx/aui/framemanager.h"
#include "wx/display.h"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "GUI.hpp"
#include "wxExtensions.hpp"
#include "Plater.hpp"
#include "MainFrame.hpp"
#include "WebViewDialog.hpp"
#include "PartPlate.hpp"
#include "ReleaseNote.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/Label.hpp"

#include <wx/dcmemory.h>
#include <wx/graphics.h>

#include <algorithm>
#include <cmath>
#include <boost/log/trivial.hpp>

#define TOPBAR_ICON_SIZE  18
#define TOPBAR_TITLE_WIDTH  300
// §3.6 project chip: the ellipsized project name is capped at 150 logical px.
#define TOPBAR_PROJECT_CHIP_MAX_W  150
// §3.5 history chip: cap the branch label so a long branch name cannot crowd the
// window controls off the caption; the branch is ellipsized past this logical width.
#define TOPBAR_HISTORY_BRANCH_MAX_W  130

// Reconciled window-control glyph sizes (titlebar-window-controls-raster +
// topbar-chrome-raster-to-glyph overlap): minimize/close read at the standard
// 18px chrome size; the box glyphs (crop_square maximize, filter_none restore)
// have a heavier bounding-box weight, so they are dialed to 15px for balance.
#define TOPBAR_WINDOW_ICON_SIZE   18
#define TOPBAR_WINDOW_BOX_SIZE    15

using namespace Slic3r;

enum CUSTOM_ID
{
    ID_TOP_MENU_TOOL = 3100,
    ID_LOGO,
    ID_TOP_FILE_MENU,
    ID_TOP_EDIT_MENU,
    ID_TOP_VIEW_MENU,
    ID_TOP_OBJECTS_MENU,
    ID_TOP_HELP_MENU,
    ID_TITLE,
    ID_MODEL_STORE,
    ID_PUBLISH,
    ID_CALIB,
    ID_HISTORY,
    ID_APPEARANCE,
    ID_TOOL_BAR = 3200,
    ID_AMS_NOTEBOOK,
};

// Wave 3 (topbar-chrome-raster-to-glyph / titlebar-window-controls-raster):
// title-bar tool icons are now Material Symbols glyphs rendered to a DPI-correct
// bitmap in a semantic colour, replacing the legacy create_scaled_bitmap PNGs
// (topbar_save/undo/redo/publish/min/max/win/close + *_inactive). State is
// expressed through colour, never the font FILL axis: enabled tools take an
// OnSurface glyph, disabled tools an Outline glyph in place of the retired
// *_inactive rasters. When the Material Symbols face is unavailable the call
// degrades to the original raster (the AxisCtrlButton capability-gate idiom) so a
// missing TTF falls back to the old look instead of tofu.
static wxBitmap topbar_glyph_bitmap(wxWindow *ref, uint32_t glyph, int glyph_px,
                                    MD3::Role role, const std::string &fallback_png,
                                    int fallback_px)
{
    if (MaterialIcon::available())
        return MaterialIcon::bitmap(ref, glyph, glyph_px, StateColor::semantic(role));
    return create_scaled_bitmap(fallback_png, ref, fallback_px);
}

// Content-scale factor for the DPI-correct composite bitmaps below. Mirrors the
// idiom in MaterialIcon::bitmap so the chips lay out at logical px on HiDPI.
static double topbar_scale(wxWindow *ref)
{
    return (ref && ref->GetDPIScaleFactor() > 0.0) ? ref->GetDPIScaleFactor() : 1.0;
}

// Allocate a transparent, antialiased, DPI-correct canvas of `logical` size and
// invoke `paint(gc)` in LOGICAL coordinates (the graphics context is pre-scaled).
template <typename Paint>
static wxBitmap topbar_make_canvas(wxWindow *ref, const wxSize &logical, Paint paint)
{
    const double scale = topbar_scale(ref);
    const int    dev_w = std::max(1, static_cast<int>(std::ceil(logical.x * scale)));
    const int    dev_h = std::max(1, static_cast<int>(std::ceil(logical.y * scale)));

    wxBitmap bmp(dev_w, dev_h);
#if defined(__WXMSW__) || defined(__WXOSX__)
    bmp.UseAlpha();
#endif
    {
        wxMemoryDC dc(bmp);
        dc.SetBackground(*wxTRANSPARENT_BRUSH);
        dc.Clear();
        wxGraphicsContext *gc = wxGraphicsContext::Create(dc);
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->Scale(scale, scale);
            paint(gc);
            delete gc; // flush before the bitmap is read
        }
        dc.SelectObject(wxNullBitmap);
    }
#if wxCHECK_VERSION(3, 1, 6)
    bmp.SetScaleFactor(scale);
#endif
    return bmp;
}

// Render a text run with plain GDI into a colour+alpha bitmap at device
// resolution. The privately registered faces (Material Symbols, Roboto Mono)
// must never go through wxGraphicsContext: GDI+ cannot resolve private HFONTs
// and intermittently corrupts its heap doing so (the startup heap-corruption
// crash dump bottomed out in exactly that path). Plain GDI resolves private
// faces correctly; alpha is derived from black-on-white coverage.
static wxBitmap topbar_text_alpha(const wxFont &logical_font, double scale,
                                  const wxString &s, const wxColour &colour)
{
    wxFont f = logical_font;
    if (scale != 1.0) {
        const wxSize ps = f.GetPixelSize();
        if (ps.y > 0)
            f.SetPixelSize(wxSize(0, std::max(1, static_cast<int>(std::lround(ps.y * scale)))));
        else
            f.SetPointSize(std::max(1, static_cast<int>(std::lround(f.GetPointSize() * scale))));
    }
    wxSize dev;
    {
        wxBitmap   probe(1, 1);
        wxMemoryDC mdc(probe);
        mdc.SetFont(f);
        dev = mdc.GetTextExtent(s);
        mdc.SelectObject(wxNullBitmap);
    }
    if (dev.x < 1) dev.x = 1;
    if (dev.y < 1) dev.y = 1;
    wxBitmap mono(dev.x, dev.y, 24);
    {
        wxMemoryDC mdc(mono);
        mdc.SetBackground(*wxWHITE_BRUSH);
        mdc.Clear();
        mdc.SetFont(f);
        mdc.SetTextForeground(*wxBLACK);
        mdc.SetBackgroundMode(wxTRANSPARENT);
        mdc.DrawText(s, 0, 0);
        mdc.SelectObject(wxNullBitmap);
    }
    wxImage cov = mono.ConvertToImage();
    wxImage out(dev.x, dev.y);
    out.InitAlpha();
    const unsigned char r = colour.Red(), g = colour.Green(), b = colour.Blue();
    unsigned char *src = cov.GetData();
    unsigned char *dst = out.GetData();
    unsigned char *al  = out.GetAlpha();
    const size_t   n   = static_cast<size_t>(dev.x) * static_cast<size_t>(dev.y);
    for (size_t i = 0; i < n; ++i) {
        const unsigned int lum = (src[3 * i] + src[3 * i + 1] + src[3 * i + 2]) / 3;
        dst[3 * i]     = r;
        dst[3 * i + 1] = g;
        dst[3 * i + 2] = b;
        al[i]          = static_cast<unsigned char>(255u - lum);
    }
    return wxBitmap(out, 32);
}

// §3.1 brand tile: a 26x26 r8 Primary rounded square carrying the on-primary
// 'deployed_code' glyph, replacing the legacy 22px BambuStudio PNG. Falls back
// to that raster when the Material Symbols face is unavailable.
static wxBitmap topbar_brand_tile_bitmap(wxWindow *ref)
{
    if (!MaterialIcon::available())
        return create_scaled_bitmap("BambuStudio", ref, 22);

    const int   side  = 26;
    const int   glyph = 18;
    wxSize      gsz;
    {
        wxBitmap   probe(1, 1);
        wxMemoryDC mdc(probe);
        gsz = MaterialIcon::measure(mdc, MaterialIcon::DeployedCode, glyph);
        mdc.SelectObject(wxNullBitmap);
    }
    return topbar_make_canvas(ref, wxSize(side, side), [&](wxGraphicsContext *gc) {
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary)));
        gc->DrawRoundedRectangle(0, 0, side, side, MD3::Metrics::radius_tiny);
        // Private icon face must not go through GDI+ (heap corruption); the
        // glyph is pre-rendered with plain GDI and composited as a bitmap.
        const wxBitmap gbmp = MaterialIcon::bitmap(ref, MaterialIcon::DeployedCode, glyph,
                                                   StateColor::semantic(MD3::Role::OnPrimary));
        gc->DrawBitmap(gbmp, (side - gsz.x) / 2.0, (side - gsz.y) / 2.0, gsz.x, gsz.y);
    });
}

// §3.5 history chip: rounded SurfaceContainer pill (SurfaceContainerHigh on
// hover) with an account_tree glyph, the branch name + short head in Roboto
// Mono, and a Primary status dot. The whole chip is baked into one bitmap so it
// reserves exactly its content width in the AUI toolbar.
static wxBitmap topbar_history_chip_bitmap(wxWindow *ref, const wxString &branch,
                                           const wxString &head, bool hover)
{
    const int  H       = 30;
    const int  padx    = 12;
    const int  gap     = 7;
    const int  glyph   = 16;
    const int  dot     = 5;
    const bool icons_ok = MaterialIcon::available();
    const bool has_head = !head.empty();
    const wxString head_str = has_head ? (wxString("#") + head) : wxString();

    wxFont mono = Label::Mono_12;
    if (!mono.IsOk())
        mono = *wxNORMAL_FONT;

    // Ellipsized to keep the baked chip from growing without bound; a long branch
    // name would otherwise push the window controls off the right of the caption.
    wxString branch_lbl = branch;
    int  glyph_w = 0, glyph_h = 0, branch_w = 0, branch_h = 0, head_w = 0, head_h = 0;
    {
        wxBitmap   probe(1, 1);
        wxMemoryDC mdc(probe);
        if (icons_ok) {
            const wxSize gs = MaterialIcon::measure(mdc, MaterialIcon::AccountTree, glyph);
            glyph_w = gs.x;
            glyph_h = gs.y;
        }
        mdc.SetFont(mono);
        mdc.GetTextExtent(branch_lbl, &branch_w, &branch_h);
        if (branch_w > TOPBAR_HISTORY_BRANCH_MAX_W) {
            branch_lbl = wxControl::Ellipsize(branch_lbl, mdc, wxELLIPSIZE_END,
                                              TOPBAR_HISTORY_BRANCH_MAX_W);
            mdc.GetTextExtent(branch_lbl, &branch_w, &branch_h);
        }
        if (has_head)
            mdc.GetTextExtent(head_str, &head_w, &head_h);
        mdc.SelectObject(wxNullBitmap);
    }

    int W = padx;
    if (icons_ok)
        W += glyph_w + gap;
    W += branch_w + gap + dot;
    if (has_head)
        W += gap + head_w;
    W += padx;

    const double chip_scale = topbar_scale(ref);
    return topbar_make_canvas(ref, wxSize(W, H), [&](wxGraphicsContext *gc) {
        const wxColour primary = StateColor::semantic(MD3::Role::Primary);
        const wxColour on_sv   = StateColor::semantic(MD3::Role::OnSurfaceVariant);

        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(StateColor::semantic(hover ? MD3::Role::SurfaceContainerHigh
                                                        : MD3::Role::SurfaceContainer)));
        gc->DrawRoundedRectangle(0, 0, W, H, H / 2.0);

        // Private faces (Material Symbols, Roboto Mono) are pre-rendered with
        // plain GDI and composited as bitmaps; GDI+ text with private HFONTs
        // corrupts the heap.
        double x = padx;
        if (icons_ok) {
            const wxBitmap abmp = MaterialIcon::bitmap(ref, MaterialIcon::AccountTree, glyph, primary);
            gc->DrawBitmap(abmp, x, (H - glyph_h) / 2.0, glyph_w, glyph_h);
            x += glyph_w + gap;
        }
        gc->DrawBitmap(topbar_text_alpha(mono, chip_scale, branch_lbl, on_sv),
                       x, (H - branch_h) / 2.0, branch_w, branch_h);
        x += branch_w + gap;

        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(primary));
        gc->DrawEllipse(x, (H - dot) / 2.0, dot, dot);
        x += dot;

        if (has_head) {
            x += gap;
            gc->DrawBitmap(topbar_text_alpha(mono, chip_scale, head_str, on_sv),
                           x, (H - head_h) / 2.0, head_w, head_h);
        }
    });
}

class BBLTopbarArt : public wxAuiDefaultToolBarArt
{
public:
    virtual void DrawLabel(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect) wxOVERRIDE;
    virtual void DrawBackground(wxDC& dc, wxWindow* wnd, const wxRect& rect) wxOVERRIDE;
    virtual void DrawButton(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect) wxOVERRIDE;
    virtual void DrawSeparator(wxDC& dc, wxWindow* wnd, const wxRect& rect) wxOVERRIDE;
};

void BBLTopbarArt::DrawLabel(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect)
{
    if (item.GetId() == ID_TITLE) {
        // §3.6 project chip: a rounded SurfaceContainer pill with a leading
        // 'description'-family glyph and the ellipsized project name. It stays a
        // non-interactive label so the caption drag path (OnMouseLeftDown /
        // OnMouseLeftDClock special-case m_title_item) keeps working.
        const int    H      = std::min(rect.height, wnd->FromDIP(30));
        const int    radius = H / 2;
        const wxRect chip(rect.x, rect.y + (rect.height - H) / 2, std::max(0, rect.width), H);

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainer)));
        dc.DrawRoundedRectangle(chip, radius);

        const wxColour fg   = StateColor::semantic(MD3::Role::OnSurfaceVariant);
        const int      padx = wnd->FromDIP(8);
        const int      gap  = wnd->FromDIP(6);
        int            x    = chip.x + padx;

        // 'description' is absent from the vendored face; 'folder_open' is the
        // nearest verified project/file glyph (reported as a followup).
        if (MaterialIcon::available()) {
            const wxSize gs = MaterialIcon::measure(dc, MaterialIcon::FolderOpen, 16);
            MaterialIcon::draw(dc, MaterialIcon::FolderOpen, 16, fg,
                               wxPoint(x, chip.y + (H - gs.y) / 2));
            x += gs.x + gap;
        }

        dc.SetFont(m_font);
        dc.SetTextForeground(fg);
        int tw = 0, th = 0;
        dc.GetTextExtent(item.GetLabel(), &tw, &th);
        dc.SetClippingRegion(chip);
        dc.DrawText(item.GetLabel(), x, chip.y + (H - th) / 2);
        dc.DestroyClippingRegion();
        return;
    }

    dc.SetFont(m_font);
    dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurface));

    int textWidth = 0, textHeight = 0;
    dc.GetTextExtent(item.GetLabel(), &textWidth, &textHeight);

    wxRect clipRect = rect;
    clipRect.width -= 1;
    dc.SetClippingRegion(clipRect);

    int textX, textY;
    if (textWidth < rect.GetWidth()) {
        textX = rect.x + 1 + (rect.width - textWidth) / 2;
    }
    else {
        textX = rect.x + 1;
    }
    textY = rect.y + (rect.height - textHeight) / 2;
    dc.DrawText(item.GetLabel(), textX, textY);
    dc.DestroyClippingRegion();
}

void BBLTopbarArt::DrawBackground(wxDC& dc, wxWindow* wnd, const wxRect& rect)
{
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    dc.SetPen(wxPen(surface));
    dc.SetBrush(wxBrush(surface));
    wxRect clipRect = rect;
    clipRect.y -= 8;
    clipRect.height += 8;
    dc.SetClippingRegion(clipRect);
    dc.DrawRectangle(rect);
    dc.DestroyClippingRegion();

    const int divider_width = std::max(1, wnd->FromDIP(1));
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), divider_width));
    dc.DrawLine(rect.GetLeft(), rect.GetBottom(), rect.GetRight(), rect.GetBottom());
}

void BBLTopbarArt::DrawSeparator(wxDC& dc, wxWindow* wnd, const wxRect& rect)
{
    // §3.8: an explicit 1px x 22px outline-variant rule, vertically centered in
    // the bar, instead of the default AUI separator art.
    const int thickness = std::max(1, wnd->FromDIP(1));
    const int height    = std::min(rect.height, wnd->FromDIP(22));
    const int x = rect.x + (rect.width - thickness) / 2;
    const int y = rect.y + (rect.height - height) / 2;
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::OutlineVariant)));
    dc.DrawRectangle(x, y, thickness, height);
}

void BBLTopbarArt::DrawButton(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect)
{
    int textWidth = 0, textHeight = 0;

    wxFont font = m_font;
    // §3.2 wordmark rides at Roboto Medium (500), not SemiBold; the menu buttons
    // (§3.3) are also Medium. The Calibration dropdown is now presented as a text
    // menu button, so it shares the same weight. Everything else keeps the
    // inherited weight.
    if (item.GetId() == ID_LOGO ||
        item.GetId() == ID_TOP_FILE_MENU || item.GetId() == ID_TOP_EDIT_MENU ||
        item.GetId() == ID_TOP_VIEW_MENU || item.GetId() == ID_TOP_OBJECTS_MENU ||
        item.GetId() == ID_TOP_HELP_MENU || item.GetId() == ID_CALIB)
        font.SetWeight(wxFONTWEIGHT_MEDIUM);
    dc.SetFont(font);

    if (m_flags & wxAUI_TB_TEXT)
    {
        int tx, ty;

        dc.GetTextExtent(wxT("ABCDHgj"), &tx, &textHeight);
        textWidth = 0;
        dc.GetTextExtent(item.GetLabel(), &textWidth, &ty);
    }

    int bmpX = 0, bmpY = 0;
    int textX = 0, textY = 0;

    // §3.5 the history chip bakes its own hover state into a second bitmap
    // (SurfaceContainerHigh background) rather than the generic state layer.
    const bool hist_hover = item.GetId() == ID_HISTORY
        && ((item.GetState() & wxAUI_BUTTON_STATE_HOVER) || item.IsSticky())
        && item.GetHoverBitmap().IsOk();
    const wxBitmap& bmp = (item.GetState() & wxAUI_BUTTON_STATE_DISABLED)
        ? item.GetDisabledBitmap()
        : (hist_hover ? item.GetHoverBitmap() : item.GetBitmap());

    const wxSize bmpSize = bmp.IsOk() ? bmp.GetScaledSize() : wxSize(0, 0);

    if (m_textOrientation == wxAUI_TBTOOL_TEXT_BOTTOM)
    {
        bmpX = rect.x +
            (rect.width / 2) -
            (bmpSize.x / 2);

        bmpY = rect.y +
            ((rect.height - textHeight) / 2) -
            (bmpSize.y / 2);

        textX = rect.x + (rect.width / 2) - (textWidth / 2) + 1;
        textY = rect.y + rect.height - textHeight - 1;
    }
    else if (m_textOrientation == wxAUI_TBTOOL_TEXT_RIGHT)
    {
        bmpX = rect.x + wnd->FromDIP(3);

        bmpY = rect.y +
            (rect.height / 2) -
            (bmpSize.y / 2);

        textX = bmpX + wnd->FromDIP(3) + bmpSize.x;
        textY = rect.y +
            (rect.height / 2) -
            (textHeight / 2);
    }


    // The brand tile (§3.1) and history chip (§3.5) bake their own background, so
    // they opt out of the shared state layer.
    const int  item_id       = item.GetId();
    const bool bakes_own_bg  = item_id == ID_LOGO || item_id == ID_HISTORY;
    if (!bakes_own_bg && !(item.GetState() & wxAUI_BUTTON_STATE_DISABLED)) {
        wxColour state_layer;
        // §3.9: the window Close control carries a destructive hover -- its state
        // layer fills Role::Error instead of the neutral surface-container tint
        // used by every other title-bar control.
        const bool is_close = item_id == wxID_CLOSE_FRAME;
        if (item.GetState() & wxAUI_BUTTON_STATE_PRESSED)
            state_layer = StateColor::semantic(is_close ? MD3::Role::Error : MD3::Role::SurfaceContainerHighest);
        else if ((item.GetState() & wxAUI_BUTTON_STATE_HOVER) || item.IsSticky())
            state_layer = StateColor::semantic(is_close ? MD3::Role::Error : MD3::Role::SurfaceContainerHigh);
        else if (item.GetState() & wxAUI_BUTTON_STATE_CHECKED)
            state_layer = StateColor::semantic(MD3::Role::SecondaryContainer);

        if (state_layer.IsOk()) {
            wxRect state_rect = rect;
            int    radius;
            if (item_id == ID_APPEARANCE) {
                // §3.7 appearance button: a circular ghost hover disc.
                const int d = std::max(1, std::min(state_rect.width, state_rect.height) - wnd->FromDIP(4));
                state_rect  = wxRect(rect.x + (rect.width - d) / 2, rect.y + (rect.height - d) / 2, d, d);
                radius      = d / 2;
            } else {
                state_rect.Deflate(wnd->FromDIP(2), wnd->FromDIP(4));
                radius = wnd->FromDIP(MD3::Metrics::compact.small_radius);
            }
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(state_layer));
            dc.DrawRoundedRectangle(state_rect, radius);
        }
    }

    if (bmp.IsOk())
        dc.DrawBitmap(bmp, bmpX, bmpY, true);

    // Semantic foregrounds remain readable on both light and dark title surfaces.
    // §3.3: the File/Edit/View/Objects/Help menu labels use OnSurfaceVariant; the
    // wordmark (and any other label) stays the stronger OnSurface.
    MD3::Role label_role;
    if (item.GetState() & wxAUI_BUTTON_STATE_DISABLED)
        label_role = MD3::Role::Outline;
    else if (item.GetId() == ID_TOP_FILE_MENU || item.GetId() == ID_TOP_EDIT_MENU ||
             item.GetId() == ID_TOP_VIEW_MENU || item.GetId() == ID_TOP_OBJECTS_MENU ||
             item.GetId() == ID_TOP_HELP_MENU || item.GetId() == ID_CALIB)
        label_role = MD3::Role::OnSurfaceVariant;
    else
        label_role = MD3::Role::OnSurface;
    dc.SetTextForeground(StateColor::semantic(label_role));

    if ((m_flags & wxAUI_TB_TEXT) && !item.GetLabel().empty())
    {
        dc.DrawText(item.GetLabel(), textX, textY);
    }
}

BBLTopbar::BBLTopbar(wxFrame* parent)
    : wxAuiToolBar(parent, ID_TOOL_BAR, wxDefaultPosition, wxDefaultSize, wxAUI_TB_TEXT | wxAUI_TB_HORZ_TEXT)
{
    Init(parent);
}

BBLTopbar::BBLTopbar(wxWindow* pwin, wxFrame* parent)
    : wxAuiToolBar(pwin, ID_TOOL_BAR, wxDefaultPosition, wxDefaultSize, wxAUI_TB_TEXT | wxAUI_TB_HORZ_TEXT)
{
    Init(parent);
}

void BBLTopbar::Init(wxFrame* parent)
{
    SetArtProvider(new BBLTopbarArt());
    m_frame = parent;
    m_brand_item = nullptr;
    m_file_menu_item = nullptr;
    m_edit_menu_item = nullptr;
    m_view_menu_item = nullptr;
    m_objects_menu_item = nullptr;
    m_help_menu_item = nullptr;
    m_file_menu = nullptr;
    m_edit_menu = nullptr;
    m_view_menu = nullptr;
    m_objects_menu = nullptr;
    m_help_menu = nullptr;
    m_skip_popup_menu_id = wxID_ANY;
    m_skip_popup_calib_menu    = false;

    wxInitAllImageHandlers();

    // titlebar-remove-nonkit-controls: Save/Undo/Redo/Publish leave the caption
    // (Save = File>Save, Undo/Redo = Edit menu; Publish is otherwise unused). The
    // members stay null and their public enable/show API becomes a no-op so every
    // external caller keeps compiling and behaving.
    m_save_item = m_undo_item = m_redo_item = m_publish_item = nullptr;
    if (m_history_branch.IsEmpty())
        m_history_branch = "main";

    this->AddSpacer(FromDIP(MD3::Metrics::compact.gap));

    // §3.1 brand tile: r8 Primary square + on-primary 'deployed_code' glyph,
    // followed by the 'Bambu Studio' wordmark (drawn as this tool's label).
    wxBitmap brand_bitmap = topbar_brand_tile_bitmap(this);
    m_brand_item = this->AddTool(ID_LOGO, _L("Bambu Studio"), brand_bitmap, wxEmptyString, wxITEM_NORMAL);
    m_brand_item->SetHoverBitmap(brand_bitmap);
    m_brand_item->SetActive(false);

    this->AddSpacer(FromDIP(MD3::Metrics::compact.gap));

    // Each application menu is a first-class top-bar control.  The menu objects
    // are still built and owned by MainFrame, so existing handlers, update-UI
    // predicates, accelerators and recent-project integration remain intact.
    m_file_menu_item = this->AddTool(ID_TOP_FILE_MENU, _L("File"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    m_edit_menu_item = this->AddTool(ID_TOP_EDIT_MENU, _L("Edit"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    m_view_menu_item = this->AddTool(ID_TOP_VIEW_MENU, _L("View"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    m_objects_menu_item = this->AddTool(ID_TOP_OBJECTS_MENU, _L("Objects"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    // titlebar-remove-nonkit-controls: the Calibration dropdown is the only entry
    // point to the calibration tests on Windows (the File/Edit menus don't carry
    // them), so instead of orphaning the feature it is re-homed here as a kit text
    // menu button beside the other menus -- its legacy Tune/raster icon is dropped.
    // It still pops m_calib_menu via OnCalibToolItem and honours ShowCalibrationButton.
    m_calib_item = this->AddTool(ID_CALIB, _L("Calibration"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    m_help_menu_item = this->AddTool(ID_TOP_HELP_MENU, _L("Help"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);

    this->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    this->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));

    // §3.6 a single right-aligned drag spacer replaces the old dual centering
    // spacers. Dragging over this region (or the project chip) moves the window.
    this->AddStretchSpacer(1);

    // §3.5 history chip -> the real version-history backend (MainFrame::show_project_history,
    // the same handler behind the File>Version history item). The chip bakes its
    // own idle/hover background bitmaps.
    m_history_item = this->AddTool(ID_HISTORY, "",
                                   topbar_history_chip_bitmap(this, m_history_branch, m_history_head, false),
                                   wxEmptyString, wxITEM_NORMAL);
    m_history_item->SetHoverBitmap(topbar_history_chip_bitmap(this, m_history_branch, m_history_head, true));
    m_history_item->SetShortHelp(_L("Version history"));
    this->AddSpacer(FromDIP(6));

    // §3.6 project chip: kept as the ID_TITLE label so the caption drag path
    // (OnMouseLeftDown / OnMouseLeftDClock) and update_responsive_title ellipsize
    // continue to work; the chip anatomy is painted in BBLTopbarArt::DrawLabel.
    m_title_item = this->AddLabel(ID_TITLE, "", FromDIP(TOPBAR_PROJECT_CHIP_MAX_W));
    m_title_item->SetAlignment(wxALIGN_CENTRE);
    this->AddSpacer(FromDIP(6));

    // §3.7 appearance / palette button (circular ghost). Guarded on the Material
    // Symbols face; when it is unavailable the button is simply omitted (there is
    // no legacy palette raster to fall back to -- reported as a followup).
    if (MaterialIcon::available()) {
        m_appearance_item = this->AddTool(ID_APPEARANCE, "",
            MaterialIcon::bitmap(this, MaterialIcon::Palette, 20, StateColor::semantic(MD3::Role::OnSurfaceVariant)),
            wxEmptyString, wxITEM_NORMAL);
        m_appearance_item->SetShortHelp(_L("Appearance"));
        this->AddSpacer(FromDIP(6));
    }

    // §3.8: a 1px x 22px outline-variant separator sits immediately before the
    // window-control cluster (drawn by BBLTopbarArt::DrawSeparator).
    this->AddSeparator();
    this->AddSpacer(FromDIP(4));

    wxBitmap iconize_bitmap = topbar_glyph_bitmap(this, MaterialIcon::Minimize, TOPBAR_WINDOW_ICON_SIZE,
                                                  MD3::Role::OnSurface, "topbar_min", TOPBAR_ICON_SIZE);
    wxAuiToolBarItem* iconize_btn = this->AddTool(wxID_ICONIZE_FRAME, "", iconize_bitmap);

    this->AddSpacer(FromDIP(4));

    maximize_bitmap = topbar_glyph_bitmap(this, MaterialIcon::CropSquare, TOPBAR_WINDOW_BOX_SIZE,
                                          MD3::Role::OnSurface, "topbar_max", TOPBAR_ICON_SIZE);
    window_bitmap = topbar_glyph_bitmap(this, MaterialIcon::FilterNone, TOPBAR_WINDOW_BOX_SIZE,
                                        MD3::Role::OnSurface, "topbar_win", TOPBAR_ICON_SIZE);
    if (m_frame->IsMaximized()) {
        maximize_btn = this->AddTool(wxID_MAXIMIZE_FRAME, "", window_bitmap);
    }
    else {
        maximize_btn = this->AddTool(wxID_MAXIMIZE_FRAME, "", maximize_bitmap);
    }

    this->AddSpacer(FromDIP(4));

    wxBitmap close_bitmap = topbar_glyph_bitmap(this, MaterialIcon::Close, TOPBAR_WINDOW_ICON_SIZE,
                                                MD3::Role::OnSurface, "topbar_close", TOPBAR_ICON_SIZE);
    wxAuiToolBarItem* close_btn = this->AddTool(wxID_CLOSE_FRAME, "", close_bitmap);

    Realize();
    // m_toolbar_h = this->GetSize().GetHeight();
    m_toolbar_h = FromDIP(MD3::Metrics::top_bar_height);
    SetMinSize({-1, m_toolbar_h});
    SetMaxSize({-1, m_toolbar_h});

    int client_w = parent->GetClientSize().GetWidth();
    this->SetSize(client_w, m_toolbar_h);

    this->Bind(wxEVT_MOTION, &BBLTopbar::OnMouseMotion, this);
    this->Bind(wxEVT_MOUSE_CAPTURE_LOST, &BBLTopbar::OnMouseCaptureLost, this);
    this->Bind(wxEVT_MENU_CLOSE, &BBLTopbar::OnMenuClose, this);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_FILE_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_EDIT_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_VIEW_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_OBJECTS_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_HELP_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnCalibToolItem, this, ID_CALIB);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnHistoryChip, this, ID_HISTORY);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnAppearanceButton, this, ID_APPEARANCE);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnIconize, this, wxID_ICONIZE_FRAME);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnFullScreen, this, wxID_MAXIMIZE_FRAME);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnCloseFrame, this, wxID_CLOSE_FRAME);
    this->Bind(wxEVT_LEFT_DCLICK, &BBLTopbar::OnMouseLeftDClock, this);
    this->Bind(wxEVT_LEFT_DOWN, &BBLTopbar::OnMouseLeftDown, this);
    this->Bind(wxEVT_LEFT_UP, &BBLTopbar::OnMouseLeftUp, this);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnOpenProject, this, wxID_OPEN);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnSaveProject, this, wxID_SAVE);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnRedo, this, wxID_REDO);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnUndo, this, wxID_UNDO);
    //this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnModelStoreClicked, this, ID_MODEL_STORE);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnPublishClicked, this, ID_PUBLISH);
    this->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        update_responsive_title(event.GetSize().GetWidth());
        event.Skip();
    });
    this->Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
        SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
        if (m_brand_item) {
            wxBitmap brand = topbar_brand_tile_bitmap(this);
            m_brand_item->SetBitmap(brand);
            m_brand_item->SetHoverBitmap(brand);
        }
        rebuild_history_chip();
        if (m_appearance_item)
            m_appearance_item->SetBitmap(MaterialIcon::bitmap(this, MaterialIcon::Palette, 20,
                                                              StateColor::semantic(MD3::Role::OnSurfaceVariant)));
        Realize();
        Refresh(false);
        event.Skip();
    });
}

BBLTopbar::~BBLTopbar()
{
    m_brand_item = nullptr;
    m_file_menu_item = nullptr;
    m_edit_menu_item = nullptr;
    m_view_menu_item = nullptr;
    m_objects_menu_item = nullptr;
    m_help_menu_item = nullptr;
    m_file_menu = nullptr;
    m_edit_menu = nullptr;
    m_view_menu = nullptr;
    m_objects_menu = nullptr;
    m_help_menu = nullptr;
}

void BBLTopbar::OnOpenProject(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->load_project();
}

void BBLTopbar::show_publish_button(bool show)
{
    // titlebar-remove-nonkit-controls: the Publish control no longer lives in the
    // caption. Retained as a null-safe no-op so external callers keep compiling.
    if (!m_publish_item)
        return;
    this->EnableTool(m_publish_item->GetId(), show);
    Refresh();
}

void BBLTopbar::OnSaveProject(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->save_project();
    EnableSaveItem(false);
}

void BBLTopbar::OnUndo(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->undo();
}

void BBLTopbar::OnRedo(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->redo();
}

void BBLTopbar::EnableSaveItem(bool enable)
{
    if (m_save_item && GetToolEnabled(m_save_item->GetId()) != enable) {
        this->EnableTool(m_save_item->GetId(), enable);
        Refresh();
    }
}

void BBLTopbar::EnableUndoItem(bool enable)
{
    if (m_undo_item && GetToolEnabled(m_undo_item->GetId()) != enable) {
        this->EnableTool(m_undo_item->GetId(), enable);
        Refresh();
    }
}

void BBLTopbar::EnableRedoItem(bool enable)
{
    if (m_redo_item && GetToolEnabled(m_redo_item->GetId()) != enable) {
        this->EnableTool(m_redo_item->GetId(), enable);
        Refresh();
    }
}

void BBLTopbar::EnableUndoRedoItems()
{
    // Undo/Redo left the caption (reachable via the Edit menu); guard their now
    // null members. Calibration remains as a menu button and still toggles.
    if (m_undo_item)
        this->EnableTool(m_undo_item->GetId(), true);
    if (m_redo_item)
        this->EnableTool(m_redo_item->GetId(), true);
    if (m_calib_item)
        this->EnableTool(m_calib_item->GetId(), true);
    Refresh();
}

void BBLTopbar::DisableUndoRedoItems()
{
    if (m_undo_item)
        this->EnableTool(m_undo_item->GetId(), false);
    if (m_redo_item)
        this->EnableTool(m_redo_item->GetId(), false);
    if (m_calib_item)
        this->EnableTool(m_calib_item->GetId(), false);
    Refresh();
}

void BBLTopbar::SaveNormalRect()
{
    m_normalRect = m_frame->GetRect();
}

void BBLTopbar::ShowCalibrationButton(bool show)
{
    m_calib_item->GetSizerItem()->Show(show);
    m_sizer->Layout();
    if (!show)
        m_calib_item->GetSizerItem()->SetDimension({-1000, 0}, {0, 0});
    Refresh();
}

void BBLTopbar::OnModelStoreClicked(wxAuiToolBarEvent& event)
{
    //GUI::wxGetApp().load_url(wxString(wxGetApp().app_config->get_web_host_url() + MODEL_STORE_URL));
}

void BBLTopbar::OnPublishClicked(wxAuiToolBarEvent& event)
{
    if (!wxGetApp().getAgent()) {
        BOOST_LOG_TRIVIAL(info) << "publish: no agent";
        return;
    }

    // record
    json j;
    NetworkAgent* agent = GUI::wxGetApp().getAgent();
    if (agent)
        agent->track_event("enter_model_mall", j.dump());

    //no more check
    //if (GUI::wxGetApp().plater()->model().objects.empty()) return;

#ifdef ENABLE_PUBLISHING
    wxGetApp().plater()->show_publish_dialog();
#endif
    wxGetApp().open_publish_page_dialog();
}

void BBLTopbar::OnHistoryChip(wxAuiToolBarEvent& event)
{
    // §3.5: the chip opens the same project version-history dialog as the
    // File>Version history menu item.
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    if (main_frame)
        main_frame->show_project_history();
}

void BBLTopbar::OnAppearanceButton(wxAuiToolBarEvent& event)
{
    // §3.7: entry point to appearance settings. A dedicated MD3 Appearance
    // popover is a followup; for now this opens the Preferences dialog whose
    // Appearance section carries theme/density/accent.
    wxGetApp().open_preferences();
}

void BBLTopbar::rebuild_history_chip()
{
    if (!m_history_item)
        return;
    m_history_item->SetBitmap(topbar_history_chip_bitmap(this, m_history_branch, m_history_head, false));
    m_history_item->SetHoverBitmap(topbar_history_chip_bitmap(this, m_history_branch, m_history_head, true));
}

void BBLTopbar::SetHistoryInfo(const wxString& branch, const wxString& head)
{
    m_history_branch = branch.IsEmpty() ? wxString("main") : branch;
    m_history_head   = head;
    rebuild_history_chip();
    Realize();
    Refresh(false);
}

void BBLTopbar::SetTopMenus(wxMenu* file_menu, wxMenu* edit_menu, wxMenu* view_menu,
                             wxMenu* objects_menu, wxMenu* help_menu)
{
    m_file_menu = file_menu;
    m_edit_menu = edit_menu;
    m_view_menu = view_menu;
    m_objects_menu = objects_menu;
    m_help_menu = help_menu;
}

wxMenu* BBLTopbar::top_menu_for_tool(int tool_id) const
{
    switch (tool_id) {
    case ID_TOP_FILE_MENU:    return m_file_menu;
    case ID_TOP_EDIT_MENU:    return m_edit_menu;
    case ID_TOP_VIEW_MENU:    return m_view_menu;
    case ID_TOP_OBJECTS_MENU: return m_objects_menu;
    case ID_TOP_HELP_MENU:    return m_help_menu;
    default:                  return nullptr;
    }
}

wxMenu* BBLTopbar::GetCalibMenu()
{
    return &m_calib_menu;
}

void BBLTopbar::SetTitle(wxString title)
{
    m_full_title = title;
    update_responsive_title();
}

int BBLTopbar::measure_fixed_content_width() const
{
    int fixed_width = 0;
    for (size_t index = 0; index < GetToolCount(); ++index) {
        const wxAuiToolBarItem* item = FindToolByIndex(static_cast<int>(index));
        if (!item || item == m_title_item || item->GetProportion() > 0)
            continue;

        const wxSizerItem* sizer_item = item->GetSizerItem();
        if (!sizer_item || !sizer_item->IsShown())
            continue;

        fixed_width += std::max(0, sizer_item->GetMinSize().GetWidth());
    }
    return fixed_width;
}

void BBLTopbar::update_responsive_title(int width)
{
    if (!m_title_item)
        return;

    if (width < 0)
        width = GetClientSize().GetWidth();

    wxGCDC dc(this);
    dc.SetFont(GetFont());

    // §3.6 project chip geometry (logical px): leading glyph + gap + name, with
    // symmetric horizontal padding. Mirror the values painted in DrawLabel.
    const int padx = FromDIP(8);
    const int gap  = FromDIP(6);
    int glyph_w = 0;
    if (MaterialIcon::available())
        glyph_w = MaterialIcon::measure(dc, MaterialIcon::FolderOpen, 16).x + gap;

    // Cap the name to 150 logical px, but never let the chip crowd out the rest
    // of the fixed chrome on a very narrow window.
    int max_text = FromDIP(TOPBAR_PROJECT_CHIP_MAX_W);
    if (width > 0) {
        const int budget = width - measure_fixed_content_width() - padx * 2 - glyph_w;
        if (budget > 0)
            max_text = std::min(max_text, std::max(FromDIP(40), budget));
    }

    const wxString title = wxControl::Ellipsize(m_full_title, dc, wxELLIPSIZE_END, max_text);

    int text_w = 0, text_h = 0;
    dc.GetTextExtent(title.IsEmpty() ? wxString(" ") : title, &text_w, &text_h);

    const int chip_w = padx * 2 + glyph_w + text_w;
    if (m_title_item->GetMinSize().GetWidth() != chip_w) {
        m_title_item->SetMinSize({chip_w, -1});
        Realize();
    }

    m_title_item->SetLabel(title);
    m_title_item->SetAlignment(wxALIGN_CENTRE);
    Refresh(false);
}

void BBLTopbar::SetMaximizedSize()
{
    maximize_btn->SetBitmap(maximize_bitmap);
}

void BBLTopbar::SetWindowSize()
{
    maximize_btn->SetBitmap(window_bitmap);
}

void BBLTopbar::UpdateToolbarWidth(int width)
{
    this->SetSize(width, m_toolbar_h);
    update_responsive_title(width);
}

void BBLTopbar::Rescale() {
    wxAuiToolBarItem* item;

    // §3.1 brand tile (falls back to the raster logo when the face is missing).
    item = this->FindTool(ID_LOGO);
    if (item) {
        wxBitmap brand = topbar_brand_tile_bitmap(this);
        item->SetBitmap(brand);
        item->SetHoverBitmap(brand);
    }

    // §3.5 history chip + §3.7 appearance button re-rasterize at the new DPI.
    rebuild_history_chip();
    if (m_appearance_item)
        m_appearance_item->SetBitmap(MaterialIcon::bitmap(this, MaterialIcon::Palette, 20,
                                                          StateColor::semantic(MD3::Role::OnSurfaceVariant)));

    // Save/Undo/Redo/Publish and the Calibration icon left the caption; the
    // Calibration menu button is text-only, so there is no bitmap to rescale.

    item = this->FindTool(wxID_ICONIZE_FRAME);
    item->SetBitmap(topbar_glyph_bitmap(this, MaterialIcon::Minimize, TOPBAR_WINDOW_ICON_SIZE,
                                        MD3::Role::OnSurface, "topbar_min", TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_MAXIMIZE_FRAME);
    maximize_bitmap = topbar_glyph_bitmap(this, MaterialIcon::CropSquare, TOPBAR_WINDOW_BOX_SIZE,
                                          MD3::Role::OnSurface, "topbar_max", TOPBAR_ICON_SIZE);
    window_bitmap   = topbar_glyph_bitmap(this, MaterialIcon::FilterNone, TOPBAR_WINDOW_BOX_SIZE,
                                          MD3::Role::OnSurface, "topbar_win", TOPBAR_ICON_SIZE);
    if (m_frame->IsMaximized()) {
        item->SetBitmap(window_bitmap);
    }
    else {
        item->SetBitmap(maximize_bitmap);
    }

    item = this->FindTool(wxID_CLOSE_FRAME);
    item->SetBitmap(topbar_glyph_bitmap(this, MaterialIcon::Close, TOPBAR_WINDOW_ICON_SIZE,
                                        MD3::Role::OnSurface, "topbar_close", TOPBAR_ICON_SIZE));

    m_toolbar_h = FromDIP(MD3::Metrics::top_bar_height);
    SetMinSize({-1, m_toolbar_h});
    SetMaxSize({-1, m_toolbar_h});
    SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    Realize();
    SetSize(GetSize().GetWidth(), m_toolbar_h);
    update_responsive_title(GetSize().GetWidth());
    if (GetParent())
        GetParent()->Layout();
    Refresh(false);
}

void BBLTopbar::OnIconize(wxAuiToolBarEvent& event)
{
    m_frame->Iconize();
}

void BBLTopbar::OnFullScreen(wxAuiToolBarEvent& event)
{
    if (m_frame->IsMaximized()) {
        m_frame->Restore();
    }
    else {
        wxDisplay display(this);
        auto      size = display.GetClientArea().GetSize();
#ifdef __WXMSW__
        HWND hWnd = m_frame->GetHandle();
        RECT      borderThickness;
        SetRectEmpty(&borderThickness);
        AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE), FALSE, 0);
        m_frame->SetMaxSize(size + wxSize{-borderThickness.left + borderThickness.right, -borderThickness.top + borderThickness.bottom});
#endif //  __WXMSW__
        m_normalRect = m_frame->GetRect();
        m_frame->Maximize();
    }
}

void BBLTopbar::OnCloseFrame(wxAuiToolBarEvent& event)
{
    m_frame->Close();
}

void BBLTopbar::OnMouseLeftDClock(wxMouseEvent& mouse)
{
    wxPoint mouse_pos = ::wxGetMousePosition();
    // check whether mouse is not on any tool item
    if (this->FindToolByCurrentPosition() != NULL &&
        this->FindToolByCurrentPosition() != m_title_item) {
        mouse.Skip();
        return;
    }
#ifdef __W1XMSW__
    ::PostMessage((HWND) m_frame->GetHandle(), WM_NCLBUTTONDBLCLK, HTCAPTION, MAKELPARAM(mouse_pos.x, mouse_pos.y));
    return;
#endif //  __WXMSW__

    wxAuiToolBarEvent evt;
    OnFullScreen(evt);
}

void BBLTopbar::OnTopMenuToolItem(wxAuiToolBarEvent& evt)
{
    wxAuiToolBar* tb = static_cast<wxAuiToolBar*>(evt.GetEventObject());
    wxMenu* menu = top_menu_for_tool(evt.GetId());
    if (!menu)
        return;

    tb->SetToolSticky(evt.GetId(), true);

    if (m_skip_popup_menu_id != evt.GetId()) {
        const wxRect tool_rect = GetToolRect(evt.GetId());
        const wxPoint screen_anchor = ClientToScreen(
            wxPoint(tool_rect.GetLeft(), GetClientSize().GetHeight() - FromDIP(1)));
        GetParent()->PopupMenu(menu, GetParent()->ScreenToClient(screen_anchor));
    } else {
        m_skip_popup_menu_id = wxID_ANY;
    }

    // Make sure the button is "un-stuck" once the modal popup loop returns.
    tb->SetToolSticky(evt.GetId(), false);
}

void BBLTopbar::OnCalibToolItem(wxAuiToolBarEvent &evt)
{
    wxAuiToolBar *tb = static_cast<wxAuiToolBar *>(evt.GetEventObject());

    tb->SetToolSticky(evt.GetId(), true);

    if (!m_skip_popup_calib_menu) {
        auto rec = this->GetToolRect(ID_CALIB);
        GetParent()->PopupMenu(&m_calib_menu, wxPoint(rec.GetLeft(), this->GetSize().GetHeight() - 2));
    } else {
        m_skip_popup_calib_menu = false;
    }

    // make sure the button is "un-stuck"
    tb->SetToolSticky(evt.GetId(), false);
}

void BBLTopbar::OnMouseLeftDown(wxMouseEvent& event)
{
    wxPoint mouse_pos = ::wxGetMousePosition();
    wxPoint frame_pos = m_frame->GetScreenPosition();
    m_delta = mouse_pos - frame_pos;

    if (FindToolByCurrentPosition() == NULL
        || this->FindToolByCurrentPosition() == m_title_item)
    {
        CaptureMouse();
#ifdef __WXMSW__
        ReleaseMouse();
        ::PostMessage((HWND) m_frame->GetHandle(), WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(mouse_pos.x, mouse_pos.y));
        return;
#endif //  __WXMSW__
    }

    event.Skip();
}

void BBLTopbar::OnMouseLeftUp(wxMouseEvent& event)
{
    wxPoint mouse_pos = ::wxGetMousePosition();
    if (HasCapture())
    {
        ReleaseMouse();
    }

    event.Skip();
}

void BBLTopbar::OnMouseMotion(wxMouseEvent& event)
{
    wxPoint mouse_pos = ::wxGetMousePosition();

    if (!HasCapture()) {
        //m_frame->OnMouseMotion(event);

        event.Skip();
        return;
    }

    if (event.Dragging() && event.LeftIsDown())
    {
        // leave max state and adjust position
        if (m_frame->IsMaximized()) {
            wxRect rect = m_frame->GetRect();
            // Filter unexcept mouse move
            if (m_delta + rect.GetLeftTop() != mouse_pos) {
                m_delta = mouse_pos - rect.GetLeftTop();
                m_delta.x = m_delta.x * m_normalRect.width / rect.width;
                m_delta.y = m_delta.y * m_normalRect.height / rect.height;
                m_frame->Restore();
            }
        }
        m_frame->Move(mouse_pos - m_delta);
    }
    event.Skip();
}

void BBLTopbar::OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
}

void BBLTopbar::OnMenuClose(wxMenuEvent& event)
{
    wxAuiToolBarItem* item = this->FindToolByCurrentPosition();
    if (item && top_menu_for_tool(item->GetId()))
        m_skip_popup_menu_id = item->GetId();
}

wxAuiToolBarItem* BBLTopbar::FindToolByCurrentPosition()
{
    wxPoint mouse_pos = ::wxGetMousePosition();
    wxPoint client_pos = this->ScreenToClient(mouse_pos);
    return this->FindToolByPosition(client_pos.x, client_pos.y);
}
