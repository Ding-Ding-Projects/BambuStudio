#ifndef slic3r_GUI_wxExtensions_hpp_
#define slic3r_GUI_wxExtensions_hpp_

#include <wx/checklst.h>
#include <wx/combo.h>
#include <wx/dataview.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/bmpcbox.h>
#include <wx/statbmp.h>
#include <wx/popupwin.h>
#include <wx/scrolwin.h>
#include <wx/spinctrl.h>
#include <wx/artprov.h>
#include <wx/colordlg.h>

#include <vector>
#include <functional>
#include "BitmapCache.hpp"
#include "Widgets/PopupWindow.hpp"

#ifdef __WXMSW__
void                msw_rescale_menu(wxMenu* menu);
#else /* __WXMSW__ */
inline void         msw_rescale_menu(wxMenu* /* menu */) {}
#endif /* __WXMSW__ */

wxMenuItem* append_menu_item(wxMenu* menu, int id, const wxString& string, const wxString& description,
    std::function<void(wxCommandEvent& event)> cb, const wxBitmap& icon, wxEvtHandler* event_handler = nullptr,
    std::function<bool()> const cb_condition = []() { return true;}, wxWindow* parent = nullptr, int insert_pos = wxNOT_FOUND);
wxMenuItem* append_menu_item(wxMenu* menu, int id, const wxString& string, const wxString& description,
    std::function<void(wxCommandEvent& event)> cb, const std::string& icon = "", wxEvtHandler* event_handler = nullptr,
    std::function<bool()> const cb_condition = []() { return true; }, wxWindow* parent = nullptr, int insert_pos = wxNOT_FOUND);

wxMenuItem* append_submenu(wxMenu* menu, wxMenu* sub_menu, int id, const wxString& string, const wxString& description,
    const std::string& icon = "",
    std::function<bool()> const cb_condition = []() { return true; }, wxWindow* parent = nullptr, int insert_pos = wxNOT_FOUND);

wxMenuItem* append_menu_radio_item(wxMenu* menu, int id, const wxString& string, const wxString& description,
    std::function<void(wxCommandEvent& event)> cb, wxEvtHandler* event_handler);

wxMenuItem* append_menu_check_item(wxMenu* menu, int id, const wxString& string, const wxString& description,
    std::function<void(wxCommandEvent & event)> cb, wxEvtHandler* event_handler,
    std::function<bool()> const enable_condition = []() { return true; },
    std::function<bool()> const check_condition = []() { return true; }, wxWindow* parent = nullptr);

void enable_menu_item(wxUpdateUIEvent& evt, std::function<bool()> const cb_condition, wxMenuItem* item, wxWindow* win);

class wxDialog;
class wxStaticText;

void    edit_tooltip(wxString& tooltip);
void    msw_buttons_rescale(wxDialog* dlg, const int em_unit, const std::vector<int>& btn_ids);
int     em_unit(wxWindow* win);
int     mode_icon_px_size();

bool    copy_text_to_clipboard(const wxString& text);
// Give a read-only wxStaticText a right-click "Copy" menu carrying its own
// label, for values the user needs to quote elsewhere (serial, model, version).
void    enable_static_text_copy_menu(wxStaticText* label);

wxBitmap create_menu_bitmap(const std::string& bmp_name);

// BBS: support resize by fill border
#if 1
wxBitmap create_scaled_bitmap(const std::string& bmp_name, wxWindow *win = nullptr,
    const int px_cnt = 16, const bool grayscale = false,
    const std::string& new_color = std::string(), // color witch will used instead of orange
    const bool menu_bitmap = false, const bool resize = false,
    const bool bitmap2 = false,// for create_scaled_bitmap2
    const std::vector<std::string>& array_new_color = std::vector<std::string>());
//used for semi transparent material
wxBitmap create_scaled_bitmap2(const std::string& bmp_name_in, Slic3r::GUI::BitmapCache& cache, wxWindow* win = nullptr,
    const int px_cnt = 16, const bool grayscale = false, const bool resize = false,
    const std::vector<std::string>& array_new_color = std::vector<std::string>()); // color witch will used instead of orange
#else
wxBitmap create_scaled_bitmap(const std::string& bmp_name, wxWindow *win = nullptr,
    const int px_cnt = 16, const bool grayscale = false, const bool resize = false);
#endif

