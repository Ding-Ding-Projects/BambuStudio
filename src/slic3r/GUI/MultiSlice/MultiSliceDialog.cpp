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
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/ProgressBar.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"
#include "../Widgets/Tag.hpp"
#include "../Tab.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/miniz_extension.hpp"
#include "libslic3r/PresetBundle.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>

#include <wx/containr.h>
#include <wx/dirdlg.h>
#include <wx/statbmp.h>
#include <wx/stdpaths.h>
#include <wx/sizer.h>

namespace Slic3r::GUI {

namespace {

constexpr int kTickMs       = 400;
constexpr int kStartTicks   = 25;   // 10 s for a slice to begin once the ink is applied
constexpr int kSliceTicks   = 4500; // 30 min for one slice to finish

// Layout from the surface restyle handoff (section 6), in design pixels.
constexpr int kBodyGap       = 14;  // between the blocks of the body
constexpr int kWellPad       = 6;   // padding of the ink list well
constexpr int kDestWellPad   = 12;  // padding of the destination well
constexpr int kRowRadius     = 12;
constexpr int kRowHeight     = 44;
constexpr int kRowPadX       = 12;
constexpr int kRowPadY       = 10;
constexpr int kRowGap        = 2;   // between two rows
constexpr int kCheckGap      = 12;  // between the check box and the name
constexpr int kCheckWidth    = 20;  // nominal check box width, used to align the list header
constexpr int kTagGap        = 8;   // between the name and the type tag
constexpr int kStatusGap     = 12;  // between the name column and the status column
constexpr int kStatusCol     = 132; // width of the status column
constexpr int kStatusColMax  = 180; // widest the status column grows in bilingual mode
constexpr int kHeaderHeight  = 28;
constexpr int kEmptyPad      = 24;
constexpr int kGlyphPx       = 20;
constexpr int kProgressGap   = 8;

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

// `base` with `top` laid over it at `amount` (0..1): the Material state layer.
wxColour blend(const wxColour &base, const wxColour &top, double amount)
{
    auto channel = [amount](int b, int t) { return static_cast<unsigned char>(b + (t - b) * amount + 0.5); };
    return wxColour(channel(base.Red(), top.Red()), channel(base.Green(), top.Green()), channel(base.Blue(), top.Blue()));
}

// The English text with its Cantonese after it in bilingual mode ("Saved · 存咗");
// outside bilingual mode, or when the catalogue has no entry, the text alone.
wxString with_cantonese(const wxString &text)
{
    const wxString cantonese = I18N::bilingual_secondary(text);
    if (!cantonese.empty() && cantonese != text)
        return text + wxString::FromUTF8(" \xC2\xB7 ") + cantonese;
    return text;
}

// A list or section header: the English part upper-cased AFTER the lookup, so the
// catalogue still finds the mixed-case msgid for its Cantonese pair.
wxString header_text(const wxString &looked_up)
{
    const wxString cantonese = I18N::bilingual_secondary(looked_up);
    wxString text = looked_up.Upper();
    if (!cantonese.empty() && cantonese != looked_up)
        text += wxString::FromUTF8(" \xC2\xB7 ") + cantonese;
    return text;
}

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

wxBitmap glyph_bitmap(wxWindow *window, uint32_t glyph, MD3::Role colour)
{
    return MaterialIcon::bitmap(window, glyph, kGlyphPx, role(colour));
}

} // namespace

// A tonal surface with a rounded fill and no border, used for the two wells and
// for the ink rows. Parent it to the dialog or to another surface; create its
// children only after the fill is set, because a label takes its background from
// the parent's fill.
class TonalSurface : public wxNavigationEnabled<StaticBox>
{
public:
    TonalSurface(wxWindow *parent, MD3::Role fill, int radius_dip)
        : m_radius_dip(radius_dip)
    {
        StaticBox::Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL);
        SetCornerRadius(FromDIP(m_radius_dip));
        SetBackgroundColor(StateColor(role(fill)));
    }

    void rescale() { SetCornerRadius(FromDIP(m_radius_dip)); }

private:
    int m_radius_dip;
};

