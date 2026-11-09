#ifndef slic3r_GUI_Base64Toolkit_Base64ToolkitPanel_hpp_
#define slic3r_GUI_Base64Toolkit_Base64ToolkitPanel_hpp_

// File > Base64 toolkit: text, whole files beside the originals, data: URLs and
// a history of names, sizes and hashes. Everything runs on this computer;
// nothing decoded is ever run or opened by itself. The decisions live in the
// wx-free Base64Codec, Base64Files and Base64Workbench modules.

#include "Base64Codec.hpp"
#include "Base64Files.hpp"
#include "Base64Workbench.hpp"

#include <wx/panel.h>
#include <wx/timer.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class wxBoxSizer;
class wxSimplebook;
class wxSizer;
class wxStaticBitmap;
class wxWindow;
class Button;
class Label;
class MD3DataViewListCtrl;
class SpinInput;
class StaticBox;
class SwitchButton;
class TextArea;
class TextInput;

namespace Slic3r { namespace GUI {

class TextTabbar;

class Base64ToolkitPanel : public wxPanel
{
public:
    explicit Base64ToolkitPanel(wxWindow *parent);
    ~Base64ToolkitPanel() override;

    enum Tab { TabText = 0, TabFiles = 1, TabDataUrl = 2, TabHistory = 3 };
    void select_tab(int tab);
    // Queues files for encoding or decoding beside the originals and shows the Files tab.
    void queue_files(const std::vector<std::filesystem::path> &paths, bool encode);

private:
    struct Choice
    {
        std::vector<std::pair<int, Button *>> buttons;
        int                                   value = 0;
    };
    struct FileJob;
    struct FileQueue;

    // ---- building
    wxWindow *build_text_page(wxWindow *parent);
    wxWindow *build_files_page(wxWindow *parent);
    wxWindow *build_url_page(wxWindow *parent);
    wxWindow *build_history_page(wxWindow *parent);
    Button   *make_button(wxWindow *parent, const wxString &label, std::uint32_t glyph, bool tonal = false);
    Button   *make_icon_button(wxWindow *parent, std::uint32_t glyph, const wxString &tip);
    Label    *make_label(wxWindow *parent, const wxFont &font, const wxString &text, bool muted = false, bool wrap = false);
    void      add_choice(wxWindow *parent, wxSizer *row, Choice &choice, const wxString &caption,
                         const std::vector<std::pair<int, wxString>> &items, std::function<void(int)> changed);
    void      set_choice(Choice &choice, int value);
    void      set_status(const wxString &message);
    void      refresh_tab_labels();

    // ---- Text tab
    void schedule_convert();
    void convert_now();
    void show_outcome();
    void apply_options_to_controls();
    void save_options() const;
    void set_input(const wxString &text);
    void swap_sides();
    void clear_text();
    void paste_input();
    void open_input_file();
    void load_input_file(const std::filesystem::path &path);
    void copy_output();
    void save_output();
    void save_bytes();
    void copy_as_data_url();
    void export_record();
    void send_to_url_tab();
    void try_lenient();
    void remove_bad_character();
    void record_text(const std::string &name);
    void undo();
    void redo();
    void apply_state(const Base64Toolkit::TextState &state);
    void refresh_undo_buttons();

    // ---- Files tab
    void choose_files(bool encode);
    void ensure_worker();
    void stop_worker();
    void poll_jobs();
    void pause_all(bool pause);
    void cancel_all();
    // One file alone: the selected row. A paused file waits where it is while
    // the others go on; a cancelled file leaves nothing behind.
    void pause_selected(bool pause);
    void cancel_selected();
    void show_jobs_menu(const wxPoint &screen);
    void refresh_selection_buttons();
    void clear_finished();
    void show_selected_in_folder();
    static void worker_loop(std::shared_ptr<FileQueue> queue);

    // ---- data: URL tab
    void url_set_mode(int mode);
    void url_from_file();
    void url_from_text();
    void url_rebuild();
    void url_read();
    void url_show_preview(int kind);
    void url_save_bytes();
    void url_copy();
    void url_save_text();
    void record_built_url(const std::string &mime, std::size_t url_size);
    wxWindow *page_at(int tab) const;

