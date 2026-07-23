#ifndef slic3r_HMSPanel_hpp_
#define slic3r_HMSPanel_hpp_

#include <wx/panel.h>
#include <wx/textctrl.h>
#include <slic3r/GUI/Widgets/Button.hpp>
#include <slic3r/GUI/DeviceManager.hpp>
#include <slic3r/GUI/Widgets/ScrolledWindow.hpp>
#include <slic3r/GUI/StatusPanel.hpp>
#include <wx/html/htmlwin.h>

#include "DeviceCore/DevHMS.h"

namespace Slic3r {
namespace GUI {

class HMSNotifyItem : public wxPanel
{
    DevHMSItem &   m_hms_item;
    std::string m_url;
    std::string dev_id;
    std::string long_error_code;

    wxPanel *       m_panel_hms;
    wxStaticBitmap *m_bitmap_notify;
    wxStaticBitmap *m_bitmap_arrow;
    wxStaticText *  m_hms_content;
    wxHtmlWindow *  m_html;
    wxPanel *       m_staticline;

    wxBitmap m_img_notify_lv1;
    wxBitmap m_img_notify_lv2;
    wxBitmap m_img_notify_lv3;
    wxBitmap m_img_arrow;

    wxString m_content_text;      // unwrapped message; Wrap() is destructive, so re-apply from this
    int      m_content_width = 0; // last applied text width (logical px), avoids redundant re-wrap

    void          init_bitmaps();
    wxBitmap &    get_notify_bitmap();

public:
     HMSNotifyItem(const std::string& dev_id, wxWindow *parent, DevHMSItem& item);
    ~HMSNotifyItem();

     // Re-wrap the message text to fit the card width the panel gives us
     // (card_width is the scrolled-window client width in logical px).
     void set_content_width(int card_width);

     void msw_rescale() {}
};


class HMSPanel : public wxPanel
{
protected:
    wxScrolledWindow *m_scrolledWindow;
    wxBoxSizer *      m_top_sizer;

    int last_status;

    void append_hms_panel(const std::string& dev_id, DevHMSItem &item);
    void delete_hms_panels();

    // Re-flow every card's text to the current client width (called on EVT_SIZE
    // and after update() rebuilds the list) so cards track the container, not a
    // fixed 730px constant.
    void relayout_items();
    void OnSize(wxSizeEvent &evt);

public:
    HMSPanel(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize, long style = wxTAB_TRAVERSAL);
    ~HMSPanel();

    void msw_rescale() {}

    bool Show(bool show = true) override;

    void update(MachineObject *obj_);

    void show_status(int status);

    void clear_hms_tag();

    MachineObject *obj { nullptr };
    std::map<std::string, DevHMSItem>    temp_hms_list;
};

wxDECLARE_EVENT(EVT_ALREADY_READ_HMS, wxCommandEvent);

}
}

#endif
