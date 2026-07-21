#include "BBLTopbar.hpp"
#include "wx/artprov.h"
#include "wx/aui/framemanager.h"
#include "wx/display.h"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "GUI.hpp"
#include "wxExtensions.hpp"
#include "Plater.hpp"
#include "MainFrame.hpp"
#include "WebViewDialog.hpp"
#include "PartPlate.hpp"
#include "ReleaseNote.hpp"
#include "Widgets/StateColor.hpp"

#include <algorithm>
#include <boost/log/trivial.hpp>

#define TOPBAR_ICON_SIZE  18
#define TOPBAR_TITLE_WIDTH  300

using namespace Slic3r;

enum CUSTOM_ID
{
    ID_TOP_MENU_TOOL = 3100,
    ID_LOGO,
    ID_TOP_FILE_MENU,
    ID_TOP_EDIT_MENU,
    ID_TOP_VIEW_MENU,
    ID_TOP_OBJECTS_MENU,
    ID_TOP_HELP_MENU,
    ID_TITLE,
    ID_MODEL_STORE,
    ID_PUBLISH,
    ID_CALIB,
    ID_TOOL_BAR = 3200,
    ID_AMS_NOTEBOOK,
};

class BBLTopbarArt : public wxAuiDefaultToolBarArt
{
public:
    virtual void DrawLabel(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect) wxOVERRIDE;
    virtual void DrawBackground(wxDC& dc, wxWindow* wnd, const wxRect& rect) wxOVERRIDE;
    virtual void DrawButton(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect) wxOVERRIDE;
    virtual void DrawSeparator(wxDC& dc, wxWindow* wnd, const wxRect& rect) wxOVERRIDE;
};

void BBLTopbarArt::DrawLabel(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect)
{
    wxFont font = m_font;
    if (item.GetId() == ID_TITLE)
        font.SetWeight(wxFONTWEIGHT_SEMIBOLD);
    dc.SetFont(font);
    dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurface));

    int textWidth = 0, textHeight = 0;
    dc.GetTextExtent(item.GetLabel(), &textWidth, &textHeight);

    wxRect clipRect = rect;
    clipRect.width -= 1;
    dc.SetClippingRegion(clipRect);

    int textX, textY;
    if (textWidth < rect.GetWidth()) {
        textX = rect.x + 1 + (rect.width - textWidth) / 2;
    }
    else {
        textX = rect.x + 1;
    }
    textY = rect.y + (rect.height - textHeight) / 2;
    dc.DrawText(item.GetLabel(), textX, textY);
    dc.DestroyClippingRegion();
}

void BBLTopbarArt::DrawBackground(wxDC& dc, wxWindow* wnd, const wxRect& rect)
{
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    dc.SetPen(wxPen(surface));
    dc.SetBrush(wxBrush(surface));
    wxRect clipRect = rect;
    clipRect.y -= 8;
    clipRect.height += 8;
    dc.SetClippingRegion(clipRect);
    dc.DrawRectangle(rect);
    dc.DestroyClippingRegion();

    const int divider_width = std::max(1, wnd->FromDIP(1));
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), divider_width));
    dc.DrawLine(rect.GetLeft(), rect.GetBottom(), rect.GetRight(), rect.GetBottom());
}

void BBLTopbarArt::DrawSeparator(wxDC& dc, wxWindow* wnd, const wxRect& rect)
{
    // §3.8: an explicit 1px x 22px outline-variant rule, vertically centered in
    // the bar, instead of the default AUI separator art.
    const int thickness = std::max(1, wnd->FromDIP(1));
    const int height    = std::min(rect.height, wnd->FromDIP(22));
    const int x = rect.x + (rect.width - thickness) / 2;
    const int y = rect.y + (rect.height - height) / 2;
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::OutlineVariant)));
    dc.DrawRectangle(x, y, thickness, height);
}

