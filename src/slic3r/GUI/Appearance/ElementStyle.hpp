#ifndef slic3r_GUI_Appearance_ElementStyle_hpp_
#define slic3r_GUI_Appearance_ElementStyle_hpp_

// Per-element appearance registry.
//
// Every rendered element that opts in is addressed by a stable string id
// ("project-tab", "menu.item", "preferences.row/dark_color_mode", ...). The
// registry maps that id to a small property bag (font family / size / weight /
// style / decorations / letter spacing / line height, foreground, background,
// highlight, border colour / width, radius, padding, margin). Values resolve
// through three layers, most specific first:
//
//   1. the user's per-element overrides            ("elements" in the file)
//   2. the active named preset                     ("presets" / shipped)
//        2a. the preset's entry for the element id
//        2b. the preset's "*" entry (every element)
//   3. the caller's base value (the MD3 token the widget would use anyway)
//
// Ids may carry a parent separated by '/': "project-tab/model.3mf" falls back
// to "project-tab" at every layer before giving up, so one rule can style a
// family of elements while a single member can still be overridden.
//
// The schema is versioned JSON. Unknown top-level keys and unknown element
// properties are kept verbatim across load/save and reported through
// LoadReport, never dropped. The registry itself depends only on wx core/base
// and nlohmann::json so tests/appearance can exercise it without GUI_App; the
// wxWindow adopter (ElementStyle::apply and friends) lives beside it.

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <wx/colour.h>
#include <wx/font.h>
#include <wx/string.h>
#include <wx/window.h>

#include "nlohmann/json.hpp"

namespace Slic3r { namespace GUI {

// Known property keys. Values are JSON scalars: strings for family / colours /
// font style, numbers for sizes and spacing, booleans for decorations.
namespace StyleProp {
inline constexpr const char *font_family    = "fontFamily";    // string, face name
inline constexpr const char *font_size      = "fontSize";      // number, points
inline constexpr const char *font_weight    = "fontWeight";    // number, 100..900
inline constexpr const char *font_style     = "fontStyle";     // "normal" | "italic"
inline constexpr const char *underline      = "underline";     // bool
inline constexpr const char *strikethrough  = "strikethrough"; // bool
inline constexpr const char *letter_spacing = "letterSpacing"; // number, px
inline constexpr const char *line_height    = "lineHeight";    // number, multiplier
inline constexpr const char *foreground     = "foreground";    // "#rrggbb[aa]"
inline constexpr const char *background     = "background";    // "#rrggbb[aa]"
inline constexpr const char *highlight      = "highlight";     // "#rrggbb[aa]"
inline constexpr const char *border_color   = "borderColor";   // "#rrggbb[aa]"
inline constexpr const char *border_width   = "borderWidth";   // number, px
inline constexpr const char *radius         = "radius";        // number, px
inline constexpr const char *padding        = "padding";       // number, px
inline constexpr const char *margin         = "margin";        // number, px

// Every key above, in display order.
const std::vector<std::string> &all();
// True when `key` is one of the known properties.
bool is_known(const std::string &key);
} // namespace StyleProp

// Result of loading a file: what was read, what was kept without being
// understood, and why a load failed.
struct StyleLoadReport
{
    bool                     ok { true };
    std::string              error;               // non-empty when !ok
    int                      schema { 0 };        // schema version found
    std::vector<std::string> unknown_top_level;   // keys kept verbatim
    // "<element id>.<property>" for every property kept but not understood.
    std::vector<std::string> unknown_properties;
};

// One preset: element id -> property bag. "*" applies to every element.
using StyleBag    = nlohmann::json; // object: property -> scalar
using StylePreset = std::map<std::string, StyleBag>;

class StyleRegistry
{
public:
    static constexpr int kSchema = 1;
    static constexpr const char *kDefaultPreset = "Material default";
    static constexpr const char *kEveryElement  = "*";

    StyleRegistry();

    // --- Resolution -------------------------------------------------------
    // Resolved value for (id, key) through the three layers; null json when no
    // layer supplies one.
    nlohmann::json resolve(const std::string &id, const std::string &key) const;
    bool           has_override(const std::string &id, const std::string &key) const;
    // Every property the element resolves to something for (user + preset).
    StyleBag       resolved_bag(const std::string &id) const;

    // --- User overrides ---------------------------------------------------
    void set(const std::string &id, const std::string &key, const nlohmann::json &value);
    // Per-property reset: drop the user override only (preset still applies).
    void reset_property(const std::string &id, const std::string &key);
    // Per-element reset: drop every user override for the id.
    void reset_element(const std::string &id);
    // Global reset: drop every user override and return to the default preset.
    // User presets are kept (they are the user's saved work).
    void reset_all();
    // Ids carrying at least one user override.
    std::vector<std::string> overridden_ids() const;
    const StyleBag *user_bag(const std::string &id) const;

    // --- Presets ----------------------------------------------------------
    const std::string &active_preset() const { return m_active_preset; }
    // Returns false when the preset does not exist.
    bool set_active_preset(const std::string &name);
    std::vector<std::string> preset_names() const; // shipped first, then user
    bool is_shipped_preset(const std::string &name) const;
    const StylePreset *preset(const std::string &name) const;
    // Save the current user overrides as a named user preset (overwrites a
    // user preset of the same name; refuses a shipped name -> false).
    bool save_preset(const std::string &name);
    bool save_preset(const std::string &name, const StylePreset &preset);
    bool delete_preset(const std::string &name); // user presets only

