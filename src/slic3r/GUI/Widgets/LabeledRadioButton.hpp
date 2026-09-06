#ifndef slic3r_GUI_LabeledRadioButton_hpp_
#define slic3r_GUI_LabeledRadioButton_hpp_

#include <vector>

#include <wx/event.h>
#include <wx/panel.h>
#include <wx/string.h>

#include "RadioBox.hpp"

class Label;

namespace Slic3r { namespace GUI {

// The stock wxRadioButton replacement built from kit primitives: the drawn MD3
// RadioBox glyph followed by a Label, laid out as one focusable row. Earlier
// waves kept native radios on purpose because the bare RadioBox lost the radio
// role, the checked state, the accessible name and arrow-key navigation; this
// row supplies all four. It exposes the wxRadioButton surface the dialogs use
// (GetValue/SetValue, SetLabel, SetFont, SetForegroundColour, SetToolTip,
// Enable) and emits wxEVT_RADIOBUTTON from itself on every user activation,
// like the native MSW control does, so existing Bind(wxEVT_RADIOBUTTON, ...)
// handlers keep working unchanged.
//
// Mutual exclusion is the job of RadioGroup below; a row on its own is a
// single radio that the user can select but not deselect.
class LabeledRadioButton : public wxPanel
{
public:
    LabeledRadioButton(wxWindow *parent, const wxString &label = wxString(), wxWindowID id = wxID_ANY);

    bool GetValue() const;
    // Programmatic; never emits an event (matches wxRadioButton::SetValue).
    void SetValue(bool value);

    void SetLabel(const wxString &label) override;
    bool SetFont(const wxFont &font) override;
    bool SetForegroundColour(const wxColour &colour) override;
    bool SetBackgroundColour(const wxColour &colour) override;
    void SetToolTip(const wxString &tip);
    bool Enable(bool enable = true) override;

    bool AcceptsFocus() const override { return IsEnabled(); }
    bool AcceptsFocusFromKeyboard() const override { return IsEnabled(); }

    RadioBox *GetRadioBox() { return m_radio; }
    Label    *GetLabelCtrl() { return m_label; }

    // Recolor the selected dot to a workspace accent (Preview / Device).
    void SetColorScheme(MD3::ColorScheme scheme);
    void Rescale();

    // User-initiated select: sets the value and emits wxEVT_RADIOBUTTON.
    void Activate();

private:
    void emitSelected();
    void onPaint(wxPaintEvent &evt);
    void onKey(wxKeyEvent &evt);

#if wxUSE_ACCESSIBILITY
    class Accessible;
#endif

    RadioBox *m_radio { nullptr };
    Label    *m_label { nullptr };
};

// Mutual exclusion plus the WAI-ARIA radiogroup keyboard model for a set of
// LabeledRadioButtons: selecting one clears the others, Up/Left and Down/Right
// move both selection and focus through the members (wrapping), Home/End jump
// to the ends. It is a plain event handler owned by the dialog, not a window,
// so members may sit in any sizer or panel. GetSelection() is -1 when nothing
// is selected, which replaces the hidden "helper radio" trick dialogs used to
// represent "no choice yet".
class RadioGroup : public wxEvtHandler
{
public:
    RadioGroup() = default;
    ~RadioGroup() override;

    void Add(LabeledRadioButton *button);
    int  GetCount() const { return int(m_buttons.size()); }
    LabeledRadioButton *Get(int index) const { return index >= 0 && index < GetCount() ? m_buttons[index] : nullptr; }

    int  GetSelection() const;
    // Programmatic; -1 clears every member. Never emits an event.
    void SetSelection(int index);
    int  IndexOf(const LabeledRadioButton *button) const;

private:
    void onMemberSelected(wxCommandEvent &evt);
    void onMemberKey(wxKeyEvent &evt);
    void moveTo(int index);

    std::vector<LabeledRadioButton *> m_buttons;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_LabeledRadioButton_hpp_
