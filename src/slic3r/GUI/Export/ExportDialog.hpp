#ifndef slic3r_GUI_Export_ExportDialog_hpp_
#define slic3r_GUI_Export_ExportDialog_hpp_

// Material Design 3 "Export..." dialog shared by every exportable surface:
// picks a format (with lossless/lossy badges and the exact loss explanation),
// line ending, optional ZIP/7z wrapping with the complete 7-Zip option surface,
// and the output path. The engine behind it is wx-free (ExportEverything.hpp).

#include "ExportEverything.hpp"
#include "slic3r/GUI/GUI_Utils.hpp"

#include <filesystem>
#include <functional>
#include <vector>

#include <wx/string.h>

class Button;
class ComboBox;
class Label;
class LabeledCheckBox;
class SearchField;
class SpinInput;
class StaticBox;
class TextInput;
class wxDataViewListCtrl;
class wxScrolledWindow;
class wxSizer;
class wxWindow;

namespace Slic3r::GUI {

class ExportDialog final : public DPIDialog
{
public:
    ExportDialog(wxWindow *parent, Export::Dataset dataset);
    ~ExportDialog() override;

    // Show the dialog modally and write the export. Returns true when a file
    // (or archive) was written.
    static bool run(wxWindow *parent, Export::Dataset dataset);

    // Path written by the last successful export (empty when cancelled).
    const std::filesystem::path &written_path() const { return m_written_path; }

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    void on_sys_color_changed() override;

private:
    struct OptionRow
    {
        wxString                label;
        wxSizer                *sizer{nullptr};
        std::vector<wxWindow *> windows;
    };

    void create_ui();
    void apply_theme();
    void populate_formats();
    void apply_search();
    void update_format_details();
    void update_archive_panel();
    void update_hints();
    void update_output_path_extension();
    void update_export_enabled();
    void set_status(const wxString &message, bool error);

    Export::Format         selected_format() const;
    Export::ArchiveFormat  selected_archive() const;
    Export::ArchiveOptions archive_options() const;
    wxString               default_file_name() const;

    void on_browse(wxCommandEvent &event);
    void on_locate_seven_zip(wxCommandEvent &event);
    void on_export(wxCommandEvent &event);

    Label *add_option(wxWindow *parent, wxSizer *sizer, const wxString &label, wxWindow *control, const wxString &tooltip);

    Export::Dataset            m_dataset;
    std::vector<Export::Format> m_visible_formats;
    std::filesystem::path      m_written_path;
    Export::SevenZipLocation   m_seven_zip;

    wxScrolledWindow   *m_body{nullptr};
    Label              *m_title_label{nullptr};
    Label              *m_subtitle_label{nullptr};
    SearchField        *m_search_field{nullptr};

    StaticBox          *m_format_card{nullptr};
    wxDataViewListCtrl *m_format_list{nullptr};
    Label              *m_format_badge_label{nullptr};
    Label              *m_format_detail_label{nullptr};

    StaticBox          *m_text_card{nullptr};
    ComboBox           *m_line_ending_combo{nullptr};
    LabeledCheckBox    *m_header_check{nullptr};
    LabeledCheckBox    *m_bom_check{nullptr};
    Label              *m_encoding_label{nullptr};

    StaticBox          *m_archive_card{nullptr};
    ComboBox           *m_archive_combo{nullptr};
    Label              *m_archive_note_label{nullptr};
    wxSizer            *m_seven_zip_sizer{nullptr};
    ComboBox           *m_method_combo{nullptr};
    ComboBox           *m_level_combo{nullptr};
    ComboBox           *m_dictionary_combo{nullptr};
    SpinInput          *m_word_size_spin{nullptr};
    LabeledCheckBox    *m_solid_check{nullptr};
    ComboBox           *m_solid_block_combo{nullptr};
    SpinInput          *m_threads_spin{nullptr};
    TextInput          *m_split_input{nullptr};
    TextInput          *m_password_input{nullptr};
    TextInput          *m_password_confirm_input{nullptr};
    LabeledCheckBox    *m_encrypt_headers_check{nullptr};
    Label              *m_encryption_warning_label{nullptr};
    Label              *m_hints_label{nullptr};
    Label              *m_seven_zip_status_label{nullptr};
    Button             *m_locate_seven_zip_button{nullptr};

    StaticBox          *m_output_card{nullptr};
    TextInput          *m_path_input{nullptr};
    Button             *m_browse_button{nullptr};

    Label              *m_status_label{nullptr};
    Button             *m_cancel_button{nullptr};
    Button             *m_export_button{nullptr};

    std::vector<OptionRow> m_option_rows;
    std::vector<Label *>   m_option_labels;
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_Export_ExportDialog_hpp_