void BBLTopbarArt::DrawButton(wxDC& dc, wxWindow* wnd, const wxAuiToolBarItem& item, const wxRect& rect)
{
    int textWidth = 0, textHeight = 0;

    wxFont font = m_font;
    // §3.2 wordmark rides at Roboto Medium (500), not SemiBold; the menu buttons
    // (§3.3) are also Medium. Everything else keeps the inherited weight.
    if (item.GetId() == ID_LOGO ||
        item.GetId() == ID_TOP_FILE_MENU || item.GetId() == ID_TOP_EDIT_MENU ||
        item.GetId() == ID_TOP_VIEW_MENU || item.GetId() == ID_TOP_OBJECTS_MENU ||
        item.GetId() == ID_TOP_HELP_MENU)
        font.SetWeight(wxFONTWEIGHT_MEDIUM);
    dc.SetFont(font);

    if (m_flags & wxAUI_TB_TEXT)
    {
        int tx, ty;

        dc.GetTextExtent(wxT("ABCDHgj"), &tx, &textHeight);
        textWidth = 0;
        dc.GetTextExtent(item.GetLabel(), &textWidth, &ty);
    }

    int bmpX = 0, bmpY = 0;
    int textX = 0, textY = 0;

    const wxBitmap& bmp = item.GetState() & wxAUI_BUTTON_STATE_DISABLED
        ? item.GetDisabledBitmap()
        : item.GetBitmap();

    const wxSize bmpSize = bmp.IsOk() ? bmp.GetScaledSize() : wxSize(0, 0);

    if (m_textOrientation == wxAUI_TBTOOL_TEXT_BOTTOM)
    {
        bmpX = rect.x +
            (rect.width / 2) -
            (bmpSize.x / 2);

        bmpY = rect.y +
            ((rect.height - textHeight) / 2) -
            (bmpSize.y / 2);

        textX = rect.x + (rect.width / 2) - (textWidth / 2) + 1;
        textY = rect.y + rect.height - textHeight - 1;
    }
    else if (m_textOrientation == wxAUI_TBTOOL_TEXT_RIGHT)
    {
        bmpX = rect.x + wnd->FromDIP(3);

        bmpY = rect.y +
            (rect.height / 2) -
            (bmpSize.y / 2);

        textX = bmpX + wnd->FromDIP(3) + bmpSize.x;
        textY = rect.y +
            (rect.height / 2) -
            (textHeight / 2);
    }


    if (item.GetId() != ID_LOGO && !(item.GetState() & wxAUI_BUTTON_STATE_DISABLED)) {
        wxColour state_layer;
        // §3.9: the window Close control carries a destructive hover -- its state
        // layer fills Role::Error instead of the neutral surface-container tint
        // used by every other title-bar control.
        const bool is_close = item.GetId() == wxID_CLOSE_FRAME;
        if (item.GetState() & wxAUI_BUTTON_STATE_PRESSED)
            state_layer = StateColor::semantic(is_close ? MD3::Role::Error : MD3::Role::SurfaceContainerHighest);
        else if ((item.GetState() & wxAUI_BUTTON_STATE_HOVER) || item.IsSticky())
            state_layer = StateColor::semantic(is_close ? MD3::Role::Error : MD3::Role::SurfaceContainerHigh);
        else if (item.GetState() & wxAUI_BUTTON_STATE_CHECKED)
            state_layer = StateColor::semantic(MD3::Role::SecondaryContainer);

        if (state_layer.IsOk()) {
            wxRect state_rect = rect;
            state_rect.Deflate(wnd->FromDIP(2), wnd->FromDIP(4));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(state_layer));
            dc.DrawRoundedRectangle(state_rect, wnd->FromDIP(MD3::Metrics::compact.small_radius));
        }
    }

    if (bmp.IsOk())
        dc.DrawBitmap(bmp, bmpX, bmpY, true);

    // Semantic foregrounds remain readable on both light and dark title surfaces.
    // §3.3: the File/Edit/View/Objects/Help menu labels use OnSurfaceVariant; the
    // wordmark (and any other label) stays the stronger OnSurface.
    MD3::Role label_role;
    if (item.GetState() & wxAUI_BUTTON_STATE_DISABLED)
        label_role = MD3::Role::Outline;
    else if (item.GetId() == ID_TOP_FILE_MENU || item.GetId() == ID_TOP_EDIT_MENU ||
             item.GetId() == ID_TOP_VIEW_MENU || item.GetId() == ID_TOP_OBJECTS_MENU ||
             item.GetId() == ID_TOP_HELP_MENU)
        label_role = MD3::Role::OnSurfaceVariant;
    else
        label_role = MD3::Role::OnSurface;
    dc.SetTextForeground(StateColor::semantic(label_role));

    if ((m_flags & wxAUI_TB_TEXT) && !item.GetLabel().empty())
    {
        dc.DrawText(item.GetLabel(), textX, textY);
    }
}

