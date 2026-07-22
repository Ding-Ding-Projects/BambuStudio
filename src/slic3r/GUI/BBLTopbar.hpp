#pragma once

#include "wx/wxprec.h"
#include "wx/aui/auibar.h"

#include "SelectMachine.hpp"
#include "DeviceManager.hpp"


using namespace Slic3r::GUI;

class BBLTopbar : public wxAuiToolBar
{
public:
    BBLTopbar(wxWindow* pwin, wxFrame* parent);
    BBLTopbar(wxFrame* parent);
    void Init(wxFrame *parent);
    ~BBLTopbar();
    void UpdateToolbarWidth(int width);
    void Rescale();
    void OnIconize(wxAuiToolBarEvent& event);
    void OnFullScreen(wxAuiToolBarEvent& event);
    void OnCloseFrame(wxAuiToolBarEvent& event);
    void OnTopMenuToolItem(wxAuiToolBarEvent& evt);
    void OnCalibToolItem(wxAuiToolBarEvent &evt);
    void OnMouseLeftDClock(wxMouseEvent& mouse);
    void OnMouseLeftDown(wxMouseEvent& event);
    void OnMouseLeftUp(wxMouseEvent& event);
    void OnMouseMotion(wxMouseEvent& event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnMenuClose(wxMenuEvent& event);
    void OnOpenProject(wxAuiToolBarEvent& event);
    void show_publish_button(bool show);
    void OnSaveProject(wxAuiToolBarEvent& event);
    void OnUndo(wxAuiToolBarEvent& event);
    void OnRedo(wxAuiToolBarEvent& event);
    void OnModelStoreClicked(wxAuiToolBarEvent& event);
    void OnPublishClicked(wxAuiToolBarEvent &event);
    // §3.5 / §3.7 kit title-bar chips added this wave.
    void OnHistoryChip(wxAuiToolBarEvent& event);
    void OnAppearanceButton(wxAuiToolBarEvent& event);

    wxAuiToolBarItem* FindToolByCurrentPosition();

    void SetTopMenus(wxMenu* file_menu, wxMenu* edit_menu, wxMenu* view_menu,
                     wxMenu* objects_menu, wxMenu* help_menu);
    wxMenu *GetCalibMenu();
    void SetTitle(wxString title);
    // §3.5 history chip label content (branch + short head). Decorative; the
    // click always opens the real version-history backend.
    void SetHistoryInfo(const wxString& branch, const wxString& head);
    void SetMaximizedSize();
    void SetWindowSize();

    void EnableSaveItem(bool enable);
    void EnableUndoItem(bool enable);
    void EnableRedoItem(bool enable);
    void EnableUndoRedoItems();
    void DisableUndoRedoItems();

    void SaveNormalRect();

    void ShowCalibrationButton(bool show = true);

private:
    wxMenu* top_menu_for_tool(int tool_id) const;
    int measure_fixed_content_width() const;
    void update_responsive_title(int width = -1);
    // Rebuild the baked history-chip bitmaps (idle + hover) from the current
    // branch/head + theme; safe to call before the item exists.
    void rebuild_history_chip();

    wxFrame* m_frame;
    wxAuiToolBarItem* m_brand_item;
    wxAuiToolBarItem* m_file_menu_item;
    wxAuiToolBarItem* m_edit_menu_item;
    wxAuiToolBarItem* m_view_menu_item;
    wxAuiToolBarItem* m_objects_menu_item;
    wxAuiToolBarItem* m_help_menu_item;
    wxRect m_normalRect;
    wxPoint m_delta;
    wxMenu* m_file_menu;
    wxMenu* m_edit_menu;
    wxMenu* m_view_menu;
    wxMenu* m_objects_menu;
    wxMenu* m_help_menu;
    wxMenu m_calib_menu;
    wxAuiToolBarItem* m_title_item;
    wxAuiToolBarItem* m_account_item;
    wxAuiToolBarItem* m_model_store_item;
    wxAuiToolBarItem* m_history_item{nullptr};
    wxAuiToolBarItem* m_appearance_item{nullptr};
    wxString          m_history_branch;
    wxString          m_history_head;

    wxAuiToolBarItem *m_publish_item;
    wxAuiToolBarItem *m_save_item;
    wxAuiToolBarItem* m_undo_item;
    wxAuiToolBarItem* m_redo_item;
    wxAuiToolBarItem* m_calib_item;
    wxAuiToolBarItem* maximize_btn;

    wxBitmap m_publish_bitmap;
    wxBitmap m_publish_disable_bitmap;

    wxBitmap maximize_bitmap;
    wxBitmap window_bitmap;

    int m_toolbar_h;
    int m_skip_popup_menu_id;
    bool m_skip_popup_calib_menu;
    wxString m_full_title;
};
