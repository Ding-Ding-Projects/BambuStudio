#ifndef slic3r_GUI_SwitchButton_hpp_
#define slic3r_GUI_SwitchButton_hpp_

#include "../wxExtensions.hpp"
#include "StateColor.hpp"

#include <wx/tglbtn.h>
#include <wx/popupwin.h>
#include <wx/timer.h>
#include "Label.hpp"
#include "Button.hpp"

wxDECLARE_EVENT(wxCUSTOMEVT_SWITCH_POS, wxCommandEvent);
wxDECLARE_EVENT(wxCUSTOMEVT_MULTISWITCH_SELECTION, wxCommandEvent);
wxDECLARE_EVENT(wxEXPAND_LEFT_DOWN, wxCommandEvent);

// Two personalities in one control:
//  - Icon mode (no labels): the MD3 Switch drawn live to spec — a 44x24 track,
//    2px border (Primary checked / Outline unchecked), Primary fill on / a
//    transparent track off, and a knob that slides 4->22 and grows 12->16px over
//    150ms. The legacy toggle_on/off PNGs are gone.
//  - Labelled mode (SetLabels): a two-position segmented toggle whose selected
//    half is the Primary thumb (OnPrimary text) and whose other half reads as the
//    SurfaceContainerHighest track (OnSurfaceVariant text). The raw Grey350 /
//    BrandGreen / White literals are replaced with theme + scheme tokens.
class SwitchButton : public wxBitmapToggleButton
{
public:
	SwitchButton(wxWindow * parent = NULL, wxWindowID id = wxID_ANY);

public:
	void SetLabels(wxString const & lbl_on, wxString const & lbl_off);

	void SetTextColor(StateColor const &color);

	void SetTextColor2(StateColor const &color);

    void SetTrackColor(StateColor const &color);

	void SetThumbColor(StateColor const &color);

	// Recolor the accent (thumb / track fill) to a workspace scheme.
	void SetColorScheme(MD3::ColorScheme scheme);

	void SetValue(bool value) override;

	void Rescale();

private:
	void update();

	bool isIconMode() const { return labels[0].IsEmpty(); }

	// Draw the icon-mode Switch at animation phase t in [0,1] (0 = off, 1 = on).
	wxBitmap renderSwitch(double t, bool enabled) const;

	void startAnim();
	void onAnimTick(wxTimerEvent &evt);

private:
	// Icon mode draws directly; labelled mode caches its two rendered halves here.
	wxBitmap m_on;
	wxBitmap m_off;

	wxString labels[2];
    StateColor   text_color;
    StateColor   text_color2;
	StateColor   track_color;
	StateColor   thumb_color;

    MD3::ColorScheme m_scheme = MD3::ColorScheme::Brand;
    bool m_text_overridden  = false;
    bool m_track_overridden = false;
    bool m_thumb_overridden = false;

    wxTimer m_anim_timer;
    double  m_anim        = 0.0; // current knob phase
    double  m_anim_target = 0.0; // 0 = off, 1 = on
};

class SwitchBoard : public wxWindow
{
public:
    SwitchBoard(wxWindow *parent = NULL, wxString leftL = "", wxString right = "", wxSize size = wxDefaultSize);
    wxString leftLabel;
    wxString rightLabel;

	void updateState(wxString target);
    void SetLabels(const wxString &left, const wxString &right);

	bool switch_left{false};
    bool switch_right{false};
    bool is_enable {true};

    void* client_data = nullptr;/*MachineObject* in StatusPanel*/

public:
    void Enable();
    void Disable();
    bool IsEnabled(){return is_enable;};

    void  SetClientData(void* data) { client_data = data; };
    void* GetClientData() { return client_data; };

    void SetAutoDisableWhenSwitch() { auto_disable_when_switch = true; };

    // Recolor the selected segment to a workspace scheme (Preview / Device).
    void SetColorScheme(MD3::ColorScheme scheme) { m_scheme = scheme; Refresh(); }

protected:
    void paintEvent(wxPaintEvent& evt);
    void render(wxDC& dc);
    void doRender(wxDC& dc);
    void on_left_down(wxMouseEvent& evt);

private:
    bool auto_disable_when_switch = false;
    MD3::ColorScheme m_scheme = MD3::ColorScheme::Brand;
};

class CustomToggleButton : public wxWindow {
public:
    CustomToggleButton(wxWindow* parent, const wxString& label,
        wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);


    void SetLabel(const wxString& label) override;

    void SetSelectedIcon(const wxString& iconPath);
    void SetUnSelectedIcon(const wxString& iconPath);

