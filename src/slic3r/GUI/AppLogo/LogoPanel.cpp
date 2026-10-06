#include "LogoPanel.hpp"

#include "../Widgets/Button.hpp"
#include "../Widgets/CheckBox.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3ColorPicker.hpp"
#include "../Widgets/SpinInput.hpp"
#include <wx/file.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/wrapsizer.h>

namespace Slic3r { namespace GUI { namespace AppLogoUI {

namespace {
constexpr std::array<unsigned, 4> preview_sizes {{16, 24, 32, 64}};
}

wxBitmap bitmap(const AppLogo::Settings& settings, unsigned pixels)
{
    const auto rendered = AppLogo::render(AppLogo::preset(settings.preset), settings, pixels);
    if (!rendered.valid()) return wxBitmap();
    wxImage image(int(rendered.width), int(rendered.height), false);
    image.InitAlpha();
    auto* rgb = image.GetData();
    auto* alpha = image.GetAlpha();
    for (size_t i = 0; i < rendered.pixels.size(); ++i) {
        const auto& p = rendered.pixels[i];
        rgb[i * 3] = p.r; rgb[i * 3 + 1] = p.g; rgb[i * 3 + 2] = p.b; alpha[i] = p.a;
    }
    return wxBitmap(image);
}

bool load(const wxString& private_file, AppLogo::Settings& settings)
{
    wxFile file;
    if (!file.Open(private_file, wxFile::read)) return false;
    const auto length = file.Length();
    if (length <= 0 || length > 64) return false;
    std::string bytes(size_t(length), '\0');
    if (file.Read(&bytes[0], bytes.size()) != static_cast<wxFileOffset>(bytes.size())) return false;
    return AppLogo::deserialize(bytes, settings);
}

bool save(const wxString& private_file, const AppLogo::Settings& settings)
{
    const std::string bytes = AppLogo::serialize(settings);
    if (bytes.empty()) return false;
    wxTempFile file(private_file);
    return file.IsOpened() && file.Write(bytes.data(), bytes.size()) && file.Commit();
}

wxString LogoPanel::tr(const char* value) const
{
    return m_translate ? m_translate(value) : wxGetTranslation(wxString::FromUTF8(value));
}

LogoPanel::LogoPanel(wxWindow* parent, const wxString& private_file, Changed changed, Translate translate)
    : wxPanel(parent), m_file(private_file), m_changed(std::move(changed)), m_translate(std::move(translate))
{
    const bool exists = wxFile::Exists(m_file);
    const bool restored = exists && load(m_file, m_settings);
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* title = new Label(this, tr("Application logo"));
    outer->Add(title, 0, wxALL, FromDIP(8));
    auto* rows = new wxFlexGridSizer(2, FromDIP(6), FromDIP(10));
    rows->AddGrowableCol(1);
    auto row = [&](const char* label, wxWindow* control) {
        const auto name = tr(label);
        rows->Add(new Label(this, name), 0, wxALIGN_CENTER_VERTICAL);
        control->SetName(name);
        rows->Add(control, 1, wxEXPAND);
    };
    m_preset = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
    m_preset->Append(tr("Green printer")); m_preset->Append(tr("Blue layers")); m_preset->Append(tr("Amber nozzle"));
    row("Logo preset", m_preset);
    m_fit = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
    m_fit->Append(tr("Contain: show the whole mark")); m_fit->Append(tr("Cover: crop to fill"));
    row("Image fit", m_fit);
    auto spin = [&](const char* label, int maximum) {
        auto* control = new SpinInput(this, "0");
        control->SetRange(0, maximum); row(label, control); return control;
    };
    m_x = spin("Horizontal focal point (%)", 100);
    m_y = spin("Vertical focal point (%)", 100);
    m_inset = spin("Safe area inset (%)", 25);
    m_transparent = new CheckBox(this);
    m_transparent->SetName(tr("Transparent background"));
    row("Transparent background", m_transparent);
    m_background = new Button(this, tr("Choose background color"));
    m_background->SetVariant(Button::Variant::Outlined);
    row("Background color", m_background);
    outer->Add(rows, 0, wxEXPAND | wxALL, FromDIP(8));
    auto* previews = new wxWrapSizer(wxHORIZONTAL);
    for (const auto size : preview_sizes) {
        auto* column = new wxBoxSizer(wxVERTICAL);
        auto* view = new wxStaticBitmap(this, wxID_ANY, bitmap(m_settings, size));
        view->SetName(tr("Logo preview") + wxString::Format(" %u px", size));
        m_previews.push_back(view);
        column->Add(view, 0, wxALIGN_CENTER | wxALL, FromDIP(5));
        column->Add(new Label(this, wxString::Format("%u px", size)), 0, wxALIGN_CENTER);
        previews->Add(column, 0, wxALL, FromDIP(5));
    }
    outer->Add(previews, 0, wxEXPAND | wxALL, FromDIP(8));
    auto* limitation = new Label(this,
        tr("Custom image import is unavailable: the packaged isolated decoder has not been verified. No file is read. Presets use local geometry. Cover crops the image; small previews use nearest-neighbor sampling. Transparent marks may have low contrast on some backgrounds. This changes in-app presentation only."));
    limitation->Wrap(FromDIP(420));
    outer->Add(limitation, 0, wxEXPAND | wxALL, FromDIP(8));
    auto* import = new Button(this, tr("Import custom image (unavailable)"));
    import->SetVariant(Button::Variant::Outlined);
    import->SetName(tr("Import custom image (unavailable)")); import->Disable();
    outer->Add(import, 0, wxALL, FromDIP(8));
    auto* reset = new Button(this, tr("Reset logo"));
    reset->SetVariant(Button::Variant::Outlined);
    reset->SetName(tr("Reset logo")); outer->Add(reset, 0, wxALL, FromDIP(8));
    m_status = new Label(this, exists && !restored ?
        tr("Saved logo settings are invalid. The default mark is shown; no saved data was overwritten.") :
        restored ? tr("Saved logo settings loaded locally.") : tr("Default logo. No custom image loaded."));
    m_status->Wrap(FromDIP(420)); outer->Add(m_status, 0, wxEXPAND | wxALL, FromDIP(8));
    SetSizer(outer);
    refresh();
    m_preset->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) { apply(); });
    m_fit->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) { apply(); });
    for (auto* control : {m_x, m_y, m_inset}) {
        control->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&) { apply(); });
    }
    m_transparent->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) { apply(); });
    m_background->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        MD3ColorPickerDialog::Options options; options.opacity = false; options.title = tr("Background color");
        MD3ColorPickerDialog picker(this, m_color, options);
        if (picker.ShowModal() == wxID_OK) { m_color = picker.GetColour(); apply(); }
    });
    reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        const AppLogo::Settings defaults;
        if (!save(m_file, defaults)) { m_status->SetLabel(tr("Unable to save logo settings. The previous logo is retained.")); return; }
        m_settings = defaults; refresh();
        m_status->SetLabel(tr("Default logo restored and saved locally."));
        if (m_changed) m_changed(m_settings);
    });
}

