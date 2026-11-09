#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <wx/timer.h>

#include "slic3r/GUI/Widgets/MD3Dialog.hpp"

class Button;
class CheckBox; // the kit widget (::CheckBox), not GUI::CheckBox
class Label;
class ProgressBar;
class SearchField;
class StaticBox;
class wxScrolledWindow;
class wxBoxSizer;
class wxStaticBitmap;

namespace Slic3r::GUI {

class InkRow; // an ink's row in the list (defined in MultiSliceDialog.cpp)
class Tag;

// Multi-slice: slice the current plate once per chosen ink and save one sliced
// file per ink, so a PLA and an ABS version of the same model go to the
// printer's USB drive without slicing by hand twice. Every ink slot of the
// project takes the chosen ink for its turn; slot colours stay. The inks the
// project used before are put back when the run ends, however it ends.
//
// The dialog is built on the shared MD3Dialog shell: an intro, a search field,
// a well that holds the ink rows (each row a rounded fill with a type tag and a
// status tag), the summary line, a destination well and the footer actions.
class MultiSliceDialog : public MD3Dialog
{
public:
    explicit MultiSliceDialog(wxWindow *parent);
    ~MultiSliceDialog() override;

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    // The header close runs the same path as the window's own close, so a run in
    // progress is stopped after the ink being sliced and the inks are restored.
    void OnHeaderClose() override;

private:
    enum class Status { Idle, Waiting, Slicing, Saved, Failed, Skipped };

    struct Ink
    {
        std::string preset;   // preset name, as the preset bundle knows it
        std::string type;     // "PLA", "ABS", ... (may be empty)
        ::CheckBox *check  = nullptr;
        InkRow     *row    = nullptr;
        Label      *name   = nullptr;
        Tag        *type_tag = nullptr;
        Tag        *status = nullptr;       // status tag, hidden while idle
        Label      *detail = nullptr;       // failure detail line under the name
        wxBoxSizer *status_col = nullptr;   // fixed-width column that holds the status tag
        Status      state  = Status::Idle;
        std::string file;     // file written, once saved
    };

    void build();
    void build_list_well(wxBoxSizer *content);
    void build_ink_row(Ink &ink);
    void build_destination_well(wxBoxSizer *content);
    void build_footer();
    void filter_inks();
    void show_empty_state(bool show, const wxString &message);
    void update_summary();
    void set_summary(const wxString &text, uint32_t glyph, MD3::Role glyph_role, MD3::Role text_role);
    void update_rows();
    void update_status_column();
    void choose_destination();
    void set_destination(const boost::filesystem::path &folder, bool removable);
    void set_running(bool running);
    void show_status(Ink &ink, Status state, const wxString &detail = wxString());

    void start();
    void begin_next();
    void apply_ink(const std::string &preset);
    void restore_inks();
    void use_slot_inks();
    void on_tick(wxTimerEvent &);
    void finish_variant(bool sliced);
    void finish_run();

    std::vector<Ink>        m_inks;
    std::vector<size_t>     m_queue;      // indexes into m_inks, in run order
    size_t                  m_next = 0;   // next m_queue position to start
    Ink                    *m_current = nullptr;
    std::vector<std::string> m_original;  // ink preset per slot before the run
    std::set<std::string>   m_taken;      // file names used in this run
    std::string             m_base;       // project file name without extension
    boost::filesystem::path m_folder;
    bool                    m_removable = false;
    bool                    m_running = false;
    bool                    m_cancel = false;

    // Slice watch for the current ink: the run accepts a result only after the
    // plate's result has been made invalid since the ink was applied, so the
    // file never carries the previous ink's G-code.
    wxTimer m_timer;
    size_t  m_epoch = 0;
    bool    m_seen_slicing = false;
    int     m_ticks = 0;

    SearchField      *m_search = nullptr;
    StaticBox        *m_list_well = nullptr;
    wxScrolledWindow *m_list = nullptr;
    wxBoxSizer       *m_list_sizer = nullptr;
    wxBoxSizer       *m_header_status_col = nullptr;
    wxStaticBitmap   *m_empty_icon = nullptr;
    Label            *m_empty_text = nullptr;
    wxStaticBitmap   *m_summary_icon = nullptr;
    Label            *m_summary = nullptr;
    ProgressBar      *m_progress = nullptr;
    StaticBox        *m_dest_well = nullptr;
    Tag              *m_usb_tag = nullptr;
    Label            *m_destination = nullptr;
    Label            *m_destination_empty = nullptr;
    Button           *m_change = nullptr;
    Button           *m_start = nullptr;
    Button           *m_close = nullptr;
    Button           *m_open = nullptr;
};

} // namespace Slic3r::GUI