    void SetIsSelected(bool selected);
    bool IsSelected() const;

    void set_primary_colour(wxColour col) {m_primary_colour = col;};
    void set_secondary_colour(wxColour col) {m_secondary_colour = col;};

private:
    void OnPaint(wxPaintEvent& event);
    void render(wxDC& dc);
    void doRender(wxDC& dc);
    void OnSize(wxSizeEvent& event);

    void on_left_down(wxMouseEvent& e);

    wxString m_label;
    wxBitmap m_selected_icon;
    wxBitmap m_unselected_icon;
    wxColour m_primary_colour{StateColor::semantic(MD3::Role::Primary)};
    wxColour m_secondary_colour{StateColor::semantic(MD3::Role::SecondaryContainer)};

    bool m_isSelected;
};

class RichTooltipPopup : public wxPopupTransientWindow {
public:
    RichTooltipPopup(wxWindow* parent, const wxString& iconName, const wxString& text);
    void ShowAtPosition(wxWindow* anchor);

private:
    void OnPaint(wxPaintEvent& event);
    wxBitmap m_icon;
    wxString m_text;
};

class ExpandButton : public wxWindow {
public:
    ExpandButton(wxWindow* parent,
        std::string bmp,
        wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);
    ~ExpandButton();
    
    void update_bitmap(std::string bmp);
    void msw_rescale();
    void SetRichTooltip(const wxString& iconName, const wxString& text);
    void ShowRichTooltip();
    void HideRichTooltip();

private:
    std::string m_bmp_str;
    wxBitmap m_bmp;
    void OnPaint(wxPaintEvent& event);
    void render(wxDC& dc);
    void doRender(wxDC& dc);
    
    wxString m_tooltip_icon;
    wxString m_tooltip_text;
    RichTooltipPopup* m_tooltip_popup{nullptr};
};

class ExpandButtonHolder : public wxPanel {
public:
    ExpandButtonHolder(wxWindow* parent,
        wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);

    //wxBoxSizer* expand_sizer{nullptr};
    wxBoxSizer* hsizer{nullptr};
    wxBoxSizer* vsizer{nullptr};
    int GetAvailable();
    void addExpandButton(wxWindowID id, std::string img);
    void ShowExpandButton(wxWindowID id, bool show);
    void updateExpandButtonBitmap(wxWindowID id, std::string bitmap);
    void EnableExpandButton(wxWindowID id, bool enb);
    void SetExpandButtonTooltip(wxWindowID id, const wxString& tooltip);
    void SetExpandButtonRichTooltip(wxWindowID id, const wxString& iconName, const wxString& text);

    void msw_rescale();
private:
    ExpandButton* FindExpandButton(wxWindowID id);
    void OnPaint(wxPaintEvent& event);
    void render(wxDC& dc);
    void doRender(wxDC& dc);
};

class MultiSwitchButton : public StaticBox
{
    std::vector<Button *> btns;
    wxBoxSizer           *sizer = nullptr;
    int                   sel   = -1;

public:
    MultiSwitchButton(wxWindow *parent = nullptr, wxWindowID id = wxID_ANY, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize, long style = 0);

    ~MultiSwitchButton();

public:
    int AppendOption(const wxString &option, void *clientData = nullptr);

    void SetOptions(const std::vector<wxString> &options);

    void DeleteAllOptions();

    unsigned int GetCount() const;

    int      GetSelection() const;
    void     SetSelection(int index);
    wxString GetSelectedText() const;

    Button*  GetButton(unsigned int index) const
    {
        return index >= 0 && index < btns.size() ? btns[index] : nullptr;
    }

    wxString GetOptionText(unsigned int index) const;
    void     SetOptionText(unsigned int index, const wxString &text);

    void *GetOptionData(unsigned int index) const;
    void  SetOptionData(unsigned int index, void *clientData);

    void SetBackgroundColor(const StateColor &color);
    void SetTextColor(const StateColor &color);
    void SetButtonTextColor(int index, const StateColor &color)
    {
        if (index >= btns.size()) return;

        btns[index]->SetTextColor(color);
        btns[index]->Refresh();
    }
    void SetButtonCornerRadius(double radius);
    void SetButtonPadding(const wxSize &padding);

    void Rescale();

protected:
    void button_clicked(wxCommandEvent &event);
    void update_button_styles();

    bool send_selection_event();

private:
    StateColor m_bg_color;
    StateColor m_bg_color_grayed;
    StateColor m_text_color;
    StateColor m_text_color_grayed;
    double     m_button_radius;
    wxSize     m_button_padding;
};

#endif // !slic3r_GUI_SwitchButton_hpp_