wxBitmap* get_default_extruder_color_icon(bool thin_icon = false);
// rounded_ring: see the single-color get_extruder_color_icon() overload below;
// forwarded only to the single-color-per-slot render path. Defaults to false.
std::vector<wxBitmap *> get_extruder_color_icons(bool thin_icon = false, bool rounded_ring = false);
// rounded_ring: MD3 filament-row swatch geometry (r8 rounded corners + a 1px
// inset OutlineVariant ring, kit ref prepare/filament-rows-preset-combobox-not-
// inforow) instead of the plain square fill. Defaults to false so every
// existing caller (dropdown list icons, calibration/object-color swatches,
// etc.) keeps its original unrounded rendering byte-for-byte.
wxBitmap * get_extruder_color_icon(std::string color, std::string label, int icon_width, int icon_height, bool rounded_ring = false);
wxBitmap * get_extruder_color_icon(std::vector<std::string> colors, bool is_gradient, std::string label, int icon_width, int icon_height);
std::vector<std::vector<std::string>> read_color_pack(std::vector<std::string> color_pack);
// The Material table look for a wxDataViewCtrl: the kit body face in OnSurface on
// SurfaceContainerLowest, a SurfaceContainerLow stripe on every other row, and a
// header in the kit's small title face and OnSurfaceVariant.
void md3_style_data_view(wxDataViewCtrl *view);

// The recently used colours (the app config's custom colour list, which the
// system colour dialog showed as its custom colours), most recent first.
std::vector<wxColour> recent_custom_colors();
// Moves an accepted pick to the front of that list.
void remember_custom_color(const wxColour &color);
// Asks for a filament colour in the Material picker: opaque, with the recently
// used colours as quick picks, and the pick remembered among them. Returns
// wxNullColour when the user cancels.
wxColour pick_filament_color(wxWindow *parent, const wxColour &initial, const wxString &title = wxEmptyString);
// The same, for callers that keep a wxColourData: returns clr_data with the
// picked colour, or unchanged when the user cancels.
wxColourData show_sys_picker_dialog(wxWindow *parent, const wxColourData &clr_data);

namespace Slic3r {
namespace GUI {
class BitmapComboBox;
}
}
void apply_extruder_selector(Slic3r::GUI::BitmapComboBox** ctrl,
                             wxWindow* parent,
                             const std::string& first_item = "",
                             wxPoint pos = wxDefaultPosition,
                             wxSize size = wxDefaultSize,
                             bool use_thin_icon = false);

class wxCheckListBoxComboPopup : public wxCheckListBox, public wxComboPopup
{
    static const unsigned int DefaultWidth;
    static const unsigned int DefaultHeight;

    wxString m_text;

    // Events sent on mouseclick are quite complex. Function OnListBoxSelection is supposed to pass the event to the checkbox, which works fine on
    // Win. On OSX and Linux the events are generated differently - clicking on the checkbox square generates the event twice (and the square
    // therefore seems not to respond).
    // This enum is meant to save current state of affairs, i.e., if the event forwarding is ok to do or not. It is only used on Linux
    // and OSX by some #ifdefs. It also stores information whether OnListBoxSelection is supposed to change the checkbox status,
    // or if it changed status on its own already (which happens when the square is clicked). More comments in OnCheckListBox(...)
    // There indeed is a better solution, maybe making a custom event used for the event passing to distinguish the original and passed message
    // and blocking one of them on OSX and Linux. Feel free to refactor, but carefully test on all platforms.
    enum class OnCheckListBoxFunction{
        FreeToProceed,
        RefuseToProceed,
        WasRefusedLastTime
    } m_check_box_events_status = OnCheckListBoxFunction::FreeToProceed;


public:
    virtual bool Create(wxWindow* parent);
    virtual wxWindow* GetControl();
    virtual void SetStringValue(const wxString& value);
    virtual wxString GetStringValue() const;
    virtual wxSize GetAdjustedSize(int minWidth, int prefHeight, int maxHeight);

    virtual void OnKeyEvent(wxKeyEvent& evt);

    void OnCheckListBox(wxCommandEvent& evt);
    void OnListBoxSelection(wxCommandEvent& evt);
};


// ***  wxDataViewTreeCtrlComboBox  ***

class wxDataViewTreeCtrlComboPopup: public wxDataViewTreeCtrl, public wxComboPopup
{
    static const unsigned int DefaultWidth;
    static const unsigned int DefaultHeight;
    static const unsigned int DefaultItemHeight;

    wxString	m_text;
    int			m_cnt_open_items{0};

public:
    virtual bool		Create(wxWindow* parent);
    virtual wxWindow*	GetControl() { return this; }
    virtual void		SetStringValue(const wxString& value) { m_text = value; }
    virtual wxString	GetStringValue() const { return m_text; }
//	virtual wxSize		GetAdjustedSize(int minWidth, int prefHeight, int maxHeight);

    virtual void		OnKeyEvent(wxKeyEvent& evt);
    void				OnDataViewTreeCtrlSelection(wxCommandEvent& evt);
    void				SetItemsCnt(int cnt) { m_cnt_open_items = cnt; }
};


