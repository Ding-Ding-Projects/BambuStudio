#include "LogoPanel.hpp"
#include "LogoRender.hpp"

#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/CheckBox.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledRadioButton.hpp"
#include "../Widgets/MD3ColorPicker.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/StateColor.hpp"
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <array>
#include <initializer_list>

namespace Slic3r { namespace GUI { namespace AppLogoUI {

namespace {
constexpr std::array<unsigned, 4> preview_sizes {{16, 24, 32, 64}};
constexpr int wrap_width = 520;
}

LogoPanel::LogoPanel(wxWindow* parent, const AppLogo::Resolved& initial, Commit on_commit)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
    , m_commit(std::move(on_commit))
    , m_settings(initial.settings)
    , m_provenance(initial.provenance)
    , m_group(std::make_unique<RadioGroup>())
{
    SetBackgroundColour(parent->GetBackgroundColour());
    auto caption = [this](const wxString& text) {
        auto* label = new Label(this, Label::Body_12, text, 0, wxSize(FromDIP(wrap_width), -1));
        label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        label->Wrap(FromDIP(wrap_width));
        return label;
    };
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(caption(_L("Choose the mark shown in the title bar, the window and taskbar icon, the About dialog and the startup screen. It changes presentation only: the installed app, its shortcuts and its updates keep the shipped identity.")),
        0, wxEXPAND | wxBOTTOM, FromDIP(8));

    // Source picker: a radio group (radio role, checked state, arrow keys)
    // filtered by its own search field, whose ".*" toggle and tune button give
    // the anchored regex builder.
    m_search = new SearchField(this, _L("Search logo sources"));
    m_search->SetOnQuery([this](const wxString&) { filter_sources(); });
    m_search->SetOnRegexToggle([this](bool) { filter_sources(); });
    outer->Add(m_search, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    m_names = {_L("Shipped mark"), _L("Green printer"), _L("Blue layers"), _L("Amber nozzle")};
    auto* source_box = new wxBoxSizer(wxVERTICAL);
    for (size_t i = 0; i < AppLogo::sources().size(); ++i) {
        auto* radio = new LabeledRadioButton(this, m_names[i]);
        radio->SetName(m_names[i]);
        // Bound before the group joins: wx calls the latest binding first, so the
        // group clears the other rows before this commit can roll them back.
        radio->Bind(wxEVT_RADIOBUTTON, [this, i](wxCommandEvent& e) { e.Skip(); select_source(i); });
        m_group->Add(radio);
        source_box->Add(radio, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
        m_rows.push_back({radio, m_names[i] + " " + wxString::FromUTF8(AppLogo::sources()[i].id)});
    }
    // A custom image is a real source the contract asks for, so it is listed,
    // but it stays disabled until a verified isolated decoder ships: no file is
    // ever opened here.
    auto* custom = new LabeledRadioButton(this, _L("Custom image (unavailable)"));
    custom->SetName(_L("Custom image (unavailable)"));
    custom->SetToolTip(_L("Custom images need the isolated image decoder, which this build does not include yet."));
    m_group->Add(custom);
    custom->Disable();
    source_box->Add(custom, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
    m_rows.push_back({custom, _L("Custom image (unavailable)") + " custom"});
    outer->Add(source_box, 0, wxEXPAND);
    m_no_match = caption(_L("No logo source matches your search."));
    m_no_match->Hide();
    outer->Add(m_no_match, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    // Presentation controls for the generated presets.
    auto* rows = new wxFlexGridSizer(2, FromDIP(6), FromDIP(10));
    rows->AddGrowableCol(1);
    auto row = [&](const wxString& name, wxWindow* control) {
        rows->Add(new Label(this, Label::Body_13, name), 0, wxALIGN_CENTER_VERTICAL);
        control->SetName(name);
        rows->Add(control, 1, wxEXPAND);
    };
    m_fit = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
    m_fit->Append(_L("Contain: show the whole mark")); m_fit->Append(_L("Cover: crop to fill"));
    row(_L("Image fit"), m_fit);
    auto spin = [&](const wxString& name, int maximum) {
        auto* control = new SpinInput(this, "0");
        control->SetRange(0, maximum); row(name, control); return control;
    };
    m_x = spin(_L("Horizontal focal point (%)"), 100);
    m_y = spin(_L("Vertical focal point (%)"), 100);
    m_inset = spin(_L("Safe area inset (%)"), 25);
    m_transparent = new CheckBox(this);
    row(_L("Transparent background"), m_transparent);
    m_background = new Button(this, _L("Choose background color"));
    m_background->SetVariant(Button::Variant::Outlined);
    m_background->SetButtonSize(Button::Size::Small);
    row(_L("Background color"), m_background);
    outer->Add(rows, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(8));
    m_edit_note = caption(_L("Fit, focal point, safe area and background apply to the presets. The shipped mark keeps its own artwork."));
    outer->Add(m_edit_note, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    // Real RGBA renders of the selection at the small chrome sizes.
    m_preview_sizer = new wxWrapSizer(wxHORIZONTAL);
    for (const auto size : preview_sizes) {
        auto* column = new wxBoxSizer(wxVERTICAL);
        auto* view = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap);
        view->SetName(wxString::Format(_L("Logo preview at %u pixels"), size));
        m_previews.push_back(view);
        column->Add(view, 0, wxALIGN_CENTER | wxALL, FromDIP(5));
        column->Add(new Label(this, Label::Body_12, wxString::Format(_L("%u px"), size)), 0, wxALIGN_CENTER);
        m_preview_sizer->Add(column, 0, wxALL, FromDIP(5));
    }
    outer->Add(m_preview_sizer, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
    m_preview_note = caption(_L("The title bar, the window and taskbar icon, the About dialog and the startup screen show their shipped artwork."));
    outer->Add(m_preview_note, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    outer->Add(caption(_L("Small sizes use nearest-neighbor sampling, so fine detail can drop out. Cover crops the mark. A transparent mark can lose contrast on some backgrounds; choose an opaque background if it does.")),
        0, wxEXPAND | wxBOTTOM, FromDIP(8));

    auto* actions = new wxWrapSizer(wxHORIZONTAL);
    auto* import = new Button(this, _L("Import custom image (unavailable)"));
    import->SetVariant(Button::Variant::Outlined);
    import->SetButtonSize(Button::Size::Small);
    import->SetName(_L("Import custom image (unavailable)"));
    import->Disable();
    auto* reset = new Button(this, _L("Reset to shipped mark"));
    reset->SetVariant(Button::Variant::Outlined);
    reset->SetButtonSize(Button::Size::Small);
    reset->SetName(_L("Reset to shipped mark"));
    actions->Add(import, 0, wxRIGHT | wxBOTTOM, FromDIP(8));
    actions->Add(reset, 0, wxBOTTOM, FromDIP(8));
    outer->Add(actions, 0, wxEXPAND);
    outer->Add(caption(_L("Custom image import is unavailable: this build has no verified isolated image decoder, so no image file is read.")),
        0, wxEXPAND | wxBOTTOM, FromDIP(8));

    m_status = new Label(this, Label::Body_13, wxEmptyString, 0, wxSize(FromDIP(wrap_width), -1));
    m_status->SetName(_L("App logo status"));
    outer->Add(m_status, 0, wxEXPAND);
    SetSizer(outer);

    refresh();
    set_status(resting_status(), m_provenance == AppLogo::Provenance::Invalid);

    m_fit->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) { apply_edits(); });
    for (auto* control : {m_x, m_y, m_inset})
        control->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&) { apply_edits(); });
    m_transparent->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { apply_edits(); });
    m_background->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        MD3ColorPickerDialog::Options options; options.opacity = false; options.title = _L("Background color");
        MD3ColorPickerDialog picker(this, m_color, options);
        if (picker.ShowModal() == wxID_OK) { m_color = picker.GetColour(); apply_edits(); }
    });
    reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (commit(AppLogo::Settings{}))
            set_status(_L("Shipped mark restored. The saved logo setting was cleared."), false);
    });
}

