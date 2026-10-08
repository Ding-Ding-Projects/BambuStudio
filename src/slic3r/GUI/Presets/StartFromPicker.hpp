#ifndef slic3r_GUI_Presets_StartFromPicker_hpp_
#define slic3r_GUI_Presets_StartFromPicker_hpp_

// "Start from" picker: the first step of an editor that would otherwise open
// empty. It lists the presets BlankEditorPresets built for that editor (the
// shipped defaults, the person's saved settings, an empty start, and any
// shipped templates), filters them with the shared search field and its regex
// builder, and states for the highlighted preset exactly what it creates,
// every setting it sets and every setting it leaves out, before anything is
// applied. Keyboard: arrows move, Enter starts from the highlighted preset,
// Escape cancels; double-click also starts.

#include "BlankEditorPresets.hpp"

#include "slic3r/GUI/Widgets/MD3Dialog.hpp"

#include <wx/string.h>

#include <cstddef>
#include <vector>

class Button;
class Label;
class SearchField;
class TextAreaEditor;

namespace Slic3r { namespace GUI {

class ListBox;

// What the preset creates, sets and leaves out, one statement per line.
wxString describe_preset(const BlankEditorPresets::Preset &preset);
// Every setting the preset sets on one line ("Theme: Light; Density: ..."),
// or a plain "Sets nothing." for an empty start.
wxString preset_settings_line(const BlankEditorPresets::Preset &preset);

class StartFromPicker final : public MD3Dialog
{
public:
    StartFromPicker(wxWindow *parent, const wxString &title, const wxString &subtitle,
                    std::vector<BlankEditorPresets::Preset> presets);

    // The preset chosen when ShowModal() returned wxID_OK; nullptr otherwise.
    const BlankEditorPresets::Preset *chosen() const;

private:
    void filter();
    void show_detail();
    void accept();
    int  highlighted() const; // index into m_presets, or -1

    std::vector<BlankEditorPresets::Preset> m_presets;
    std::vector<std::size_t>                m_visible; // list row -> preset index
    int                                     m_chosen { -1 };
    SearchField                            *m_search { nullptr };
    ListBox                                *m_list { nullptr };
    Label                                  *m_no_match { nullptr };
    TextAreaEditor                         *m_detail { nullptr };
    Button                                 *m_start { nullptr };
};

} } // namespace Slic3r::GUI

#endif // slic3r_GUI_Presets_StartFromPicker_hpp_
