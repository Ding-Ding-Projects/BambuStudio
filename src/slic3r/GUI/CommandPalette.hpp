#ifndef slic3r_GUI_CommandPalette_hpp_
#define slic3r_GUI_CommandPalette_hpp_

#include <cstdint>
#include <functional>
#include <vector>

#include <wx/dialog.h>

class SearchField;
class wxScrolledWindow;
class wxBoxSizer;
class wxPanel;

namespace Slic3r::GUI {

class MainFrame;

// Ctrl+F command palette: one searchable surface over everything the app can
// do. Every entry carries a Material Symbols icon, a title, and a
// description; entries that ARE a setting render rich inline controls
// (theme light/dark, density, accent swatches) instead of a bare action row.
//
// Sources:
//   * every enabled menubar command (path-titled, e.g. "File / Import"), run
//     by posting its wxEVT_MENU to the frame;
//   * main navigation targets (Home / Prepare / Preview / Device / ...);
//   * quick settings rows with live controls (theme, density, accent seed);
//   * a handful of feature landmarks (Preferences, Version history, Config
//     profiles & backup, parameter search).
//
// Search runs through the shared SearchField pill, so plain text is the
// default and the `.*` toggle plus the full regex builder are one click away
// — the palette is itself a search bar and follows the same rules as every
// other one. Keyboard: type to filter, Up/Down select, Enter runs, Esc closes.
class CommandPalette final : public wxDialog
{
public:
    explicit CommandPalette(MainFrame *frame);

    // Show centred over the frame; returns after the palette closes.
    static void ShowPalette(MainFrame *frame);

private:
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

    MainFrame          *m_frame { nullptr };
    SearchField        *m_search { nullptr };
    wxScrolledWindow   *m_list { nullptr };
    std::vector<Entry>  m_entries;
    std::vector<int>    m_visible;   // entry indices currently shown
    std::vector<wxPanel *> m_rows;   // row panels parallel to m_visible
    int                 m_selected { -1 };
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_CommandPalette_hpp_
