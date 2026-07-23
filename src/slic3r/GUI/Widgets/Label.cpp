#include "libslic3r/Utils.hpp"
#include "Label.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"
#include "StaticBox.hpp"

#include "../GUI_App.hpp"
#include "libslic3r/AppConfig.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <wx/app.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/font.h>
#include <wx/fontenum.h>
#include <wx/settings.h>

namespace {

// The GUI_App's AppConfig, or nullptr when no app object exists yet. The very
// first Label::initSysFont() runs from the CLI bootstrap (BambuStudio.cpp)
// before GUI_Run() creates the wxApp, and wxGetApp() dereferences
// wxApp::GetInstance(); guarding on it keeps that early call crash-safe (fonts
// then build from defaults).
Slic3r::AppConfig *uiAppConfig()
{
    if (!wxApp::GetInstance())
        return nullptr;
    return Slic3r::GUI::wxGetApp().app_config;
}

// Trimmed value of the "ui_font_family" AppConfig key ("" when unset/blank).
std::string uiFontFamilyConfig()
{
    Slic3r::AppConfig *cfg = uiAppConfig();
    if (!cfg)
        return {};
    std::string v = cfg->get("ui_font_family");
    const auto  b = v.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return {};
    const auto e = v.find_last_not_of(" \t\r\n");
    return v.substr(b, e - b + 1);
}

// Raw "ui_font_scale" multiplier (1.0 when unset/invalid). The legible 0.8..1.4
// clamp is applied by MD3::Type::setUiFontScale when the value is installed.
double readUiFontScaleConfig()
{
    Slic3r::AppConfig *cfg = uiAppConfig();
    if (!cfg)
        return 1.0;
    const std::string v = cfg->get("ui_font_scale");
    if (v.empty())
        return 1.0;
    try {
        return std::stod(v);
    } catch (...) {
        return 1.0;
    }
}

bool isCJKLang(const std::string &lang_code)
{
    return lang_code == "zh_TW" || lang_code == "zh_HK" || lang_code == "yue_HK" ||
           lang_code == "bilingual_en_yue_HK" || lang_code == "ja" || lang_code == "ko";
}

// True when `face` names an installed/registered family. wxFont::IsOk() alone is
// insufficient — GDI substitutes a default face for an unknown name and still
// reports Ok — so validate the name against the enumerated font list, with a
// construct-and-compare fallback for faces that enumerate under a variant name.
bool faceIsInstalled(const wxString &face)
{
    if (face.empty())
        return false;
    if (wxFontEnumerator::IsValidFacename(face))
        return true;
    wxFont probe(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, face);
    return probe.IsOk() && probe.GetFaceName() == face;
}

// True when `face` covers the CJK ideograph block (probe glyph U+4E00). Only
// consulted for CJK locales so a user-picked ui_font_family lacking CJK glyphs
// falls back to the locale's CJK face instead of rendering tofu. Defined per
// platform after the Win32 include below (Windows probes the font's Unicode
// ranges via GDI; other platforms rely on the toolkit's own glyph fallback).
bool faceSupportsCJK(const wxString &face);

// Bundled default UI face for a locale: Roboto for Latin, the platform CJK
// family for CJK locales (Roboto carries no CJK glyphs). Used when no valid
// user override applies.
wxString md3DefaultFaceName(const std::string &lang_code)
{
    if (lang_code == "zh_TW" || lang_code == "zh_HK" || lang_code == "yue_HK" ||
        lang_code == "bilingual_en_yue_HK") {
#ifdef __WXMSW__
        // Roboto does not contain Traditional Chinese glyphs. This Windows UI
        // family is the preferred Cantonese/Traditional CJK face and retains
        // normal system fallback when its optional font pack is unavailable.
        return wxString::FromUTF8("Microsoft JhengHei UI");
#else
        return wxString::FromUTF8("Noto Sans CJK TC");
#endif
    } else if (lang_code == "ja") {
        return wxString::FromUTF8("Source Han Sans JP Normal");
    } else if (lang_code == "ko") {
        return wxString::FromUTF8("NanumGothic");
    }
    return wxString::FromUTF8("Roboto");
}

// UI face for a locale. A user-chosen "ui_font_family" wins when it is installed
// and — for CJK locales — actually carries CJK glyphs; otherwise the bundled
// Roboto/CJK default applies. A bundled private face named here is already
// session-registered (see SessionFontRegistrar) and safe for the text path;
// only the MaterialIcon plain-GDI path must avoid private faces (untouched).
wxString md3FaceName(const std::string &lang_code)
{
    const std::string fam = uiFontFamilyConfig();
    if (!fam.empty()) {
        const wxString face = wxString::FromUTF8(fam);
        if (faceIsInstalled(face) && (!isCJKLang(lang_code) || faceSupportsCJK(face)))
            return face;
    }
    return md3DefaultFaceName(lang_code);
}

// Nearest wx font-weight enum for an MD3 numeric weight (400/500/600/700). Used
// only to seed the wxFont constructor; SetNumericWeight() then records the exact
// design weight so font matching can pick Medium/SemiBold faces when present.
wxFontWeight md3WeightEnum(int numeric_weight)
{
    if (numeric_weight >= 700) return wxFONTWEIGHT_BOLD;
    if (numeric_weight >= 600) return wxFONTWEIGHT_SEMIBOLD;
    if (numeric_weight >= 500) return wxFONTWEIGHT_MEDIUM;
    return wxFONTWEIGHT_NORMAL;
}

// Build a font that follows an MD3 type token (fractional design px size + a
// precise numeric weight). The design px -> wx point-size scaling matches
// sysFont() so the Head_/Body_ helpers stay consistent with it.
wxFont md3StyledFont(const MD3::TypeStyle &style, const std::string &lang_code)
{
    double point_size = style.size;
#ifndef __APPLE__
    point_size = point_size * 4.0 / 5.0; // design px -> wx point size
#endif
    point_size *= MD3::Type::uiFontScale(); // Appearance > Font size (runtime scale)
    const int          initial     = point_size < 1.0 ? 1 : static_cast<int>(point_size);
    const wxFontWeight enum_weight = md3WeightEnum(style.weight);

    wxString face = md3FaceName(lang_code);
    wxFont   font{initial, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, enum_weight, false, face};
    font.SetFaceName(face);
    font.SetFractionalPointSize(point_size);
    font.SetNumericWeight(style.weight);

    if (!font.IsOk() && lang_code != "ja" && lang_code != "ko") {
        face = wxString::FromUTF8("HarmonyOS Sans SC");
        font = wxFont{initial, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, enum_weight, false, face};
        font.SetFaceName(face);
        font.SetFractionalPointSize(point_size);
        font.SetNumericWeight(style.weight);
    }
    if (!font.IsOk()) {
        font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        font.SetNumericWeight(style.weight);
        font.SetFractionalPointSize(point_size);
    }
    return font;
}

// Mono variant of md3StyledFont — Roboto Mono with a generic monospace
// fallback, for the numeric/technical values the design renders in mono.
wxFont md3MonoFont(const MD3::TypeStyle &style)
{
    double point_size = style.size;
#ifndef __APPLE__
    point_size = point_size * 4.0 / 5.0; // design px -> wx point size
#endif
    point_size *= MD3::Type::uiFontScale(); // Appearance > Font size (runtime scale)
    const int          initial     = point_size < 1.0 ? 1 : static_cast<int>(point_size);
    const wxFontWeight enum_weight = md3WeightEnum(style.weight);

    wxString face = wxString::FromUTF8(MD3::Type::font_mono);
    wxFont   font{initial, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, enum_weight, false, face};
    font.SetFaceName(face);
    font.SetFractionalPointSize(point_size);
    font.SetNumericWeight(style.weight);
    if (!font.IsOk()) {
        font = wxFont{initial, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, enum_weight, false};
        font.SetNumericWeight(style.weight);
        font.SetFractionalPointSize(point_size);
    }
    return font;
}

} // namespace

