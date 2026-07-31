#include "AboutDialog.hpp"
#include "I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "format.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StaticBox.hpp"

#include <wx/clipbrd.h>
#include <wx/image.h>

namespace Slic3r {
namespace GUI {

// wxHtml takes its chrome colours as literal "#RRGGBB" strings, so a themed page
// has to bake the resolved role in when the page is built. Both dialogs here are
// modal and rebuilt on every open, so a theme switch between opens is picked up.
static wxString md3_html_colour(MD3::Role role)
{
    const wxColour c = StateColor::semantic(role);
    return wxString::Format(wxT("#%02X%02X%02X"), c.Red(), c.Green(), c.Blue());
}

AboutDialogLogo::AboutDialogLogo(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
{
    this->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    this->logo = ScalableBitmap(this, Slic3r::var("BambuStudio_192px.png"), wxBITMAP_TYPE_PNG);
    this->SetMinSize(this->logo.GetBmpSize());

    this->Bind(wxEVT_PAINT, &AboutDialogLogo::onRepaint, this);
}

void AboutDialogLogo::onRepaint(wxEvent &event)
{
    wxPaintDC dc(this);
    dc.SetBackgroundMode(wxTRANSPARENT);

    wxSize size = this->GetSize();
    int logo_w = this->logo.GetBmpWidth();
    int logo_h = this->logo.GetBmpHeight();
    dc.DrawBitmap(this->logo.bmp(), (size.GetWidth() - logo_w)/2, (size.GetHeight() - logo_h)/2, true);

    event.Skip();
}


// -----------------------------------------
// CopyrightsDialog
// -----------------------------------------
CopyrightsDialog::CopyrightsDialog()
    : DPIDialog(static_cast<wxWindow*>(wxGetApp().mainframe), wxID_ANY, from_u8((boost::format("%1% - %2%")
        % (wxGetApp().is_editor() ? SLIC3R_APP_FULL_NAME : GCODEVIEWER_APP_NAME)
        % _utf8(L("Portions copyright"))).str()),
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    this->SetFont(wxGetApp().normal_font());
	this->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));

    std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    wxStaticLine *staticline1 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );

	auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add( staticline1, 0, wxEXPAND | wxALL, 5 );

    fill_entries();

    m_html = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition,
                              wxSize(40 * em_unit(), 20 * em_unit()), wxHW_SCROLLBAR_AUTO);
    m_html->SetMinSize(wxSize(FromDIP(870),FromDIP(520)));
    m_html->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    wxFont font = get_default_font(this);
    const int fs = font.GetPointSize();
    const int fs2 = static_cast<int>(1.2f*fs);
    int size[] = { fs, fs, fs, fs, fs2, fs2, fs2 };

    m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), size);
    m_html->SetBorders(2);
    m_html->SetPage(get_html_text());

    sizer->Add(m_html, 1, wxEXPAND | wxALL, 15);
    m_html->Bind(wxEVT_HTML_LINK_CLICKED, &CopyrightsDialog::onLinkClicked, this);

    SetSizer(sizer);
    sizer->SetSizeHints(this);
    MD3DialogCaption::Adopt(this);
    CenterOnParent();
    wxGetApp().UpdateDlgDarkUI(this);
}

