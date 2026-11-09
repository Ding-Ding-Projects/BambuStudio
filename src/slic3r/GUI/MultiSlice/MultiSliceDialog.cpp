#include "MultiSliceDialog.hpp"
#include "MultiSliceNames.hpp"

#include "../BilingualDecorator.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../PartPlate.hpp"
#include "../Plater.hpp"
#include "../RemovableDriveManager.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/CheckBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabelGrowWithWrap.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Tab.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/miniz_extension.hpp"
#include "libslic3r/PresetBundle.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>

#include <wx/dirdlg.h>
#include <wx/stdpaths.h>
#include <wx/sizer.h>

namespace Slic3r::GUI {

namespace {

constexpr int kTickMs       = 400;
constexpr int kStartTicks   = 25;   // 10 s for a slice to begin once the ink is applied
constexpr int kSliceTicks   = 4500; // 30 min for one slice to finish

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

std::string ink_type(const Preset &preset)
{
    const auto *types = preset.config.option<ConfigOptionStrings>("filament_type");
    return types && !types->values.empty() ? types->values.front() : std::string();
}

// The ink a written sliced file was really sliced with, read back from the
// project settings stored in the file. Empty when it cannot be read.
std::string ink_in_file(const boost::filesystem::path &file)
{
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!open_zip_reader(&zip, file.string())) return {};
    std::string ink;
    size_t size = 0;
    if (void *data = mz_zip_reader_extract_file_to_heap(&zip, "Metadata/project_settings.config", &size, 0)) {
        try {
            const auto settings = nlohmann::json::parse(static_cast<const char *>(data), static_cast<const char *>(data) + size);
            const auto it = settings.find("filament_settings_id");
            if (it != settings.end() && it->is_array() && !it->empty() && it->front().is_string())
                ink = it->front().get<std::string>();
        } catch (...) {
        }
        mz_free(data);
    }
    close_zip_reader(&zip);
    return ink;
}

PartPlate *current_plate()
{
    Plater *plater = wxGetApp().plater();
    return plater ? plater->get_partplate_list().get_curr_plate() : nullptr;
}

} // namespace

MultiSliceDialog::MultiSliceDialog(wxWindow *parent)
    : DPIDialog(parent, wxID_ANY, _L("Multi-slice"), wxDefaultPosition, wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_timer(this)
{
    SetBackgroundColour(role(MD3::Role::Surface));
    Bind(wxEVT_TIMER, &MultiSliceDialog::on_tick, this);
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &evt) {
        // Closing during a run stops after the ink being sliced and puts the
        // original inks back; the window closes once that is done.
        if (m_running && evt.CanVeto()) {
            m_cancel = true;
            evt.Veto();
            update_summary();
            return;
        }
        evt.Skip();
    });
    build();
    wxGetApp().UpdateDlgDarkUI(this);
    SetMinSize(FromDIP(wxSize(560, 520)));
    SetSize(FromDIP(wxSize(640, 680)));
    CenterOnParent();
}

MultiSliceDialog::~MultiSliceDialog()
{
    m_timer.Stop();
}