// One ink in the list: a rounded row that fills on hover and while its ink is
// chosen. The fill is the selection signal; the check box inside stays for the
// keyboard and for assistive technology. Hover over any child counts as hover
// over the row (StateHandler::attach_child), and the row keeps its children's
// background colour equal to its current fill so no child shows a stale plate.
class InkRow : public TonalSurface
{
public:
    explicit InkRow(wxWindow *parent)
        : TonalSurface(parent, MD3::Role::SurfaceContainerLow, kRowRadius)
    {
        apply_fills();
    }

    void set_checked(bool checked)
    {
        if (m_checked == checked) return;
        m_checked = checked;
        apply_fills();
    }

    // False during a run: the row no longer reacts to the pointer.
    void set_hover_enabled(bool enabled)
    {
        if (m_hover_enabled == enabled) return;
        m_hover_enabled = enabled;
        apply_fills();
    }

    // Count the pointer over `child` as the pointer over this row.
    void hover_through(wxWindow *child) { state_handler.attach_child(child); }

protected:
    void doRender(wxDC &dc) override
    {
        StaticBox::doRender(dc);
        const wxColour fill = state_handler.colorFor(background_color);
        if (fill.IsOk() && fill != m_synced) {
            m_synced = fill;
            for (wxWindow *child : GetChildren()) {
                child->SetBackgroundColour(fill);
                child->Refresh(false);
            }
        }
    }

private:
    void apply_fills()
    {
        wxColour fill  = role(MD3::Role::SurfaceContainerLow);
        wxColour hover = role(MD3::Role::SurfaceContainerHigh);
        if (m_checked) {
            fill  = role(MD3::Role::SecondaryContainer);
            hover = blend(fill, role(MD3::Role::OnSecondaryContainer), 0.08);
        }
        // The hover entry is always present, so the hover bindings stay in place;
        // while hover is off it simply equals the resting fill.
        if (!m_hover_enabled) hover = fill;
        SetBackgroundColor(StateColor(std::pair<wxColour, int>(hover, (int) StateColor::Hovered),
                                      std::pair<wxColour, int>(fill, (int) StateColor::Normal)));
    }

    bool     m_checked       = false;
    bool     m_hover_enabled = true;
    wxColour m_synced;
};

MultiSliceDialog::MultiSliceDialog(wxWindow *parent)
    : MD3Dialog(parent, _L("Multi-slice"), wxEmptyString, MaterialIcon::Stack,
                MD3Dialog::Options{/*resizable*/ true, /*forced_dark*/ false})
    , m_timer(this)
{
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
    // Escape takes the same path as the header close.
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &evt) {
        if (evt.GetKeyCode() == WXK_ESCAPE) {
            Close();
            return;
        }
        evt.Skip();
    });
    build();
    SetMinSize(FromDIP(wxSize(560, 520)));
    SetSize(FromDIP(wxSize(640, 680)));
    CenterOnParent();
}

MultiSliceDialog::~MultiSliceDialog()
{
    m_timer.Stop();
}

void MultiSliceDialog::OnHeaderClose()
{
    // Not EndModal(): that would destroy the dialog under a run in progress.
    // Close() reaches the close handler above, which stops the run first.
    Close();
}

