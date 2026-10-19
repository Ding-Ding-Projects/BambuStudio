#include "MelodyCreatorDialog.hpp"

#include "../DeviceCore/DevManager.h"
#include "../DeviceManager.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../NotificationManager.hpp"
#include "../Plater.hpp"
#include "../Tab.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Grid.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/TextInput.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Utils.hpp"

#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/sound.h>

#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>

#include <sstream>

namespace Slic3r { namespace GUI {

namespace {

constexpr int COL_NOTE[3]   = {0, 3, 6};
constexpr int COL_TICKS[3]  = {1, 4, 7};
constexpr int COL_VOLUME[3] = {2, 5, 8};
constexpr int COLUMNS       = 9;

const char *APP_CONFIG_SECTION = "melody";

wxString preset_title(const MelodyPreset &preset)
{
    // The preset names are English in the model; the catalog carries them.
    const std::string id = preset.id;
    if (id == "x1_start") return _L("Bambu Lab X1 start");
    if (id == "x1_finish") return _L("Bambu Lab X1 finish");
    if (id == "a1_start") return _L("Bambu Lab A1 start");
    if (id == "a1_mini_start") return _L("Bambu Lab A1 mini start");
    if (id == "h2d_start") return _L("Bambu Lab H2D start");
    if (id == "h2d_finish") return _L("Bambu Lab H2D finish");
    if (id == "c_major_scale") return _L("C major scale");
    return wxString::FromUTF8(preset.name);
}

wxString problem_text(MelodyProblem problem)
{
    switch (problem) {
    case MelodyProblem::None: return wxEmptyString;
    case MelodyProblem::NoSteps: return _L("Add at least one step.");
    case MelodyProblem::TooManySteps: return wxString::Format(_L("A melody can have at most %d steps."), MELODY_MAX_STEPS);
    case MelodyProblem::NameTooLong: return wxString::Format(_L("The name can have at most %d characters."), static_cast<int>(MELODY_MAX_NAME));
    case MelodyProblem::PitchOutOfRange: return _L("Every note must be between C#-1 and G9, or empty for silence.");
    case MelodyProblem::TicksOutOfRange: return wxString::Format(_L("Every duration must be between 0 and %d ticks."), MELODY_MAX_TICKS);
    case MelodyProblem::VolumeOutOfRange: return _L("Every volume must be between 0 and 100.");
    case MelodyProblem::TickLengthOutOfRange:
        return wxString::Format(_L("The tick length must be between %d and %d milliseconds."), MELODY_MIN_TICK_MS, MELODY_MAX_TICK_MS);
    }
    return wxEmptyString;
}

boost::filesystem::path melodies_dir()
{
    boost::filesystem::path dir = boost::filesystem::path(data_dir()) / "melodies";
    boost::system::error_code ec;
    boost::filesystem::create_directories(dir, ec);
    return dir;
}

} // namespace

MelodyCreatorDialog::MelodyCreatorDialog(wxWindow *parent)
    : MD3Dialog(parent, _L("Melody creator"),
                _L("Compose a tune the printer plays through M1006: three voices per step, each with a note, a duration in ticks and a volume."),
                MaterialIcon::PlayArrow, MD3Dialog::Options{true, false})
{
    const wxColour bg = GetBackgroundColour();
    auto *scroll = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxTAB_TRAVERSAL | wxBORDER_NONE);
    scroll->SetBackgroundColour(bg);
    scroll->SetScrollRate(0, FromDIP(12));
    auto *body = new wxBoxSizer(wxVERTICAL);
    scroll->SetSizer(body);
    GetContentSizer()->Add(scroll, 1, wxEXPAND);
    wxWindow *host = scroll;

