#include "ExportDialog.hpp"

#include "ExportEverything.hpp"

#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/NotificationManager.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/LabeledCheckBox.hpp"
#include "slic3r/GUI/Widgets/MD3DialogChrome.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"
#include "slic3r/GUI/Widgets/SearchField.hpp"
#include "slic3r/GUI/Widgets/SpinInput.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/GUI/Widgets/StaticBox.hpp"
#include "slic3r/GUI/Widgets/TextInput.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/libslic3r.h"

#include <wx/dataview.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/variant.h>

namespace Slic3r::GUI {

using namespace Export;

namespace {

constexpr const char *CONFIG_SECTION    = "export_everything";
constexpr const char *CONFIG_LAST_DIR   = "last_dir";
constexpr const char *CONFIG_SEVEN_ZIP  = "seven_zip_path";

const std::vector<unsigned> DICTIONARY_CHOICES{0, 1, 4, 16, 32, 64, 128, 256, 512, 1024};
const std::vector<unsigned> SOLID_BLOCK_CHOICES{0, 1, 4, 16, 64, 256, 1024, 4096};
const std::vector<SevenZipMethod> METHOD_CHOICES{SevenZipMethod::LZMA2, SevenZipMethod::LZMA, SevenZipMethod::PPMd,
                                                 SevenZipMethod::BZip2, SevenZipMethod::Deflate};
const std::vector<SevenZipLevel> LEVEL_CHOICES{SevenZipLevel::Store, SevenZipLevel::Fastest, SevenZipLevel::Fast,
                                               SevenZipLevel::Normal, SevenZipLevel::Maximum, SevenZipLevel::Ultra};

StateColor filled_button_background()
{
    return StateColor(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Disabled),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Primary), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Primary), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Primary), StateColor::Normal));
}

StateColor filled_button_text()
{
    return StateColor(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnSurfaceVariant), StateColor::Disabled),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnPrimary), StateColor::Normal));
}

StateColor outlined_button_background()
{
    return StateColor(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Surface), StateColor::Normal));
}

wxString level_name(SevenZipLevel level)
{
    switch (level) {
    case SevenZipLevel::Store: return _L("Store (0) - no compression");
    case SevenZipLevel::Fastest: return _L("Fastest (1)");
    case SevenZipLevel::Fast: return _L("Fast (3)");
    case SevenZipLevel::Normal: return _L("Normal (5) - default");
    case SevenZipLevel::Maximum: return _L("Maximum (7)");
    case SevenZipLevel::Ultra: return _L("Ultra (9) - slowest, most RAM");
    }
    return wxEmptyString;
}

wxString mib_choice_name(unsigned mib)
{
    if (mib == 0) return _L("Level default");
    if (mib >= 1024) return wxString::Format(_L("%u GiB"), mib / 1024);
    return wxString::Format(_L("%u MiB"), mib);
}

