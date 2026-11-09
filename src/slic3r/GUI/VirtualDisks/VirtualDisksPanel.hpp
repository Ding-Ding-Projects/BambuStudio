#ifndef slic3r_GUI_VirtualDisks_VirtualDisksPanel_hpp_
#define slic3r_GUI_VirtualDisks_VirtualDisksPanel_hpp_

// File > Virtual disks: every VHD, VHDX, VHD Set and ISO the person added,
// read from the files themselves, with what Windows has attached and where,
// chain tests, clashing disk IDs, and every operation: create, attach (read-
// write or read-only), detach, grow, compact, make a child, merge into the
// parent, reconnect a moved parent, a new disk ID and Quick pack. Each
// operation shows its steps (Copy as PowerShell, Copy as DiskPart) and runs
// with one administrator prompt; history records how each one ended. Nothing
// here touches a disk the person did not choose. The rules live in
// DiskImage, DiskPlan and DiskSystem.

#include "DiskImage.hpp"
#include "DiskPlan.hpp"
#include "DiskSystem.hpp"

#include <wx/panel.h>
#include <wx/timer.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Button;
class Label;
class MD3DataViewListCtrl;
class TextInput;
class wxBoxSizer;
class wxWrapSizer;

namespace Slic3r { namespace GUI { namespace VirtualDisks {

class VirtualDisksPanel : public wxPanel
{
public:
    explicit VirtualDisksPanel(wxWindow *parent);
    ~VirtualDisksPanel() override;
    static void open();

private:
    struct Row
    {
        DiskInfo                 info;
        Attachment               attach;
        ChainTest                chain;
        int                      parent = -1;
        std::vector<std::size_t> children;
        int                      clash_with = -1; // another row with the same disk ID
    };

    void build();
    void load();
    void save();
    void refresh();         // reads every disk again and asks Windows what is attached
    void refresh_list();    // the filter chips, the search and the rows shown
    void relayout_details();
    void refresh_details(); // the selected disk
    const Row *selected() const;
    std::vector<DiskInfo> infos() const;

    void add_files(const std::vector<std::string> &paths);
    void add_file_dialog();
    void add_folder_dialog();
    void remove_selected();
    void show_history();
    void create_disk(bool child_of_selected);
    void quick_pack();
    void copy_steps(wxWindow *anchor);

    void op_attach(bool read_only);
    void op_detach();
    void op_grow();
    void op_compact();
    void op_merge();
    void op_reconnect();
    void op_new_id();
    void op_show_in_folder();

    // Runs a planned operation with administrator permission; one at a time.
    void run(const Script &script, const std::string &disk, std::function<void(const RunOutcome &)> after = {});
    void set_running(bool running, const wxString &what = wxString());

    Library                         m_library;
    std::vector<Row>                m_rows;
    std::vector<int>                m_visible;
    std::map<std::string, DiskInfo> m_last_read; // the last good read of each disk, for one attached read-write
    int                             m_filter = 0; // All, Attached, In chains, Problems
    std::string                     m_selected;   // path
    Routes                          m_routes;
    bool                            m_attach_known = false;
    bool                            m_running      = false;
    std::shared_ptr<std::atomic<bool>> m_cancel;
    std::shared_ptr<bool>              m_alive;
    wxTimer                            m_reread;           // reads again after an operation ends
    int                                m_rereads_left = 0;

    Label                     *m_routes_label = nullptr;
    Label                     *m_system_label = nullptr;
    TextInput                 *m_search       = nullptr;
    std::vector<Button *>      m_chips;
    MD3DataViewListCtrl       *m_list         = nullptr;
    Label                     *m_title        = nullptr;
    Label                     *m_state        = nullptr;
    Label                     *m_facts        = nullptr;
    Label                     *m_problem      = nullptr;
    wxScrolledWindow          *m_details_pane = nullptr;
    std::vector<Button *>      m_actions;
    wxWindow                  *m_running_bar  = nullptr;
    Label                     *m_running_what = nullptr;
    Label                     *m_running_step = nullptr;
    Button                    *m_cancel_button = nullptr;
};

}}} // namespace Slic3r::GUI::VirtualDisks

#endif // slic3r_GUI_VirtualDisks_VirtualDisksPanel_hpp_