void CopyrightsDialog::fill_entries()
{
    m_entries = {
        { "@radix-ui/primitive",                            "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-compose-refs",                   "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-context",                        "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-dialog",                         "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-dismissable-layer",              "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-focus-guards",                   "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-focus-scope",                    "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-id",                             "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-portal",                         "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-presence",                       "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-primitive",                      "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-slot",                           "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-use-callback-ref",               "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-use-controllable-state",         "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-use-escape-keydown",             "",      "https://radix-ui.com/primitives" },
        { "@radix-ui/react-use-layout-effect",              "",      "https://radix-ui.com/primitives" },
        { "@tanstack/history",                              "",      "https://tanstack.com/router" },
        { "@tanstack/react-router",                         "",      "https://tanstack.com/router" },
        { "@tanstack/react-store",                          "",      "https://tanstack.com/store" },
        { "@tanstack/router-core",                          "",      "https://tanstack.com/router" },
        { "@tanstack/store",                                "",      "https://tanstack.com/store" },
        { "Admesh",                                         "",      "https://admesh.readthedocs.io/" },
        { "Anti-Grain Geometry",                            "",      "http://antigrain.com" },
        { "ArcWelderLib",                                   "",      "https://plugins.octoprint.org/plugins/arc_welder" },
        { "aria-hidden",                                    "",      "https://github.com/theKashey/aria-hidden#readme" },
        { "Assimp",                                         "",      "https://www.assimp.org" },
        { "Boost",                                          "",      "http://www.boost.org" },
        { "Cereal",                                         "",      "http://uscilab.github.io/cereal" },
        { "CGAL",                                           "",      "https://www.cgal.org" },
        { "Clipper",                                        "",      "http://www.angusj.co" },
        { "Eigen3",                                         "",      "http://eigen.tuxfamily.org" },
        { "Expat",                                          "",      "http://www.libexpat.org" },
        { "fast_float",                                     "",      "https://github.com/fastfloat/fast_float" },
        { "get-nonce",                                      "",      "https://github.com/theKashey/get-nonce" },
        { "GLEW (The OpenGL Extension Wrangler Library)",   "",      "http://glew.sourceforge.net" },
        { "GLFW",                                           "",      "https://www.glfw.org" },
        { "GNU gettext",                                    "",      "https://www.gnu.org/software/gettext" },
        { "i18next",                                        "",      "https://www.i18next.com" },
        { "ImGUI",                                          "",      "https://github.com/ocornut/imgui" },
        { "immer",                                          "",      "https://github.com/immerjs/immer#readme" },
        { "jsesc",                                          "",      "https://mths.be/jsesc" },
        { "lib_fts",                                        "",      "https://www.forrestthewoods.com" },
        { "libcurl",                                        "",      "https://curl.se/libcurl" },
        { "Libigl",                                         "",      "https://libigl.github.io" },
        { "libnest2d",                                      "",      "https://github.com/tamasmeszaros/libnest2d" },
        { "Mesa 3D",                                        "",      "https://mesa3d.org" },
        { "Miniz",                                          "",      "https://github.com/richgel999/miniz" },
        { "Nanosvg",                                        "",      "https://github.com/memononen/nanosvg" },
        { "nlohmann/json",                                  "",      "https://json.nlohmann.me" },
        { "Open Cascade",                                   "",      "https://www.opencascade.com" },
        { "OpenGL",                                         "",      "https://www.opengl.org" },
        { "PoEdit",                                         "",      "https://poedit.net" },
        { "PrusaSlicer",                                    "",      "https://www.prusa3d.com" },
        { "Qhull",                                          "",      "http://qhull.org" },
        { "react",                                          "",      "https://react.dev/" },
        { "react-dom",                                      "",      "https://react.dev/" },
        { "react-i18next",                                  "",      "https://github.com/i18next/react-i18next" },
        { "react-remove-scroll",                            "",      "https://www.npmjs.com/package/react-remove-scroll" },
        { "react-remove-scroll-bar",                        "",      "https://www.npmjs.com/package/react-remove-scroll-bar" },
        { "react-style-singleton",                          "",      "https://github.com/theKashey/react-style-singleton#readme" },
        { "Real-Time DXT1/DXT5 C compression library",      "",      "https://github.com/Cyan4973/RygsDXTc" },
        { "scheduler",                                      "",      "https://react.dev/" },
        { "SemVer",                                         "",      "https://semver.org" },
        { "Shinyprofiler",                                  "",      "https://code.google.com/p/shinyprofiler" },
        { "SuperSlicer",                                    "",      "https://github.com/supermerill/SuperSlicer" },
        { "TBB",                                            "",      "https://www.intel.cn/content/www/cn/zh/developer/tools/oneapi/onetbb.html" },
        { "tiny-invariant",                                 "",      "https://www.npmjs.com/package/tiny-invariant" },
        { "tiny-warning",                                   "",      "https://www.npmjs.com/package/tiny-warning" },
        { "tslib",                                          "",      "https://www.typescriptlang.org/" },
        { "use-callback-ref",                               "",      "https://www.npmjs.com/package/use-callback-ref" },
        { "use-sidecar",                                    "",      "https://github.com/theKashey/use-sidecar" },
        { "use-sync-external-store",                        "",      "https://www.npmjs.com/package/use-sync-external-store" },
        { "wxWidgets",                                      "",      "https://www.wxwidgets.org" },
        { "zlib",                                           "",      "http://zlib.net" },
        { "zustand",                                        "",      "https://github.com/pmndrs/zustand" },
    };
}