    auto make_label = [&](const wxString &text, bool secondary = false, bool wrap = false) {
        auto *l = new Label(host, secondary ? Label::Body_12 : Label::Body_13, text, wrap ? LB_AUTO_WRAP : 0);
        l->SetBackgroundColour(bg);
        l->SetForegroundColour(StateColor::semantic(secondary ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
        if (wrap) l->SetMinSize(wxSize(0, -1));
        return l;
    };
    auto make_text = [&](const wxString &value, const wxString &name, const wxString &hint, int width) {
        auto *input = new TextInput(host, value, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(width), -1));
        StateColor fill(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Disabled),
                        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Enabled));
        input->SetBackgroundColor(fill);
        input->SetCornerRadius(FromDIP(10));
        input->GetTextCtrl()->SetFont(Label::Body_13);
        input->GetTextCtrl()->SetName(name);
        input->GetTextCtrl()->SetHint(hint);
        input->GetTextCtrl()->SetMaxLength(1024);
        return input;
    };
    auto make_button = [&](const wxString &text, Button::Variant variant, const wxString &tip, std::function<void()> action) {
        auto *b = new Button(host, text);
        b->SetVariant(variant);
        b->SetToolTip(tip);
        b->Bind(wxEVT_BUTTON, [action](wxCommandEvent &) { action(); });
        return b;
    };

