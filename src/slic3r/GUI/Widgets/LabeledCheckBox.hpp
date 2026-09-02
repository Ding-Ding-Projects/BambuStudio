#ifndef slic3r_GUI_LabeledCheckBox_hpp_
#define slic3r_GUI_LabeledCheckBox_hpp_

#include <wx/panel.h>
#include <wx/string.h>

#include "CheckBox.hpp"

class Label;

// A stock wxCheckBox replacement built from the kit primitives: the MD3
// CheckBox glyph followed by a Label, laid out as one row (the kit Checkbox
// component is exactly that pair). It mirrors the wxCheckBox surface the
// dialogs already use — GetValue/SetValue/IsChecked, SetLabel, SetFont,
// SetForegroundColour, SetToolTip, Enable — and re-emits wxEVT_CHECKBOX from
// itself when the glyph toggles or the label is clicked, so existing
// Bind(wxEVT_CHECKBOX, …) handlers keep working unchanged.
class LabeledCheckBox : public wxPanel
{
public:
    LabeledCheckBox(wxWindow *parent, const wxString &label, wxWindowID id = wxID_ANY);

    bool GetValue() const;
    bool IsChecked() const { return GetValue(); }
    void SetValue(bool value);

    void SetLabel(const wxString &label) override;
    bool SetFont(const wxFont &font) override;
    bool SetForegroundColour(const wxColour &colour) override;
    bool SetBackgroundColour(const wxColour &colour) override;
    void SetToolTip(const wxString &tip);
    bool Enable(bool enable = true) override;

    CheckBox *GetCheckBox() { return m_check; }
    Label    *GetLabelCtrl() { return m_label; }

    // Recolor the glyph to a workspace accent (Preview / Device).
    void SetColorScheme(MD3::ColorScheme scheme);
    void Rescale();

private:
    void emitChange();

    CheckBox *m_check { nullptr };
    Label    *m_label { nullptr };
};

#endif // slic3r_GUI_LabeledCheckBox_hpp_