wxFont Label::sysFont(int size, bool bold, std::string lang_code)
{
//#ifdef __linux__
//    return wxFont{};
//#endif
#ifndef __APPLE__
    size = size * 4 / 5;
#endif
    // Appearance > Font size (runtime scale), mirroring md3StyledFont/md3MonoFont.
    size = std::max(1, static_cast<int>(std::lround(size * MD3::Type::uiFontScale())));

    wxString face = md3FaceName(lang_code);

    wxFont font{size, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL, false, face};
    font.SetFaceName(face);
    if (!font.IsOk() && lang_code != "ja" && lang_code != "ko") {
        face = wxString::FromUTF8("HarmonyOS Sans SC");
        font = wxFont{size, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL, false, face};
        font.SetFaceName(face);
    }
    if (!font.IsOk()) {
        font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
        if (bold) font.MakeBold();
        font.SetPointSize(size);
    }

    return font;
}
wxFont Label::Head_48;
wxFont Label::Head_32;
wxFont Label::Head_24;
wxFont Label::Head_20;
wxFont Label::Head_18;
wxFont Label::Head_16;
wxFont Label::Head_15;
wxFont Label::Head_14;
wxFont Label::Head_13;
wxFont Label::Head_12;
wxFont Label::Head_11;
wxFont Label::Head_10;