    // --- name, tick length, presets ---------------------------------------
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        auto *name_col = new wxBoxSizer(wxVERTICAL);
        name_col->Add(make_label(_L("Name")), 0, wxBOTTOM, FromDIP(4));
        m_name = make_text(wxEmptyString, _L("Melody name"), _L("My tune"), 240);
        m_name->GetTextCtrl()->SetMaxLength(static_cast<unsigned long>(MELODY_MAX_NAME));
        m_name->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) { read_name_and_tick(); e.Skip(); });
        name_col->Add(m_name, 0, wxEXPAND);
        row->Add(name_col, 1, wxEXPAND);

        auto *tick_col = new wxBoxSizer(wxVERTICAL);
        tick_col->Add(make_label(_L("Tick length (ms, preview only)")), 0, wxBOTTOM, FromDIP(4));
        int saved_tick = MELODY_MIN_TICK_MS;
        if (AppConfig *cfg = wxGetApp().app_config) {
            const std::string s = cfg->get(APP_CONFIG_SECTION, "tick_ms");
            saved_tick = s.empty() ? 50 : std::atoi(s.c_str());
        }
        m_melody.tick_ms = std::max(MELODY_MIN_TICK_MS, std::min(MELODY_MAX_TICK_MS, saved_tick));
        m_tick_ms = new SpinInput(host, wxString::Format("%d", m_melody.tick_ms), wxEmptyString, wxDefaultPosition, wxSize(FromDIP(110), -1), 0,
                                  MELODY_MIN_TICK_MS, MELODY_MAX_TICK_MS, m_melody.tick_ms);
        m_tick_ms->SetName(_L("Tick length"));
        m_tick_ms->SetToolTip(_L("How long one firmware tick lasts in the preview. The printer's own tick length is not documented; adjust this until the preview matches what the printer plays."));
        m_tick_ms->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &e) { read_name_and_tick(); e.Skip(); });
        m_tick_ms->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) { read_name_and_tick(); e.Skip(); });
        tick_col->Add(m_tick_ms, 0);
        row->Add(tick_col, 0, wxLEFT, FromDIP(16));

        auto *preset_col = new wxBoxSizer(wxVERTICAL);
        preset_col->Add(make_label(_L("Start from")), 0, wxBOTTOM, FromDIP(4));
        m_presets = new ComboBox(host, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(220), -1), 0, nullptr, wxCB_READONLY);
        m_presets->SetFont(Label::Body_13);
        m_presets->GetDropDown().SetFont(Label::Body_13);
        m_presets->SetCornerRadius(FromDIP(10));
        m_presets->SetName(_L("Preset"));
        m_presets->Append(_L("Empty melody"));
        for (const MelodyPreset &preset : melody_presets()) m_presets->Append(preset_title(preset));
        m_presets->SetSelection(0);
        m_presets->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &e) { load_preset(m_presets->GetSelection()); e.Skip(); });
        preset_col->Add(m_presets, 0);
        row->Add(preset_col, 0, wxLEFT, FromDIP(16));
        body->Add(row, 0, wxEXPAND);
    }

    // --- the step table ---------------------------------------------------
    body->Add(make_label(_L("Steps: one line per time step, three voices. A note is written like C4 or F#3; leave it empty for silence."), true, true), 0,
              wxEXPAND | wxTOP, FromDIP(12));
    m_grid = new MD3Grid(host, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(260)));
    m_grid->CreateGrid(0, COLUMNS);
    m_grid->SetName(_L("Melody steps"));
    m_grid->SetRowLabelSize(FromDIP(40));
    for (int v = 0; v < 3; ++v) {
        m_grid->SetColLabelValue(COL_NOTE[v], wxString::Format(_L("Note %d"), v + 1));
        m_grid->SetColLabelValue(COL_TICKS[v], wxString::Format(_L("Ticks %d"), v + 1));
        m_grid->SetColLabelValue(COL_VOLUME[v], wxString::Format(_L("Volume %d"), v + 1));
        m_grid->SetColSize(COL_NOTE[v], FromDIP(70));
        m_grid->SetColSize(COL_TICKS[v], FromDIP(62));
        m_grid->SetColSize(COL_VOLUME[v], FromDIP(70));
    }
    m_grid->EnableDragRowSize(false);
    m_grid->SetSelectionMode(wxGrid::wxGridSelectRows);
    m_grid->Bind(wxEVT_GRID_CELL_CHANGED, &MelodyCreatorDialog::on_cell_changed, this);
    body->Add(m_grid, 1, wxEXPAND | wxTOP, FromDIP(4));

    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(make_button(_L("Add step"), Button::Variant::Tonal, _L("Append a silent step after the last one"), [this] { add_step(); }), 0);
        row->Add(make_button(_L("Duplicate"), Button::Variant::Text, _L("Copy the selected step below itself"), [this] { duplicate_step(); }), 0, wxLEFT, FromDIP(8));
        row->Add(make_button(_L("Move up"), Button::Variant::Text, _L("Move the selected step one earlier"), [this] { move_step(-1); }), 0, wxLEFT, FromDIP(8));
        row->Add(make_button(_L("Move down"), Button::Variant::Text, _L("Move the selected step one later"), [this] { move_step(1); }), 0, wxLEFT, FromDIP(8));
        row->Add(make_button(_L("Delete step"), Button::Variant::Text, _L("Remove the selected step"), [this] { delete_step(); }), 0, wxLEFT, FromDIP(8));
        body->Add(row, 0, wxTOP, FromDIP(8));
    }

    // --- quick notation ---------------------------------------------------
    body->Add(make_label(_L("Quick notation (first voice): notes with an optional /ticks, r for a rest, for example c4 e4/5 g4/20 r c5")), 0, wxTOP, FromDIP(12));
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        m_notation = make_text(wxEmptyString, _L("Quick notation"), "c4 e4 g4 c5/20", 420);
        row->Add(m_notation, 1, wxEXPAND);
        row->Add(make_button(_L("Replace steps"), Button::Variant::Tonal, _L("Replace every step with the notes typed on the left"), [this] { import_notation(); }), 0,
                 wxLEFT, FromDIP(8));
        body->Add(row, 0, wxEXPAND | wxTOP, FromDIP(4));
    }

    m_status = make_label(wxEmptyString, true, true);
    body->Add(m_status, 0, wxEXPAND | wxTOP, FromDIP(12));

    // --- footer -------------------------------------------------------------
    auto footer_button = [&](const wxString &text, Button::Variant variant, const wxString &tip, std::function<void()> action) {
        auto *b = new Button(this, text);
        b->SetVariant(variant);
        b->SetToolTip(tip);
        b->Bind(wxEVT_BUTTON, [action](wxCommandEvent &) { action(); });
        AddFooterButton(b);
        return b;
    };
    footer_button(_L("Open"), Button::Variant::Text, _L("Open a melody file (.json)"), [this] { open_from_file(); });
    footer_button(_L("Save"), Button::Variant::Text, _L("Save the melody as a file (.json)"), [this] { save_to_file(); });
    footer_button(_L("Copy G-code"), Button::Variant::Text, _L("Copy the M1006 block to the clipboard"), [this] { copy_gcode(); });
    footer_button(_L("Add to start G-code"), Button::Variant::Outlined, _L("Put the melody into the printer preset's start G-code (an earlier melody block is replaced)"),
                  [this] { insert_into_printer_gcode("machine_start_gcode", _L("start G-code")); });
    footer_button(_L("Add to end G-code"), Button::Variant::Outlined, _L("Put the melody into the printer preset's end G-code (an earlier melody block is replaced)"),
                  [this] { insert_into_printer_gcode("machine_end_gcode", _L("end G-code")); });
    m_preview = footer_button(_L("Preview"), Button::Variant::Tonal, _L("Play the melody on this computer with square waves"), [this] { preview(); });
    m_play    = footer_button(_L("Play on printer"), Button::Variant::Filled, _L("Send the melody to the selected printer now (it must be idle)"), [this] { play_on_printer(); });
    footer_button(_L("Close"), Button::Variant::Text, wxEmptyString, [this] { EndModal(wxID_CLOSE); });

    SetMinSize(wxSize(FromDIP(860), FromDIP(640)));
    SetSize(wxSize(FromDIP(900), FromDIP(680)));
    refresh_grid();
    update_status();
    Layout();
    CentreOnParent();
}

