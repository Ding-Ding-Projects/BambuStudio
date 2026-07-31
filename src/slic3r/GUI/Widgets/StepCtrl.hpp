#ifndef slic3r_GUI_StepCtrlBase_hpp_
#define slic3r_GUI_StepCtrlBase_hpp_

#include "MD3Tokens.hpp"
#include "StaticBox.hpp"

wxDECLARE_EVENT( EVT_STEP_CHANGING, wxCommandEvent );
wxDECLARE_EVENT( EVT_STEP_CHANGED, wxCommandEvent );

class StepCtrlBase : public StaticBox
{
protected:
    wxFont font_tip;
    StateColor clr_bar;
    StateColor clr_step;
    StateColor clr_text;
    StateColor clr_tip;
    int radius = 7;
    int bar_width = 4;

    std::vector<wxString> steps;
    std::vector<wxString> tips;
    wxString hint;

    int step = -1;

    wxPoint drag_offset;
    wxPoint pos_thumb;

public:
    StepCtrlBase(wxWindow *      parent,
             wxWindowID      id,
             const wxPoint & pos       = wxDefaultPosition,
             const wxSize &  size      = wxDefaultSize,
             long            style     = 0);

    ~StepCtrlBase();

public:
    void SetHint(wxString hint);

    bool SetTipFont(wxFont const & font);

public:
    int AppendItem(const wxString &item, wxString const & tip = {});

    void DeleteAllItems();

    unsigned int GetCount() const;

    int  GetSelection() const;

    void SelectItem(int item);
    void Idle();

    wxString GetItemText(unsigned int item) const;
    int      GetItemUseText(wxString txt) const;
    void     SetItemText(unsigned int item, wxString const& value);

    // Retint the rail for the workspace that owns it -- Device teal, Preview
    // purple, or the default brand accent. Only the step dot and the numeral
    // painted inside it follow the accent; the track, the border and the step
    // captions stay on neutral roles, which are scheme-independent.
    void SetColorScheme(MD3::ColorScheme scheme);

protected:
    // (Re)build clr_bar / clr_step / clr_text / clr_tip from the MD3 roles for
    // the scheme currently held in StaticBox::m_scheme. Every constructor calls
    // it once, and SetColorScheme() calls it again on each scheme change --
    // which is why the colours cannot simply be baked into the init list.
    virtual void applyColorScheme();

    // Shared by both indicator rails: the dot carries the scheme accent and the
    // numeral drawn *inside* it carries OnPrimary. The plain rail instead paints
    // its tips on the surface beside the dot, so it keeps the base mapping.
    void applyIndicatorColorScheme();

private:
    // some useful events
    bool sendStepCtrlEvent(bool changing = false);
};

class StepCtrl : public StepCtrlBase
{
    ScalableBitmap bmp_thumb;

public:
    StepCtrl(wxWindow *      parent,
             wxWindowID      id,
             const wxPoint & pos       = wxDefaultPosition,
             const wxSize &  size      = wxDefaultSize,
             long            style     = 0);

    virtual void Rescale();

private:
    void mouseDown(wxMouseEvent &event);
    void mouseMove(wxMouseEvent &event);
    void mouseUp(wxMouseEvent &event);
    void mouseCaptureLost(wxMouseCaptureLostEvent &event);

    void doRender(wxDC &dc) override;

    DECLARE_EVENT_TABLE()
};

class StepIndicator : public StepCtrlBase
{
    // Kept as the graceful fallback for the completed-step tick (and as the
    // DPI metric that sizes the dot); the tick itself is now a Material Symbols
    // glyph whenever the icon font resolved.
    ScalableBitmap bmp_ok;

public:
    StepIndicator(wxWindow *parent,
             wxWindowID      id,
             const wxPoint & pos       = wxDefaultPosition,
             const wxSize &  size      = wxDefaultSize,
             long            style     = 0);

    virtual void Rescale();

    void SelectNext();

protected:
    void applyColorScheme() override;

private:
    void doRender(wxDC &dc) override;
};


class FilamentStepIndicator : public StepCtrlBase

{
    ScalableBitmap bmp_ok;
    //wxBitmap bmp_extruder;
    wxString m_slot_information = "";

public:
    FilamentStepIndicator(wxWindow* parent,
        wxWindowID      id,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long            style = 0);

    virtual void Rescale();

    void SelectNext();
    void SetSlotInformation(wxString slot);

protected:
    void applyColorScheme() override;

private:
    void doRender(wxDC& dc) override;
};


#endif // !slic3r_GUI_StepCtrlBase_hpp_