void LogoPanel::refresh()
{
    m_preset->SetSelection(int(m_settings.preset)); m_fit->SetSelection(int(m_settings.fit));
    m_x->SetValue(int(m_settings.focal_x)); m_y->SetValue(int(m_settings.focal_y)); m_inset->SetValue(int(m_settings.inset));
    m_transparent->SetValue(m_settings.transparent);
    const auto& p = m_settings.background;
    m_color = wxColour(p.r, p.g, p.b);
    m_background->SetLabel(m_color.GetAsString(wxC2S_HTML_SYNTAX)); m_background->Enable(!m_settings.transparent);
    for (size_t i = 0; i < m_previews.size(); ++i) m_previews[i]->SetBitmap(bitmap(m_settings, preview_sizes[i]));
    Layout();
}

void LogoPanel::apply()
{
    auto candidate = m_settings;
    candidate.preset = unsigned(m_preset->GetSelection()); candidate.fit = AppLogo::Fit(m_fit->GetSelection());
    candidate.focal_x = unsigned(m_x->GetValue()); candidate.focal_y = unsigned(m_y->GetValue());
    candidate.inset = unsigned(m_inset->GetValue()); candidate.transparent = m_transparent->GetValue();
    const auto color = m_color;
    candidate.background = {color.Red(), color.Green(), color.Blue(), 255};
    if (!candidate.valid() || !save(m_file, candidate)) {
        refresh(); m_status->SetLabel(tr("Unable to save logo settings. The previous logo is retained.")); return;
    }
    m_settings = candidate; refresh(); m_status->SetLabel(tr("Logo settings saved locally."));
    if (m_changed) m_changed(m_settings);
}

} } }