MelodyCreatorDialog::~MelodyCreatorDialog() = default;

// ---- model <-> controls -------------------------------------------------------

void MelodyCreatorDialog::refresh_grid()
{
    m_refreshing = true;
    if (m_grid->GetNumberRows() > 0) m_grid->DeleteRows(0, m_grid->GetNumberRows());
    if (!m_melody.steps.empty()) m_grid->AppendRows(static_cast<int>(m_melody.steps.size()));
    for (size_t i = 0; i < m_melody.steps.size(); ++i) {
        const int row = static_cast<int>(i);
        m_grid->SetRowLabelValue(row, wxString::Format("%d", row + 1));
        for (int v = 0; v < 3; ++v) {
            const MelodyVoice &voice = m_melody.steps[i].voices[v];
            m_grid->SetCellValue(row, COL_NOTE[v], wxString::FromUTF8(melody_note_name(voice.pitch)));
            m_grid->SetCellValue(row, COL_TICKS[v], wxString::Format("%d", voice.ticks));
            m_grid->SetCellValue(row, COL_VOLUME[v], wxString::Format("%d", voice.volume));
        }
    }
    m_name->GetTextCtrl()->ChangeValue(wxString::FromUTF8(m_melody.name));
    m_refreshing = false;
}

void MelodyCreatorDialog::read_name_and_tick()
{
    if (m_refreshing) return;
    m_melody.name    = into_u8(m_name->GetTextCtrl()->GetValue());
    m_melody.tick_ms = m_tick_ms->GetValue();
    if (AppConfig *cfg = wxGetApp().app_config)
        cfg->set(APP_CONFIG_SECTION, "tick_ms", std::to_string(m_melody.tick_ms));
    update_status();
}