BBLTopbar::BBLTopbar(wxFrame* parent)
    : wxAuiToolBar(parent, ID_TOOL_BAR, wxDefaultPosition, wxDefaultSize, wxAUI_TB_TEXT | wxAUI_TB_HORZ_TEXT)
{
    Init(parent);
}

BBLTopbar::BBLTopbar(wxWindow* pwin, wxFrame* parent)
    : wxAuiToolBar(pwin, ID_TOOL_BAR, wxDefaultPosition, wxDefaultSize, wxAUI_TB_TEXT | wxAUI_TB_HORZ_TEXT)
{
    Init(parent);
}

void BBLTopbar::Init(wxFrame* parent)
{
    SetArtProvider(new BBLTopbarArt());
    m_frame = parent;
    m_brand_item = nullptr;
    m_file_menu_item = nullptr;
    m_edit_menu_item = nullptr;
    m_view_menu_item = nullptr;
    m_objects_menu_item = nullptr;
    m_help_menu_item = nullptr;
    m_file_menu = nullptr;
    m_edit_menu = nullptr;
    m_view_menu = nullptr;
    m_objects_menu = nullptr;
    m_help_menu = nullptr;
    m_skip_popup_menu_id = wxID_ANY;
    m_skip_popup_calib_menu    = false;

    wxInitAllImageHandlers();

    this->AddSpacer(FromDIP(MD3::Metrics::compact.gap));

    wxBitmap logo_bitmap = create_scaled_bitmap("BambuStudio", this, 22);
    m_brand_item = this->AddTool(ID_LOGO, _L("Bambu Studio"), logo_bitmap, wxEmptyString, wxITEM_NORMAL);
    m_brand_item->SetHoverBitmap(logo_bitmap);
    m_brand_item->SetActive(false);

    this->AddSpacer(FromDIP(MD3::Metrics::compact.gap));

    // Each application menu is a first-class top-bar control.  The menu objects
    // are still built and owned by MainFrame, so existing handlers, update-UI
    // predicates, accelerators and recent-project integration remain intact.
    m_file_menu_item = this->AddTool(ID_TOP_FILE_MENU, _L("File"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    m_edit_menu_item = this->AddTool(ID_TOP_EDIT_MENU, _L("Edit"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    m_view_menu_item = this->AddTool(ID_TOP_VIEW_MENU, _L("View"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    m_objects_menu_item = this->AddTool(ID_TOP_OBJECTS_MENU, _L("Objects"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);
    m_help_menu_item = this->AddTool(ID_TOP_HELP_MENU, _L("Help"), wxNullBitmap, wxEmptyString, wxITEM_NORMAL);

    this->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    this->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));

    this->AddSpacer(FromDIP(MD3::Metrics::compact.gap));
    this->AddSeparator();
    this->AddSpacer(FromDIP(MD3::Metrics::compact.gap));

    //wxBitmap open_bitmap = create_scaled_bitmap("topbar_open", nullptr, TOPBAR_ICON_SIZE);
    //wxAuiToolBarItem* tool_item = this->AddTool(wxID_OPEN, "", open_bitmap);

    this->AddSpacer(FromDIP(10));

    // Cross-platform modifier prefix ("Ctrl+" / "⌘") for the tooltip shortcuts.
    const wxString ctrl = wxString::FromUTF8(Slic3r::GUI::shortkey_ctrl_prefix().c_str());

    wxBitmap save_bitmap = create_scaled_bitmap("topbar_save", nullptr, TOPBAR_ICON_SIZE);
    m_save_item          = this->AddTool(wxID_SAVE, "", save_bitmap);
    wxBitmap save_inactive_bitmap = create_scaled_bitmap("topbar_save_inactive", nullptr, TOPBAR_ICON_SIZE);
    m_save_item->SetDisabledBitmap(save_inactive_bitmap);
    m_save_item->SetShortHelp(_L("Save Project") + " (" + ctrl + "S)");
    this->AddSpacer(FromDIP(10));

    wxBitmap undo_bitmap = create_scaled_bitmap("topbar_undo", nullptr, TOPBAR_ICON_SIZE);
    m_undo_item = this->AddTool(wxID_UNDO, "", undo_bitmap);
    wxBitmap undo_inactive_bitmap = create_scaled_bitmap("topbar_undo_inactive", nullptr, TOPBAR_ICON_SIZE);
    m_undo_item->SetDisabledBitmap(undo_inactive_bitmap);
    m_undo_item->SetShortHelp(_L("Undo") + " (" + ctrl + "Z)");

    this->AddSpacer(FromDIP(10));

    wxBitmap redo_bitmap = create_scaled_bitmap("topbar_redo", nullptr, TOPBAR_ICON_SIZE);
    m_redo_item = this->AddTool(wxID_REDO, "", redo_bitmap);
    wxBitmap redo_inactive_bitmap = create_scaled_bitmap("topbar_redo_inactive", nullptr, TOPBAR_ICON_SIZE);
    m_redo_item->SetDisabledBitmap(redo_inactive_bitmap);
    m_redo_item->SetShortHelp(_L("Redo") + " (" + ctrl + "Y)");

    this->AddSpacer(FromDIP(10));

    wxBitmap calib_bitmap          = create_scaled_bitmap("calib_sf", nullptr, TOPBAR_ICON_SIZE);
    wxBitmap calib_bitmap_inactive = create_scaled_bitmap("calib_sf_inactive", nullptr, TOPBAR_ICON_SIZE);
    m_calib_item                   = this->AddTool(ID_CALIB, _L("Calibration"), calib_bitmap);
    m_calib_item->SetDisabledBitmap(calib_bitmap_inactive);

    this->AddSpacer(FromDIP(10));
    this->AddStretchSpacer(1);

    m_title_item = this->AddLabel(ID_TITLE, "", FromDIP(TOPBAR_TITLE_WIDTH));
    m_title_item->SetAlignment(wxALIGN_CENTRE);

    this->AddSpacer(FromDIP(10));
    this->AddStretchSpacer(1);

    m_publish_bitmap = create_scaled_bitmap("topbar_publish", nullptr, TOPBAR_ICON_SIZE);
    m_publish_item = this->AddTool(ID_PUBLISH, "", m_publish_bitmap);
    m_publish_disable_bitmap = create_scaled_bitmap("topbar_publish_disable", nullptr, TOPBAR_ICON_SIZE);
    m_publish_item->SetDisabledBitmap(m_publish_disable_bitmap);
    this->EnableTool(m_publish_item->GetId(), false);
    this->AddSpacer(FromDIP(4));

    /*wxBitmap model_store_bitmap = create_scaled_bitmap("topbar_store", nullptr, TOPBAR_ICON_SIZE);
    m_model_store_item = this->AddTool(ID_MODEL_STORE, "", model_store_bitmap);
    this->AddSpacer(12);
    */

    // §3.8: a 1px x 22px outline-variant separator sits immediately before the
    // window-control cluster (drawn by BBLTopbarArt::DrawSeparator).
    this->AddSeparator();
    this->AddSpacer(FromDIP(4));

    wxBitmap iconize_bitmap = create_scaled_bitmap("topbar_min", nullptr, TOPBAR_ICON_SIZE);
    wxAuiToolBarItem* iconize_btn = this->AddTool(wxID_ICONIZE_FRAME, "", iconize_bitmap);

    this->AddSpacer(FromDIP(4));

    maximize_bitmap = create_scaled_bitmap("topbar_max", nullptr, TOPBAR_ICON_SIZE);
    window_bitmap = create_scaled_bitmap("topbar_win", nullptr, TOPBAR_ICON_SIZE);
    if (m_frame->IsMaximized()) {
        maximize_btn = this->AddTool(wxID_MAXIMIZE_FRAME, "", window_bitmap);
    }
    else {
        maximize_btn = this->AddTool(wxID_MAXIMIZE_FRAME, "", maximize_bitmap);
    }

    this->AddSpacer(FromDIP(4));

    wxBitmap close_bitmap = create_scaled_bitmap("topbar_close", nullptr, TOPBAR_ICON_SIZE);
    wxAuiToolBarItem* close_btn = this->AddTool(wxID_CLOSE_FRAME, "", close_bitmap);

    Realize();
    // m_toolbar_h = this->GetSize().GetHeight();
    m_toolbar_h = FromDIP(MD3::Metrics::top_bar_height);
    SetMinSize({-1, m_toolbar_h});
    SetMaxSize({-1, m_toolbar_h});

    int client_w = parent->GetClientSize().GetWidth();
    this->SetSize(client_w, m_toolbar_h);

    this->Bind(wxEVT_MOTION, &BBLTopbar::OnMouseMotion, this);
    this->Bind(wxEVT_MOUSE_CAPTURE_LOST, &BBLTopbar::OnMouseCaptureLost, this);
    this->Bind(wxEVT_MENU_CLOSE, &BBLTopbar::OnMenuClose, this);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_FILE_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_EDIT_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_VIEW_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_OBJECTS_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnTopMenuToolItem, this, ID_TOP_HELP_MENU);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnCalibToolItem, this, ID_CALIB);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnIconize, this, wxID_ICONIZE_FRAME);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnFullScreen, this, wxID_MAXIMIZE_FRAME);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnCloseFrame, this, wxID_CLOSE_FRAME);
    this->Bind(wxEVT_LEFT_DCLICK, &BBLTopbar::OnMouseLeftDClock, this);
    this->Bind(wxEVT_LEFT_DOWN, &BBLTopbar::OnMouseLeftDown, this);
    this->Bind(wxEVT_LEFT_UP, &BBLTopbar::OnMouseLeftUp, this);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnOpenProject, this, wxID_OPEN);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnSaveProject, this, wxID_SAVE);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnRedo, this, wxID_REDO);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnUndo, this, wxID_UNDO);
    //this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnModelStoreClicked, this, ID_MODEL_STORE);
    this->Bind(wxEVT_AUITOOLBAR_TOOL_DROPDOWN, &BBLTopbar::OnPublishClicked, this, ID_PUBLISH);
    this->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        update_responsive_title(event.GetSize().GetWidth());
        event.Skip();
    });
    this->Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event) {
        SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
        if (m_brand_item) {
            wxBitmap logo = create_scaled_bitmap("BambuStudio", this, 22);
            m_brand_item->SetBitmap(logo);
            m_brand_item->SetHoverBitmap(logo);
        }
        Realize();
        Refresh(false);
        event.Skip();
    });
}

