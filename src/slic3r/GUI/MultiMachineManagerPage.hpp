#ifndef slic3r_MultiMachineMangerPage_hpp_
#define slic3r_MultiMachineMangerPage_hpp_

#include "GUI_Utils.hpp"
#include "MultiMachine.hpp"
#include "Bulk/BulkSelection.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

class CheckBox;
class SearchField;

namespace Slic3r {
namespace GUI {

#define DEVICE_LEFT_PADDING_LEFT 15
#define DEVICE_LEFT_DEV_NAME 180
#define DEVICE_LEFT_PRO_NAME 180
#define DEVICE_LEFT_PRO_INFO 320

// MD3 device-farm card grid (ui-md3 Multi.jsx): each MultiMachineItem is a
// responsive Card, not a full-width list row. Cards are laid out in a wxWrapSizer
// whose scroll host now expands fluidly (wxEXPAND) instead of being pinned to the
// fixed DEVICE_ITEM_MAX_WIDTH strip, so the grid reflows/wraps to the actual
// available width (fewer cards per row as the window narrows). DEVICE_CARD_GAP is
// the half-gutter (applied as a wxALL border, so the visible gutter is 2x this).
#define DEVICE_CARD_WIDTH  268
#define DEVICE_CARD_HEIGHT 208
#define DEVICE_CARD_GAP    8

class MultiMachineItem : public DeviceItem
{

public:
    MultiMachineItem(wxWindow* parent, MachineObject* obj);
    ~MultiMachineItem() {};

    void OnEnterWindow(wxMouseEvent& evt);
    void OnLeaveWindow(wxMouseEvent& evt);
    void OnLeftDown(wxMouseEvent& evt);
    void OnMove(wxMouseEvent& evt);

    void         paintEvent(wxPaintEvent& evt);
    void         render(wxDC& dc);
    void         DrawTextWithEllipsis(wxDC& dc, const wxString& text, int maxWidth,  int left, int top = 0);
    void         doRender(wxDC& dc);
    void         post_event(wxCommandEvent&& event);
    virtual void DoSetSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO);

    // Bulk-selection checkbox at the left of the card header. The page owns
    // the selection model; the card only reflects it and reports toggles
    // (shift = true when Shift was held, for range selection).
    void SetSelected(bool selected);
    bool IsSelected() const { return m_selected; }
    void SetOnToggle(std::function<void(MultiMachineItem*, bool shift)> callback) { m_on_toggle = std::move(callback); }

public:
    bool m_hover{ false };
    bool m_selected{ false };
    CheckBox* m_check{ nullptr };
    std::function<void(MultiMachineItem*, bool)> m_on_toggle;
    void update_accessible_name();
    ScalableBitmap m_bitmap_check_disable;
    ScalableBitmap m_bitmap_check_off;
    ScalableBitmap m_bitmap_check_on;
    wxString get_left_time(int mc_left_time);
};
    
class MultiMachineManagerPage : public wxPanel
{
public:
    MultiMachineManagerPage(wxWindow* parent);
    ~MultiMachineManagerPage() {};

    void update_page();
    void refresh_user_device(bool clear = false);
    
    void sync_state(MachineObject* obj_);
    bool Show(bool show);

    std::vector<ObjState> extractRange(const std::vector<ObjState>& source, int start, int end);

    void start_timer();
    void update_page_number();
    void on_timer(wxTimerEvent& event);
    void clear_page();

    void page_num_enter_evt();

    void msw_rescale();

private:
    // Bulk selection over device ids; lives on the page so it survives paging
    // and filtering. m_match_ids is every device matching the search across
    // all pages (display order), m_page_ids the slice rendered on this page.
    void on_item_toggled(MultiMachineItem* item, bool shift);
    void select_page();
    void select_all_matches();
    void invert_selection();
    void clear_selection();
    void apply_selection_to_items();
    void update_bulk_controls();
    void bulk_export();
    void on_char_hook(wxKeyEvent& event);

    Bulk::BulkSelection<std::string>     m_bulk;
    std::string                          m_bulk_anchor;
    std::vector<std::string>             m_match_ids;
    std::vector<std::string>             m_page_ids;
    std::map<std::string, MachineObject*> m_user_machines;
    Label*                               m_bulk_counts{ nullptr };
    Button*                              m_button_select_page{ nullptr };
    Button*                              m_button_select_all{ nullptr };
    Button*                              m_button_invert{ nullptr };
    Button*                              m_button_clear{ nullptr };
    Button*                              m_button_export{ nullptr };

    std::vector<ObjState>          m_state_objs;
    std::vector<MultiMachineItem*> m_device_items;
    SortItem                m_sort;
    bool                    device_dev_name_big{ true };
    bool                    device_state_big{ true };


    Button*                 m_button_edit{nullptr};
    // Farm toolbar live search (ui-md3 Multi.jsx). m_search_filter holds the raw
    // query (case handled by SearchField::textMatches); it is applied to the card
    // grid BEFORE paging so the page count / flipping stay consistent with what is
    // shown.
    SearchField*            m_search{ nullptr };
    wxString                m_search_filter;
    wxBoxSizer*             page_sizer{ nullptr };
    wxPanel*                m_main_panel{ nullptr };
    wxBoxSizer*             m_main_sizer{nullptr};
    wxBoxSizer*             m_sizer_machine_list{nullptr};
    wxScrolledWindow*       m_machine_list{ nullptr };
    wxStaticText*           m_selected_num{ nullptr };

    // table head
    wxPanel*                m_table_head_panel{ nullptr };
    wxBoxSizer*             m_table_head_sizer{ nullptr };
    Button*                 m_printer_name{ nullptr };
    Button*                 m_task_name{ nullptr };
    Button*                 m_status{ nullptr };
    Button*                 m_action{ nullptr };
    Button*                 m_stop_all_botton{nullptr};

    // tip when no device
    wxStaticText*           m_tip_text{ nullptr };
    Button*                 m_button_add{ nullptr };

    // Flipping pages
    int                         m_current_page{ 0 };
    int                         m_total_page{ 0 };
    int                         m_total_count{ 0 };
    int                         m_count_page_item{ 10 };

    bool                        prev{ false };
    bool                        next{ false };
    Button*                     btn_last_page{ nullptr };
    Button*                     btn_next_page{ nullptr };
    wxStaticText*               st_page_number{ nullptr };
    wxBoxSizer*                 m_flipping_page_sizer{ nullptr };
    wxBoxSizer*                 m_page_sizer{ nullptr };
    wxPanel*                    m_flipping_panel{ nullptr };
    wxTimer*                    m_flipping_timer{ nullptr };
    TextInput*                  m_page_num_input{ nullptr };
    Button*                     m_page_num_enter{ nullptr };
};

} // namespace GUI
} // namespace Slic3r

#endif