void MultiSliceDialog::build()
{
    wxBoxSizer *content = GetContentSizer();

    // 1. Intro. In bilingual mode the decorator adds the Cantonese paragraph below.
    auto *intro = new Label(this, Label::Body_13,
        _L("Slice this plate once for each ink you choose and save one sliced file per ink, ready for the printer's USB drive. "
           "For each file every ink slot uses the chosen ink; slot colours stay. Your current inks come back when it is done."),
        LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    intro->SetMinSize(wxSize(FromDIP(200), -1));
    GrowWithWrap(intro);
    intro->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    content->Add(intro, 0, wxEXPAND);

    // 2. Search, directly on the dialog surface so its fill reads as an input.
    m_search = new SearchField(this, _L("Search inks by name or type"));
    m_search->SetOnQuery([this](const wxString &) { filter_inks(); });
    m_search->SetOnRegexToggle([this](bool) { filter_inks(); });
    content->Add(m_search, 0, wxEXPAND | wxTOP, FromDIP(kBodyGap));

    // 3. The ink list well, 4. its status tags (inside each row).
    build_list_well(content);

    // 5. Summary line: a glyph, the text, and the progress bar while a run is going.
    auto *summary = new wxBoxSizer(wxVERTICAL);
    auto *summary_line = new wxBoxSizer(wxHORIZONTAL);
    if (MaterialIcon::available()) {
        m_summary_icon = new wxStaticBitmap(this, wxID_ANY, glyph_bitmap(this, MaterialIcon::Info, MD3::Role::OnSurfaceVariant));
        summary_line->Add(m_summary_icon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    }
    m_summary = new Label(this, Label::Body_13, _L("Choose at least two inks. Each one gets its own sliced file."),
                          LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_summary->SetMinSize(wxSize(FromDIP(200), -1));
    GrowWithWrap(m_summary);
    summary_line->Add(m_summary, 1, wxALIGN_CENTER_VERTICAL);
    summary->Add(summary_line, 0, wxEXPAND);
    m_progress = new ProgressBar(this, wxID_ANY, 1, wxDefaultPosition, wxSize(-1, FromDIP(8)), false);
    m_progress->Hide();
    summary->Add(m_progress, 0, wxEXPAND | wxTOP, FromDIP(kProgressGap));
    content->Add(summary, 0, wxEXPAND | wxTOP, FromDIP(kBodyGap));

    // 6. Destination well.
    build_destination_well(content);

    // Footer: Show files on the left, Close and Slice and save on the right.
    build_footer();

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
    update_status_column();
    update_summary();
}

void MultiSliceDialog::build_list_well(wxBoxSizer *content)
{
    auto *well = new TonalSurface(this, MD3::Role::SurfaceContainerLow, MD3::Metrics::radius_well);
    m_list_well = well;
    auto *well_sizer = new wxBoxSizer(wxVERTICAL);

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

    // Header row: INK at the name column, STATUS at the status column.
    auto *header = new wxBoxSizer(wxHORIZONTAL);
    header->SetMinSize(wxSize(0, FromDIP(kHeaderHeight)));
    header->AddSpacer(FromDIP(kRowPadX + kCheckWidth + kCheckGap));
    auto *ink_header = new Label(well, Label::Head_11, header_text(_L("Ink")));
    ink_header->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    header->Add(ink_header, 1, wxALIGN_CENTER_VERTICAL);
    header->AddSpacer(FromDIP(kStatusGap));
    m_header_status_col = new wxBoxSizer(wxHORIZONTAL);
    m_header_status_col->AddStretchSpacer();
    auto *status_header = new Label(well, Label::Head_11, header_text(_L("Status")));
    status_header->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    m_header_status_col->Add(status_header, 0, wxALIGN_CENTER_VERTICAL);
    header->Add(m_header_status_col, 0, wxALIGN_CENTER_VERTICAL);
    header->AddSpacer(FromDIP(kRowPadX));
    well_sizer->Add(header, 0, wxEXPAND);

    m_list = new MD3ScrolledWindow(well, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    m_list->SetBackgroundColour(role(MD3::Role::SurfaceContainerLow));
    m_list->SetScrollRate(0, FromDIP(12));
    m_list_sizer = new wxBoxSizer(wxVERTICAL);
    for (Ink &ink : m_inks) build_ink_row(ink);

    // The empty and no-match message: a glyph over the text, centred.
    if (MaterialIcon::available()) {
        m_empty_icon = new wxStaticBitmap(m_list, wxID_ANY, glyph_bitmap(m_list, MaterialIcon::Info, MD3::Role::OnSurfaceVariant));
        m_empty_icon->SetBackgroundColour(m_list->GetBackgroundColour());
        m_list_sizer->Add(m_empty_icon, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(kEmptyPad));
    }
    m_empty_text = new Label(m_list, Label::Body_13, wxString(), LB_AUTO_WRAP | wxST_NO_AUTORESIZE | wxALIGN_CENTRE_HORIZONTAL);
    m_empty_text->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    m_list_sizer->Add(m_empty_text, 0, wxEXPAND | wxALL, FromDIP(kEmptyPad));
    if (m_inks.empty())
        show_empty_state(true, _L("No inks fit the current printer. Add or show inks in the ink list first."));
    else
        show_empty_state(false, wxString());

    m_list->SetSizer(m_list_sizer);
    well_sizer->Add(m_list, 1, wxEXPAND);

    auto *padded = new wxBoxSizer(wxVERTICAL);
    padded->Add(well_sizer, 1, wxEXPAND | wxALL, FromDIP(kWellPad));
    well->SetSizer(padded);
    // Three rows plus the header at least.
    well->SetMinSize(wxSize(-1, FromDIP(2 * kWellPad + kHeaderHeight + 3 * (kRowHeight + kRowGap))));
    content->Add(well, 1, wxEXPAND | wxTOP, FromDIP(kBodyGap));
}

void MultiSliceDialog::build_ink_row(Ink &ink)
{
    auto *row = new InkRow(m_list);
    ink.row   = row;

    auto *columns = new wxBoxSizer(wxHORIZONTAL);
    ink.check = new ::CheckBox(row);
    ink.check->SetName(wxString::FromUTF8(ink.preset));
    ink.check->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) { update_summary(); e.Skip(); });
    columns->Add(ink.check, 0, wxALIGN_CENTER_VERTICAL);
    columns->AddSpacer(FromDIP(kCheckGap));

    // Name and type tag on one line; a failed ink adds its detail line below.
    auto *middle = new wxBoxSizer(wxVERTICAL);
    auto *line   = new wxBoxSizer(wxHORIZONTAL);
    const wxString name = wxControl::EscapeMnemonics(wxString::FromUTF8(ink.preset));
    ink.name = new Label(row, Label::Body_13, name, wxST_ELLIPSIZE_END);
    ink.name->SetMinSize(wxSize(FromDIP(60), -1));
    ink.name->SetToolTip(wxString::FromUTF8(ink.preset));
    line->Add(ink.name, 1, wxALIGN_CENTER_VERTICAL);
    if (!ink.type.empty()) {
        ink.type_tag = new Tag(row);
        ink.type_tag->SetText(wxString::FromUTF8(ink.type));
        line->AddSpacer(FromDIP(kTagGap));
        line->Add(ink.type_tag, 0, wxALIGN_CENTER_VERTICAL);
    }
    middle->Add(line, 0, wxEXPAND);
    ink.detail = new Label(row, Label::Body_12, wxString(), wxST_ELLIPSIZE_END);
    ink.detail->SetMinSize(wxSize(FromDIP(60), -1));
    ink.detail->SetForegroundColour(role(MD3::Role::Error));
    ink.detail->Hide();
    middle->Add(ink.detail, 0, wxEXPAND | wxTOP, FromDIP(2));
    columns->Add(middle, 1, wxALIGN_CENTER_VERTICAL);
    columns->AddSpacer(FromDIP(kStatusGap));

    // The status column has a fixed width; the tag sits at its right end.
    ink.status_col = new wxBoxSizer(wxHORIZONTAL);
    ink.status_col->SetMinSize(wxSize(FromDIP(kStatusCol), -1));
    ink.status_col->AddStretchSpacer();
    ink.status = new Tag(row);
    ink.status->Hide();
    ink.status_col->Add(ink.status, 0, wxALIGN_CENTER_VERTICAL);
    columns->Add(ink.status_col, 0, wxALIGN_CENTER_VERTICAL);

    auto *padded = new wxBoxSizer(wxHORIZONTAL);
    padded->AddSpacer(FromDIP(kRowPadX));
    padded->Add(columns, 1, wxEXPAND);
    padded->AddSpacer(FromDIP(kRowPadX));
    auto *vertical = new wxBoxSizer(wxVERTICAL);
    vertical->Add(padded, 1, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(kRowPadY));
    row->SetSizer(vertical);
    row->SetMinSize(wxSize(-1, FromDIP(kRowHeight)));

    // Hovering a child counts as hovering the row.
    for (wxWindow *child : {static_cast<wxWindow *>(ink.check), static_cast<wxWindow *>(ink.name),
                            static_cast<wxWindow *>(ink.detail), static_cast<wxWindow *>(ink.status)})
        row->hover_through(child);
    if (ink.type_tag) row->hover_through(ink.type_tag);

    // A click anywhere on the row toggles its box.
    auto toggle = [this, &ink](wxMouseEvent &) {
        if (m_running) return;
        ink.check->SetValue(!ink.check->GetValue());
        update_summary();
    };
    row->Bind(wxEVT_LEFT_UP, toggle);
    ink.name->Bind(wxEVT_LEFT_UP, toggle);
    ink.detail->Bind(wxEVT_LEFT_UP, toggle);
    ink.status->Bind(wxEVT_LEFT_UP, toggle);
    if (ink.type_tag) ink.type_tag->Bind(wxEVT_LEFT_UP, toggle);

    m_list_sizer->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(kRowGap));
}

void MultiSliceDialog::build_destination_well(wxBoxSizer *content)
{
    m_dest_well = new TonalSurface(this, MD3::Role::SurfaceContainerLow, MD3::Metrics::radius_well);
    auto *line_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto *text_sizer = new wxBoxSizer(wxVERTICAL);

    // Line 1: SAVE TO, and a green tag when the destination is the USB drive.
    auto *label_line = new wxBoxSizer(wxHORIZONTAL);
    auto *save_to    = new Label(m_dest_well, Label::Head_11, header_text(_L("Save to")));
    save_to->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    label_line->Add(save_to, 0, wxALIGN_CENTER_VERTICAL);
    m_usb_tag = new Tag(m_dest_well);
    m_usb_tag->SetKind(Tag::Kind::Green);
    m_usb_tag->SetText(with_cantonese(_L("USB drive")));
    m_usb_tag->Hide();
    label_line->Add(m_usb_tag, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    text_sizer->Add(label_line, 0, wxEXPAND);

    // Line 2: the folder once, in the monospace face, middle-ellipsised.
    m_destination = new Label(m_dest_well, Label::Mono_12, wxString(), wxST_ELLIPSIZE_MIDDLE);
    m_destination->SetMinSize(wxSize(FromDIP(60), -1));
    text_sizer->Add(m_destination, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_destination_empty = new Label(m_dest_well, Label::Body_13, _L("No folder chosen yet."), wxST_ELLIPSIZE_END);
    m_destination_empty->SetMinSize(wxSize(FromDIP(60), -1));
    m_destination_empty->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    text_sizer->Add(m_destination_empty, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_destination_empty->Hide();
    line_sizer->Add(text_sizer, 1, wxALIGN_CENTER_VERTICAL);
    line_sizer->AddSpacer(FromDIP(12));

    m_change = new Button(m_dest_well, _L("Change folder"));
    m_change->SetVariant(Button::Variant::Outlined);
    m_change->SetButtonSize(Button::Size::Action);
    m_change->SetGlyph(MaterialIcon::FolderOpen, 20);
    m_change->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { choose_destination(); });
    line_sizer->Add(m_change, 0, wxALIGN_CENTER_VERTICAL);

    auto *padded = new wxBoxSizer(wxVERTICAL);
    padded->Add(line_sizer, 0, wxEXPAND | wxALL, FromDIP(kDestWellPad));
    m_dest_well->SetSizer(padded);
    content->Add(m_dest_well, 0, wxEXPAND | wxTOP, FromDIP(kBodyGap));
}

void MultiSliceDialog::build_footer()
{
    // Show files is a utility follow-up: a text button at the left of the footer,
    // before the shell's stretch spacer and without the footer-button left margin.
    m_open = new Button(this, _L("Show files"));
    m_open->SetVariant(Button::Variant::Text);
    m_open->SetButtonSize(Button::Size::Action);
    m_open->SetGlyph(MaterialIcon::FolderOpen, 20);
    m_open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { desktop_open_any_folder(m_folder.string()); });
    m_open->Hide();
    GetFooterSizer()->Insert(0, m_open, 0, wxALIGN_CENTER_VERTICAL);

    m_close = new Button(this, _L("Close"));
    m_close->SetVariant(Button::Variant::Outlined);
    m_close->SetButtonSize(Button::Size::Action);
    m_close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Close(); });
    AddFooterButton(m_close);

    m_start = new Button(this, _L("Slice and save"));
    m_start->SetVariant(Button::Variant::Filled);
    m_start->SetButtonSize(Button::Size::Action);
    m_start->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { start(); });
    AddFooterButton(m_start);
}