wxFont Label::Body_16;
wxFont Label::Body_15;
wxFont Label::Body_14;
wxFont Label::Body_13;
wxFont Label::Body_12;
wxFont Label::Body_11;
wxFont Label::Body_10;
wxFont Label::Body_9;
wxFont Label::Body_8;
wxFont Label::Mono_14;
wxFont Label::Mono_13;
wxFont Label::Mono_12;
wxFont Label::Mono_11;

#ifdef __WXMSW__
#include <windows.h>
namespace {
// GDI+ builds its font-family table from the session font list. Faces added
// with FR_PRIVATE (wxFont::AddPrivateFont) are invisible to that table, and
// handing such an HFONT to any wxGraphicsContext/wxGCDC poisons GDI+'s
// family cache — a use-after-free inside GdipCloneFontFamily that surfaced
// as the deterministic startup heap-corruption crash (PageHeap-verified).
// Register the bundled faces session-visible instead, and remove them again
// when the process exits so the session font table stays clean.
struct SessionFontRegistrar {
    std::vector<std::wstring> paths;
    bool add(const wxString &path)
    {
        const std::wstring w = path.ToStdWstring();
        if (::AddFontResourceExW(w.c_str(), 0, nullptr) > 0) {
            paths.push_back(w);
            return true;
        }
        return false;
    }
    ~SessionFontRegistrar()
    {
        for (const auto &w : paths)
            ::RemoveFontResourceExW(w.c_str(), 0, nullptr);
    }
};
SessionFontRegistrar g_session_fonts;
bool add_app_font(const wxString &path) { return g_session_fonts.add(path); }
} // namespace
#else
static bool add_app_font(const wxString &path) { return wxFont::AddPrivateFont(path); }
#endif