wxString join_lines(const std::vector<std::string> &lines)
{
    wxString out;
    for (const std::string &line : lines) {
        if (!out.empty()) out += "\n";
        out += wxString::FromUTF8(line);
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------

ExportDialog::ExportDialog(wxWindow *parent, Dataset dataset)
    : DPIDialog(parent, wxID_ANY, _L("Export"), wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxBORDER_NONE)
    , m_dataset(std::move(dataset))
{
    const std::string override = wxGetApp().app_config != nullptr ? wxGetApp().app_config->get(CONFIG_SECTION, CONFIG_SEVEN_ZIP) : std::string();
    m_seven_zip = find_seven_zip(override.empty() ? std::filesystem::path() : std::filesystem::path(override));

    create_ui();
    apply_theme();
    populate_formats();
    update_archive_panel();
    update_format_details();
    update_export_enabled();

    SetEscapeId(wxID_CANCEL);
    SetAffirmativeId(wxID_OK);
    SetMinSize(FromDIP(wxSize(720, 560)));
    SetSize(FromDIP(wxSize(760, 820)));
    CentreOnParent();
    MD3DialogCaption::FinishChrome(this);
}

ExportDialog::~ExportDialog() = default;

bool ExportDialog::run(wxWindow *parent, Dataset dataset)
{
    ExportDialog dialog(parent, std::move(dataset));
    return dialog.ShowModal() == wxID_OK && !dialog.written_path().empty();
}

// ---------------------------------------------------------------------------

Label *ExportDialog::add_option(wxWindow *parent, wxSizer *sizer, const wxString &label, wxWindow *control, const wxString &tooltip)
{
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    auto *text = new Label(parent, Label::Body_13, label);
    text->SetMinSize(FromDIP(wxSize(200, -1)));
    row->Add(text, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    row->Add(control, 1, wxALIGN_CENTER_VERTICAL);
    sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    // Accessible name: the row label is what a screen reader announces for the control.
    control->SetName(label);
    if (!tooltip.empty()) {
        control->SetToolTip(tooltip);
        text->SetToolTip(tooltip);
    }
    m_option_rows.push_back(OptionRow{label + " " + tooltip, row, {text, control}});
    m_option_labels.push_back(text);
    return text;
}

void ExportDialog::create_ui()
{
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(new MD3DialogCaption(this, _L("Export")), 0, wxEXPAND);

    m_body = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    m_body->SetScrollRate(0, FromDIP(16));
    auto *body = new wxBoxSizer(wxVERTICAL);

    // TRN: %s is the dataset name, e.g. "Project version history".
    m_title_label = new Label(m_body, Label::Head_24, wxString::Format(_L("Export %s"), wxString::FromUTF8(m_dataset.name)));
    body->Add(m_title_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(24));
    wxString kind;
    switch (m_dataset.kind) {
    case DatasetKind::Tabular: kind = wxString::Format(_L("%zu row(s), %zu column(s)"), m_dataset.rows.size(), m_dataset.columns.size()); break;
    case DatasetKind::Structured: kind = wxString::Format(_L("structured record with %zu top-level field(s)"), m_dataset.record_count()); break;
    case DatasetKind::Prose: kind = _L("prose document"); break;
    }
    // TRN: %1$s is the schema id, %2$s the record summary.
    m_subtitle_label = new Label(m_body, Label::Body_14,
        wxString::Format(_L("Schema %s v%d - %s. Every export is UTF-8 and carries its schema, encoding and line-ending in a header."),
                         wxString::FromUTF8(m_dataset.schema_id), m_dataset.schema_version, kind),
        LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_subtitle_label->SetMinSize(wxSize(0, -1));
    body->Add(m_subtitle_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // TRN: Placeholder of the search field filtering export formats and options.
    m_search_field = new SearchField(m_body, _L("Search formats and options"));
    m_search_field->SetOnQuery([this](const wxString &) { apply_search(); });
    m_search_field->SetOnRegexToggle([this](bool) { apply_search(); });
    body->Add(m_search_field, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // --- Format card -----------------------------------------------------
    m_format_card = new StaticBox(m_body);
    auto *format_sizer = new wxBoxSizer(wxVERTICAL);
    format_sizer->Add(new Label(m_format_card, Label::Head_14, _L("Format")), 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(14));
    m_format_list = new wxDataViewListCtrl(m_format_card, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 220)),
                                           wxDV_SINGLE | wxBORDER_NONE);
    m_format_list->AppendTextColumn(_L("Format"), wxDATAVIEW_CELL_INERT, FromDIP(150), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_format_list->AppendTextColumn(_L("Fidelity"), wxDATAVIEW_CELL_INERT, FromDIP(110), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_format_list->AppendTextColumn(_L("Details"), wxDATAVIEW_CELL_INERT, FromDIP(380), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_format_list->SetName(_L("Export format"));
    m_format_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &) {
        update_format_details();
        update_output_path_extension();
        update_export_enabled();
    });
    wxGetApp().UpdateDVCDarkUI(m_format_list);
    format_sizer->Add(m_format_list, 0, wxEXPAND | wxALL, FromDIP(8));
    m_format_badge_label = new Label(m_format_card, Label::Head_13, wxEmptyString);
    format_sizer->Add(m_format_badge_label, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(14));
    m_format_detail_label = new Label(m_format_card, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_format_detail_label->SetMinSize(wxSize(0, -1));
    format_sizer->Add(m_format_detail_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(14));
    m_format_card->SetSizer(format_sizer);
    body->Add(m_format_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // --- Text options card -------------------------------------------------
    m_text_card = new StaticBox(m_body);
    auto *text_sizer = new wxBoxSizer(wxVERTICAL);
    text_sizer->Add(new Label(m_text_card, Label::Head_14, _L("Text encoding")), 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(14));
    m_encoding_label = new Label(m_text_card, Label::Body_13, _L("UTF-8 (always)"));
    add_option(m_text_card, text_sizer, _L("Encoding"), m_encoding_label,
               _L("Every export is UTF-8. The header records it so the reader never has to guess."));
    m_line_ending_combo = new ComboBox(m_text_card, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(260, 32)), 0, nullptr, wxCB_READONLY);
    m_line_ending_combo->Append(_L("LF - Unix, Git-friendly (default)"));
    m_line_ending_combo->Append(_L("CRLF - Windows Notepad / Excel"));
    m_line_ending_combo->SetSelection(0);
    m_line_ending_combo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { update_format_details(); });
    add_option(m_text_card, text_sizer, _L("Line endings"), m_line_ending_combo,
               _L("Choose CRLF only for tools that cannot read LF. The header names whichever you pick."));
    m_header_check = new LabeledCheckBox(m_text_card, _L("Include the schema header (comment, envelope or .meta.json sidecar)"));
    m_header_check->SetValue(true);
    m_header_check->SetToolTip(_L("CSV and TSV have no comment syntax, so their schema goes in a sidecar file next to the data."));
    text_sizer->Add(m_header_check, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    m_option_rows.push_back(OptionRow{_L("schema header sidecar"), nullptr, {m_header_check}});
    m_bom_check = new LabeledCheckBox(m_text_card, _L("Write a UTF-8 byte-order mark (only for spreadsheets that misread CSV without one)"));
    m_bom_check->SetValue(false);
    text_sizer->Add(m_bom_check, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    m_option_rows.push_back(OptionRow{_L("byte-order mark BOM UTF-8"), nullptr, {m_bom_check}});
    m_text_card->SetSizer(text_sizer);
    body->Add(m_text_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // --- Archive card ------------------------------------------------------
    m_archive_card = new StaticBox(m_body);
    auto *archive_sizer = new wxBoxSizer(wxVERTICAL);
    archive_sizer->Add(new Label(m_archive_card, Label::Head_14, _L("Archive")), 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(14));
    m_archive_combo = new ComboBox(m_archive_card, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(300, 32)), 0, nullptr, wxCB_READONLY);
    m_archive_combo->Append(_L("None - write the file directly"));
    m_archive_combo->Append(_L("ZIP - Deflate, built in, no encryption"));
    m_archive_combo->Append(_L("7z - via installed 7-Zip, full option set"));
    m_archive_combo->SetSelection(0);
    m_archive_combo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) {
        update_archive_panel();
        update_output_path_extension();
        update_export_enabled();
    });
    add_option(m_archive_card, archive_sizer, _L("Wrap in archive"), m_archive_combo,
               _L("An archive keeps the data file and its sidecar together. Paths inside are always relative."));
    m_archive_note_label = new Label(m_archive_card, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_archive_note_label->SetMinSize(wxSize(0, -1));
    archive_sizer->Add(m_archive_note_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));

    m_seven_zip_sizer = new wxBoxSizer(wxVERTICAL);
    m_method_combo = new ComboBox(m_archive_card, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(260, 32)), 0, nullptr, wxCB_READONLY);
    for (SevenZipMethod m : METHOD_CHOICES) m_method_combo->Append(wxString(seven_zip_method_switch(m)));
    m_method_combo->SetSelection(0);
    m_method_combo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { update_hints(); });
    add_option(m_archive_card, m_seven_zip_sizer, _L("Compression method"), m_method_combo,
               _L("LZMA2 (default): best general ratio, multi-threaded. LZMA: single-threaded. PPMd: best for text. BZip2/Deflate: compatibility, weaker ratio."));
    m_level_combo = new ComboBox(m_archive_card, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(260, 32)), 0, nullptr, wxCB_READONLY);
    for (SevenZipLevel l : LEVEL_CHOICES) m_level_combo->Append(level_name(l));
    m_level_combo->SetSelection(3);
    m_level_combo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { update_hints(); });
    add_option(m_archive_card, m_seven_zip_sizer, _L("Compression level"), m_level_combo,
               _L("Higher levels use larger dictionaries: slower and hungrier for RAM, but smaller."));
    m_dictionary_combo = new ComboBox(m_archive_card, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(260, 32)), 0, nullptr, wxCB_READONLY);
    for (unsigned mib : DICTIONARY_CHOICES) m_dictionary_combo->Append(mib_choice_name(mib));
    m_dictionary_combo->SetSelection(0);
    m_dictionary_combo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { update_hints(); });
    add_option(m_archive_card, m_seven_zip_sizer, _L("Dictionary size"), m_dictionary_combo,
               _L("LZMA/LZMA2 dictionary or PPMd memory. Compression needs about 11x this much RAM; extraction needs this much. Ignored by BZip2 and Deflate."));
    m_word_size_spin = new SpinInput(m_archive_card, "0", wxEmptyString, wxDefaultPosition, FromDIP(wxSize(140, 32)), wxALIGN_LEFT, 0, 273, 0);
    m_word_size_spin->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) { update_hints(); });
    add_option(m_archive_card, m_seven_zip_sizer, _L("Word size (0 = default)"), m_word_size_spin,
               _L("Fast bytes for LZMA/LZMA2/Deflate (5-273) or model order for PPMd (2-32). Larger is a little slower and a little smaller."));
    m_solid_check = new LabeledCheckBox(m_archive_card, _L("Solid archive (files share one compression block)"));
    m_solid_check->SetValue(true);
    m_solid_check->SetToolTip(_L("Solid gives a better ratio; extracting one file then decompresses its whole block."));
    m_solid_check->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { update_hints(); });
    m_seven_zip_sizer->Add(m_solid_check, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    m_option_rows.push_back(OptionRow{_L("solid archive block"), nullptr, {m_solid_check}});
    m_solid_block_combo = new ComboBox(m_archive_card, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(260, 32)), 0, nullptr, wxCB_READONLY);
    for (unsigned mib : SOLID_BLOCK_CHOICES) m_solid_block_combo->Append(mib_choice_name(mib));
    m_solid_block_combo->SetSelection(0);
    m_solid_block_combo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { update_hints(); });
    add_option(m_archive_card, m_seven_zip_sizer, _L("Solid block size"), m_solid_block_combo,
               _L("How much data shares one block. Smaller blocks extract single files faster; larger blocks compress better."));
    m_threads_spin = new SpinInput(m_archive_card, "0", wxEmptyString, wxDefaultPosition, FromDIP(wxSize(140, 32)), wxALIGN_LEFT, 0, 64, 0);
    m_threads_spin->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) { update_hints(); });
    add_option(m_archive_card, m_seven_zip_sizer, _L("Threads (0 = all cores)"), m_threads_spin,
               _L("LZMA2 memory grows with the thread count; LZMA and PPMd compress on one thread regardless."));
    m_split_input = new TextInput(m_archive_card, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(200, 32)));
    m_split_input->GetTextCtrl()->SetHint("100m");
    add_option(m_archive_card, m_seven_zip_sizer, _L("Split into volumes (empty = single file)"), m_split_input,
               _L("A size such as 100m or 4g. 7-Zip numbers the parts .7z.001, .7z.002 ... and every part is needed to extract."));
    m_password_input = new TextInput(m_archive_card, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(260, 32)), wxTE_PASSWORD);
    m_password_input->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { update_hints(); update_export_enabled(); });
    add_option(m_archive_card, m_seven_zip_sizer, _L("AES-256 password (empty = no encryption)"), m_password_input,
               _L("Encrypts the file contents with AES-256. The password is passed to 7-Zip for this run only and never stored."));
    m_password_confirm_input = new TextInput(m_archive_card, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(260, 32)), wxTE_PASSWORD);
    m_password_confirm_input->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { update_export_enabled(); });
    add_option(m_archive_card, m_seven_zip_sizer, _L("Confirm password"), m_password_confirm_input, _L("Type the same password again."));
    m_encrypt_headers_check = new LabeledCheckBox(m_archive_card, _L("Encrypt headers too, so file names are hidden (-mhe=on)"));
    m_encrypt_headers_check->SetValue(true);
    m_encrypt_headers_check->SetToolTip(_L("Without this, anyone can list the file names inside the archive even though the contents are encrypted."));
    m_encrypt_headers_check->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { update_hints(); });
    m_seven_zip_sizer->Add(m_encrypt_headers_check, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    m_option_rows.push_back(OptionRow{_L("encrypt headers hidden file names"), nullptr, {m_encrypt_headers_check}});
    m_encryption_warning_label = new Label(m_archive_card, Label::Head_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_encryption_warning_label->SetMinSize(wxSize(0, -1));
    m_seven_zip_sizer->Add(m_encryption_warning_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    m_hints_label = new Label(m_archive_card, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_hints_label->SetMinSize(wxSize(0, -1));
    m_seven_zip_sizer->Add(m_hints_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    auto *seven_zip_row = new wxBoxSizer(wxHORIZONTAL);
    m_seven_zip_status_label = new Label(m_archive_card, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_seven_zip_status_label->SetMinSize(wxSize(0, -1));
    m_locate_seven_zip_button = new Button(m_archive_card, _L("Locate 7z.exe..."));
    m_locate_seven_zip_button->SetMinSize(FromDIP(wxSize(140, 36)));
    m_locate_seven_zip_button->Bind(wxEVT_BUTTON, &ExportDialog::on_locate_seven_zip, this);
    seven_zip_row->Add(m_seven_zip_status_label, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    seven_zip_row->Add(m_locate_seven_zip_button, 0, wxALIGN_CENTER_VERTICAL);
    m_seven_zip_sizer->Add(seven_zip_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    archive_sizer->Add(m_seven_zip_sizer, 0, wxEXPAND);
    m_archive_card->SetSizer(archive_sizer);
    body->Add(m_archive_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    // --- Output card -------------------------------------------------------
    m_output_card = new StaticBox(m_body);
    auto *output_sizer = new wxBoxSizer(wxVERTICAL);
    output_sizer->Add(new Label(m_output_card, Label::Head_14, _L("Output")), 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(14));
    auto *path_row = new wxBoxSizer(wxHORIZONTAL);
    m_path_input = new TextInput(m_output_card, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(-1, 32)));
    m_path_input->SetName(_L("Output file path"));
    m_path_input->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { update_export_enabled(); });
    m_browse_button = new Button(m_output_card, _L("Browse..."));
    m_browse_button->SetName(_L("Browse for output file"));
    m_browse_button->SetMinSize(FromDIP(wxSize(110, 36)));
    m_browse_button->Bind(wxEVT_BUTTON, &ExportDialog::on_browse, this);
    path_row->Add(m_path_input, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    path_row->Add(m_browse_button, 0, wxALIGN_CENTER_VERTICAL);
    output_sizer->Add(path_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(14));
    m_option_rows.push_back(OptionRow{_L("output file path browse"), path_row, {m_path_input, m_browse_button}});
    m_output_card->SetSizer(output_sizer);
    body->Add(m_output_card, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    m_status_label = new Label(m_body, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_status_label->SetMinSize(wxSize(0, -1));
    body->Add(m_status_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(24));
    m_body->SetSizer(body);
    root->Add(m_body, 1, wxEXPAND);

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_cancel_button = new Button(this, _L("Cancel"), "", 0, 0, wxID_CANCEL);
    m_export_button = new Button(this, _L("Export"), "", 0, 0, wxID_OK);
    m_cancel_button->SetMinSize(FromDIP(wxSize(104, 40)));
    m_export_button->SetMinSize(FromDIP(wxSize(124, 40)));
    m_cancel_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    m_export_button->Bind(wxEVT_BUTTON, &ExportDialog::on_export, this);
    actions->AddStretchSpacer();
    actions->Add(m_cancel_button, 0, wxRIGHT, FromDIP(8));
    actions->Add(m_export_button, 0);
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(24));

    SetSizer(root);

    // Default output path: last export directory (or the home directory) plus
    // the dataset's stem and the selected format's extension.
    std::string last_dir = wxGetApp().app_config != nullptr ? wxGetApp().app_config->get(CONFIG_SECTION, CONFIG_LAST_DIR) : std::string();
    wxString    dir      = last_dir.empty() ? wxGetHomeDir() : wxString::FromUTF8(last_dir);
    m_path_input->GetTextCtrl()->ChangeValue(wxFileName(dir, default_file_name()).GetFullPath());
}

void ExportDialog::apply_theme()
{
    const wxColour surface   = StateColor::semantic(MD3::Role::Surface);
    const wxColour card      = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour list      = StateColor::semantic(MD3::Role::SurfaceContainerLowest);
    const wxColour alternate = StateColor::semantic(MD3::Role::SurfaceContainer);
    const wxColour text      = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour secondary = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const wxColour outline   = StateColor::semantic(MD3::Role::OutlineVariant);

    SetBackgroundColour(surface);
    m_body->SetBackgroundColour(surface);
    for (Label *label : {m_title_label, m_subtitle_label, m_status_label}) {
        label->SetBackgroundColour(surface);
        label->SetForegroundColour(label == m_title_label ? text : secondary);
    }
    for (StaticBox *box : {m_format_card, m_text_card, m_archive_card, m_output_card}) {
        box->SetBackgroundColorNormal(card);
        box->SetBorderColorNormal(outline);
        box->SetBorderWidth(1);
    }
    for (Label *label : m_option_labels) {
        label->SetBackgroundColour(card);
        label->SetForegroundColour(text);
    }
    for (Label *label : {m_format_badge_label, m_format_detail_label, m_encoding_label, m_archive_note_label, m_hints_label,
                         m_seven_zip_status_label, m_encryption_warning_label}) {
        label->SetBackgroundColour(card);
        label->SetForegroundColour(secondary);
    }
    m_encryption_warning_label->SetForegroundColour(StateColor::semantic(MD3::Role::Error));
    for (LabeledCheckBox *check : {m_header_check, m_bom_check, m_solid_check, m_encrypt_headers_check}) {
        check->SetBackgroundColour(card);
        check->SetForegroundColour(text);
    }
    m_format_list->SetBackgroundColour(list);
    m_format_list->SetForegroundColour(text);
    m_format_list->SetAlternateRowColour(alternate);

    const StateColor outlined_bg = outlined_button_background();
    for (Button *button : {m_cancel_button, m_browse_button, m_locate_seven_zip_button}) {
        button->SetBackgroundColor(outlined_bg);
        button->SetBorderColor(StateColor(outline));
        button->SetTextColor(StateColor(text));
    }
    m_export_button->SetBackgroundColor(filled_button_background());
    m_export_button->SetBorderColor(StateColor(StateColor::semantic(MD3::Role::Primary)));
    m_export_button->SetTextColor(filled_button_text());
    Refresh();
}

// ---------------------------------------------------------------------------

void ExportDialog::populate_formats()
{
    m_format_list->DeleteAllItems();
    m_visible_formats.clear();
    // Natural formats first, then the rest in declaration order.
    std::vector<Format> ordered;
    for (Format f : all_formats()) if (format_is_natural_for(f, m_dataset.kind)) ordered.push_back(f);
    for (Format f : all_formats()) if (!format_is_natural_for(f, m_dataset.kind)) ordered.push_back(f);

    SearchField::MatchPass pass(m_search_field->GetValue(), m_search_field->IsRegexEnabled(), m_search_field->IsCaseSensitive(),
                                m_search_field->IsWholeWord(), m_search_field->IsMultiline());
    for (Format f : ordered) {
        const LossReport report = compute_loss_report(m_dataset, f);
        wxString name = wxString(format_name(f)) + " (." + format_extension(f) + ")";
        if (format_is_natural_for(f, m_dataset.kind)) name += " " + _L("- recommended");
        const wxString badge = report.lossless ? _L("Lossless") : _L("Lossy");
        wxString detail;
        if (!report.losses.empty()) detail = wxString::FromUTF8(report.losses.front());
        else if (!report.notes.empty()) detail = wxString::FromUTF8(report.notes.front());
        else detail = _L("Every field survives unchanged.");
        const wxString haystack = name + " " + badge + " " + detail;
        if (!pass.matches(haystack)) continue;
        wxVector<wxVariant> row;
        row.push_back(wxVariant(name));
        row.push_back(wxVariant(badge));
        row.push_back(wxVariant(detail));
        m_format_list->AppendItem(row);
        m_visible_formats.push_back(f);
    }
    if (!m_visible_formats.empty()) m_format_list->SelectRow(0);
}

void ExportDialog::apply_search()
{
    populate_formats();
    SearchField::MatchPass pass(m_search_field->GetValue(), m_search_field->IsRegexEnabled(), m_search_field->IsCaseSensitive(),
                                m_search_field->IsWholeWord(), m_search_field->IsMultiline());
    const bool empty = m_search_field->GetValue().Trim().empty();
    for (OptionRow &row : m_option_rows) {
        const bool show = empty || pass.matches(row.label);
        for (wxWindow *w : row.windows) w->Show(show);
    }
    update_archive_panel();
    update_format_details();
    update_export_enabled();
}

Format ExportDialog::selected_format() const
{
    const int row = m_format_list->GetSelectedRow();
    if (row < 0 || static_cast<std::size_t>(row) >= m_visible_formats.size())
        return m_visible_formats.empty() ? Format::JSON : m_visible_formats.front();
    return m_visible_formats[static_cast<std::size_t>(row)];
}

ArchiveFormat ExportDialog::selected_archive() const
{
    switch (m_archive_combo->GetSelection()) {
    case 1: return ArchiveFormat::Zip;
    case 2: return ArchiveFormat::SevenZip;
    default: return ArchiveFormat::None;
    }
}

ArchiveOptions ExportDialog::archive_options() const
{
    ArchiveOptions o;
    o.format = selected_archive();
    const int method = m_method_combo->GetSelection();
    o.method = method >= 0 && static_cast<std::size_t>(method) < METHOD_CHOICES.size() ? METHOD_CHOICES[static_cast<std::size_t>(method)] : SevenZipMethod::LZMA2;
    const int level = m_level_combo->GetSelection();
    o.level = level >= 0 && static_cast<std::size_t>(level) < LEVEL_CHOICES.size() ? LEVEL_CHOICES[static_cast<std::size_t>(level)] : SevenZipLevel::Normal;
    const int dict = m_dictionary_combo->GetSelection();
    o.dictionary_mib = dict >= 0 && static_cast<std::size_t>(dict) < DICTIONARY_CHOICES.size() ? DICTIONARY_CHOICES[static_cast<std::size_t>(dict)] : 0;
    o.word_size = static_cast<unsigned>(std::max(0, m_word_size_spin->GetValue()));
    o.solid = m_solid_check->GetValue();
    const int block = m_solid_block_combo->GetSelection();
    o.solid_block_mib = block >= 0 && static_cast<std::size_t>(block) < SOLID_BLOCK_CHOICES.size() ? SOLID_BLOCK_CHOICES[static_cast<std::size_t>(block)] : 0;
    o.threads = static_cast<unsigned>(std::max(0, m_threads_spin->GetValue()));
    o.split_volume = m_split_input->GetTextCtrl()->GetValue().Trim().ToUTF8().data();
    o.password = m_password_input->GetTextCtrl()->GetValue().ToUTF8().data();
    o.encrypt_headers = m_encrypt_headers_check->GetValue();
    return o;
}

wxString ExportDialog::default_file_name() const
{
    const std::string stem = m_dataset.file_stem.empty() ? std::string("export") : m_dataset.file_stem;
    switch (selected_archive()) {
    case ArchiveFormat::Zip: return wxString::FromUTF8(stem) + ".zip";
    case ArchiveFormat::SevenZip: return wxString::FromUTF8(stem) + ".7z";
    default: return wxString::FromUTF8(stem) + "." + format_extension(selected_format());
    }
}

void ExportDialog::update_format_details()
{
    const Format     f      = selected_format();
    const LossReport report = compute_loss_report(m_dataset, f);
    m_format_badge_label->SetLabel(report.lossless ? wxString::Format(_L("%s: lossless"), format_name(f))
                                                   : wxString::Format(_L("%s: lossy - read what will be lost before exporting"), format_name(f)));
    m_format_badge_label->SetForegroundColour(StateColor::semantic(report.lossless ? MD3::Role::Primary : MD3::Role::Error));
    wxString detail;
    if (!report.losses.empty()) detail += _L("Lost:") + "\n" + join_lines(report.losses);
    if (!report.notes.empty()) {
        if (!detail.empty()) detail += "\n";
        detail += _L("Notes:") + "\n" + join_lines(report.notes);
    }
    if (detail.empty()) detail = _L("Every field survives unchanged.");
    const LineEnding le = m_line_ending_combo->GetSelection() == 1 ? LineEnding::CRLF : LineEnding::LF;
    detail += "\n" + wxString::Format(_L("Header: schema %s v%d, encoding UTF-8, line endings %s."), wxString::FromUTF8(m_dataset.schema_id),
                                       m_dataset.schema_version, line_ending_name(le));
    m_format_detail_label->SetLabel(detail);
    m_body->Layout();
    m_body->FitInside();
}

void ExportDialog::update_archive_panel()
{
    const ArchiveFormat a = selected_archive();
    const bool seven = a == ArchiveFormat::SevenZip;
    const bool empty_search = m_search_field->GetValue().Trim().empty();
    m_seven_zip_sizer->ShowItems(seven);
    if (seven && !empty_search) {
        // Re-apply the search filter on the 7z rows that ShowItems just revealed.
        SearchField::MatchPass pass(m_search_field->GetValue(), m_search_field->IsRegexEnabled(), m_search_field->IsCaseSensitive(),
                                    m_search_field->IsWholeWord(), m_search_field->IsMultiline());
        for (OptionRow &row : m_option_rows)
            for (wxWindow *w : row.windows)
                if (w->GetParent() == m_archive_card && w != m_archive_combo) w->Show(pass.matches(row.label));
    }
    switch (a) {
    case ArchiveFormat::None:
        m_archive_note_label->SetLabel(_L("The data file is written directly. CSV and TSV also get a .meta.json sidecar carrying the schema."));
        break;
    case ArchiveFormat::Zip:
        m_archive_note_label->SetLabel(_L("ZIP is written by the bundled miniz (Deflate). It is never encrypted: nothing in it is protected, and the file names are visible. Choose 7z for AES-256."));
        break;
    case ArchiveFormat::SevenZip:
        m_archive_note_label->SetLabel(_L("7z is produced by the 7-Zip command line on this PC with exactly the switches shown after export. All member paths are relative."));
        break;
    }
    if (m_seven_zip.found)
        m_seven_zip_status_label->SetLabel(wxString::Format(_L("7-Zip found: %s"), wxString::FromUTF8(m_seven_zip.executable.string())));
    else
        m_seven_zip_status_label->SetLabel(wxString::Format(_L("7-Zip not found. Install 7-Zip or locate 7z.exe. Searched: %s"), wxString::FromUTF8(m_seven_zip.searched)));
    update_hints();
    m_body->Layout();
    m_body->FitInside();
}

void ExportDialog::update_hints()
{
    const ArchiveOptions o = archive_options();
    if (o.format != ArchiveFormat::SevenZip) return;
    m_hints_label->SetLabel(join_lines(seven_zip_cost_hints(o)));
    if (seven_zip_filenames_visible(o))
        m_encryption_warning_label->SetLabel(_L("Warning: the contents will be encrypted but the file names inside the archive stay readable by anyone. Tick \"Encrypt headers too\" to hide them."));
    else if (!o.password.empty())
        m_encryption_warning_label->SetLabel(_L("AES-256 with encrypted headers: contents and file names are both protected. Losing the password loses the data."));
    else
        m_encryption_warning_label->SetLabel(wxEmptyString);
    m_body->Layout();
    m_body->FitInside();
}

void ExportDialog::update_output_path_extension()
{
    wxFileName current(m_path_input->GetTextCtrl()->GetValue());
    if (current.GetFullName().empty()) {
        current.SetFullName(default_file_name());
    } else {
        wxFileName wanted(default_file_name());
        current.SetName(current.GetName().empty() ? wanted.GetName() : current.GetName());
        current.SetExt(wanted.GetExt());
    }
    m_path_input->GetTextCtrl()->ChangeValue(current.GetFullPath());
}

void ExportDialog::update_export_enabled()
{
    wxString reason;
    if (m_visible_formats.empty()) reason = _L("No format matches the search.");
    else if (m_path_input->GetTextCtrl()->GetValue().Trim().empty()) reason = _L("Choose an output path.");
    else if (selected_archive() == ArchiveFormat::SevenZip) {
        if (!m_seven_zip.found) reason = _L("7-Zip was not found on this PC; ZIP and plain files still work.");
        else if (m_password_input->GetTextCtrl()->GetValue() != m_password_confirm_input->GetTextCtrl()->GetValue())
            reason = _L("The two password fields differ.");
    }
    m_export_button->Enable(reason.empty());
    m_export_button->SetToolTip(reason.empty() ? _L("Write the export") : reason);
    if (!reason.empty()) set_status(reason, false);
    else if (m_status_label->GetLabel() != wxEmptyString && m_written_path.empty()) set_status(wxEmptyString, false);
}

void ExportDialog::set_status(const wxString &message, bool error)
{
    m_status_label->SetLabel(message);
    m_status_label->SetForegroundColour(StateColor::semantic(error ? MD3::Role::Error : MD3::Role::OnSurfaceVariant));
    m_status_label->SetToolTip(message);
    m_body->Layout();
    m_body->FitInside();
}

// ---------------------------------------------------------------------------

void ExportDialog::on_browse(wxCommandEvent &)
{
    wxFileName current(m_path_input->GetTextCtrl()->GetValue());
    wxString   wildcard;
    switch (selected_archive()) {
    case ArchiveFormat::Zip: wildcard = _L("ZIP archive (*.zip)|*.zip"); break;
    case ArchiveFormat::SevenZip: wildcard = _L("7z archive (*.7z)|*.7z"); break;
    default: {
        const Format f = selected_format();
        wildcard = wxString::Format("%s (*.%s)|*.%s", format_name(f), format_extension(f), format_extension(f));
    }
    }
    wildcard += "|" + _L("All files (*.*)") + "|*.*";
    wxFileDialog dialog(this, _L("Export to"), current.GetPath(), current.GetFullName().empty() ? default_file_name() : current.GetFullName(),
                        wildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    m_path_input->GetTextCtrl()->ChangeValue(dialog.GetPath());
    update_export_enabled();
}

void ExportDialog::on_locate_seven_zip(wxCommandEvent &)
{
    wxFileDialog dialog(this, _L("Locate 7z.exe"), wxEmptyString, "7z.exe",
                        _L("7-Zip executable (7z.exe;7za.exe)|7z.exe;7za.exe|All files (*.*)|*.*"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    const std::string chosen = dialog.GetPath().ToUTF8().data();
    m_seven_zip = find_seven_zip(std::filesystem::path(chosen));
    if (m_seven_zip.found && wxGetApp().app_config != nullptr)
        wxGetApp().app_config->set(CONFIG_SECTION, CONFIG_SEVEN_ZIP, chosen);
    update_archive_panel();
    update_export_enabled();
}

void ExportDialog::on_export(wxCommandEvent &)
{
    ExportJob job;
    job.dataset = m_dataset;
    job.format  = selected_format();
    job.serialize_options.line_ending           = m_line_ending_combo->GetSelection() == 1 ? LineEnding::CRLF : LineEnding::LF;
    job.serialize_options.include_schema_header = m_header_check->GetValue();
    job.serialize_options.write_bom             = m_bom_check->GetValue();
    job.serialize_options.generator             = std::string(SLIC3R_APP_NAME) + " " + SLIC3R_VERSION;
    job.archive                                 = archive_options();
    job.output_path                             = std::filesystem::path(m_path_input->GetTextCtrl()->GetValue().ToStdWstring());
    if (m_seven_zip.found) job.seven_zip_override = m_seven_zip.executable;

    if (job.archive.format == ArchiveFormat::SevenZip && job.archive.password != std::string(m_password_confirm_input->GetTextCtrl()->GetValue().ToUTF8().data())) {
        set_status(_L("The two password fields differ."), true);
        return;
    }

    const LossReport report = compute_loss_report(job.dataset, job.format);
    if (!report.lossless) {
        // The loss is already spelled out on screen; the status line repeats it
        // so the moment of export carries the same words as the format card.
        set_status(_L("Exporting as a lossy format:") + " " + join_lines(report.losses), false);
    }

    m_export_button->Enable(false);
    ExportOutcome outcome = run_export(job);
    m_export_button->Enable(true);
    if (!outcome.ok) {
        set_status(wxString::Format(_L("Export failed: %s"), wxString::FromUTF8(outcome.error)), true);
        return;
    }
    m_written_path = outcome.written_path;
    if (wxGetApp().app_config != nullptr)
        wxGetApp().app_config->set(CONFIG_SECTION, CONFIG_LAST_DIR, outcome.written_path.parent_path().string());

    wxString members;
    for (const std::string &m : outcome.members) {
        if (!members.empty()) members += ", ";
        members += wxString::FromUTF8(m);
    }
    wxString summary = wxString::Format(_L("Wrote %s (%s)."), wxString::FromUTF8(outcome.written_path.string()), members);
    if (!outcome.error.empty()) summary += " " + wxString::FromUTF8(outcome.error);
    if (!outcome.command_line.empty()) summary += "\n" + _L("7-Zip command:") + " " + wxString::FromUTF8(outcome.command_line);
    if (Plater *plater = wxGetApp().plater(); plater != nullptr && plater->get_notification_manager() != nullptr)
        plater->get_notification_manager()->push_exporting_finished_notification(outcome.written_path.string(),
                                                                                  outcome.written_path.parent_path().string(), false);
    set_status(summary, false);
    EndModal(wxID_OK);
}

// ---------------------------------------------------------------------------

void ExportDialog::on_dpi_changed(const wxRect &)
{
    SetMinSize(FromDIP(wxSize(720, 560)));
    for (Button *button : {m_cancel_button, m_browse_button, m_locate_seven_zip_button}) button->SetMinSize(FromDIP(wxSize(104, 36)));
    m_export_button->SetMinSize(FromDIP(wxSize(124, 40)));
    Layout();
    m_body->FitInside();
    Refresh();
}

void ExportDialog::on_sys_color_changed()
{
    apply_theme();
    update_format_details();
}

} // namespace Slic3r::GUI
