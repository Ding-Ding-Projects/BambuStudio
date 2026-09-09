#ifndef slic3r_GUI_CommandPalette_hpp_
#define slic3r_GUI_CommandPalette_hpp_

#include <cstdint>
#include <functional>
#include <vector>

#include <wx/dialog.h>

#include "CommandPaletteIndex.hpp"

class SearchField;
class Button;
class wxScrolledWindow;
class wxBoxSizer;
class wxPanel;

namespace Slic3r::GUI {

class MainFrame;

// Ctrl+Shift+F command palette: one searchable surface over everything the
// app can do. Every entry carries a Material Symbols icon, a title, and a
// description; entries that ARE a setting render rich inline controls
// (theme light/dark, density, accent swatches) instead of a bare action row.
//
// Sources (see CommandPaletteIndex.hpp for the data half and the guard that
// keeps it complete):
//   * every enabled menubar command (path-titled, e.g. "File / Import"), run
//     by posting its wxEVT_MENU to the frame;
//   * every workspace tab (Home / Prepare / Preview / Device / ...) that the
//     current frame actually has a page for;
//   * every Preferences setting — selecting one teleports: Preferences opens
//     on the owning page, scrolls the row into view, focuses its control and
//     flashes it;
//   * every documentation article under docs/features (title only; opens the
//     rendered article);
//   * quick settings rows with live controls (theme, density, accent seed);
//   * a handful of feature landmarks (Preferences, parameter search).
//
// Search runs through the shared SearchField pill, so plain text is the
// default and the `.*` toggle plus the full regex builder are one click away
// — the palette is itself a search bar and follows the same rules as every
// other one. Keyboard: type to filter, Up/Down select, Enter runs, Esc closes.
//
// Size: a bounded card (default) or the full main-window area, toggled by
// the button beside the search pill and persisted in AppConfig under
// PaletteIndex::kPaletteSizeKey.
class CommandPalette final : public wxDialog
{
public:
    explicit CommandPalette(MainFrame *frame);

    // Show over the frame; returns after the palette closes.
    static void ShowPalette(MainFrame *frame);

    PaletteIndex::PaletteSize size_choice() const { return m_size; }

private:
    // Esc/close routing: EndModal() is only valid while the modal loop runs.
    void dismiss() { if (IsModal()) EndModal(wxID_CANCEL); else Close(); }

    enum class Rich : std::uint8_t { None, Theme, Density, Accent };

    struct Entry
    {
        std::uint32_t         glyph;
        wxString              title;
        wxString              desc;
        std::function<void()> run;   // unused for rich rows
        Rich                  rich { Rich::None };
    };

    void collect_entries();
    void rebuild_rows();
    void select_row(int index);
    void run_selected();
    wxPanel *make_row(const Entry &entry, int index);
    void add_rich_controls(wxPanel *row, wxBoxSizer *sizer, Rich rich);
    void apply_size(PaletteIndex::PaletteSize size, bool persist);

    MainFrame          *m_frame { nullptr };
    SearchField        *m_search { nullptr };
    Button             *m_size_button { nullptr };
    wxScrolledWindow   *m_list { nullptr };
    std::vector<Entry>  m_entries;
    std::vector<int>    m_visible;   // entry indices currently shown
    std::vector<wxPanel *> m_rows;   // row panels parallel to m_visible
    int                 m_selected { -1 };
    PaletteIndex::PaletteSize m_size { PaletteIndex::PaletteSize::Card };
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_CommandPalette_hpp_