LogoPanel::~LogoPanel()
{
    // The radio group's destroy handlers point at the group, so the rows go
    // first, while it is still alive.
    DestroyChildren();
}

wxString LogoPanel::source_name(const AppLogo::Settings& settings) const
{
    const int index = AppLogo::source_index(settings);
    return index >= 0 && size_t(index) < m_names.size() ? m_names[size_t(index)] : m_names.front();
}

wxString LogoPanel::resting_status() const
{
    if (m_provenance == AppLogo::Provenance::Invalid)
        return _L("Saved logo settings are invalid. The shipped mark is shown, and the saved value is kept until you choose a logo or reset.");
    if (m_settings.shipped())
        return _L("Shipped mark in use. No custom image loaded.");
    return wxString::Format(_L("%s in use, saved in your settings. No custom image loaded."), source_name(m_settings));
}

void LogoPanel::relayout()
{
    // Rows appear and disappear (search, shipped versus preset, status text),
    // so the hosting scroll area has to re-measure its virtual size too.
    Layout();
    if (wxWindow* host = GetParent()) {
        host->Layout();
        host->FitInside();
    }
}

void LogoPanel::set_status(const wxString& text, bool error)
{
    m_status->SetForegroundColour(StateColor::semantic(error ? MD3::Role::Error : MD3::Role::OnSurfaceVariant));
    m_status->SetLabel(text);
    m_status->Wrap(FromDIP(wrap_width));
    relayout();
}

