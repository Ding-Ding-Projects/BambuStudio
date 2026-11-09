#pragma once

#include <set>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <wx/timer.h>

#include "slic3r/GUI/GUI_Utils.hpp"

class Button;
class CheckBox;
class Label;
class SearchField;
class wxScrolledWindow;
class wxBoxSizer;

namespace Slic3r::GUI {

// Multi-slice: slice the current plate once per chosen ink and save one sliced
// file per ink, so a PLA and an ABS version of the same model go to the
// printer's USB drive without slicing by hand twice. Every ink slot of the
// project takes the chosen ink for its turn; slot colours stay. The inks the
// project used before are put back when the run ends, however it ends.
class MultiSliceDialog : public DPIDialog
{
public:
    explicit MultiSliceDialog(wxWindow *parent);
    ~MultiSliceDialog() override;

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    enum class Status { Idle, Waiting, Slicing, Saved, Failed, Skipped };

    struct Ink
    {
        std::string preset;   // preset name, as the preset bundle knows it
        std::string type;     // "PLA", "ABS", ... (may be empty)
        CheckBox   *check  = nullptr;
        wxWindow   *row    = nullptr;
        Label      *status = nullptr;
        Status      state  = Status::Idle;
        std::string file;     // file written, once saved
    };

    void build();
    void filter_inks();
    void update_summary();
    void choose_destination();
    void set_destination(const boost::filesystem::path &folder, bool removable);
    void set_running(bool running);
    void show_status(Ink &ink, Status state, const wxString &detail = wxString());

    void start();
    void begin_next();
    void apply_ink(const std::string &preset);
    void restore_inks();
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
    wxScrolledWindow *m_list = nullptr;
    wxBoxSizer       *m_list_sizer = nullptr;
    Label            *m_summary = nullptr;
    Label            *m_destination = nullptr;
    Button           *m_change = nullptr;
    Button           *m_start = nullptr;
    Button           *m_close = nullptr;
    Button           *m_open = nullptr;
};

} // namespace Slic3r::GUI