BBLTopbar::~BBLTopbar()
{
    m_brand_item = nullptr;
    m_file_menu_item = nullptr;
    m_edit_menu_item = nullptr;
    m_view_menu_item = nullptr;
    m_objects_menu_item = nullptr;
    m_help_menu_item = nullptr;
    m_file_menu = nullptr;
    m_edit_menu = nullptr;
    m_view_menu = nullptr;
    m_objects_menu = nullptr;
    m_help_menu = nullptr;
}

void BBLTopbar::OnOpenProject(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->load_project();
}

void BBLTopbar::show_publish_button(bool show)
{
    this->EnableTool(m_publish_item->GetId(), show);
    Refresh();
}

void BBLTopbar::OnSaveProject(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->save_project();
    EnableSaveItem(false);
}

void BBLTopbar::OnUndo(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->undo();
}

void BBLTopbar::OnRedo(wxAuiToolBarEvent& event)
{
    MainFrame* main_frame = dynamic_cast<MainFrame*>(m_frame);
    Plater* plater = main_frame->plater();
    plater->redo();
}

void BBLTopbar::EnableSaveItem(bool enable)
{
    if (m_save_item && GetToolEnabled(m_save_item->GetId()) != enable) {
        this->EnableTool(m_save_item->GetId(), enable);
        Refresh();
    }
}