void MultiSliceDialog::show_empty_state(bool show, const wxString &message)
{
    if (!m_empty_text) return;
    if (show) m_empty_text->SetLabel(message);
    m_empty_text->Show(show);
    if (m_empty_icon) m_empty_icon->Show(show);
    if (m_list) {
        m_list->Layout();
        m_list->FitInside();
    }
}

void MultiSliceDialog::filter_inks()
{
    SearchField::MatchPass pass(m_search->GetValue(), m_search->IsRegexEnabled(), m_search->IsCaseSensitive(),
                                m_search->IsWholeWord(), m_search->IsMultiline());
    int shown = 0;
    for (Ink &ink : m_inks) {
        const bool show = pass.matches(wxString::FromUTF8(ink.preset + " " + ink.type)) || ink.check->GetValue();
        ink.row->Show(show);
        shown += show ? 1 : 0;
    }
    if (!m_inks.empty()) show_empty_state(shown == 0, _L("No matches."));
    m_list->Layout();
    m_list->FitInside();
}

void MultiSliceDialog::set_summary(const wxString &text, uint32_t glyph, MD3::Role glyph_role, MD3::Role text_role)
{
    m_summary->SetLabel(text);
    m_summary->SetForegroundColour(role(text_role));
    if (m_summary_icon) m_summary_icon->SetBitmap(glyph_bitmap(this, glyph, glyph_role));
}

