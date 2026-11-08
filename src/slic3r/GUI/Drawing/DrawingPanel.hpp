#ifndef slic3r_GUI_Drawing_DrawingPanel_hpp_
#define slic3r_GUI_Drawing_DrawingPanel_hpp_

// File > Drawing board: draw on a capture of the current plate with seven
// pens and an eraser, undo and redo, versions kept on this computer, export as
// SVG or PNG, and Clear behind the two-key confirmation. Drawings are saved
// per plate in the data folder and never uploaded. The rules live in the
// wx-free DrawingModel.

#include "DrawingModel.hpp"

#include <wx/bitmap.h>
#include <wx/panel.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class Button;
class Label;
class MD3DataViewListCtrl;
class SpinInput;

namespace Slic3r { namespace GUI { namespace Drawing {

class BoardCanvas;

class DrawingPanel : public wxPanel
{
public:
    explicit DrawingPanel(wxWindow *parent);
    ~DrawingPanel() override;
    static void open();

private:
    friend class BoardCanvas;
    void build_toolbar(wxSizer *sizer);
    void set_pen(Pen pen);
    void set_color(std::uint32_t rgb);
    void pick_custom_color();
    // Takes a new picture of the plate; false when the 3D view could not draw one.
    bool capture_plate(bool tell_when_unavailable);
    void choose_plate();
    void switch_plate(int plate);
    void load();
    void save();
    void changed(); // after a stroke, an erase, undo, redo, restore or clear
    void undo();
    void redo();
    void keep_version(const std::string &label);
    void restore_version(int index);
    void clear_all();
    void export_drawing();
    void refresh_status();
    void refresh_versions();
    void refresh_tools();

    int                       m_plate = 1;
    std::filesystem::path     m_folder;
    Document                  m_doc;
    std::vector<Version>      m_versions;
    History                   m_history;
    wxBitmap                  m_background;
    Pen                       m_pen      = Pen::Marker;
    std::uint32_t             m_color    = 0x00AE42;
    float                     m_widths[int(Pen::Count)];
    float                     m_opacity[int(Pen::Count)];
    float                     m_smoothing = 0.4f;
    std::vector<std::uint32_t> m_recent;
    std::int64_t              m_saved_at = 0;

    BoardCanvas              *m_canvas   = nullptr;
    Label                    *m_title    = nullptr;
    Label                    *m_status   = nullptr;
    Button                   *m_plate_button = nullptr;
    std::vector<Button *>     m_pen_buttons;
    wxSizer                  *m_swatches = nullptr;
    wxWindow                 *m_swatch_parent = nullptr;
    SpinInput                *m_width_input = nullptr;
    SpinInput                *m_opacity_input = nullptr;
    SpinInput                *m_smoothing_input = nullptr;
    Button                   *m_undo = nullptr, *m_redo = nullptr;
    MD3DataViewListCtrl      *m_version_list = nullptr;
};

}}} // namespace Slic3r::GUI::Drawing

#endif // slic3r_GUI_Drawing_DrawingPanel_hpp_