void BBLTopbar::EnableUndoItem(bool enable)
{
    if (m_undo_item && GetToolEnabled(m_undo_item->GetId()) != enable) {
        this->EnableTool(m_undo_item->GetId(), enable);
        Refresh();
    }
}

void BBLTopbar::EnableRedoItem(bool enable)
{
    if (m_redo_item && GetToolEnabled(m_redo_item->GetId()) != enable) {
        this->EnableTool(m_redo_item->GetId(), enable);
        Refresh();
    }
}

void BBLTopbar::EnableUndoRedoItems()
{
    this->EnableTool(m_undo_item->GetId(), true);
    this->EnableTool(m_redo_item->GetId(), true);
    this->EnableTool(m_calib_item->GetId(), true);
    Refresh();
}

void BBLTopbar::DisableUndoRedoItems()
{
    this->EnableTool(m_undo_item->GetId(), false);
    this->EnableTool(m_redo_item->GetId(), false);
    this->EnableTool(m_calib_item->GetId(), false);
    Refresh();
}

void BBLTopbar::SaveNormalRect()
{
    m_normalRect = m_frame->GetRect();
}

void BBLTopbar::ShowCalibrationButton(bool show)
{
    m_calib_item->GetSizerItem()->Show(show);
    m_sizer->Layout();
    if (!show)
        m_calib_item->GetSizerItem()->SetDimension({-1000, 0}, {0, 0});
    Refresh();
}