void MultiSliceDialog::update_rows()
{
    for (Ink &ink : m_inks) {
        const bool checked = ink.check->GetValue();
        ink.row->set_checked(checked);
        ink.row->set_hover_enabled(!m_running);
        // Names keep their rest colours while a run is going, so they stay
        // readable next to their live status tags.
        ink.name->SetForegroundColour(role(checked ? MD3::Role::OnSecondaryContainer : MD3::Role::OnSurface));
        ink.name->Refresh();
    }
}

void MultiSliceDialog::update_summary()
{
    int chosen = 0;
    for (const Ink &ink : m_inks) chosen += ink.check->GetValue() ? 1 : 0;
    update_rows();
    bool show_progress = false;
    if (m_running && m_cancel) {
        set_summary(_L("Stopping after the ink being sliced, then putting your inks back."), MaterialIcon::Stop,
                    MD3::Role::OnSurfaceVariant, MD3::Role::OnSurfaceVariant);
    } else if (m_running) {
        set_summary(wxString::Format(_L("Slicing %d of %d. Leave the plate as it is until this finishes."),
                                     int(std::min(m_next, m_queue.size())), int(m_queue.size())),
                    MaterialIcon::Sync, MD3::Role::Primary, MD3::Role::OnSurface);
        show_progress = true;
        // The current ink is queued but not finished yet.
        m_progress->SetRange(std::max<int>(1, int(m_queue.size())));
        m_progress->SetValue(m_next > 0 ? int(m_next) - 1 : 0);
    } else if (chosen < 2) {
        set_summary(_L("Choose at least two inks. Each one gets its own sliced file."), MaterialIcon::Info,
                    MD3::Role::OnSurfaceVariant, MD3::Role::OnSurfaceVariant);
    } else {
        set_summary(wxString::Format(_L("%d inks chosen: %d sliced files will be saved."), chosen, chosen),
                    MaterialIcon::TaskAlt, MD3::Role::Primary, MD3::Role::OnSurface);
    }
    m_progress->Show(show_progress);
    m_start->Enable(!m_running && chosen >= 2 && !m_folder.empty());
    Layout();
    // Text set after the window was shown is paired with its Cantonese again.
    I18N::refresh_bilingual_decoration(this);
}