void MelodyCreatorDialog::on_cell_changed(wxGridEvent &event)
{
    event.Skip();
    if (m_refreshing) return;
    const int row = event.GetRow();
    const int col = event.GetCol();
    if (row < 0 || row >= static_cast<int>(m_melody.steps.size())) return;
    const std::string text = into_u8(m_grid->GetCellValue(row, col));
    for (int v = 0; v < 3; ++v) {
        MelodyVoice &voice = m_melody.steps[static_cast<size_t>(row)].voices[v];
        if (col == COL_NOTE[v]) {
            const int pitch = melody_note_number(text);
            if (pitch < 0) {
                update_status(wxString::Format(_L("\"%s\" is not a note. Write a letter, an optional # or b and an octave, such as C4 or F#3."), wxString::FromUTF8(text)), true);
                m_grid->SetCellValue(row, col, wxString::FromUTF8(melody_note_name(voice.pitch)));
                return;
            }
            voice.pitch = pitch;
            m_grid->SetCellValue(row, col, wxString::FromUTF8(melody_note_name(pitch)));
        } else if (col == COL_TICKS[v] || col == COL_VOLUME[v]) {
            char *end = nullptr;
            const long value = std::strtol(text.c_str(), &end, 10);
            const bool ticks = col == COL_TICKS[v];
            const int  max   = ticks ? MELODY_MAX_TICKS : MELODY_MAX_VOLUME;
            if (end == text.c_str() || *end != '\0' || value < 0 || value > max) {
                update_status(wxString::Format(ticks ? _L("A duration is a whole number of ticks from 0 to %d.") : _L("A volume is a whole number from 0 to %d."), max), true);
                m_grid->SetCellValue(row, col, wxString::Format("%d", ticks ? voice.ticks : voice.volume));
                return;
            }
            (ticks ? voice.ticks : voice.volume) = static_cast<int>(value);
        }
    }
    update_status();
}

bool MelodyCreatorDialog::melody_ready(wxString &why) const
{
    const MelodyProblem problem = melody_validate(m_melody);
    why = problem_text(problem);
    return problem == MelodyProblem::None;
}

void MelodyCreatorDialog::update_status(const wxString &message, bool problem)
{
    wxString why;
    const bool ready = melody_ready(why);
    wxString text = message;
    if (text.empty()) {
        text = ready ? wxString::Format(_L("%d steps, about %.1f s at %d ms per tick."), static_cast<int>(m_melody.steps.size()),
                                        melody_total_ms(m_melody) / 1000.0, m_melody.tick_ms)
                     : why;
        problem = !ready;
    }
    m_status->SetLabel(text);
    m_status->SetForegroundColour(StateColor::semantic(problem ? MD3::Role::Error : MD3::Role::OnSurfaceVariant));
    m_status->Wrap(FromDIP(800));
    if (m_preview) m_preview->Enable(ready);
    if (m_play) m_play->Enable(ready);
    Layout();
}

int MelodyCreatorDialog::selected_step() const
{
    const wxArrayInt rows = m_grid->GetSelectedRows();
    if (!rows.empty()) return rows[0];
    const int cursor = m_grid->GetGridCursorRow();
    return cursor >= 0 && cursor < static_cast<int>(m_melody.steps.size()) ? cursor : -1;
}

void MelodyCreatorDialog::select_step(int index)
{
    if (index < 0 || index >= m_grid->GetNumberRows()) return;
    m_grid->SelectRow(index);
    m_grid->SetGridCursor(index, 0);
    m_grid->MakeCellVisible(index, 0);
}

// ---- actions --------------------------------------------------------------------

void MelodyCreatorDialog::add_step()
{
    if (m_melody.steps.size() >= static_cast<size_t>(MELODY_MAX_STEPS)) {
        update_status(problem_text(MelodyProblem::TooManySteps), true);
        return;
    }
    MelodyStep step;
    for (MelodyVoice &voice : step.voices) voice = MelodyVoice{0, 10, 0};
    m_melody.steps.push_back(step);
    refresh_grid();
    select_step(static_cast<int>(m_melody.steps.size()) - 1);
    update_status();
}

void MelodyCreatorDialog::duplicate_step()
{
    const int index = selected_step();
    if (index < 0) { update_status(_L("Select a step first."), true); return; }
    if (m_melody.steps.size() >= static_cast<size_t>(MELODY_MAX_STEPS)) { update_status(problem_text(MelodyProblem::TooManySteps), true); return; }
    m_melody.steps.insert(m_melody.steps.begin() + index + 1, m_melody.steps[static_cast<size_t>(index)]);
    refresh_grid();
    select_step(index + 1);
    update_status();
}