void BBLTopbar::OnModelStoreClicked(wxAuiToolBarEvent& event)
{
    //GUI::wxGetApp().load_url(wxString(wxGetApp().app_config->get_web_host_url() + MODEL_STORE_URL));
}

void BBLTopbar::OnPublishClicked(wxAuiToolBarEvent& event)
{
    if (!wxGetApp().getAgent()) {
        BOOST_LOG_TRIVIAL(info) << "publish: no agent";
        return;
    }

    // record
    json j;
    NetworkAgent* agent = GUI::wxGetApp().getAgent();
    if (agent)
        agent->track_event("enter_model_mall", j.dump());

    //no more check
    //if (GUI::wxGetApp().plater()->model().objects.empty()) return;

#ifdef ENABLE_PUBLISHING
    wxGetApp().plater()->show_publish_dialog();
#endif
    wxGetApp().open_publish_page_dialog();
}

void BBLTopbar::SetTopMenus(wxMenu* file_menu, wxMenu* edit_menu, wxMenu* view_menu,
                             wxMenu* objects_menu, wxMenu* help_menu)
{
    m_file_menu = file_menu;
    m_edit_menu = edit_menu;
    m_view_menu = view_menu;
    m_objects_menu = objects_menu;
    m_help_menu = help_menu;
}

wxMenu* BBLTopbar::top_menu_for_tool(int tool_id) const
{
    switch (tool_id) {
    case ID_TOP_FILE_MENU:    return m_file_menu;
    case ID_TOP_EDIT_MENU:    return m_edit_menu;
    case ID_TOP_VIEW_MENU:    return m_view_menu;
    case ID_TOP_OBJECTS_MENU: return m_objects_menu;
    case ID_TOP_HELP_MENU:    return m_help_menu;
    default:                  return nullptr;
    }
}

wxMenu* BBLTopbar::GetCalibMenu()
{
    return &m_calib_menu;
}

void BBLTopbar::SetTitle(wxString title)
{
    m_full_title = title;
    update_responsive_title();
}

int BBLTopbar::measure_fixed_content_width() const
{
    int fixed_width = 0;
    for (size_t index = 0; index < GetToolCount(); ++index) {
        const wxAuiToolBarItem* item = FindToolByIndex(static_cast<int>(index));
        if (!item || item == m_title_item || item->GetProportion() > 0)
            continue;

        const wxSizerItem* sizer_item = item->GetSizerItem();
        if (!sizer_item || !sizer_item->IsShown())
            continue;

        fixed_width += std::max(0, sizer_item->GetMinSize().GetWidth());
    }
    return fixed_width;
}

void BBLTopbar::update_responsive_title(int width)
{
    if (!m_title_item)
        return;

    if (width < 0)
        width = GetClientSize().GetWidth();

    const int title_width = std::min(
        FromDIP(TOPBAR_TITLE_WIDTH),
        std::max(0, width - measure_fixed_content_width()));

    if (m_title_item->GetMinSize().GetWidth() != title_width) {
        m_title_item->SetMinSize({title_width, -1});
        Realize();
    }

    wxGCDC dc(this);
    dc.SetFont(GetFont());
    const wxString title = title_width > 0 ?
        wxControl::Ellipsize(m_full_title, dc, wxELLIPSIZE_END, title_width) : wxString();
    m_title_item->SetLabel(title);
    m_title_item->SetAlignment(wxALIGN_CENTRE);
    Refresh(false);
}

void BBLTopbar::SetMaximizedSize()
{
    maximize_btn->SetBitmap(maximize_bitmap);
}

void BBLTopbar::SetWindowSize()
{
    maximize_btn->SetBitmap(window_bitmap);
}

void BBLTopbar::UpdateToolbarWidth(int width)
{
    this->SetSize(width, m_toolbar_h);
    update_responsive_title(width);
}