    // ---- History tab
    void refresh_history();
    void export_history();
    void clear_history();
    void set_keep_payloads(bool keep);
    void restore_history_row(int row);
    void append_history(Base64Toolkit::HistoryEntry entry);

    // ---- shared widgets
    TextTabbar   *m_tabs   = nullptr;
    wxSimplebook *m_book   = nullptr;
    Label        *m_status = nullptr;
    int           m_tab    = TabText;
    wxString      m_tab_labels;

    // ---- Text tab state and widgets
    Base64Toolkit::TextOptions m_options;
    Base64Toolkit::TextOutcome m_outcome;
    std::string                m_input_u8; // the input the outcome was computed from
    Base64Toolkit::UndoStack   m_undo;
    bool                       m_applying_state = false;
    wxTimer                    m_convert_timer;
    Choice     m_direction, m_alphabet, m_lines, m_padding, m_decoder, m_encoding;
    SpinInput *m_width        = nullptr;
    Label     *m_in_title     = nullptr;
    Label     *m_in_counts    = nullptr;
    Label     *m_out_title    = nullptr;
    Label     *m_out_counts   = nullptr;
    TextArea  *m_input        = nullptr;
    TextArea  *m_output       = nullptr;
    Label     *m_output_note  = nullptr;
    Label     *m_bytes_title  = nullptr;
    Label     *m_bytes_hex    = nullptr;
    wxWindow  *m_check_icon   = nullptr;
    Label     *m_check_text   = nullptr;
    Label     *m_check_hash   = nullptr;
    StaticBox *m_problem_card = nullptr;
    Label     *m_problem_title  = nullptr;
    Label     *m_problem_where  = nullptr;
    Label     *m_problem_before = nullptr;
    Label     *m_problem_at     = nullptr;
    Label     *m_problem_after  = nullptr;
    Label     *m_problem_kind   = nullptr;
    Label     *m_problem_explain = nullptr;
    Button    *m_try_lenient     = nullptr;
    Button    *m_remove_bad      = nullptr;
    Button    *m_use_utf8        = nullptr;
    Button    *m_save_bytes      = nullptr;
    std::vector<Button *> m_output_actions;
    wxWindow  *m_text_page = nullptr;

    // ---- Files tab
    std::shared_ptr<FileQueue> m_queue;
    std::thread                m_worker;
    wxTimer                    m_job_timer;
    MD3DataViewListCtrl       *m_jobs        = nullptr;
    Button                    *m_pause       = nullptr;
    Button                    *m_cancel      = nullptr;
    Button                    *m_pause_one   = nullptr;
    Button                    *m_cancel_one  = nullptr;
    Label                     *m_files_rules = nullptr;
    Label                     *m_files_empty = nullptr;

    // ---- data: URL tab
    Choice          m_url_mode;
    Choice          m_url_preview;
    wxSimplebook   *m_url_book       = nullptr;
    TextInput      *m_url_mime       = nullptr;
    TextInput      *m_url_charset    = nullptr;
    Label          *m_url_source     = nullptr;
    TextArea       *m_url_built      = nullptr;
    Label          *m_url_built_info = nullptr;
    TextArea       *m_url_input      = nullptr;
    Label          *m_url_details    = nullptr;
    Label          *m_url_signature  = nullptr;
    Label          *m_url_image_info = nullptr;
    wxStaticBitmap *m_url_image      = nullptr;
    TextArea       *m_url_preview_text = nullptr;
    Label          *m_url_preview_note = nullptr;
    Button         *m_url_save        = nullptr;
    wxTimer         m_url_timer;
    Base64Toolkit::Bytes m_url_source_bytes;
    std::string          m_url_source_name;
    Base64Toolkit::DataUrl m_url_parsed;
    bool                   m_url_parsed_ok = false;
    std::string            m_url_last_read; // digest and type of the last URL recorded in the history

    // ---- History tab
    Base64Toolkit::History m_history;
    MD3DataViewListCtrl   *m_history_list = nullptr;
    SwitchButton          *m_keep_payloads = nullptr;
    Button                *m_undo_button = nullptr;
    Button                *m_redo_button = nullptr;
    std::vector<int>       m_history_rows; // table row -> entry index, newest first
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_Base64Toolkit_Base64ToolkitPanel_hpp_