void LogoPanel::refresh()
{
    m_group->SetSelection(AppLogo::source_index(m_settings));
    const bool preset = !m_settings.shipped();
    m_fit->SetSelection(int(m_settings.fit));
    m_x->SetValue(int(m_settings.focal_x)); m_y->SetValue(int(m_settings.focal_y)); m_inset->SetValue(int(m_settings.inset));
    m_transparent->SetValue(m_settings.transparent);
    const auto& p = m_settings.background;
    m_color = wxColour(p.r, p.g, p.b);
    m_background->SetLabel(m_color.GetAsString(wxC2S_HTML_SYNTAX));
    for (wxWindow* control : std::initializer_list<wxWindow*>{m_fit, m_x, m_y, m_inset, m_transparent})
        control->Enable(preset);
    m_background->Enable(preset && !m_settings.transparent);
    m_edit_note->Show(!preset);
    for (size_t i = 0; i < m_previews.size(); ++i)
        m_previews[i]->SetBitmap(preset ? bitmap(m_settings, preview_sizes[i]) : wxBitmap());
    m_preview_sizer->ShowItems(preset);
    m_preview_note->Show(!preset);
    relayout();
}

void LogoPanel::filter_sources()
{
    SearchField::MatchPass pass(m_search->GetValue(), m_search->IsRegexEnabled(), m_search->IsCaseSensitive(),
                                m_search->IsWholeWord(), m_search->IsMultiline());
    bool any = false;
    for (auto& source : m_rows) {
        const bool match = pass.matches(source.haystack);
        source.radio->Show(match);
        any |= match;
    }
    m_no_match->Show(!any);
    relayout();
}

void LogoPanel::select_source(size_t index)
{
    const AppLogo::Settings candidate = AppLogo::with_source(m_settings, index);
    if (AppLogo::same(candidate, m_settings) && m_provenance != AppLogo::Provenance::Invalid) return;
    if (commit(candidate))
        set_status(wxString::Format(_L("%s applied and saved in your settings."), source_name(candidate)), false);
}

void LogoPanel::apply_edits()
{
    auto candidate = m_settings;
    candidate.fit = AppLogo::Fit(std::max(0, m_fit->GetSelection()));
    candidate.focal_x = unsigned(std::max(0, m_x->GetValue())); candidate.focal_y = unsigned(std::max(0, m_y->GetValue()));
    candidate.inset = unsigned(std::max(0, m_inset->GetValue())); candidate.transparent = m_transparent->GetValue();
    candidate.background = {m_color.Red(), m_color.Green(), m_color.Blue(), 255};
    if (commit(candidate))
        set_status(_L("Logo settings applied and saved in your settings."), false);
}

bool LogoPanel::commit(const AppLogo::Settings& candidate)
{
    if (!candidate.valid() || !m_commit || !m_commit(candidate)) {
        refresh();
        set_status(_L("Unable to save logo settings. The previous logo is kept."), true);
        return false;
    }
    m_settings = candidate;
    m_provenance = AppLogo::same(candidate, AppLogo::Settings{}) ? AppLogo::Provenance::Default : AppLogo::Provenance::Stored;
    refresh();
    return true;
}

} } }
