#pragma once

#include "LogoRender.hpp"
#include <wx/colour.h>
#include <wx/panel.h>
#include <functional>
#include <memory>
#include <vector>

class Button;
class CheckBox;
class ComboBox;
class Label;
class SearchField;
class SpinInput;
class wxBoxSizer;
class wxStaticBitmap;

namespace Slic3r { namespace GUI {

class LabeledRadioButton;
class RadioGroup;

namespace AppLogoUI {

// The app logo editor hosted in Preferences > Appearance. The host owns
// persistence and every chrome consumer: Commit persists and applies the
// candidate and returns false when it could not, in which case the panel keeps
// showing the previous logo. The panel never reads a file, never changes
// application or package identity, and never touches the installed
// operating-system icon.
class LogoPanel : public wxPanel {
public:
    using Commit = std::function<bool(const AppLogo::Settings&)>;
    LogoPanel(wxWindow* parent, const AppLogo::Resolved& initial, Commit on_commit);
    ~LogoPanel() override;
    const AppLogo::Settings& settings() const { return m_settings; }

private:
    struct SourceRow {
        LabeledRadioButton* radio = nullptr;
        wxString haystack;
    };
    void refresh();
    void relayout();
    void filter_sources();
    void select_source(size_t index);
    void apply_edits();
    bool commit(const AppLogo::Settings& candidate);
    void set_status(const wxString& text, bool error);
    wxString source_name(const AppLogo::Settings& settings) const;
    wxString resting_status() const;

    Commit m_commit;
    AppLogo::Settings m_settings;
    AppLogo::Provenance m_provenance;
    std::unique_ptr<RadioGroup> m_group;
    std::vector<SourceRow> m_rows;
    std::vector<wxString> m_names;
    SearchField* m_search = nullptr;
    Label* m_no_match = nullptr;
    ComboBox* m_fit = nullptr;
    SpinInput* m_x = nullptr;
    SpinInput* m_y = nullptr;
    SpinInput* m_inset = nullptr;
    CheckBox* m_transparent = nullptr;
    Button* m_background = nullptr;
    Label* m_edit_note = nullptr;
    wxColour m_color;
    std::vector<wxStaticBitmap*> m_previews;
    wxBoxSizer* m_preview_sizer = nullptr;
    Label* m_preview_note = nullptr;
    Label* m_status = nullptr;
};

} } }
