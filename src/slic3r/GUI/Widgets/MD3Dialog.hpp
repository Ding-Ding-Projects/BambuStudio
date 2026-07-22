#ifndef slic3r_GUI_MD3Dialog_hpp_
#define slic3r_GUI_MD3Dialog_hpp_

#include <wx/bitmap.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "../GUI_Utils.hpp" // DPIDialog
#include "MaterialIcon.hpp" // MaterialIcon::Glyph
#include "MD3Tokens.hpp"

class Button;

namespace Slic3r { namespace GUI {

// Internal owner-drawn 44x44 header icon tile (defined in MD3Dialog.cpp).
class MD3HeaderTile;

// MD3Dialog — the shared Material 3 "Dialog" shell primitive (ui-md3 kit,
// components/containment/Dialog). Every migrated modal in the kit is built on
// this shell so the dialog chrome lives in exactly one place:
//
//   * borderless top-level window (no wxCAPTION); a wxFRAME_SHAPED rounded 28px
//     silhouette (MD3::Metrics::radius_dialog), reshaped on every resize;
//   * header = 44x44 PrimaryContainer icon tile (r14) + 24px Material Symbol
//     glyph + title (18/600, OnSurface) + optional subtitle (12.5,
//     OnSurfaceVariant) + a 36px circular close IconButton;
//   * body   = GetContentSizer(): a vertical sizer padded 24px on each side,
//     children parented to the dialog (`this`);
//   * footer = GetFooterSizer(): a right-aligned (flex-end) row padded 14/24
//     with a 1px OutlineVariant top border; AddFooterButton() appends kit
//     Buttons to it in call order.
//
// PINNED API (the parallel leaf-dialog groups target these exact signatures —
// do not change them):
//
//     MD3Dialog(wxWindow* parent, const wxString& title,
//               const wxString& subtitle, MaterialIcon::Glyph header_glyph);
//     wxBoxSizer* GetContentSizer();
//     wxBoxSizer* GetFooterSizer();
//     Button*     AddFooterButton(Button* btn);
//
// The elev-5 drop shadow of the kit spec is a native/DWM concern that a
// wxFRAME_SHAPED window cannot render (the shape region clips anything outside
// the rounded rect); the shell provides the borderless 28px silhouette and the
// shadow can be layered later without an API change. See the .cpp note.
class MD3Dialog : public DPIDialog
{
public:
    MD3Dialog(wxWindow *          parent,
              const wxString &    title,
              const wxString &    subtitle,
              MaterialIcon::Glyph header_glyph);
    ~MD3Dialog() override;

    // Body sizer. Parent body children to the dialog (`this`) and add them here;
    // padded 24px on each side. Overflow is handled by the caller / auto-fit.
    wxBoxSizer *GetContentSizer() const { return m_content_sizer; }

    // Footer sizer (right-aligned row, 1px OutlineVariant top border, pad 14/24).
    wxBoxSizer *GetFooterSizer() const { return m_footer_sizer; }

    // Append a kit Button to the footer; buttons cluster at the right edge in
    // call order (Cancel/secondary first, primary last). Returns btn.
    Button *AddFooterButton(Button *btn);

    // Header mutators (theme-adaptive; repaint on change).
    void SetHeaderGlyph(MaterialIcon::Glyph glyph);
    void SetHeaderTitle(const wxString &title);
    void SetHeaderSubtitle(const wxString &subtitle);
    // Recolour the icon tile (default PrimaryContainer / OnPrimaryContainer).
    void SetHeaderAccent(MD3::Role container_role, MD3::Role on_role);
    // Contextual accent scheme (Brand/Preview/Device) for the tile + close glyph.
    void SetColorScheme(MD3::ColorScheme scheme);

    void on_dpi_changed(const wxRect &suggested_rect) override;

protected:
    // Circular-close action; default EndModal(wxID_CANCEL) to mirror the native
    // window [x]. Override to customise the close semantics.
    virtual void OnHeaderClose();

    // Recompute the rounded-rect window shape for the current size. Safe to call
    // repeatedly; a no-op for non-positive sizes.
    void UpdateShape();

    wxBoxSizer *m_content_sizer = nullptr;
    wxBoxSizer *m_footer_sizer  = nullptr;

private:
    void build_shell(const wxString &title, const wxString &subtitle, MaterialIcon::Glyph glyph);
    // Make w a drag handle for the borderless frame (restores movability lost
    // with the native title bar).
    void bind_drag(wxWindow *w);

    MD3HeaderTile *m_tile        = nullptr;
    wxStaticText * m_title_txt   = nullptr;
    wxStaticText * m_subtitle_txt = nullptr;
    Button *       m_close_btn   = nullptr;
    wxWindow *     m_footer_line = nullptr;

    int              m_corner_radius = MD3::Metrics::radius_dialog; // 28
    MD3::ColorScheme m_scheme        = MD3::ColorScheme::Brand;

    // Borderless-drag state.
    bool    m_dragging = false;
    wxPoint m_drag_offset;
};

}} // namespace Slic3r::GUI

#endif // !slic3r_GUI_MD3Dialog_hpp_