namespace {
#ifdef __WXMSW__
// Probe the font's own Unicode ranges (via GDI) for the CJK ideograph U+4E00.
// GDI font linking would silently substitute glyphs at draw time, so coverage
// cannot be inferred by measuring text — it must be read from the face itself.
// Any failure (bitmap face, unsupported query, unknown name) reports no coverage
// so the caller keeps the locale's CJK fallback. Read-only GDI, no wx font path.
bool faceSupportsCJK(const wxString &face)
{
    HDC hdc = ::CreateCompatibleDC(nullptr);
    if (!hdc)
        return false;
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    ::lstrcpynW(lf.lfFaceName, face.ToStdWstring().c_str(), LF_FACESIZE);
    HFONT hfont = ::CreateFontIndirectW(&lf);
    bool  has   = false;
    if (hfont) {
        HGDIOBJ     old = ::SelectObject(hdc, hfont);
        const DWORD sz  = ::GetFontUnicodeRanges(hdc, nullptr);
        if (sz) {
            std::vector<unsigned char> buf(sz);
            GLYPHSET *gs = reinterpret_cast<GLYPHSET *>(buf.data());
            gs->cbThis   = sz;
            if (::GetFontUnicodeRanges(hdc, gs)) {
                for (DWORD i = 0; i < gs->cRanges && !has; ++i) {
                    const WCRANGE &r    = gs->ranges[i];
                    const unsigned low  = r.wcLow;
                    const unsigned high = low + (r.cGlyphs ? r.cGlyphs - 1u : 0u);
                    if (0x4E00u >= low && 0x4E00u <= high)
                        has = true;
                }
            }
        }
        ::SelectObject(hdc, old);
        ::DeleteObject(hfont);
    }
    ::DeleteDC(hdc);
    return has;
}
#else
// Non-Windows toolkits (Pango / Core Text) perform their own per-glyph fallback
// at draw time, so CJK still renders even under a non-CJK primary face; accept
// the user family here rather than probing coverage.
bool faceSupportsCJK(const wxString &) { return true; }
#endif
} // namespace

void Label::initSysFont(std::string lang_code, bool load_font_resource)
{
    if (load_font_resource) {
        const std::string& resource_path = Slic3r::resources_dir();
        wxString font_path = wxString::FromUTF8(resource_path+"/fonts/Roboto-Regular.ttf");
        bool result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of Roboto-Regular returns %1%")%result;
        font_path = wxString::FromUTF8(resource_path+"/fonts/Roboto-Medium.ttf");
        result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of Roboto-Medium returns %1%")%result;
        font_path = wxString::FromUTF8(resource_path+"/fonts/Roboto-Bold.ttf");
        result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of Roboto-Bold returns %1%")%result;
        // Roboto Mono renders every numeric/technical value in the MD3 design
        // system (temperatures, times, dimensions, commit hashes).
        font_path = wxString::FromUTF8(resource_path+"/fonts/RobotoMono-Regular.ttf");
        result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of RobotoMono-Regular returns %1%")%result;
        font_path = wxString::FromUTF8(resource_path+"/fonts/RobotoMono-Medium.ttf");
        result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of RobotoMono-Medium returns %1%")%result;
        font_path = wxString::FromUTF8(resource_path+"/fonts/RobotoMono-Bold.ttf");
        result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of RobotoMono-Bold returns %1%")%result;
        // Material Symbols Outlined is the MD3 icon face (see MD3::Type::font_icon
        // and Widgets/MaterialIcon). Registered here alongside Roboto so icon
        // glyphs resolve from the first paint. MaterialIcon::font() re-registers
        // defensively via std::call_once for the initSysFont(false) paths.
        font_path = wxString::FromUTF8(resource_path+"/fonts/MaterialSymbolsOutlined.ttf");
        result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of MaterialSymbolsOutlined returns %1%")%result;
    }
#ifdef __linux__
    if (load_font_resource) {
        const std::string& resource_path = Slic3r::resources_dir();
        // TODO: Bundle Roboto TTFs in resources/fonts; HarmonyOS remains the fallback.
        wxString font_path = wxString::FromUTF8(resource_path+"/fonts/HarmonyOS_Sans_SC_Bold.ttf");
        bool result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of HarmonyOS_Sans_SC_Bold returns %1%")%result;
        font_path = wxString::FromUTF8(resource_path+"/fonts/HarmonyOS_Sans_SC_Regular.ttf");
        result = add_app_font(font_path);
        BOOST_LOG_TRIVIAL(info) << boost::format("add font of HarmonyOS_Sans_SC_Regular returns %1%")%result;
    }
#endif
    // Build the Head_/Body_/Mono_ font objects from the current AppConfig. Kept
    // in rebuild_fonts so a later font-family / font-scale change can rebuild
    // them without re-registering the bundled faces (which must happen once).
    rebuild_fonts(lang_code);
}