    // --- Serialisation ----------------------------------------------------
    nlohmann::json  to_json() const;
    StyleLoadReport from_json(const nlohmann::json &doc);
    std::string     dump() const; // pretty JSON of to_json()
    StyleLoadReport parse(const std::string &text);

    // File I/O. Paths are UTF-8. save() creates the parent directory.
    StyleLoadReport load(const std::string &path);
    bool            save(const std::string &path, std::string *error = nullptr) const;
    // Export/import a standalone theme file (same schema; import merges the
    // file's presets and replaces the overrides + active preset).
    bool            export_theme(const std::string &path, std::string *error = nullptr) const;
    StyleLoadReport import_theme(const std::string &path);

    const StyleLoadReport &last_report() const { return m_last_report; }

    // --- Change notification ------------------------------------------------
    // Callback receives the element id that changed, or "*" when everything
    // may have (preset switch, reset all, import).
    using Listener = std::function<void(const std::string &id)>;
    int  subscribe(Listener listener);
    void unsubscribe(int token);

    // Every id ever passed to set()/resolve() while a window was adopted, so
    // the editor can offer "recently styled" targets. Registration is explicit.
    void register_id(const std::string &id, const wxString &display_name);
    const std::map<std::string, wxString> &known_ids() const { return m_known_ids; }

private:
    void                 notify(const std::string &id);
    static void          install_shipped_presets(std::map<std::string, StylePreset> &into);
    const nlohmann::json *lookup(const StyleBag *bag, const std::string &key) const;
    nlohmann::json        resolve_exact(const std::string &id, const std::string &key) const;

    std::string                          m_active_preset;
    std::map<std::string, StylePreset>   m_shipped;
    std::map<std::string, StylePreset>   m_user_presets;
    std::map<std::string, StyleBag>      m_elements;
    nlohmann::json                       m_unknown_top_level; // object
    StyleLoadReport                      m_last_report;
    std::map<int, Listener>              m_listeners;
    int                                  m_next_token { 1 };
    std::map<std::string, wxString>      m_known_ids;
};

// Colour helpers shared by the registry, the editor and the tests.
wxColour style_colour_from_json(const nlohmann::json &value, const wxColour &fallback);
std::string style_colour_to_string(const wxColour &colour);

// Font helper: apply the resolved typography of `bag` to a copy of `base`.
wxFont style_font_from_bag(const StyleBag &bag, const wxFont &base);

// --------------------------------------------------------------------------
// Process-wide facade + wxWindow adopter.
// --------------------------------------------------------------------------
class ElementStyle
{
public:
    // The one registry the live UI consults.
    static StyleRegistry &registry();

    // Where the registry persists (data_dir()/appearance/element-styles.json
    // once GUI_App has set it). Loading happens here; a missing file is fine.
    static void set_storage_dir(const std::string &dir);
    static const std::string &storage_dir();
    static std::string storage_file();
    // Persist the live registry to storage_file(); no-op without a dir.
    static bool save();

    // --- Paint-site hooks ----------------------------------------------------
    // Typography of `id` applied over `base`; `base` unchanged when nothing is
    // configured. Cheap enough to call per paint.
    static wxFont   font_for(const std::string &id, const wxFont &base);
    // role: StyleProp::foreground / background / highlight / border_color.
    static wxColour colour_for(const std::string &id, const char *role, const wxColour &base);
    // Numeric property (radius, padding, margin, border_width, letter_spacing,
    // line_height) or `base` when unset.
    static double   number_for(const std::string &id, const char *key, double base);

    // --- Adopter -----------------------------------------------------------
    // Register `window` under `id`: remembers the window's current font and
    // colours as the base, applies the resolved style now, re-applies on every
    // registry change, and wires the "Edit appearance..." context menu plus
    // Ctrl+Shift+E. Safe to call again (re-adopts under the new id).
    // wire_context_menu=false skips the attach hook for widgets that already
    // own a right-click menu and add the item themselves (tabs, object list).
    static void apply(wxWindow *window, const std::string &id, const wxString &display_name = wxString(),
                      bool wire_context_menu = true);
    // Stop styling a window (called automatically on wxEVT_DESTROY).
    static void release(wxWindow *window);
    // The id `window` (or its nearest adopted ancestor) was adopted under;
    // empty when none.
    static std::string element_id_of(const wxWindow *window);
    static wxString    display_name_of(const std::string &id);
    // Re-apply the style to every adopted window (after a theme flip, for
    // example, when the remembered base colours went stale).
    static void restyle_all();
    // Refresh the remembered base font/colours from the window's current state
    // (a widget that re-fonted itself after Label::rebuild_fonts calls this).
    static void rebase(wxWindow *window);

    // Hook the editor installs (AppearanceEditor::init): called by apply() so
    // every adopted window gets the "Edit appearance..." context menu, the
    // Shift+right-click path and Ctrl+Shift+E without this module depending on
    // the editor. Null (the default, and in tests) wires nothing.
    using AttachHook = std::function<void(wxWindow *, const std::string &)>;
    static void set_attach_hook(AttachHook hook);

private:
    struct Adopted;
    static std::map<wxWindow *, std::shared_ptr<Adopted>> &adopted();
    static void re_apply(wxWindow *window, Adopted &a);
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_Appearance_ElementStyle_hpp_
