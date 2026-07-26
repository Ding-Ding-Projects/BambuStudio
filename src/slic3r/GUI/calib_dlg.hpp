#ifndef slic3r_calib_dlg_hpp_
#define slic3r_calib_dlg_hpp_

#include "wxExtensions.hpp"
#include "GUI_Utils.hpp"
#include "Widgets/RadioBox.hpp"
#include "Widgets/SwitchButton.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/TextInput.hpp"
#include "Widgets/MD3Dialog.hpp"
#include "GUI_App.hpp"
#include "wx/hyperlink.h"
#include "libslic3r/Calib.hpp"
#include <vector>

namespace Slic3r { namespace GUI {

class PA_Calibration_Dlg : public MD3Dialog
{
public:
    PA_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~PA_Calibration_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;
	void on_show(wxShowEvent& event);
protected:
    void reset_params();
	virtual void on_start(wxCommandEvent& event);
	virtual void on_extruder_type_changed(wxCommandEvent& event);
	virtual void on_method_changed(wxCommandEvent& event);

protected:
	bool m_hasBowdenExtruder{false};
	int  m_bowdenExtruderId{-1};
	Calib_Params m_params;

	MultiSwitchButton* m_rbExtruderType{nullptr};
	MultiSwitchButton* m_rbMethod;
	TextInput* m_tiStartPA;
	TextInput* m_tiEndPA;
	TextInput* m_tiPAStep;
	CheckBox* m_cbPrintNum;
	Button* m_btnStart;

	Plater* m_plater;
};

class Temp_Calibration_Dlg : public MD3Dialog
{
public:
    Temp_Calibration_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~Temp_Calibration_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:
    
    virtual void on_start(wxCommandEvent& event);
    // Rewired off wxRadioBox: the kit RadioBox chip group reports its selected
    // index directly, so the handler takes the index instead of a wxCommandEvent.
    void on_filament_type_changed(int selection);
    Calib_Params m_params;

    std::vector<RadioBox*> m_filamentRadios;
    int m_filamentSel{0};
    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Button* m_btnStart;
    Plater* m_plater;
};

class MaxVolumetricSpeed_Test_Dlg : public MD3Dialog
{
public:
    MaxVolumetricSpeed_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~MaxVolumetricSpeed_Test_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:

    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Button* m_btnStart;
    Plater* m_plater;
};

class VFA_Test_Dlg : public MD3Dialog {
public:
    VFA_Test_Dlg(wxWindow* parent, wxWindowID id, Plater* plater);
    ~VFA_Test_Dlg();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:
    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Button* m_btnStart;
    Plater* m_plater;
};


class Retraction_Test_Dlg : public MD3Dialog
{
public:
    Retraction_Test_Dlg (wxWindow* parent, wxWindowID id, Plater* plater);
    ~Retraction_Test_Dlg ();
    void on_dpi_changed(const wxRect& suggested_rect) override;

protected:

    virtual void on_start(wxCommandEvent& event);
    Calib_Params m_params;

    TextInput* m_tiStart;
    TextInput* m_tiEnd;
    TextInput* m_tiStep;
    Button* m_btnStart;
    Plater* m_plater;
};

}} // namespace Slic3r::GUI

#endif