void Label::rebuild_fonts(std::string lang_code)
{
    // Refresh the runtime UI font scale from AppConfig before (re)building, so
    // every md3StyledFont/md3MonoFont point size below picks up the multiplier.
    MD3::Type::setUiFontScale(readUiFontScaleConfig());

    // Retarget the Head_/Body_ helpers onto the MD3 type scale (see
    // Widgets/MD3Tokens.hpp, namespace MD3::Type) so the ~1200 existing call
    // sites inherit the design sizes and per-role weights unchanged. Head_
    // helpers carry the title/label emphasis weights (600-700); Body_ helpers
    // stay regular (400). Sizes/weights outside the named scale (display heads,
    // 9/8px captions) use explicit tokens. The active UI face (md3FaceName) and
    // scale both come from AppConfig (ui_font_family / ui_font_scale).
    Head_48 = md3StyledFont(MD3::TypeStyle{48.0f, 700}, lang_code); // display, above scale
    Head_32 = md3StyledFont(MD3::TypeStyle{32.0f, 700}, lang_code); // display, above scale
    Head_24 = md3StyledFont(MD3::Type::headline, lang_code);        // 23/700
    Head_20 = md3StyledFont(MD3::Type::page_title, lang_code);      // 20/700
    Head_18 = md3StyledFont(MD3::Type::dialog_title, lang_code);    // 18/600
    Head_16 = md3StyledFont(MD3::Type::section_title, lang_code);   // 16/700
    Head_15 = md3StyledFont(MD3::Type::card_title, lang_code);      // 15/600
    Head_14 = md3StyledFont(MD3::TypeStyle{14.0f, 600}, lang_code); // emphasized body
    Head_13 = md3StyledFont(MD3::TypeStyle{13.5f, 600}, lang_code); // body-s size, strong
    Head_12 = md3StyledFont(MD3::TypeStyle{12.5f, 600}, lang_code); // body-xs size, strong
    Head_11 = md3StyledFont(MD3::Type::label, lang_code);           // 11/600
    Head_10 = md3StyledFont(MD3::TypeStyle{10.5f, 600}, lang_code); // micro size, strong

    Body_16 = md3StyledFont(MD3::TypeStyle{16.0f, 400}, lang_code);
    Body_15 = md3StyledFont(MD3::TypeStyle{15.0f, 400}, lang_code);
    Body_14 = md3StyledFont(MD3::Type::body, lang_code);            // 14/400
    Body_13 = md3StyledFont(MD3::TypeStyle{13.0f, 400}, lang_code); // compact-density body
    Body_12 = md3StyledFont(MD3::Type::body_xs, lang_code);         // 12.5/400
    Body_11 = md3StyledFont(MD3::Type::caption, lang_code);         // 11.5/400
    Body_10 = md3StyledFont(MD3::Type::micro, lang_code);           // 10.5/400
    Body_9  = md3StyledFont(MD3::TypeStyle{9.0f, 400}, lang_code);
    Body_8  = md3StyledFont(MD3::TypeStyle{8.0f, 400}, lang_code);

    Mono_14 = md3MonoFont(MD3::TypeStyle{14.0f, 400});
    Mono_13 = md3MonoFont(MD3::TypeStyle{13.5f, 500}); // row primary values
    Mono_12 = md3MonoFont(MD3::TypeStyle{12.5f, 400});
    Mono_11 = md3MonoFont(MD3::TypeStyle{11.5f, 400}); // metadata values
}

class WXDLLIMPEXP_CORE wxTextWrapper2
{
public:
    wxTextWrapper2() { m_eol = false; }

    // win is used for getting the font, text is the text to wrap, width is the
    // max line width or -1 to disable wrapping
    void Wrap(wxWindow *win, const wxString &text, int widthMax)
    {
        const wxClientDC dc(win);
        Wrap(dc, text, widthMax);
    }