wxString CopyrightsDialog::get_html_text()
{
    // Resolve the page chrome from the kit roles rather than from the OS
    // window/window-text pair: those follow the Windows theme, not the app's, so
    // the library list drifted away from the surface it is embedded in. The link
    // tone is Primary — the default browser blue this page used to inherit sits
    // at ~1.2:1 on the dark surface.
    const wxString bgr_clr_str  = md3_html_colour(MD3::Role::SurfaceContainerLowest);
    const wxString text_clr_str = md3_html_colour(MD3::Role::OnSurface);
    const wxString link_clr_str = md3_html_colour(MD3::Role::Primary);

    const wxString copyright_str = _(L("Copyright")) + "&copy; ";

    wxString text = wxString::Format(
        "<html>"
            "<body bgcolor= %s link= %s>"
            "<font color=%s>"
                "<font size=\"5\">%s</font><br/>"
                "<font size=\"5\">%s</font>"
                "<a href=\"%s\">%s.</a><br/>"
                "<font size=\"5\">%s.</font><br/>"
                "<br /><br />"
                "<font size=\"5\">%s</font><br/>"
                "<font size=\"5\">%s:</font><br/>"
                "<br />"
                "<font size=\"3\">",
         bgr_clr_str, link_clr_str, text_clr_str,
        _L("License"),
        _L("Bambu Studio is licensed under "),
        "https://www.gnu.org/licenses/agpl-3.0.html",_L("GNU Affero General Public License, version 3"),
        _L("Bambu Studio is based on PrusaSlicer by Prusa Research, which is from Slic3r by Alessandro Ranellucci and the RepRap community"),
        _L("Libraries"),
        _L("This software uses open source components whose copyright and other proprietary rights belong to their respective owners"));

    for (auto& entry : m_entries) {
        text += format_wxstr(
                    "%s<br/>"
                    , entry.lib_name);

         text += wxString::Format(
                    "<a href=\"%s\">%s</a><br/><br/>"
                    , entry.link, entry.link);
    }

    text += wxString(
                "</font>"
            "</font>"
            "</body>"
        "</html>");

    return text;
}

void CopyrightsDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    const wxFont& font = GetFont();
    const int fs = font.GetPointSize();
    const int fs2 = static_cast<int>(1.2f*fs);
    int font_size[] = { fs, fs, fs, fs, fs2, fs2, fs2 };

    m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), font_size);

    const int& em = em_unit();

    msw_buttons_rescale(this, em, { wxID_CLOSE });

    const wxSize& size = wxSize(40 * em, 20 * em);

    m_html->SetMinSize(size);
    m_html->Refresh();

    SetMinSize(size);
    Fit();

    Refresh();
}

void CopyrightsDialog::onLinkClicked(wxHtmlLinkEvent &event)
{
    wxGetApp().open_browser_with_warning_dialog(event.GetLinkInfo().GetHref());
    event.Skip(false);
}

void CopyrightsDialog::onCloseDialog(wxEvent &)
{
     this->EndModal(wxID_CLOSE);
}

// The colour a rounded overlay on the About banner has to blend its corners
// into. The banner is exempt brand artwork: every glyph it draws sits in the top
// ~55% of the image and the band below that is a flat fill, so one sample from
// that band is the backdrop. Sampled rather than hardcoded so the overlay keeps
// tracking the artwork if the artwork is ever redrawn.
static wxColour about_banner_backdrop(const wxBitmap &banner)
{
    if (banner.IsOk()) {
        const wxImage img = banner.ConvertToImage();
        if (img.IsOk() && img.GetWidth() > 0 && img.GetHeight() > 0) {
            const int x = img.GetWidth() / 2;
            const int y = img.GetHeight() * 7 / 8;
            return wxColour(img.GetRed(x, y), img.GetGreen(x, y), img.GetBlue(x, y));
        }
    }
    // No artwork to sample from: the accent is the closest stand-in.
    return StateColor::semantic(MD3::Role::Primary);
}