// The status column is as wide as the widest tag showing, between its resting
// width and its limit, and the list header's column follows it.
void MultiSliceDialog::update_status_column()
{
    int width = FromDIP(kStatusCol);
    for (const Ink &ink : m_inks)
        if (ink.status && ink.status->IsShown()) width = std::max(width, ink.status->GetBestSize().x);
    width = std::min(width, FromDIP(kStatusColMax));
    for (Ink &ink : m_inks)
        if (ink.status_col) ink.status_col->SetMinSize(wxSize(width, -1));
    if (m_header_status_col) m_header_status_col->SetMinSize(wxSize(width, -1));
}

void MultiSliceDialog::set_destination(const boost::filesystem::path &folder, bool removable)
{
    m_folder    = folder;
    m_removable = removable;
    const bool has_folder = !folder.empty();
    if (has_folder) {
        const wxString path = from_path(folder);
        m_destination->SetLabel(wxControl::EscapeMnemonics(path));
        m_destination->SetToolTip(path);
    }
    m_destination->Show(has_folder);
    m_destination_empty->Show(!has_folder);
    m_usb_tag->Show(has_folder && removable);
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
    // Stop abandons the remaining inks, so it takes the Danger variant.
    m_close->SetLabel(running ? _L("Stop") : _L("Close"));
    m_close->SetVariant(running ? Button::Variant::Danger : Button::Variant::Outlined);
    update_summary();
}