void MultiSliceDialog::build()
{
    auto *root = new wxBoxSizer(wxVERTICAL);
    const int pad = FromDIP(20), gap = FromDIP(10);

    auto *title = new Label(this, Label::Head_20, _L("Multi-slice"));
    root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    auto *intro = new Label(this, Label::Body_13,
        _L("Slice this plate once for each ink you choose and save one sliced file per ink, ready for the printer's USB drive. "
           "For each file every ink slot uses the chosen ink; slot colours stay. Your current inks come back when it is done."),
        LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    intro->SetMinSize(wxSize(FromDIP(200), -1));
    GrowWithWrap(intro);
    intro->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    root->Add(intro, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    m_search = new SearchField(this, _L("Search inks by name or type"));
    m_search->SetOnQuery([this](const wxString &) { filter_inks(); });
    m_search->SetOnRegexToggle([this](bool) { filter_inks(); });
    root->Add(m_search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    m_list = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    m_list->SetBackgroundColour(role(MD3::Role::SurfaceContainerLow));
    m_list->SetScrollRate(0, FromDIP(12));
    m_list_sizer = new wxBoxSizer(wxVERTICAL);

    // The same inks the ink list offers: the shown ones that fit this printer,
    // and every ink a slot uses now. A profile that has not picked its shown
    // inks yet lists every ink that fits instead, so there is always a choice.
    PresetBundle *bundle = wxGetApp().preset_bundle;
    std::set<std::string> listed;
    auto add = [this, &listed](const Preset &preset) {
        if (preset.is_default || !listed.insert(preset.name).second) return;
        m_inks.push_back({preset.name, ink_type(preset)});
    };
    for (const Preset &preset : bundle->filaments.get_presets()) {
        const bool in_use = std::find(bundle->filament_presets.begin(), bundle->filament_presets.end(), preset.name) !=
                            bundle->filament_presets.end();
        if (in_use || (preset.is_visible && preset.is_compatible)) add(preset);
    }
    if (m_inks.size() < 2)
        for (const Preset &preset : bundle->filaments.get_presets())
            if (preset.is_compatible) add(preset);
    for (Ink &ink : m_inks) {
        auto *row = new wxPanel(m_list);
        row->SetBackgroundColour(m_list->GetBackgroundColour());
        auto *rs = new wxBoxSizer(wxHORIZONTAL);
        ink.check = new ::CheckBox(row);
        ink.check->SetName(wxString::FromUTF8(ink.preset));
        ink.check->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) { update_summary(); e.Skip(); });
        rs->Add(ink.check, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        auto *text = new wxBoxSizer(wxVERTICAL);
        auto *name = new Label(row, Label::Body_14, wxControl::EscapeMnemonics(wxString::FromUTF8(ink.preset)),
                               LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
        name->SetMinSize(wxSize(FromDIP(160), -1));
        text->Add(name, 0, wxEXPAND);
        if (!ink.type.empty()) {
            auto *type = new Label(row, Label::Body_12, wxString::FromUTF8(ink.type));
            type->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
            text->Add(type, 0);
        }
        rs->Add(text, 1, wxALIGN_CENTER_VERTICAL);
        ink.status = new Label(row, Label::Body_12, wxString());
        ink.status->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
        rs->Add(ink.status, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        row->SetSizer(rs);
        // A click anywhere on the row toggles its box.
        auto toggle = [this, &ink](wxMouseEvent &) {
            if (m_running) return;
            ink.check->SetValue(!ink.check->GetValue());
            update_summary();
        };
        row->Bind(wxEVT_LEFT_UP, toggle);
        name->Bind(wxEVT_LEFT_UP, toggle);
        ink.row = row;
        m_list_sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(8));
    }
    if (m_inks.empty()) {
        auto *empty = new Label(m_list, Label::Body_13, _L("No inks fit the current printer. Add or show inks in the ink list first."),
                                LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
        m_list_sizer->Add(empty, 0, wxEXPAND | wxALL, FromDIP(12));
    }
    m_list_sizer->AddSpacer(FromDIP(8));
    m_list->SetSizer(m_list_sizer);
    root->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    m_summary = new Label(this, Label::Body_13, _L("Choose at least two inks. Each one gets its own sliced file."),
                          LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_summary->SetMinSize(wxSize(FromDIP(200), -1));
    GrowWithWrap(m_summary);
    root->Add(m_summary, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *dest = new wxBoxSizer(wxHORIZONTAL);
    // Created with text so its row starts one line tall; GrowWithWrap lets it
    // take the lines a long folder path wraps onto.
    m_destination = new Label(this, Label::Body_13, _L("No folder chosen yet."), LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_destination->SetMinSize(wxSize(FromDIP(200), -1));
    GrowWithWrap(m_destination);
    dest->Add(m_destination, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    m_change = new Button(this, _L("Change folder"));
    m_change->SetVariant(Button::Variant::Outlined);
    m_change->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { choose_destination(); });
    dest->Add(m_change, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(dest, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_open = new Button(this, _L("Show files"));
    m_open->SetVariant(Button::Variant::Text);
    m_open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { desktop_open_any_folder(m_folder.string()); });
    m_open->Hide();
    actions->Add(m_open, 0, wxALIGN_CENTER_VERTICAL);
    actions->AddStretchSpacer();
    m_close = new Button(this, _L("Close"));
    m_close->SetVariant(Button::Variant::Outlined);
    m_close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Close(); });
    actions->Add(m_close, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    m_start = new Button(this, _L("Slice and save"));
    m_start->SetVariant(Button::Variant::Filled);
    m_start->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { start(); });
    actions->Add(m_start, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(actions, 0, wxEXPAND | wxALL, pad);

    SetSizer(root);

    // The printer's USB drive when one is plugged in, else the last folder
    // used, else the Documents folder, so there is always somewhere to save.
    AppConfig &config = *wxGetApp().app_config;
    std::string last = config.get_last_output_dir("", false);
    boost::system::error_code ec;
    if (last.empty() || !boost::filesystem::is_directory(boost::filesystem::path(last), ec))
        last = into_u8(wxStandardPaths::Get().GetDocumentsDir());
    const std::string usb  = wxGetApp().removable_drive_manager()->get_removable_drive_path(last);
    if (!usb.empty()) set_destination(boost::filesystem::path(usb), true);
    else set_destination(boost::filesystem::path(last), false);

    if (Plater *plater = wxGetApp().plater()) {
        const wxString name = plater->get_export_gcode_filename(".gcode.3mf", true, false);
        std::string base = into_u8(name);
        const std::string ext = ".gcode.3mf";
        if (base.size() > ext.size() && base.compare(base.size() - ext.size(), ext.size(), ext) == 0)
            base.erase(base.size() - ext.size());
        m_base = base.empty() ? std::string("plate") : base;
    }
    update_summary();
}

void MultiSliceDialog::filter_inks()
{
    SearchField::MatchPass pass(m_search->GetValue(), m_search->IsRegexEnabled(), m_search->IsCaseSensitive(),
                                m_search->IsWholeWord(), m_search->IsMultiline());
    for (Ink &ink : m_inks)
        ink.row->Show(pass.matches(wxString::FromUTF8(ink.preset + " " + ink.type)) || ink.check->GetValue());
    m_list->Layout();
    m_list->FitInside();
}

void MultiSliceDialog::update_summary()
{
    int chosen = 0;
    for (const Ink &ink : m_inks) chosen += ink.check->GetValue() ? 1 : 0;
    if (m_running && m_cancel)
        m_summary->SetLabel(_L("Stopping after the ink being sliced, then putting your inks back."));
    else if (m_running)
        m_summary->SetLabel(wxString::Format(_L("Slicing %d of %d. Leave the plate as it is until this finishes."),
                                             int(std::min(m_next, m_queue.size())), int(m_queue.size())));
    else if (chosen < 2)
        m_summary->SetLabel(_L("Choose at least two inks. Each one gets its own sliced file."));
    else
        m_summary->SetLabel(wxString::Format(_L("%d inks chosen: %d sliced files will be saved."), chosen, chosen));
    m_start->Enable(!m_running && chosen >= 2 && !m_folder.empty());
    Layout();
    // Text set after the window was shown is paired with its Cantonese again.
    I18N::refresh_bilingual_decoration(this);
}

void MultiSliceDialog::set_destination(const boost::filesystem::path &folder, bool removable)
{
    m_folder    = folder;
    m_removable = removable;
    if (folder.empty())
        m_destination->SetLabel(_L("No folder chosen yet."));
    else
        m_destination->SetLabel(wxString::Format(removable ? _L("Saving to the USB drive %s") : _L("Saving to %s"),
                                                 from_path(folder)));
    m_destination->InvalidateBestSize();
    Layout();
    I18N::refresh_bilingual_decoration(this);
}

void MultiSliceDialog::choose_destination()
{
    wxDirDialog dlg(this, _L("Choose where to save the sliced files"), from_path(m_folder), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string path = into_u8(dlg.GetPath());
    set_destination(boost::filesystem::path(path), wxGetApp().removable_drive_manager()->is_path_on_removable_drive(path));
    update_summary();
}

void MultiSliceDialog::set_running(bool running)
{
    m_running = running;
    m_search->Enable(!running);
    m_change->Enable(!running);
    for (Ink &ink : m_inks) ink.check->Enable(!running);
    m_close->SetLabel(running ? _L("Stop") : _L("Close"));
    update_summary();
}

void MultiSliceDialog::show_status(Ink &ink, Status state, const wxString &detail)
{
    ink.state = state;
    wxString text;
    switch (state) {
    case Status::Idle:    text = wxString(); break;
    case Status::Waiting: text = _L("Waiting"); break;
    case Status::Slicing: text = _L("Slicing"); break;
    case Status::Saved:   text = _L("File saved"); break;
    case Status::Failed:  text = detail.empty() ? _L("Not saved") : wxString::Format(_L("Not saved: %s"), detail); break;
    case Status::Skipped: text = _L("Stopped"); break;
    }
    // A short status has no room to grow into, so the decorator never pairs it;
    // in bilingual mode it carries its Cantonese itself, "Saved · 存咗".
    if (const wxString cantonese = I18N::bilingual_secondary(text); !cantonese.empty() && cantonese != text)
        text += wxString::FromUTF8(" \xC2\xB7 ") + cantonese;
    ink.status->SetLabel(text);
    ink.status->SetForegroundColour(role(state == Status::Failed ? MD3::Role::Error :
                                         state == Status::Saved ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant));
    ink.row->Layout();
    m_list->Layout();
    m_list->FitInside();
    I18N::refresh_bilingual_decoration(this);
}

void MultiSliceDialog::start()
{
    Plater *plater = wxGetApp().plater();
    if (m_running || plater == nullptr || current_plate() == nullptr) return;
    if (!current_plate()->has_printable_instances()) {
        m_summary->SetLabel(_L("This plate has nothing to print. Put a model on it first."));
        return;
    }
    boost::system::error_code ec;
    if (!boost::filesystem::is_directory(m_folder, ec)) {
        m_summary->SetLabel(_L("The folder cannot be reached. Plug the USB drive back in or choose another folder."));
        return;
    }

    m_queue.clear();
    m_taken.clear();
    for (size_t i = 0; i < m_inks.size(); ++i) {
        show_status(m_inks[i], Status::Idle);
        if (m_inks[i].check->GetValue()) {
            m_queue.push_back(i);
            show_status(m_inks[i], Status::Waiting);
        }
    }
    if (m_queue.size() < 2) return;
    m_original = wxGetApp().preset_bundle->filament_presets;
    m_next     = 0;
    m_cancel   = false;
    m_open->Hide();
    BOOST_LOG_TRIVIAL(info) << "multi-slice: " << m_queue.size() << " inks";
    set_running(true);
    begin_next();
}

void MultiSliceDialog::apply_ink(const std::string &preset)
{
    PresetBundle *bundle = wxGetApp().preset_bundle;
    Plater       *plater = wxGetApp().plater();
    const size_t  slots  = bundle->filament_presets.size();
    for (size_t slot = 0; slot < slots; ++slot) {
        if (bundle->filament_presets[slot] == preset) continue;
        bundle->set_filament_preset(slot, preset);
        plater->on_filament_change(slot);
    }
    // Make the ink take effect the way the ink list does: the first slot's ink
    // becomes the edited ink, and the plater takes the new configuration.
    // Setting the slot names alone leaves the old ink in the sliced settings.
    use_slot_inks();
}

void MultiSliceDialog::use_slot_inks()
{
    PresetBundle *bundle = wxGetApp().preset_bundle;
    if (Tab *tab = wxGetApp().get_tab(Preset::TYPE_FILAMENT); tab && !bundle->filament_presets.empty())
        tab->select_preset(bundle->filament_presets.front(), false, "", true);
    wxGetApp().plater()->on_config_change(bundle->full_config());
    wxGetApp().plater()->sidebar().update_presets(Preset::TYPE_FILAMENT);
}

void MultiSliceDialog::restore_inks()
{
    PresetBundle *bundle = wxGetApp().preset_bundle;
    Plater       *plater = wxGetApp().plater();
    if (m_original.empty() || plater == nullptr) return;
    const size_t slots = std::min(bundle->filament_presets.size(), m_original.size());
    for (size_t slot = 0; slot < slots; ++slot) {
        if (bundle->filament_presets[slot] == m_original[slot]) continue;
        bundle->set_filament_preset(slot, m_original[slot]);
        plater->on_filament_change(slot);
    }
    use_slot_inks();
    m_original.clear();
}

void MultiSliceDialog::begin_next()
{
    if (m_cancel || m_next >= m_queue.size()) {
        finish_run();
        return;
    }
    m_current = &m_inks[m_queue[m_next++]];
    show_status(*m_current, Status::Slicing);
    update_summary();

    PartPlate *plate = current_plate();
    // A result is accepted only once it has been made invalid after this point
    // and become valid again, so it was sliced with this ink.
    m_epoch = plate->get_slice_invalid_epoch();
    apply_ink(m_current->preset);
    // The same ink as before changes nothing and would leave the old result
    // valid; mark it invalid so this turn's file is always freshly sliced.
    if (plate->is_slice_result_valid() || plate->get_slice_invalid_epoch() == m_epoch)
        plate->update_slice_result_valid_state(false);
    m_seen_slicing = false;
    m_ticks        = 0;
    wxGetApp().plater()->reslice();
    m_timer.Start(kTickMs);
}

void MultiSliceDialog::on_tick(wxTimerEvent &)
{
    Plater    *plater = wxGetApp().plater();
    PartPlate *plate  = current_plate();
    if (m_current == nullptr || plater == nullptr || plate == nullptr) {
        m_timer.Stop();
        finish_variant(false);
        return;
    }
    ++m_ticks;
    const bool slicing = plater->is_background_process_slicing();
    m_seen_slicing     = m_seen_slicing || slicing;
    if (slicing) {
        if (m_ticks > kSliceTicks) { m_timer.Stop(); plater->stop_jobs(); finish_variant(false); }
        return;
    }
    const bool resliced = plate->get_slice_invalid_epoch() != m_epoch;
    if (plate->is_slice_result_valid() && resliced) {
        m_timer.Stop();
        finish_variant(plate->is_slice_result_ready_for_print());
        return;
    }
    // Slicing ran and stopped without a result, or it never started.
    if (m_seen_slicing || m_ticks > kStartTicks) {
        m_timer.Stop();
        finish_variant(false);
    }
}

void MultiSliceDialog::finish_variant(bool sliced)
{
    Ink &ink = *m_current;
    if (!sliced) {
        show_status(ink, Status::Failed, _L("the plate did not slice with this ink"));
    } else {
        const std::string name   = MultiSlice::variant_file_name(m_base, ink.preset, m_taken);
        const auto        target = m_folder / boost::filesystem::path(name);
        Plater           *plater = wxGetApp().plater();
        const int plate_idx      = plater->get_partplate_list().get_curr_plate_index();
        const int ret = plater->export_3mf(target, SaveStrategy::Silence | SaveStrategy::SplitModel | SaveStrategy::WithGcode |
                                                        SaveStrategy::SkipModel, plate_idx);
        // The last word on which ink a file holds is the file itself: a file
        // sliced with any other ink is removed rather than saved under this name.
        const std::string written = ret < 0 ? std::string() : ink_in_file(target);
        if (ret < 0) {
            show_status(ink, Status::Failed, _L("the file could not be written"));
        } else if (written != ink.preset) {
            boost::system::error_code ec;
            boost::filesystem::remove(target, ec);
            BOOST_LOG_TRIVIAL(warning) << "multi-slice: file for " << ink.preset << " was sliced with " << written << "; removed";
            show_status(ink, Status::Failed, _L("it was sliced with a different ink, so it was not kept"));
        } else {
            ink.file = target.string();
            show_status(ink, Status::Saved);
        }
    }
    m_current = nullptr;
    // Let the slice result settle before the next ink changes it.
    CallAfter([this] { begin_next(); });
}

void MultiSliceDialog::finish_run()
{
    for (size_t i = m_next; i < m_queue.size(); ++i) show_status(m_inks[m_queue[i]], Status::Skipped);
    restore_inks();
    int saved = 0;
    for (size_t index : m_queue) saved += m_inks[index].state == Status::Saved ? 1 : 0;
    const bool stopped = m_cancel;
    set_running(false);
    m_cancel = false;
    if (saved > 0) {
        wxGetApp().app_config->update_last_output_dir(m_folder.string(), m_removable);
        if (m_removable) wxGetApp().removable_drive_manager()->set_and_verify_last_save_path(m_folder.string());
        m_open->Show();
    }
    m_summary->SetLabel(stopped ? wxString::Format(_L("Stopped. %d of %d sliced files saved; your inks are back."), saved, int(m_queue.size()))
                                : wxString::Format(_L("Done. %d of %d sliced files saved; your inks are back."), saved, int(m_queue.size())));
    BOOST_LOG_TRIVIAL(info) << "multi-slice: saved " << saved << " of " << m_queue.size();
    Layout();
    I18N::refresh_bilingual_decoration(this);
}

void MultiSliceDialog::on_dpi_changed(const wxRect &)
{
    Layout();
    Refresh();
}

} // namespace Slic3r::GUI