void BBLTopbar::Rescale() {
    wxAuiToolBarItem* item;

    item = this->FindTool(ID_LOGO);
    if (item) {
        wxBitmap logo = create_scaled_bitmap("BambuStudio", this, 22);
        item->SetBitmap(logo);
        item->SetHoverBitmap(logo);
    }

    //item = this->FindTool(wxID_OPEN);
    //item->SetBitmap(create_scaled_bitmap("topbar_open", nullptr, TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_SAVE);
    item->SetBitmap(create_scaled_bitmap("topbar_save", this, TOPBAR_ICON_SIZE));
    item->SetDisabledBitmap(create_scaled_bitmap("topbar_save_inactive", nullptr, TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_UNDO);
    item->SetBitmap(create_scaled_bitmap("topbar_undo", this, TOPBAR_ICON_SIZE));
    item->SetDisabledBitmap(create_scaled_bitmap("topbar_undo_inactive", nullptr, TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_REDO);
    item->SetBitmap(create_scaled_bitmap("topbar_redo", this, TOPBAR_ICON_SIZE));
    item->SetDisabledBitmap(create_scaled_bitmap("topbar_redo_inactive", nullptr, TOPBAR_ICON_SIZE));

    item = this->FindTool(ID_CALIB);
    item->SetBitmap(create_scaled_bitmap("calib_sf", nullptr, TOPBAR_ICON_SIZE));
    item->SetDisabledBitmap(create_scaled_bitmap("calib_sf_inactive", nullptr, TOPBAR_ICON_SIZE));

    /*item = this->FindTool(ID_PUBLISH);
    item->SetBitmap(create_scaled_bitmap("topbar_publish", this, TOPBAR_ICON_SIZE));
    item->SetDisabledBitmap(create_scaled_bitmap("topbar_publish_disable", nullptr, TOPBAR_ICON_SIZE));*/

    /*item = this->FindTool(ID_MODEL_STORE);
    item->SetBitmap(create_scaled_bitmap("topbar_store", this, TOPBAR_ICON_SIZE));
    */

    item = this->FindTool(wxID_ICONIZE_FRAME);
    item->SetBitmap(create_scaled_bitmap("topbar_min", this, TOPBAR_ICON_SIZE));

    item = this->FindTool(wxID_MAXIMIZE_FRAME);
    maximize_bitmap = create_scaled_bitmap("topbar_max", this, TOPBAR_ICON_SIZE);
    window_bitmap   = create_scaled_bitmap("topbar_win", this, TOPBAR_ICON_SIZE);
    if (m_frame->IsMaximized()) {
        item->SetBitmap(window_bitmap);
    }
    else {
        item->SetBitmap(maximize_bitmap);
    }

    item = this->FindTool(wxID_CLOSE_FRAME);
    item->SetBitmap(create_scaled_bitmap("topbar_close", this, TOPBAR_ICON_SIZE));

    m_toolbar_h = FromDIP(MD3::Metrics::top_bar_height);
    SetMinSize({-1, m_toolbar_h});
    SetMaxSize({-1, m_toolbar_h});
    SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    Realize();
    SetSize(GetSize().GetWidth(), m_toolbar_h);
    update_responsive_title(GetSize().GetWidth());
    if (GetParent())
        GetParent()->Layout();
    Refresh(false);
}

void BBLTopbar::OnIconize(wxAuiToolBarEvent& event)
{
    m_frame->Iconize();
}

void BBLTopbar::OnFullScreen(wxAuiToolBarEvent& event)
{
    if (m_frame->IsMaximized()) {
        m_frame->Restore();
    }
    else {
        wxDisplay display(this);
        auto      size = display.GetClientArea().GetSize();
#ifdef __WXMSW__
        HWND hWnd = m_frame->GetHandle();
        RECT      borderThickness;
        SetRectEmpty(&borderThickness);
        AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE), FALSE, 0);
        m_frame->SetMaxSize(size + wxSize{-borderThickness.left + borderThickness.right, -borderThickness.top + borderThickness.bottom});
#endif //  __WXMSW__
        m_normalRect = m_frame->GetRect();
        m_frame->Maximize();
    }
}

void BBLTopbar::OnCloseFrame(wxAuiToolBarEvent& event)
{
    m_frame->Close();
}

void BBLTopbar::OnMouseLeftDClock(wxMouseEvent& mouse)
{
    wxPoint mouse_pos = ::wxGetMousePosition();
    // check whether mouse is not on any tool item
    if (this->FindToolByCurrentPosition() != NULL &&
        this->FindToolByCurrentPosition() != m_title_item) {
        mouse.Skip();
        return;
    }
#ifdef __W1XMSW__
    ::PostMessage((HWND) m_frame->GetHandle(), WM_NCLBUTTONDBLCLK, HTCAPTION, MAKELPARAM(mouse_pos.x, mouse_pos.y));
    return;
#endif //  __WXMSW__

    wxAuiToolBarEvent evt;
    OnFullScreen(evt);
}