    void Wrap(wxDC const &dc, const wxString &text, int widthMax, int maxCount = 0)
    {
        const wxArrayString ls = wxSplit(text, '\n', '\0');
        for (wxArrayString::const_iterator i = ls.begin(); i != ls.end(); ++i) {
            wxString line = *i;
            int count = 0;

            if (i != ls.begin()) {
                // Do this even if the line is empty, except if it's the first one.
                OnNewLine();
            }

            // Is this a special case when wrapping is disabled?
            if (widthMax < 0) {
                DoOutputLine(line);
                continue;
            }

            for (bool newLine = false; !line.empty(); newLine = true) {
                if (newLine) OnNewLine();

                if (1 == line.length()) {
                    DoOutputLine(line);
                    break;
                }

                wxArrayInt widths;
                dc.GetPartialTextExtents(line, widths);

                const size_t posEnd = std::lower_bound(widths.begin(), widths.end(), widthMax) - widths.begin();

                // Does the entire remaining line fit?
                if (posEnd == line.length()) {
                    DoOutputLine(line);
                    break;
                }

                // Find the last word to chop off.
                size_t lastSpace = posEnd;
                while (lastSpace > 0) {
                    auto c = line[lastSpace];
                    if (c == ' ')
                        break;
                    if (c > 0x4E00) {
                        if (lastSpace != posEnd)
                            ++lastSpace;
                        break;
                    }
                    --lastSpace;
                }
                if (lastSpace == 0) {
                    // No spaces, so can't wrap.
                    lastSpace = posEnd;
                }
                if (lastSpace == 0) {
                    // Break at least one char
                    lastSpace = 1;
                }

                // Output the part that fits.
                DoOutputLine(line.substr(0, lastSpace));

                // And redo the layout with the rest.
                if (line[lastSpace] == ' ') ++lastSpace;
                line = line.substr(lastSpace);

                if (maxCount > 0 && ++count == maxCount - 1) {
                    OnNewLine();
                    DoOutputLine(line);
                    break;
                }
            }
        }
    }

    // we don't need it, but just to avoid compiler warnings
    virtual ~wxTextWrapper2() {}

protected:
    // line may be empty
    virtual void OnOutputLine(const wxString &line) = 0;

    // called at the start of every new line (except the very first one)
    virtual void OnNewLine() {}

private:
    // call OnOutputLine() and set m_eol to true
    void DoOutputLine(const wxString &line)
    {
        OnOutputLine(line);

        m_eol = true;
    }

    // this function is a destructive inspector: when it returns true it also
    // resets the flag to false so calling it again wouldn't return true any
    // more
    bool IsStartOfNewLine()
    {
        if (!m_eol) return false;

        m_eol = false;

        return true;
    }

    bool m_eol;
};

class wxLabelWrapper2 : public wxTextWrapper2
{
public:
    void WrapLabel(wxDC const & dc, wxString const & label, int widthMax)
    {
        m_text.clear();
        Wrap(dc, label, widthMax);
    }

    void WrapLabel(wxWindow *text, wxString const & label, int widthMax)
    {
        m_text.clear();
        Wrap(text, label, widthMax);
    }

    wxString GetText() const { return m_text; }

protected:
    virtual void OnOutputLine(const wxString &line) wxOVERRIDE { m_text += line; }

    virtual void OnNewLine() wxOVERRIDE { m_text += wxT('\n'); }

private:
    wxString m_text;
};


wxSize Label::split_lines(wxDC &dc, int width, const wxString &text, wxString &multiline_text, int max_count)
{
    wxLabelWrapper2 wrap;
    wrap.Wrap(dc, text, width, max_count);
    multiline_text = wrap.GetText();
    return dc.GetMultiLineTextExtent(multiline_text);
}

Label::Label(wxWindow *parent, wxString const &text, long style, wxSize size) : Label(parent, Body_14, text, style, size) {}