AboutDialog::AboutDialog()
    : DPIDialog(static_cast<wxWindow *>(wxGetApp().mainframe),wxID_ANY,from_u8((boost::format(_utf8(L("About %s"))) % (wxGetApp().is_editor() ? SLIC3R_APP_FULL_NAME : GCODEVIEWER_APP_NAME)).str()),wxDefaultPosition,
        wxDefaultSize, /*wxCAPTION*/wxDEFAULT_DIALOG_STYLE)
{
    SetFont(wxGetApp().normal_font());
	SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));

    std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    wxPanel *m_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(560), FromDIP(237)), wxTAB_TRAVERSAL);

    wxBoxSizer *panel_versizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *vesizer  = new wxBoxSizer(wxVERTICAL);

    m_panel->SetSizer(panel_versizer);

    wxBoxSizer *ver_sizer = new wxBoxSizer(wxVERTICAL);

	auto main_sizer = new wxBoxSizer(wxVERTICAL);
    main_sizer->Add(m_panel, 1, wxEXPAND | wxALL, 0);
    main_sizer->Add(ver_sizer, 0, wxEXPAND | wxALL, 0);

    // logo
    m_logo_bitmap = ScalableBitmap(this, "BambuStudio_about", 250);
    m_logo = new wxStaticBitmap(this, wxID_ANY, m_logo_bitmap.bmp(), wxDefaultPosition,wxDefaultSize, 0);
    m_logo->SetSizer(vesizer);

    panel_versizer->Add(m_logo, 1, wxALL | wxEXPAND, 0);

    // The version lines sit ON the banner, which is exempt brand artwork and so
    // stays the same colour in both themes: no surface tone reads against it in
    // light AND dark. MD3 answers that with a badge (kit containment/Badge) — an
    // opaque SecondaryContainer chip carrying an OnSecondaryContainer label,
    // whose contrast is self-contained whatever the artwork does. It replaces the
    // white-on-BrandGreen wxStaticText fill, which was a text-control background
    // and therefore clipped to the glyph extents with no padding at all.
    const wxColour banner_clr = about_banner_backdrop(m_logo_bitmap.bmp());
    auto make_version_badge = [this, banner_clr](const wxString &text, const wxFont &font) {
        auto *badge = new StaticBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        badge->SetBackgroundColor(StateColor(StateColor::semantic(MD3::Role::SecondaryContainer)));
        // StaticBox clears everything OUTSIDE its rounded rect with the plain
        // window background, which it seeds from the parent surface. Floating
        // over the banner that would nick all four corners with a dialog-coloured
        // wedge, so the backing has to be the artwork's own fill. Must come after
        // SetBackgroundColor(), which re-seeds the backing from the parent.
        badge->SetBackgroundColour(banner_clr);
        // No SetCornerRadius() on purpose: the inherited default (compact r12) is
        // the only radius StaticBox rescales on a monitor-DPI change, and on a
        // ~32px chip it already reads as the pill the kit draws.

        auto *label = new Label(badge, font, text);
        label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSecondaryContainer));
        label->SetBackgroundColour(StateColor::semantic(MD3::Role::SecondaryContainer));

        // The label paints its own square background, so its horizontal inset has
        // to clear the corner arcs (>= the r12 radius) or its corners show through
        // them.
        auto *pad_h = new wxBoxSizer(wxHORIZONTAL);
        pad_h->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(14));
        auto *pad_v = new wxBoxSizer(wxVERTICAL);
        pad_v->Add(pad_h, 0, wxTOP | wxBOTTOM, FromDIP(4));
        badge->SetSizer(pad_v);
        return badge;
    };

    // version
    {
        vesizer->Add(0, FromDIP(165), 1, wxEXPAND, FromDIP(5));
        auto version_text = GUI_App::format_display_version();
#if BBL_INTERNAL_TESTING
        wxString versionText    = BBL_INTERNAL_TESTING == 1 ? _L("Internal Version") : _L("Beta Version");
        auto     version_string = versionText + " " + std::string(version_text);
#else
        auto version_string = _L("Version") + " " + std::string(version_text);
#endif
        // Head_18 (kit dialog_title, 18px/600) instead of the old
        // SetPointSize(FromDIP(16)): a DIP-scaled POINT size is scaled twice --
        // once by FromDIP and again when wx maps points to pixels -- so the line
        // grew 1.5x beyond the banner at 150% display scale.
        vesizer->Add(make_version_badge(version_string, Label::Head_18), 0, wxALL | wxALIGN_CENTER_HORIZONTAL, FromDIP(5));
#if BBL_INTERNAL_TESTING
        wxString plugin_version = wxString::Format("Plugin Version: %s", wxGetApp().getAgent() ? wxGetApp().getAgent()->get_version() : "");
        vesizer->Add(make_version_badge(plugin_version, Label::Body_12), 0, wxALL | wxALIGN_CENTER_HORIZONTAL, FromDIP(5));

        wxString build_time = wxString::Format("Build Time: %s", std::string(SLIC3R_BUILD_TIME));
        vesizer->Add(make_version_badge(build_time, Label::Body_12), 0, wxALL | wxALIGN_CENTER_HORIZONTAL, FromDIP(5));
#endif
        vesizer->Add(0, 0, 1, wxEXPAND, FromDIP(5));
    }

    wxBoxSizer *text_sizer_horiz = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *text_sizer = new wxBoxSizer(wxVERTICAL);
    text_sizer_horiz->Add( 0, 0, 0, wxLEFT, FromDIP(20));

    std::vector<wxString> text_list;
    text_list.push_back(_L("Bambu Studio is based on PrusaSlicer by PrusaResearch and SuperSlicer by Merill(supermerill)."));
    text_list.push_back(_L("PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci."));
    text_list.push_back(_L("Slic3r was created by Alessandro Ranellucci with the help of many other contributors."));
    text_list.push_back(_L("Bambu Studio also referenced some ideas from Cura by Ultimaker."));
    text_list.push_back(_L("There many parts of the software that come from community contributions, so we're unable to list them one-by-one, and instead, they'll be attributed in the corresponding code comments."));

    text_sizer->Add( 0, 0, 0, wxTOP, FromDIP(33));
    bool is_zh = wxGetApp().app_config->get("language") == "zh_CN";
    for (int i = 0; i < text_list.size(); i++)
    {
        auto staticText = new Label(this, Label::Body_12, wxEmptyString, wxALIGN_LEFT, wxSize(FromDIP(520), -1));
        staticText->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        staticText->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
        staticText->SetMinSize(wxSize(FromDIP(520), -1));
        if (is_zh) {
            wxString find_txt = "";
            wxString count_txt = "";
            for (auto  o = 0; o < text_list[i].length(); o++) {
                auto size = staticText->GetTextExtent(count_txt);
                if (size.x < FromDIP(506)) {
                    find_txt += text_list[i][o];
                    count_txt += text_list[i][o];
                } else {
                    find_txt += std::string("\n") + text_list[i][o];
                    count_txt = text_list[i][o];
                }
            }
            staticText->SetLabel(find_txt);
        } else {
            staticText->SetLabel(text_list[i]);
            staticText->Wrap(FromDIP(520));
        }

        text_sizer->Add( staticText, 0, wxUP | wxDOWN, FromDIP(3));
    }

    text_sizer_horiz->Add(text_sizer, 1, wxALL,0);
    ver_sizer->Add(text_sizer_horiz, 0, wxALL,0);
    ver_sizer->Add( 0, 0, 0, wxTOP, FromDIP(43));

    wxBoxSizer *copyright_ver_sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *copyright_hor_sizer = new wxBoxSizer(wxHORIZONTAL);

    copyright_hor_sizer->Add(copyright_ver_sizer, 0, wxLEFT, FromDIP(20));

    Label *html_text = new Label(this, Label::Body_12, "Copyright(C) 2021-2025 Lunkuo All Rights Reserved");
    html_text->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    html_text->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));

    copyright_ver_sizer->Add(html_text, 0, wxALL , 0);

    m_html = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_NEVER /*NEVER*/);
      {
          wxFont font = get_default_font(this);
          const int fs = font.GetPointSize()-1;
          int size[] = {fs,fs,fs,fs,fs,fs,fs};
          m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), size);
          m_html->SetMinSize(wxSize(FromDIP(-1), FromDIP(16)));
          m_html->SetBorders(2);
          m_html->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
          // wxHtml owns its own colours: with no explicit link tone this page kept
          // the default browser blue, which lands at ~1.2:1 on the dark surface.
          const wxString text = wxString::Format(
              "<html>"
              "<body bgcolor=\"%s\" text=\"%s\" link=\"%s\">"
              "<p style=\"text-align:left\"><a  href=\"www.bambulab.com\">www.bambulab.com</ a></p>"
              "</body>"
              "</html>",
              md3_html_colour(MD3::Role::SurfaceContainerLowest),
              md3_html_colour(MD3::Role::OnSurfaceVariant),
              md3_html_colour(MD3::Role::Primary));
          m_html->SetPage(text);
          copyright_ver_sizer->Add(m_html, 0, wxEXPAND, 0);
          m_html->Bind(wxEVT_HTML_LINK_CLICKED, &AboutDialog::onLinkClicked, this);
      }
    //Add "Portions copyright" button
    // MD3 outlined button (kit actions/Button): transparent interior + 1px Outline
    // ring, OnSurface label, pill radius (height/2) and a SurfaceContainerHigh
    // hover wash — all resolved from semantic roles by Button::applyMD3Style(),
    // replacing the White/Grey250/Grey400 + TextPrimary-ring r12 stack. Outlined
    // rather than Text because the legacy button drew a visible ring; dropping it
    // would change the affordance, not just the skin.
    Button* button_portions = new Button(this,_L("Portions copyright"));
    m_btn_portions = button_portions;
    button_portions->SetVariant(Button::Variant::Outlined);
    button_portions->SetButtonSize(Button::Size::Small);

    wxBoxSizer *copyright_button_ver = new wxBoxSizer(wxVERTICAL);
    copyright_button_ver->Add( 0, 0, 0, wxTOP, FromDIP(10));
    copyright_button_ver->Add(button_portions, 0, wxALL,0);

    copyright_hor_sizer->AddStretchSpacer();
    copyright_hor_sizer->Add(copyright_button_ver, 0, wxRIGHT, FromDIP(20));

    ver_sizer->Add(copyright_hor_sizer, 0, wxEXPAND ,0);
    ver_sizer->Add( 0, 0, 0, wxTOP, FromDIP(30));
    button_portions->Bind(wxEVT_BUTTON, &AboutDialog::onCopyrightBtn, this);

    wxGetApp().UpdateDlgDarkUI(this);
	SetSizer(main_sizer);
    Layout();
    Fit();
    MD3DialogCaption::Adopt(this);
    CenterOnParent();
}

void AboutDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    m_logo_bitmap.msw_rescale();
    m_logo->SetBitmap(m_logo_bitmap.bmp());

    const wxFont& font = GetFont();
    const int fs = font.GetPointSize() - 1;
    int font_size[] = { fs, fs, fs, fs, fs, fs, fs };
    m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), font_size);

    const int& em = em_unit();

    msw_buttons_rescale(this, em, { wxID_CLOSE, m_copy_rights_btn_id });

    // A variant Button re-derives its pill radius, padding, height and font from
    // the new DPI; msw_buttons_rescale above only knows about native wxButtons.
    if (m_btn_portions)
        m_btn_portions->Rescale();

    m_html->SetMinSize(wxSize(-1, 16 * em));
    m_html->Refresh();

    const wxSize& size = wxSize(65 * em, 30 * em);

    SetMinSize(size);
    Fit();
    Refresh();
}

void AboutDialog::onLinkClicked(wxHtmlLinkEvent &event)
{
    wxGetApp().open_browser_with_warning_dialog(event.GetLinkInfo().GetHref());
    event.Skip(false);
}

void AboutDialog::onCloseDialog(wxEvent &)
{
    this->EndModal(wxID_CLOSE);
}

void AboutDialog::onCopyrightBtn(wxEvent &)
{
    CopyrightsDialog dlg;
    dlg.ShowModal();
}

void AboutDialog::onCopyToClipboard(wxEvent&)
{
    wxTheClipboard->Open();
    wxTheClipboard->SetData(new wxTextDataObject(_L("Version") + " " + GUI_App::format_display_version()));
    wxTheClipboard->Close();
}

} // namespace GUI
} // namespace Slic3r