void BBLTopbar::OnTopMenuToolItem(wxAuiToolBarEvent& evt)
{
    wxAuiToolBar* tb = static_cast<wxAuiToolBar*>(evt.GetEventObject());
    wxMenu* menu = top_menu_for_tool(evt.GetId());
    if (!menu)
        return;

    tb->SetToolSticky(evt.GetId(), true);

    if (m_skip_popup_menu_id != evt.GetId()) {
        const wxRect tool_rect = GetToolRect(evt.GetId());
        const wxPoint screen_anchor = ClientToScreen(
            wxPoint(tool_rect.GetLeft(), GetClientSize().GetHeight() - FromDIP(1)));
        GetParent()->PopupMenu(menu, GetParent()->ScreenToClient(screen_anchor));
    } else {
        m_skip_popup_menu_id = wxID_ANY;
    }

    // Make sure the button is "un-stuck" once the modal popup loop returns.
    tb->SetToolSticky(evt.GetId(), false);
}

void BBLTopbar::OnCalibToolItem(wxAuiToolBarEvent &evt)
{
    wxAuiToolBar *tb = static_cast<wxAuiToolBar *>(evt.GetEventObject());

    tb->SetToolSticky(evt.GetId(), true);

    if (!m_skip_popup_calib_menu) {
        auto rec = this->GetToolRect(ID_CALIB);
        GetParent()->PopupMenu(&m_calib_menu, wxPoint(rec.GetLeft(), this->GetSize().GetHeight() - 2));
    } else {
        m_skip_popup_calib_menu = false;
    }

    // make sure the button is "un-stuck"
    tb->SetToolSticky(evt.GetId(), false);
}

void BBLTopbar::OnMouseLeftDown(wxMouseEvent& event)
{
    wxPoint mouse_pos = ::wxGetMousePosition();
    wxPoint frame_pos = m_frame->GetScreenPosition();
    m_delta = mouse_pos - frame_pos;

    if (FindToolByCurrentPosition() == NULL
        || this->FindToolByCurrentPosition() == m_title_item)
    {
        CaptureMouse();
#ifdef __WXMSW__
        ReleaseMouse();
        ::PostMessage((HWND) m_frame->GetHandle(), WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(mouse_pos.x, mouse_pos.y));
        return;
#endif //  __WXMSW__
    }

    event.Skip();
}

void BBLTopbar::OnMouseLeftUp(wxMouseEvent& event)
{
    wxPoint mouse_pos = ::wxGetMousePosition();
    if (HasCapture())
    {
        ReleaseMouse();
    }

    event.Skip();
}

void BBLTopbar::OnMouseMotion(wxMouseEvent& event)
{
    wxPoint mouse_pos = ::wxGetMousePosition();

    if (!HasCapture()) {
        //m_frame->OnMouseMotion(event);

        event.Skip();
        return;
    }

    if (event.Dragging() && event.LeftIsDown())
    {
        // leave max state and adjust position
        if (m_frame->IsMaximized()) {
            wxRect rect = m_frame->GetRect();
            // Filter unexcept mouse move
            if (m_delta + rect.GetLeftTop() != mouse_pos) {
                m_delta = mouse_pos - rect.GetLeftTop();
                m_delta.x = m_delta.x * m_normalRect.width / rect.width;
                m_delta.y = m_delta.y * m_normalRect.height / rect.height;
                m_frame->Restore();
            }
        }
        m_frame->Move(mouse_pos - m_delta);
    }
    event.Skip();
}

void BBLTopbar::OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
{
}

void BBLTopbar::OnMenuClose(wxMenuEvent& event)
{
    wxAuiToolBarItem* item = this->FindToolByCurrentPosition();
    if (item && top_menu_for_tool(item->GetId()))
        m_skip_popup_menu_id = item->GetId();
}

wxAuiToolBarItem* BBLTopbar::FindToolByCurrentPosition()
{
    wxPoint mouse_pos = ::wxGetMousePosition();
    wxPoint client_pos = this->ScreenToClient(mouse_pos);
    return this->FindToolByPosition(client_pos.x, client_pos.y);
}