Label::Label(wxWindow *parent, wxFont const &font, wxString const &text, long style, wxSize size)
    : wxStaticText(parent, wxID_ANY, text, wxDefaultPosition, size, style)
{
    this->m_font = font;
    this->m_text = text;
    SetFont(font);
    SetForegroundColour(*wxBLACK);
    SetBackgroundColour(StaticBox::GetParentBackgroundColor(parent));
    SetForegroundColour(ThemeColor::TextPrimary);
    if (style & LB_PROPAGATE_MOUSE_EVENT) {
        for (auto evt : { wxEVT_LEFT_UP, wxEVT_LEFT_DOWN })
            Bind(evt, [this] (auto & e) { GetParent()->GetEventHandler()->ProcessEventLocally(e); });
    };
    if (style & LB_AUTO_WRAP) {
        Bind(wxEVT_SIZE, &Label::OnSize, this);
        Wrap(GetSize().x);
    }
}

void Label::SetLabel(const wxString& label)
{
    if (m_text == label)
        return;
    m_text = label;
    if ((GetWindowStyle() & LB_AUTO_WRAP)) {
        Wrap(GetSize().x);
    } else {
        wxStaticText::SetLabel(label);
    }
#ifdef __WXOSX__
    if ((GetWindowStyle() & LB_HYPERLINK)) {
        SetLabelMarkup(label);
        return;
    }
#endif
}

void Label::SetWindowStyleFlag(long style)
{
    if (style == GetWindowStyle())
        return;
    wxStaticText::SetWindowStyleFlag(style);
    if (style & LB_HYPERLINK) {
        this->m_color = GetForegroundColour();
        static wxColor clr_url(ThemeColor::BrandGreen);
        SetFont(this->m_font.Underlined());
        SetForegroundColour(clr_url);
        SetCursor(wxCURSOR_HAND);
#ifdef __WXOSX__
        SetLabelMarkup(m_text);
#endif
    } else {
        SetForegroundColour(this->m_color);
        SetFont(this->m_font);
        SetCursor(wxCURSOR_ARROW);
#ifdef __WXOSX__
        wxStaticText::SetLabel({});
        SetLabel(m_text);
#endif
    }
    Refresh();
}

wxSize Label::DoGetBestClientSize() const
{
    wxSize size = wxStaticText::DoGetBestClientSize();
#ifdef WIN32
    // GetTextExtentPoint32 can underestimate the width needed by the native
    // STATIC control to render text. Add a small margin to prevent clipping.
    if (size.x > 0) size.x += FromDIP(4);
#endif
    return size;
}

void Label::Wrap(int width)
{
    wxLabelWrapper2 wrapper;
    wrapper.Wrap(this, m_text, width);
    m_skip_size_evt = true;
    wxStaticText::SetLabel(wrapper.GetText());
    m_skip_size_evt = false;
}

void Label::OnSize(wxSizeEvent &evt)
{
    evt.Skip();
    if (m_skip_size_evt) return;
    Wrap(evt.GetSize().x);
}

// ---------------------------------------------------------------------------
// SectionHeader
// ---------------------------------------------------------------------------

namespace {

// Width of a run drawn with per-glyph letter-spacing: the natural extent plus
// one tracking step between each pair of glyphs.
double trackedTextWidth(wxDC &dc, const wxString &text, double tracking)
{
    if (text.empty()) return 0.0;
    wxCoord w = 0, h = 0;
    dc.GetTextExtent(text, &w, &h);
    return static_cast<double>(w) + tracking * (text.length() - 1);
}

// Draw text one glyph at a time so the +tracking letter-spacing lands between
// characters (native STATIC / DrawText cannot apply tracking).
void drawTrackedText(wxDC &dc, const wxString &text, double x, int y, double tracking)
{
    for (size_t i = 0; i < text.length(); ++i) {
        const wxString ch = text.SubString(i, i);
        dc.DrawText(ch, wxPoint(static_cast<int>(x + 0.5), y));
        wxCoord w = 0, h = 0;
        dc.GetTextExtent(ch, &w, &h);
        x += static_cast<double>(w) + tracking;
    }
}

} // namespace

SectionHeader::SectionHeader(wxWindow *parent, wxString const &text, uint32_t leading_icon, long style)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, style)
    , m_text(text)
    , m_icon(leading_icon)
{
    SetBackgroundColour(StaticBox::GetParentBackgroundColor(parent));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    wxWindow::SetLabel(text); // keep the base label in sync for accessibility
    Bind(wxEVT_PAINT, &SectionHeader::OnPaint, this);
    InvalidateBestSize();
}

