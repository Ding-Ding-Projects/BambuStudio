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
//
// ADDITIVE VARIANTS (opt-in via the Options ctor; defaults reproduce the
// classic borderless, shaped, theme-adaptive shell byte-for-byte, so every
// subclass built on the pinned 4-arg ctor is unaffected):
//
//   * Options::resizable  — keep native window chrome (a wxRESIZE_BORDER title
//     bar + resize grip) and lay the MD3 header/footer panels inside the client
//     area, instead of the borderless wxFRAME_SHAPED rounded silhouette. For
//     dialogs that embed a wxGLCanvas or must be user-resizable. The MD3
//     circular close is hidden in this mode (the native title bar owns close),
//     and no window shape / borderless-drag is applied.
//   * Options::forced_dark — pin the shell chrome (background, header
//     title/subtitle/close glyph, icon tile, footer divider) to the dark colour
//     scheme regardless of the running app theme, for always-dark brand
//     surfaces (e.g. Helio). Also toggleable post-construction via
//     SetForcedDark().
class MD3Dialog : public DPIDialog
{
public:
    // Shell-variant options. The defaults reproduce the classic borderless,
    // shaped shell; the pinned 4-arg ctor delegates here with Options{}.
    struct Options
    {
        bool resizable   = false;
        bool forced_dark = false;
    };

    MD3Dialog(wxWindow *          parent,
              const wxString &    title,
              const wxString &    subtitle,
              MaterialIcon::Glyph header_glyph);
    MD3Dialog(wxWindow *          parent,
              const wxString &    title,
              const wxString &    subtitle,
              MaterialIcon::Glyph header_glyph,
              const Options &     opts);
    ~MD3Dialog() override;

    // ADDITIVE two-phase construction path.
    //
    // A subclass whose own construction is itself two-phase — default-construct,
    // then a later Create() that historically owned the wxDialog window creation
    // (ProgressDialog) — cannot delegate to the single-shot ctors above (those
    // build the window and shell in one step, before the subclass ctor body runs
    // its Create()). Such a subclass instead delegates to the protected default
    // ctor (which creates a bare classic borderless shaped window with no chrome)
    // and later calls CreateShell() from its own Create() to lay out the MD3
    // header/body/footer. Every existing subclass uses the single-shot ctors and
    // is unaffected. See the protected members below.

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

    // Pin (or release) dark-scheme chrome after construction; repaints the
    // shell. The resizable flag is fixed at construction (it selects the window
    // style) and cannot be toggled here.
    void SetForcedDark(bool dark);
    bool IsForcedDark() const { return m_forced_dark; }
    bool IsResizable() const { return m_resizable; }

    void on_dpi_changed(const wxRect &suggested_rect) override;

protected:
    // Two-phase: default-construct a bare classic borderless shaped window with
    // NO chrome (background, header, body, footer are all deferred). Only a
    // subclass whose Create() finishes construction via CreateShell() uses this.
    MD3Dialog();

    // Build the MD3 shell chrome onto a default-constructed shell: record the
    // owner (for centering), set the title + surface background, and lay out the
    // header (tile + title/subtitle + close) / body / footer — mirroring the
    // single-shot ctor. Call exactly once; a no-op (returns true) if already
    // built. In the two-phase path the window itself was created classic
    // (borderless shaped) by the default ctor, so opts.resizable is not applied
    // (the window style is fixed at creation); opts.forced_dark is honoured.
    bool CreateShell(wxWindow *          parent,
                     const wxString &    title,
                     const wxString &    subtitle,
                     MaterialIcon::Glyph header_glyph,
                     const Options &     opts = Options{});

    // Show/hide the header circular close affordance (e.g. an uncancelable
    // progress shell hides it). No-op before the shell is built.
    void ShowHeaderClose(bool show);
    // Show/hide the footer divider + button row as a unit (a shell with no footer
    // actions hides it so no empty divider is left). No-op before build.
    void ShowFooter(bool show);

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
    // with the native title bar). No-op in the resizable variant (the native
    // title bar already moves the window).
    void bind_drag(wxWindow *w);
    // Resolve a chrome role honouring m_forced_dark (dark scheme pinned) or the
    // live app theme otherwise.
    wxColour chrome_color(MD3::Role role) const;
    // (Re)apply the header close IconButton colours for a forced-dark shell;
    // a no-op while the shell follows the live app theme.
    void style_close_button();

    MD3HeaderTile *m_tile        = nullptr;
    wxStaticText * m_title_txt   = nullptr;
    wxStaticText * m_subtitle_txt = nullptr;
    Button *       m_close_btn   = nullptr;
    wxWindow *     m_footer_line = nullptr;
    // Footer button-row wrapper (divider is m_footer_line); stored so ShowFooter()
    // can hide the whole footer as a unit.
    wxSizer *      m_footer_wrap = nullptr;

    // True once build_shell() has run (guards the two-phase CreateShell()).
    bool m_shell_built = false;

    int              m_corner_radius = MD3::Metrics::radius_dialog; // 28
    MD3::ColorScheme m_scheme        = MD3::ColorScheme::Brand;

    // Additive shell variants (see Options). Both default false => classic
    // borderless, shaped, theme-adaptive shell.
    bool m_resizable   = false;
    bool m_forced_dark = false;

    // Borderless-drag state.
    bool    m_dragging = false;
    wxPoint m_drag_offset;
};

}} // namespace Slic3r::GUI

#endif // !slic3r_GUI_MD3Dialog_hpp_