// ----------------------------------------------------------------------------
// ScalableBitmap
// ----------------------------------------------------------------------------

class ScalableBitmap
{
public:
    ScalableBitmap() {};
    // Wrap a bitmap the caller already rendered (a colour swatch, a composed
    // preview). It has no icon name, so msw_rescale() leaves it alone rather
    // than trying to reload a resource that does not exist.
    ScalableBitmap(wxWindow *parent, const wxBitmap &bitmap)
        : m_parent(parent), m_bmp(bitmap), m_icon_name(), m_px_cnt(bitmap.IsOk() ? bitmap.GetHeight() : 16) {}
    ScalableBitmap( wxWindow *parent,
                    const std::string& icon_name = "",
                    const int px_cnt = 16,
                    const bool grayscale = false,
                    const bool resize = false,
                    const bool bitmap2 = false,
                    const std::vector<std::string>& new_color = std::vector<std::string>());// BBS: support resize by fill border

    ~ScalableBitmap() {}

    wxSize  GetBmpSize() const;
    static wxSize GetBmpSize(const wxBitmap &bmp);

    int     GetBmpWidth() const;
    int     GetBmpHeight() const;

    void                msw_rescale();

    const wxBitmap&     bmp() const { return m_bmp; }
    wxBitmap&           bmp()       { return m_bmp; }
    const std::string&  name() const{ return m_icon_name; }

    int                 px_cnt()const           {return m_px_cnt;}

private:
    wxWindow*       m_parent{ nullptr };
    wxBitmap        m_bmp = wxBitmap();
    std::string     m_icon_name = "";
    int             m_px_cnt {16};
    bool            m_grayscale{ false };
    bool            m_resize{ false };
};


// ----------------------------------------------------------------------------
// ScalableButton
// ----------------------------------------------------------------------------

// The flat icon button is a kit Button and is defined in Widgets/Button.hpp: Button
// derives from StaticBox, which includes this header, so the class cannot be
// defined here. A source that constructs one, derives from one or calls a method on
// one includes Widgets/Button.hpp; a pointer member only needs this declaration.
class ScalableButton;



// ----------------------------------------------------------------------------
// MenuWithSeparators
// ----------------------------------------------------------------------------

class MenuWithSeparators : public wxMenu
{
public:
    MenuWithSeparators(const wxString& title, long style = 0)
        : wxMenu(title, style) {}

    MenuWithSeparators(long style = 0)
        : wxMenu(style) {}

    ~MenuWithSeparators() {}

    void DestroySeparators();
    void SetFirstSeparator();
    void SetSecondSeparator();

private:
    wxMenuItem* m_separator_frst { nullptr };    // use like separator before settings item
    wxMenuItem* m_separator_scnd { nullptr };   // use like separator between settings items
};


// ----------------------------------------------------------------------------
// BlinkingBitmap
// ----------------------------------------------------------------------------

class BlinkingBitmap : public wxStaticBitmap
{
public:
    BlinkingBitmap() {};
    BlinkingBitmap(wxWindow* parent, const std::string& icon_name = "blank_16");

    ~BlinkingBitmap() {}

    void    msw_rescale();
    void    invalidate();
    void    activate();
    void    blink();

    const wxBitmap& get_bmp() const { return bmp.bmp(); }

private:
    ScalableBitmap  bmp;
    bool            show {false};
};


// BBS add new custom widget
// ----------------------------------------------------------------------------
// ImageTransientPopup
// ----------------------------------------------------------------------------

class ImageTransientPopup : public PopupWindow
{
    public:
    ImageTransientPopup( wxWindow *parent, bool scrolled, wxBitmap bmp);
    virtual ~ImageTransientPopup();

    void SetImage(wxBitmap bmp);

    // PopupWindow virtual methods are all overridden to log them
    virtual void Popup(wxWindow *focus = NULL) wxOVERRIDE;
    virtual void OnDismiss() wxOVERRIDE;
    virtual bool ProcessLeftDown(wxMouseEvent& event) wxOVERRIDE;
    virtual bool Show( bool show = true ) wxOVERRIDE;

private:

    wxScrolledWindow *m_panel;
    wxStaticBitmap* m_image;

private:
    void OnMouse( wxMouseEvent &event );
    void OnSize( wxSizeEvent &event );
    void OnSetFocus( wxFocusEvent &event );
    void OnKillFocus( wxFocusEvent &event );

private:
    wxDECLARE_ABSTRACT_CLASS(ImageTransientPopup);
    wxDECLARE_EVENT_TABLE();
};


#endif // slic3r_GUI_wxExtensions_hpp_