void MultiSliceDialog::show_status(Ink &ink, Status state, const wxString &detail)
{
    ink.state = state;
    wxString    text;
    Tag::Kind   kind  = Tag::Kind::Neutral;
    uint32_t    glyph = 0;
    switch (state) {
    case Status::Idle:    break;
    case Status::Waiting: text = _L("Waiting");    kind = Tag::Kind::Neutral; glyph = MaterialIcon::Schedule;    break;
    case Status::Slicing: text = _L("Slicing");    kind = Tag::Kind::Tonal;   glyph = MaterialIcon::Sync;        break;
    case Status::Saved:   text = _L("File saved"); kind = Tag::Kind::Green;   glyph = MaterialIcon::CheckCircle; break;
    case Status::Failed:  text = _L("Not saved");  kind = Tag::Kind::Red;     glyph = MaterialIcon::Error;       break;
    case Status::Skipped: text = _L("Stopped");    kind = Tag::Kind::Neutral; glyph = MaterialIcon::Stop;        break;
    }
    if (state == Status::Idle) {
        ink.status->Hide();
    } else {
        // A short status has no room to grow into, so the decorator never pairs it;
        // in bilingual mode it carries its Cantonese itself, "Saved · 存咗".
        ink.status->SetKind(kind);
        ink.status->SetGlyph(glyph);
        ink.status->SetText(with_cantonese(text));
        ink.status->Show();
    }
    // A failed ink says why on a line of its own under the name.
    if (state == Status::Failed && !detail.empty()) {
        ink.detail->SetLabel(detail);
        ink.detail->SetToolTip(detail);
        ink.detail->Show();
    } else {
        ink.detail->Hide();
    }
    update_status_column();
    ink.row->Layout();
    m_list->Layout();
    m_list->FitInside();
    Layout();
    I18N::refresh_bilingual_decoration(this);
}

void MultiSliceDialog::start()
{
    Plater *plater = wxGetApp().plater();
    if (m_running || plater == nullptr || current_plate() == nullptr) return;
    if (!current_plate()->has_printable_instances()) {
        set_summary(_L("This plate has nothing to print. Put a model on it first."), MaterialIcon::Error,
                    MD3::Role::Error, MD3::Role::OnSurface);
        return;
    }
    boost::system::error_code ec;
    if (!boost::filesystem::is_directory(m_folder, ec)) {
        set_summary(_L("The folder cannot be reached. Plug the USB drive back in or choose another folder."),
                    MaterialIcon::Error, MD3::Role::Error, MD3::Role::OnSurface);
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
    if (stopped)
        set_summary(wxString::Format(_L("Stopped. %d of %d sliced files saved; your inks are back."), saved, int(m_queue.size())),
                    MaterialIcon::Stop, MD3::Role::OnSurfaceVariant, MD3::Role::OnSurface);
    else
        set_summary(wxString::Format(_L("Done. %d of %d sliced files saved; your inks are back."), saved, int(m_queue.size())),
                    MaterialIcon::TaskAlt, MD3::Role::Primary, MD3::Role::OnSurface);
    BOOST_LOG_TRIVIAL(info) << "multi-slice: saved " << saved << " of " << m_queue.size();
    Layout();
    I18N::refresh_bilingual_decoration(this);
}

void MultiSliceDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    MD3Dialog::on_dpi_changed(suggested_rect);
    // The wells and rows pin their corner radius, so give them back their DIP size.
    if (m_list_well) static_cast<TonalSurface *>(m_list_well)->rescale();
    if (m_dest_well) static_cast<TonalSurface *>(m_dest_well)->rescale();
    for (Ink &ink : m_inks) {
        ink.row->rescale();
        ink.row->SetMinSize(wxSize(-1, FromDIP(kRowHeight)));
    }
    update_status_column();
    Layout();
    Refresh();
}

} // namespace Slic3r::GUI