void SectionHeader::SetLabel(const wxString &label)
{
    if (m_text == label) return;
    m_text = label;
    wxWindow::SetLabel(label); // keep the base label in sync for accessibility
    InvalidateBestSize();
    Refresh();
}

void SectionHeader::SetLeadingIcon(uint32_t codepoint)
{
    if (m_icon == codepoint) return;
    m_icon = codepoint;
    InvalidateBestSize();
    Refresh();
}

wxSize SectionHeader::DoGetBestClientSize() const
{
    wxClientDC dc(const_cast<SectionHeader *>(this));
    dc.SetFont(Label::Head_11);

    const double scale    = static_cast<double>(FromDIP(1000)) / 1000.0; // fractional DPI factor
    const double tracking = MD3::Type::label_tracking * scale;
    const int    gap      = FromDIP(6);
    const int    icon_px  = 16; // logical px; the icon font scales with the DC

    const wxString up = m_text.Upper();

    wxCoord th = 0, tmp = 0;
    dc.GetTextExtent(up.empty() ? wxString("X") : up, &tmp, &th);

    double width  = trackedTextWidth(dc, up, tracking);
    int    height = th;

    if (m_icon) {
        const wxSize is = MaterialIcon::measure(dc, m_icon, icon_px);
        width += (up.empty() ? 0 : gap) + is.x;
        height = std::max(height, is.y);
    }

    return wxSize(static_cast<int>(width + 0.5), height);
}

void SectionHeader::OnPaint(wxPaintEvent &)
{
    wxPaintDC pdc(this);
    const wxSize sz = GetSize();
    pdc.SetBackground(wxBrush(GetBackgroundColour()));
    pdc.Clear();

#ifdef __WXMSW__
    wxGCDC dc(pdc);
    // Guard: GDI+ heap-corrupts when handed a font whose face is missing from
    // the session font table (see the MaterialIcon.cpp plain-GDI rewrite), so
    // verify the Label-table face before it reaches this wxGCDC. Deliberately
    // NOT faceIsInstalled(): its construct-and-compare fallback echoes the
    // requested name on wxMSW and so never fails; a strict enumerator check is
    // required for the heap-safety property. Cached per face — it can only
    // change on Label::rebuild_fonts.
    const wxFont header_font = [] {
        static wxString s_face;
        static bool     s_ok = false;
        const wxString  face = Label::Head_11.GetFaceName();
        if (face != s_face) {
            s_face = face;
            s_ok   = !face.empty() && wxFontEnumerator::IsValidFacename(face);
        }
        return s_ok ? Label::Head_11 : wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    }();
#else
    wxDC &dc = pdc;
    const wxFont &header_font = Label::Head_11;
#endif

    dc.SetFont(header_font);
    const wxColour fg = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    dc.SetTextForeground(fg);

    const double scale    = static_cast<double>(FromDIP(1000)) / 1000.0;
    const double tracking = MD3::Type::label_tracking * scale;
    const int    gap      = FromDIP(6);
    const int    icon_px  = 16;

    const wxString up = m_text.Upper();

    wxCoord th = 0, tmp = 0;
    dc.GetTextExtent(up.empty() ? wxString("X") : up, &tmp, &th);

    int content_h = th;
    wxSize is(0, 0);
    if (m_icon) {
        is       = MaterialIcon::measure(dc, m_icon, icon_px);
        content_h = std::max(content_h, is.y);
    }

    const int y0 = (sz.y - content_h) / 2;
    double    x  = 0;

    if (m_icon) {
        const int iy = y0 + (content_h - is.y) / 2;
        MaterialIcon::draw(dc, m_icon, icon_px, fg, wxPoint(static_cast<int>(x + 0.5), iy));
        x += is.x + (up.empty() ? 0 : gap);
    }

    const int ty = y0 + (content_h - th) / 2;
    drawTrackedText(dc, up, x, ty, tracking);
}
