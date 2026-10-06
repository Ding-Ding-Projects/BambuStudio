#pragma once

#include "libslic3r/AppLogo/Logo.hpp"
#include <wx/panel.h>
#include <wx/bitmap.h>
#include <wx/colour.h>
#include <functional>

class ComboBox;
class SpinInput;
class CheckBox;
class Button;
class Label;
class wxStaticBitmap;
class wxStaticText;

namespace Slic3r { namespace GUI { namespace AppLogoUI {

wxBitmap bitmap(const AppLogo::Settings& settings, unsigned pixels);
bool load(const wxString& private_file, AppLogo::Settings& settings);
bool save(const wxString& private_file, const AppLogo::Settings& settings);

// Integrator owns the private, writable data directory and all presentation
// consumers. Changed fires only after atomic persistence succeeds. It never
// changes application/package identity or the operating system's installed icon.
class LogoPanel : public wxPanel {
public:
    using Changed = std::function<void(const AppLogo::Settings&)>;
    using Translate = std::function<wxString(const char*)>;
    LogoPanel(wxWindow* parent, const wxString& private_file,
        Changed changed = {}, Translate translate = {});
    const AppLogo::Settings& settings() const { return m_settings; }
private:
    void refresh();
    void apply();
    wxString tr(const char* value) const;
    wxString m_file;
    Changed m_changed;
    Translate m_translate;
    AppLogo::Settings m_settings;
    ComboBox* m_preset = nullptr;
    ComboBox* m_fit = nullptr;
    SpinInput* m_x = nullptr;
    SpinInput* m_y = nullptr;
    SpinInput* m_inset = nullptr;
    CheckBox* m_transparent = nullptr;
    Button* m_background = nullptr;
    wxColour m_color;
    std::vector<wxStaticBitmap*> m_previews;
    Label* m_status = nullptr;
};

} } }