void MelodyCreatorDialog::move_step(int delta)
{
    const int index = selected_step();
    if (index < 0) { update_status(_L("Select a step first."), true); return; }
    const int target = index + delta;
    if (target < 0 || target >= static_cast<int>(m_melody.steps.size())) return;
    std::swap(m_melody.steps[static_cast<size_t>(index)], m_melody.steps[static_cast<size_t>(target)]);
    refresh_grid();
    select_step(target);
    update_status();
}

void MelodyCreatorDialog::delete_step()
{
    const int index = selected_step();
    if (index < 0) { update_status(_L("Select a step first."), true); return; }
    m_melody.steps.erase(m_melody.steps.begin() + index);
    refresh_grid();
    select_step(std::min(index, static_cast<int>(m_melody.steps.size()) - 1));
    update_status();
}

void MelodyCreatorDialog::import_notation()
{
    Melody parsed;
    const int bad = melody_parse_notation(into_u8(m_notation->GetTextCtrl()->GetValue()), parsed);
    if (bad > 0) {
        update_status(wxString::Format(_L("Token %d of the quick notation could not be read. Use notes such as c4, f#3/8 or r/5."), bad), true);
        return;
    }
    if (parsed.steps.empty()) {
        update_status(_L("Type some notes first, for example c4 e4 g4 c5/20."), true);
        return;
    }
    m_melody.steps = parsed.steps;
    m_presets->SetSelection(0);
    refresh_grid();
    update_status();
}

void MelodyCreatorDialog::load_preset(int index)
{
    if (index <= 0) {
        m_melody.steps.clear();
        m_melody.name.clear();
    } else {
        const auto &presets = melody_presets();
        if (index - 1 >= static_cast<int>(presets.size())) return;
        const int tick = m_melody.tick_ms;
        m_melody         = melody_from_preset(presets[static_cast<size_t>(index - 1)]);
        m_melody.name    = into_u8(preset_title(presets[static_cast<size_t>(index - 1)]));
        m_melody.tick_ms = tick;
    }
    refresh_grid();
    update_status();
}

void MelodyCreatorDialog::preview()
{
    wxString why;
    if (!melody_ready(why)) { update_status(why, true); return; }
#if wxUSE_SOUND
    m_sound.reset();
    m_sound_data = melody_render_wav(m_melody);
    m_sound      = std::make_unique<wxSound>(m_sound_data.size(), m_sound_data.data());
    if (m_sound->IsOk() && m_sound->Play(wxSOUND_ASYNC)) {
        update_status(wxString::Format(_L("Playing the preview: %d steps, about %.1f s. The printer's own sound differs."), static_cast<int>(m_melody.steps.size()),
                                       melody_total_ms(m_melody) / 1000.0));
        return;
    }
#endif
    update_status(_L("The preview cannot play on this computer. Play on printer still works."), true);
}

void MelodyCreatorDialog::play_on_printer()
{
    wxString why;
    if (!melody_ready(why)) { update_status(why, true); return; }
    DeviceManager *dev = wxGetApp().getDeviceManager();
    MachineObject *obj = dev ? dev->get_selected_machine() : nullptr;
    if (!obj) { update_status(_L("Select a printer on the Device page first."), true); return; }
    if (!obj->is_connected()) { update_status(_L("The selected printer is not connected."), true); return; }
    if (obj->is_in_printing()) { update_status(_L("The printer is printing. Play the melody when it is idle."), true); return; }
    const int result = obj->publish_gcode(melody_to_gcode(m_melody));
    if (result != 0) { update_status(wxString::Format(_L("The printer did not accept the melody (code %d)."), result), true); return; }
    const wxString sent = wxString::Format(_L("Melody sent to %s."), wxString::FromUTF8(obj->get_dev_name()));
    update_status(sent);
    if (Plater *plater = wxGetApp().plater())
        if (NotificationManager *nm = plater->get_notification_manager())
            nm->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::RegularNotificationLevel, into_u8(sent));
}

