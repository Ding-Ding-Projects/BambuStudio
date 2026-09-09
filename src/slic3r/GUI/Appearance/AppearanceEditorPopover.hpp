#ifndef slic3r_GUI_Appearance_AppearanceEditorPopover_hpp_
#define slic3r_GUI_Appearance_AppearanceEditorPopover_hpp_

// Per-element appearance editor: a NON-MODAL Material card anchored beside the
// exact element being edited. It tracks the anchor while open (window moves,
// relayouts), re-places itself against the display edges with the same
// arithmetic as the Material menus (MD3::Menu::place_root), never detaches
// from its anchor, and returns focus to the anchor when it closes.
//
// Sections (tabs inside the card): Typography, Colours, Shape & spacing,
// Presets. Every change writes the registry (ElementStyle) at once, so the
// anchor and every other adopted window re-style live, and the file under
// data_dir()/appearance is saved.
//
// Opening routes:
//   * "Edit appearance..." in every Material context menu (via
//     AppearanceEditor::append_edit_appearance_item, or automatically for any
//     menu whose owner window was adopted with ElementStyle::apply);
//   * Shift+right-click on an adopted element opens the editor directly;
//   * Ctrl+Shift+E on the focused control (MainFrame accelerator and the
//     per-window CHAR_HOOK the adopter installs).

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <wx/frame.h>
#include <wx/timer.h>
#include <wx/weakref.h>

class wxMenu;
class wxMenuItem;
class wxSimplebook;
class wxSpinCtrlDouble;
class Button;
class CheckBox;
class ComboBox;
class Label;
class SearchField;
class SpinInput;

namespace Slic3r { namespace GUI {

class ListBox;

class AppearanceEditorPopover : public wxFrame
{
public:
    // Only one editor is open at a time; opening for another element re-targets
    // the existing card. Returns the (possibly reused) editor.
    static AppearanceEditorPopover *open_for(wxWindow *anchor, const std::string &element_id);
    static AppearanceEditorPopover *current();
    static void                     close_current();

    const std::string &element_id() const { return m_id; }
    wxWindow *         anchor() const { return m_anchor.get(); }

    // Re-target the open card to another element (rebuilds the controls).
    void retarget(wxWindow *anchor, const std::string &element_id);

    // Select a section by index: 0 Typography, 1 Colours, 2 Shape, 3 Presets.
    void show_section(int index);

private:
    AppearanceEditorPopover(wxWindow *anchor, const std::string &element_id);
    ~AppearanceEditorPopover() override;

    void build();
    void build_typography(wxWindow *page);
    void build_colours(wxWindow *page);
    void build_shape(wxWindow *page);
    void build_presets(wxWindow *page);
    void refresh_from_registry();
    void refresh_font_list();
    void refresh_preset_list();
    void refresh_reset_buttons();

    void write_number(const char *key, double value);
    void write_bool(const char *key, bool value);
    void write_string(const char *key, const std::string &value);
    void reset_property(const char *key);
    void persist();

    void place();
    void on_tick(wxTimerEvent &);
    void on_char_hook(wxKeyEvent &);
    void close_and_return_focus();
    void paint(wxPaintEvent &);

    Button *make_reset(wxWindow *parent, const char *key, const wxString &what);
    Button *make_swatch(wxWindow *parent, const char *key, const wxString &what);

    std::string         m_id;
    wxWeakRef<wxWindow> m_anchor;
    wxRect              m_last_anchor_rect;
    wxTimer             m_tick;
    int                 m_registry_token { 0 };
    bool                m_loading { false }; // suppress control -> registry echo
    bool                m_save_pending { false };

    // Chrome
    Label *               m_title { nullptr };
    Label *               m_subtitle { nullptr };
    std::vector<Button *> m_section_buttons;
    wxSimplebook *        m_book { nullptr };
    int                   m_section { 0 };

    // Typography
    SearchField *     m_font_search { nullptr };
    ListBox *         m_font_list { nullptr };
    std::vector<wxString> m_all_fonts;   // display rows (bundled first)
    std::vector<wxString> m_font_faces;  // face names parallel to m_all_fonts
    std::vector<int>      m_font_visible; // indices into m_all_fonts
    Label *           m_font_preview { nullptr };
    wxSpinCtrlDouble *m_size { nullptr };
    ComboBox *        m_weight { nullptr };
    CheckBox *        m_italic { nullptr };
    CheckBox *        m_underline { nullptr };
    CheckBox *        m_strike { nullptr };
    wxSpinCtrlDouble *m_letter_spacing { nullptr };
    wxSpinCtrlDouble *m_line_height { nullptr };

    // Colours
    std::map<std::string, Button *> m_swatches; // key -> swatch button

    // Shape
    SpinInput *m_border_width { nullptr };
    SpinInput *m_radius { nullptr };
    SpinInput *m_padding { nullptr };
    SpinInput *m_margin { nullptr };

    // Presets
    SearchField *            m_preset_search { nullptr };
    ListBox *                m_preset_list { nullptr };
    std::vector<std::string> m_preset_visible;
    Button *                 m_preset_apply { nullptr };
    Button *                 m_preset_delete { nullptr };
    Label *                  m_preset_active { nullptr };

    std::map<std::string, Button *> m_reset_buttons; // key -> per-property reset

    static AppearanceEditorPopover *s_current;
};

// Free-function entry points used by menus, tab strips and MainFrame.
namespace AppearanceEditor {

// Storage + hooks. Call once at startup after data_dir() is known.
void init(const std::string &storage_dir);

// "Ctrl+Shift+E" (platform notation), shown in menus and the shortcuts dialog.
wxString shortcut_text();

// Open the editor beside `anchor` for `element_id`.
void open_for(wxWindow *anchor, const std::string &element_id);
// Open for the focused control (its adopted id, or a generic id derived from
// the control's name). Returns false when nothing sensible has focus.
bool open_for_focused();

// Append a separator (when the menu is non-empty) and an "Edit appearance..."
// item bound to open the editor for `element_id`, anchored to `anchor` (or to
// the menu's invoking window when null). `label` overrides the item text, e.g.
// "Edit tab appearance...". Idempotent: a menu that already carries the item
// gets it re-bound, not duplicated. Returns the item.
wxMenuItem *append_edit_appearance_item(wxMenu &menu, const std::string &element_id,
                                        wxWindow *anchor = nullptr, const wxString &label = wxString());
// Remove the item this helper appended (used by the automatic per-popup path).
void remove_edit_appearance_item(wxMenu &menu);
// The fixed id the item uses (so callers can test for it).
int edit_appearance_item_id();

// Wire right-click (context menu with the item), Shift+right-click (editor
// directly) and Ctrl+Shift+E onto a window for `element_id`. ElementStyle::apply
// calls this through the hook installed by init().
void attach(wxWindow *window, const std::string &element_id);

} // namespace AppearanceEditor

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_Appearance_AppearanceEditorPopover_hpp_