void MelodyCreatorDialog::copy_gcode()
{
    wxString why;
    if (!melody_ready(why)) { update_status(why, true); return; }
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(melody_to_gcode(m_melody))));
        wxTheClipboard->Close();
        update_status(_L("The melody G-code is on the clipboard."));
    } else {
        update_status(_L("The clipboard is not available right now."), true);
    }
}

void MelodyCreatorDialog::insert_into_printer_gcode(const char *key, const wxString &field_name)
{
    wxString why;
    if (!melody_ready(why)) { update_status(why, true); return; }
    PresetBundle *bundle = wxGetApp().preset_bundle;
    Tab          *tab    = wxGetApp().get_tab(Preset::TYPE_PRINTER);
    if (!bundle || !tab) { update_status(_L("The printer settings are not available right now."), true); return; }
    std::string gcode = bundle->printers.get_edited_preset().config.opt_string(key);
    const bool  replaced = melody_insert_block(gcode, melody_to_gcode(m_melody));
    if (!Tab::validate_custom_gcode(field_name, gcode)) return;
    DynamicPrintConfig cfg;
    cfg.set_key_value(key, new ConfigOptionString(gcode));
    tab->load_config(cfg);
    update_status(replaced ? wxString::Format(_L("The melody replaced the earlier one in the printer preset's %s. Save the preset to keep it."), field_name)
                           : wxString::Format(_L("The melody was added to the end of the printer preset's %s. Save the preset to keep it."), field_name));
}

void MelodyCreatorDialog::save_to_file()
{
    wxString why;
    if (!melody_ready(why)) { update_status(why, true); return; }
    wxString folder = wxString::FromUTF8(melodies_dir().string());
    if (AppConfig *cfg = wxGetApp().app_config) {
        const std::string saved = cfg->get(APP_CONFIG_SECTION, "folder");
        if (!saved.empty() && wxDirExists(wxString::FromUTF8(saved))) folder = wxString::FromUTF8(saved);
    }
    wxString name = wxString::FromUTF8(m_melody.name.empty() ? "melody" : m_melody.name);
    wxFileDialog dialog(this, _L("Save melody"), folder, name + ".json", "JSON (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const wxString path = dialog.GetPath();
    boost::nowide::ofstream out(into_u8(path).c_str(), std::ios::binary);
    out << melody_to_json(m_melody);
    if (!out) { update_status(wxString::Format(_L("Could not write %s."), path), true); return; }
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(APP_CONFIG_SECTION, "folder", into_u8(wxFileName(path).GetPath()));
    update_status(wxString::Format(_L("Saved %s."), path));
}

void MelodyCreatorDialog::open_from_file()
{
    wxString folder = wxString::FromUTF8(melodies_dir().string());
    if (AppConfig *cfg = wxGetApp().app_config) {
        const std::string saved = cfg->get(APP_CONFIG_SECTION, "folder");
        if (!saved.empty() && wxDirExists(wxString::FromUTF8(saved))) folder = wxString::FromUTF8(saved);
    }
    wxFileDialog dialog(this, _L("Open melody"), folder, wxEmptyString, "JSON (*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    const wxString path = dialog.GetPath();
    boost::nowide::ifstream in(into_u8(path).c_str(), std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    Melody loaded;
    if (!in || !melody_from_json(buffer.str(), loaded)) {
        update_status(wxString::Format(_L("%s is not a melody file."), path), true);
        return;
    }
    const MelodyProblem problem = melody_validate(loaded);
    if (problem != MelodyProblem::None) {
        update_status(wxString::Format(_L("%s cannot be used: %s"), path, problem_text(problem)), true);
        return;
    }
    m_melody = loaded;
    m_tick_ms->SetValue(m_melody.tick_ms);
    m_presets->SetSelection(0);
    refresh_grid();
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(APP_CONFIG_SECTION, "folder", into_u8(wxFileName(path).GetPath()));
    update_status(wxString::Format(_L("Opened %s."), path));
}

}} // namespace Slic3r::GUI
