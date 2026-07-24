#include "MainFrame.hpp"
#include "GLToolbar.hpp"
#include <wx/panel.h>
#include <wx/notebook.h>
#include <wx/listbook.h>
#include <wx/simplebook.h>
#include <wx/icon.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/menu.h>
#include <wx/progdlg.h>
#include <wx/textentry.h>
#include <wx/tooltip.h>
#include <wx/glcanvas.h>
#include <wx/filename.h>
#include <wx/debug.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_tree/ptree.hpp>

#include "libslic3r/Print.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/SLAPrint.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/AppConfig.hpp"

#include "Tab.hpp"
#include "ProgressStatusBar.hpp"
#include "3DScene.hpp"
#include "ParamsDialog.hpp"
#include "UserPresetsDialog.hpp"
#include "PrintHostDialogs.hpp"
#include "wxExtensions.hpp"
#include "GUI_ObjectList.hpp"
#include "Mouse3DController.hpp"
//#include "RemovableDriveManager.hpp"
#include "InstanceCheck.hpp"
#include "I18N.hpp"
#include "GLCanvas3D.hpp"
#include "Plater.hpp"
#include "ProjectHistoryDialog.hpp"
#include "WebViewDialog.hpp"
#include "../Utils/Process.hpp"
#include "../Utils/ExternalEditor.hpp"
#include "format.hpp"
// BBS
#include "PartPlate.hpp"
#include "Preferences.hpp"
#include "Widgets/ProgressDialog.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "BindDialog.hpp"
#include "../Utils/MacDarkMode.hpp"

#include <fstream>
#include <filesystem>
#include <string_view>

#include "GUI_App.hpp"
#include "UnsavedChangesDialog.hpp"
#include "MsgDialog.hpp"
#include "Notebook.hpp"
// BBS: session file-tabs
#include "ProjectTabBar.hpp"
#include "libslic3r/Utils.hpp"
#include <boost/filesystem.hpp>
#include <atomic>
#include "GUI_Factories.hpp"
#include "GUI_ObjectList.hpp"
#include "NotificationManager.hpp"
#include "MarkdownTip.hpp"
#include "NetworkTestDialog.hpp"
#include "ConfigWizard.hpp"
#include "Widgets/WebView.hpp"
#include "DailyTips.hpp"
#include "FilamentGroupPopup.hpp"
#include "FilamentMapDialog.hpp"

#include "DeviceCore/DevManager.h"
#include "slic3r/GUI/DeviceWeb/DeviceWebPage.hpp"

#ifdef _WIN32
#include <dbt.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dwmapi.h>
#endif // _WIN32
#include <slic3r/GUI/CreatePresetsDialog.hpp>


namespace Slic3r {
namespace GUI {

wxDEFINE_EVENT(EVT_SELECT_TAB, wxCommandEvent);
wxDEFINE_EVENT(EVT_HTTP_ERROR, wxCommandEvent);
wxDEFINE_EVENT(EVT_USER_LOGIN, wxCommandEvent);
wxDEFINE_EVENT(EVT_USER_LOGIN_HANDLE, wxCommandEvent);
wxDEFINE_EVENT(EVT_CHECK_PRIVACY_VER, wxCommandEvent);
wxDEFINE_EVENT(EVT_CHECK_PRIVACY_SHOW, wxCommandEvent);
wxDEFINE_EVENT(EVT_SHOW_IP_DIALOG, wxCommandEvent);
wxDEFINE_EVENT(EVT_UPDATE_MACHINE_LIST, wxCommandEvent);
wxDEFINE_EVENT(EVT_UPDATE_PRESET_CB, SimpleEvent);



// BBS: backup
wxDEFINE_EVENT(EVT_BACKUP_POST, wxCommandEvent);
wxDEFINE_EVENT(EVT_LOAD_URL, wxCommandEvent);
wxDEFINE_EVENT(EVT_LOAD_PRINTER_URL, wxCommandEvent);

static bool is_text_entry_focused()
{
    for (wxWindow *window = wxWindow::FindFocus(); window != nullptr; window = window->GetParent())
        if (dynamic_cast<wxTextEntry *>(window) != nullptr)
            return true;

    return false;
}

static bool should_skip_fit_camera_shortcut(Plater *plater)
{
    if (is_text_entry_focused())
        return true;

    if (wxGetApp().imgui()->want_text_input())
        return true;

    if (!plater)
        return false;

    auto is_gizmo_running = [](GLCanvas3D *canvas) {
        return canvas && canvas->get_gizmos_manager().is_running();
    };

    return is_gizmo_running(plater->get_view3D_canvas3D()) ||
           is_gizmo_running(plater->get_assmeble_canvas3D()) ||
           is_gizmo_running(plater->get_current_canvas3D());
}

static bool should_block_window_resize_for_assembly(Plater *plater)
{
    GLCanvas3D *canvas = plater ? plater->get_assmeble_canvas3D() : nullptr;
    return canvas && canvas->is_assembly_play_or_export_mode();
}

enum class ERescaleTarget
{
    Mainframe,
    SettingsDialog
};

#ifdef __APPLE__
class BambuStudioTaskBarIcon : public wxTaskBarIcon
{
public:
    BambuStudioTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE) : wxTaskBarIcon(iconType) {}
    wxMenu *CreatePopupMenu() override {
        wxMenu *menu = new wxMenu;
        if (wxGetApp().app_config->get("single_instance") == "false") {
            // Only allow opening a new PrusaSlicer instance on OSX if "single_instance" is disabled,
            // as starting new instances would interfere with the locking mechanism of "single_instance" support.
            append_menu_item(menu, wxID_ANY, _L("New Window"), _L("Open a new window"),
            [](wxCommandEvent&) { start_new_slicer(); }, "", nullptr);
        }
//        append_menu_item(menu, wxID_ANY, _L("G-code Viewer") + dots, _L("Open G-code Viewer"),
//            [](wxCommandEvent&) { start_new_gcodeviewer_open_file(); }, "", nullptr);
        return menu;
    }
};
/*class GCodeViewerTaskBarIcon : public wxTaskBarIcon
{
public:
    GCodeViewerTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE) : wxTaskBarIcon(iconType) {}
    wxMenu *CreatePopupMenu() override {
        wxMenu *menu = new wxMenu;
        append_menu_item(menu, wxID_ANY, _L("Open PrusaSlicer"), _L("Open a new PrusaSlicer"),
            [](wxCommandEvent&) { start_new_slicer(nullptr, true); }, "", nullptr);
        //append_menu_item(menu, wxID_ANY, _L("G-code Viewer") + dots, _L("Open new G-code Viewer"),
        //    [](wxCommandEvent&) { start_new_gcodeviewer_open_file(); }, "", nullptr);
        return menu;
    }
};*/
#endif // __APPLE__

// Load the icon either from the exe, or from the ico file.
static wxIcon main_frame_icon(GUI_App::EAppMode app_mode)
{
#if _WIN32
    std::wstring path(size_t(MAX_PATH), wchar_t(0));
    int len = int(::GetModuleFileName(nullptr, path.data(), MAX_PATH));
    if (len > 0 && len < MAX_PATH) {
        path.erase(path.begin() + len, path.end());
        //BBS: remove GCodeViewer as seperate APP logic
        /*if (app_mode == GUI_App::EAppMode::GCodeViewer) {
            // Only in case the slicer was started with --gcodeviewer parameter try to load the icon from prusa-gcodeviewer.exe
            // Otherwise load it from the exe.
            for (const std::wstring_view exe_name : { std::wstring_view(L"prusa-slicer.exe"), std::wstring_view(L"prusa-slicer-console.exe") })
                if (boost::iends_with(path, exe_name)) {
                    path.erase(path.end() - exe_name.size(), path.end());
                    path += L"prusa-gcodeviewer.exe";
                    break;
                }
        }*/
    }
    return wxIcon(path, wxBITMAP_TYPE_ICO);
#else // _WIN32
    return wxIcon(Slic3r::var("BambuStudio_128px.png"), wxBITMAP_TYPE_PNG);
#endif // _WIN32
}

// BBS
#ifndef __APPLE__
#define BORDERLESS_FRAME_STYLE (wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)
#else
#define BORDERLESS_FRAME_STYLE (wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX)
#endif

wxDEFINE_EVENT(EVT_SYNC_CLOUD_PRESET,     SimpleEvent);

#ifdef __APPLE__
static const wxString ctrl = ("Ctrl+");
#else
static const wxString ctrl = _L("Ctrl+");
#endif

MainFrame::MainFrame() :
DPIFrame(NULL, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, BORDERLESS_FRAME_STYLE, "mainframe")
    , m_printhost_queue_dlg(new PrintHostQueueDialog(this))
    // BBS
    , m_recent_projects(18)
    , m_settings_dialog(this)
    , diff_dialog(this)
{
#ifdef __WXOSX__
    set_miniaturizable(GetHandle());
#endif

#ifdef _WIN32
    // Enable DWM hardware-accelerated compositing for this borderless window.
    // Without this, dragging the window by the title bar causes lag because
    // DWM can't efficiently composite a frameless window during drag.
    MARGINS margins = {0, 0, 0, 1};
    DwmExtendFrameIntoClientArea((HWND)GetHandle(), &margins);
#endif

    if (!wxGetApp().app_config->has("user_mode")) {
        wxGetApp().app_config->set("user_mode", "simple");
        wxGetApp().app_config->set_bool("developer_mode", false);
        wxGetApp().app_config->save();
    }

    wxGetApp().app_config->set_bool("internal_developer_mode", false);

    wxString max_recent_count_str = wxGetApp().app_config->get("max_recent_count");
    long max_recent_count = 18;
    if (max_recent_count_str.ToLong(&max_recent_count))
        set_max_recent_count((int)max_recent_count);

    //reset log level
    auto loglevel = wxGetApp().app_config->get("severity_level");
    std::map<std::string, int> wx_log_levels{{"fatal", wxLOG_FatalError}, {"error", wxLOG_FatalError}, {"warning", wxLOG_Warning},
                                             {"info", wxLOG_Info},        {"debug", wxLOG_Debug},      {"trace", wxLOG_Trace}};
    wxLog::SetLogLevel(wx_log_levels[loglevel]);

    // BBS
    m_recent_projects.SetMenuPathStyle(wxFH_PATH_SHOW_ALWAYS);
    MarkdownTip::Recreate(this);

    // Fonts were created by the DPIFrame constructor for the monitor, on which the window opened.
    wxGetApp().update_fonts(this);

    // Paint the frame's own client area with the caption surface. Any region a
    // child does not cover (e.g. the caption row while the topbar catches up
    // with an external resize) otherwise shows the stock wxFrame APPWORKSPACE
    // grey — which the dark remap turns into a light #94959f band to the right
    // of the window controls in dark mode.
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));

#ifndef __APPLE__
    m_topbar         = new BBLTopbar(this);
#else
    auto panel_topbar = new wxPanel(this, wxID_ANY);
    // Title-bar surface role (matches BBLTopbar's SurfaceContainerLow background).
    panel_topbar->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    auto sizer_tobar = new wxBoxSizer(wxVERTICAL);
    panel_topbar->SetSizer(sizer_tobar);
    panel_topbar->Layout();
#endif

    //wxAuiToolBar* toolbar = new wxAuiToolBar();
/*
#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
    this->SetFont(this->normal_font());
#endif
    // Font is already set in DPIFrame constructor
*/

#ifdef __APPLE__
	m_reset_title_text_colour_timer = new wxTimer();
	m_reset_title_text_colour_timer->SetOwner(this);
	Bind(wxEVT_TIMER, [this](auto& e) {
		set_title_colour_after_set_title(GetHandle());
		m_reset_title_text_colour_timer->Stop();
	});
	this->Bind(wxEVT_FULLSCREEN, [this](wxFullScreenEvent& e) {
		set_tag_when_enter_full_screen(e.IsFullScreen());
		if (!e.IsFullScreen()) {
            if (m_reset_title_text_colour_timer) {
                m_reset_title_text_colour_timer->Stop();
                m_reset_title_text_colour_timer->Start(500);
            }
            m_mac_fullscreen = false;
        } else {
            m_mac_fullscreen = true;
        }
        auto int_event = new IntEvent(EVT_NOTICE_FULL_SCREEN_CHANGED, e.IsFullScreen() ? 1 : 0);
        wxQueueEvent(wxGetApp().plater(), int_event);
		e.Skip();
	});
#endif

#ifdef __APPLE__
    // Initialize the docker task bar icon.
    switch (wxGetApp().get_app_mode()) {
    default:
    case GUI_App::EAppMode::Editor:
        m_taskbar_icon = std::make_unique<BambuStudioTaskBarIcon>(wxTBI_DOCK);
        m_taskbar_icon->SetIcon(wxIcon(Slic3r::var("BambuStudio-mac_256px.ico"), wxBITMAP_TYPE_ICO), "BambuStudio");
        break;
    case GUI_App::EAppMode::GCodeViewer:
        break;
    }
#endif // __APPLE__

    // Load the icon either from the exe, or from the ico file.
    SetIcon(main_frame_icon(wxGetApp().get_app_mode()));

    // initialize tabpanel and menubar
    init_tabpanel();
    if (wxGetApp().is_gcode_viewer())
        init_menubar_as_gcodeviewer();
    else
        init_menubar_as_editor();

    // BBS
#if 0
    // This is needed on Windows to fake the CTRL+# of the window menu when using the numpad
    wxAcceleratorEntry entries[6];
    entries[0].Set(wxACCEL_CTRL, WXK_NUMPAD1, wxID_HIGHEST + 1);
    entries[1].Set(wxACCEL_CTRL, WXK_NUMPAD2, wxID_HIGHEST + 2);
    entries[2].Set(wxACCEL_CTRL, WXK_NUMPAD3, wxID_HIGHEST + 3);
    entries[3].Set(wxACCEL_CTRL, WXK_NUMPAD4, wxID_HIGHEST + 4);
    entries[4].Set(wxACCEL_CTRL, WXK_NUMPAD5, wxID_HIGHEST + 5);
    entries[5].Set(wxACCEL_CTRL, WXK_NUMPAD6, wxID_HIGHEST + 6);
    wxAcceleratorTable accel(6, entries);
    SetAcceleratorTable(accel);
#endif // _WIN32

    // BBS
    //wxAcceleratorEntry entries[13];
    //int index = 0;
    //entries[index++].Set(wxACCEL_CTRL, (int)'N', wxID_HIGHEST + wxID_NEW);
    //entries[index++].Set(wxACCEL_CTRL, (int)'O', wxID_HIGHEST + wxID_OPEN);
    //entries[index++].Set(wxACCEL_CTRL, (int)'S', wxID_HIGHEST + wxID_SAVE);
    //entries[index++].Set(wxACCEL_CTRL | wxACCEL_SHIFT, (int)'S', wxID_HIGHEST + wxID_SAVEAS);
    //entries[index++].Set(wxACCEL_CTRL, (int)'X', wxID_HIGHEST + wxID_CUT);
    ////entries[index++].Set(wxACCEL_CTRL, (int)'I', wxID_HIGHEST + wxID_ADD);
    //entries[index++].Set(wxACCEL_CTRL, (int)'A', wxID_HIGHEST + wxID_SELECTALL);
    //entries[index++].Set(wxACCEL_NORMAL, (int)27 /* escape */, wxID_HIGHEST + wxID_CANCEL);
    //entries[index++].Set(wxACCEL_CTRL, (int)'Z', wxID_HIGHEST + wxID_UNDO);
    //entries[index++].Set(wxACCEL_CTRL, (int)'Y', wxID_HIGHEST + wxID_REDO);
    //entries[index++].Set(wxACCEL_CTRL, (int)'C', wxID_HIGHEST + wxID_COPY);
    //entries[index++].Set(wxACCEL_CTRL, (int)'V', wxID_HIGHEST + wxID_PASTE);
    //entries[index++].Set(wxACCEL_CTRL, (int)'P', wxID_HIGHEST + wxID_PREFERENCES);
    //entries[index++].Set(wxACCEL_CTRL, (int)'I', wxID_HIGHEST + wxID_FILE6);
    //wxAcceleratorTable accel(sizeof(entries) / sizeof(entries[0]), entries);
    //SetAcceleratorTable(accel);

    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_plater->new_project(); }, wxID_HIGHEST + wxID_NEW);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_plater->load_project(); }, wxID_HIGHEST + wxID_OPEN);
    //// BBS: close save project
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(); }, wxID_HIGHEST + wxID_SAVE);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(true); }, wxID_HIGHEST + wxID_SAVEAS);
    ////Bind(wxEVT_MENU, [this](wxCommandEvent&) { if (m_plater) m_plater->add_model(); }, wxID_HIGHEST + wxID_ADD);
    ////Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_plater->remove_selected(); }, wxID_HIGHEST + wxID_DELETE);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) {
    //        if (!can_add_models())
    //            return;
    //        if (m_plater) {
    //            m_plater->add_model();
    //        }
    //    }, wxID_HIGHEST + wxID_FILE6);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_plater->select_all(); }, wxID_HIGHEST + wxID_SELECTALL);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_plater->deselect_all(); }, wxID_HIGHEST + wxID_CANCEL);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) {
    //    if (m_plater->is_view3D_shown())
    //        m_plater->undo();
    //    }, wxID_HIGHEST + wxID_UNDO);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) {
    //    if (m_plater->is_view3D_shown())
    //        m_plater->redo();
    //    }, wxID_HIGHEST + wxID_REDO);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_plater->copy_selection_to_clipboard(); }, wxID_HIGHEST + wxID_COPY);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_plater->paste_from_clipboard(); }, wxID_HIGHEST + wxID_PASTE);
    //Bind(wxEVT_MENU, [this](wxCommandEvent&) { m_plater->cut_selection_to_clipboard(); }, wxID_HIGHEST + wxID_CUT);
    Bind(wxEVT_SIZE, [this](wxSizeEvent&) {
            BOOST_LOG_TRIVIAL(trace) << "mainframe: size changed, is maximized = " << this->IsMaximized();
#ifndef __APPLE__
            if (this->IsMaximized()) {
                m_topbar->SetWindowSize();
            } else {
                m_topbar->SetMaximizedSize();
            }
            // Keep the caption row spanning the full client width even when the
            // early-outs below skip the frame Layout() (assembly view block /
            // interactive-resize throttle). A stale-width topbar left a light
            // unpainted band right of the window controls in dark captures.
            if (m_topbar) {
                const int client_w = GetClientSize().GetWidth();
                if (client_w > 0 && m_topbar->GetSize().GetWidth() != client_w)
                    m_topbar->UpdateToolbarWidth(client_w);
            }
#endif
        if (should_block_window_resize_for_assembly(m_plater))
            return;

#ifdef _WIN32
        if (m_is_in_move_or_resize) {
            ULONGLONG now = GetTickCount64();
            if (now - m_last_resize_layout_ms < 33)
                return;
            m_last_resize_layout_ms = now;
        }
#endif
        Refresh();
        Layout();
        wxQueueEvent(wxGetApp().plater(), new SimpleEvent(EVT_NOTICE_CHILDE_SIZE_CHANGED));
        });

    //BBS
    Bind(EVT_SELECT_TAB, [this](wxCommandEvent&evt) {
        TabPosition pos = (TabPosition)evt.GetInt();
        m_tabpanel->SetSelection(pos);
    });

    Bind(EVT_SYNC_CLOUD_PRESET, &MainFrame::on_select_default_preset, this);

//    Bind(wxEVT_MENU,
//        [this](wxCommandEvent&)
//        {
//            PreferencesDialog dlg(this);
//            dlg.ShowModal();
//#if ENABLE_GCODE_LINES_ID_IN_H_SLIDER
//            if (dlg.seq_top_layer_only_changed() || dlg.seq_seq_top_gcode_indices_changed())
//#else
//            if (dlg.seq_top_layer_only_changed())
//#endif // ENABLE_GCODE_LINES_ID_IN_H_SLIDER
//                plater()->refresh_print();
//        }, wxID_HIGHEST + wxID_PREFERENCES);


    // set default tooltip timer in msec
    // SetAutoPop supposedly accepts long integers but some bug doesn't allow for larger values
    // (SetAutoPop is not available on GTK.)
    wxToolTip::SetAutoPop(32767);

    m_loaded = true;

    // initialize layout
    m_main_sizer = new wxBoxSizer(wxVERTICAL);
    wxSizer* sizer = new wxBoxSizer(wxVERTICAL);
#ifndef __APPLE__
     sizer->Add(m_topbar, 0, wxEXPAND);
#else
     sizer->Add(panel_topbar, 0, wxEXPAND);
#endif // __WINDOWS__

    // BBS: session file-tabs. The project bar lives in the top-level frame sizer,
    // BETWEEN the title bar and the workspace tabs, so the layout reads
    // title-bar -> project-tabs -> workspace-tabs -> canvas.
    m_project_tabbar = new ProjectTabBar(this);
    Bind(EVT_PROJECT_TAB_SWITCH, [this](wxCommandEvent& e) { switch_project_tab(e.GetInt()); });
    Bind(EVT_PROJECT_TAB_CLOSE,  [this](wxCommandEvent& e) { close_project_tab(e.GetInt()); });
    Bind(EVT_PROJECT_TAB_NEW,    [this](wxCommandEvent&)   { new_project_tab(); });
    sizer->Add(m_project_tabbar, 0, wxEXPAND);

    // Restore persisted tab entries; seed a single tab for the current (Untitled)
    // project when none were restored so single-tab use behaves exactly like today.
    m_project_tabbar->LoadFromConfig();
    if (m_project_tabbar->Count() == 0) {
        const wxString cur_file  = m_plater ? m_plater->get_project_filename() : wxString();
        wxString       cur_title = m_plater ? m_plater->get_project_name() : wxString();
        if (cur_title.IsEmpty())
            cur_title = _L("Untitled");
        m_project_tabbar->AddTab(into_u8(cur_file), cur_title, /*activate=*/true);
    } else if (m_project_tabbar->GetActive() < 0) {
        m_project_tabbar->SetActive(0);
    }
    // Defer the first activation (loading the active tab's document) until the frame
    // and canvases are realized and the app finished its own startup file loading.
    CallAfter([this]() { reconcile_initial_project_tab(); });

    sizer->Add(m_main_sizer, 1, wxEXPAND);
    SetSizerAndFit(sizer);
    // initialize layout from config
    update_layout();
    sizer->SetSizeHints(this);

    // BBS: fix taskbar overlay on windows
#ifdef WIN32
    auto setMaxSize = [this]() {
        wxDisplay display(this);
        auto size = display.GetClientArea().GetSize();
        HWND      hWnd = GetHandle();
        RECT      borderThickness;
        SetRectEmpty(&borderThickness);
        AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE), FALSE, 0);
        SetMaxSize(size + wxSize{-borderThickness.left + borderThickness.right, -borderThickness.top + borderThickness.bottom});
    };
    this->Bind(wxEVT_DPI_CHANGED, [setMaxSize](auto & e) {
        setMaxSize();
        e.Skip();
        });
    setMaxSize();
    // SetMaximize already position window at left/top corner, even if Windows Task Bar is at left side.
    // Not known why, but fix it here
    this->Bind(wxEVT_MAXIMIZE, [this](auto &e) {
        wxDisplay display(this);
        auto pos = display.GetClientArea().GetPosition();
        HWND      hWnd = GetHandle();
        RECT      borderThickness;
        SetRectEmpty(&borderThickness);
        AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE), FALSE, 0);
        Move(pos + wxPoint{borderThickness.left, borderThickness.top});
        e.Skip();
    });
#endif // WIN32
    // BBS
    Fit();

    const wxSize min_size = wxGetApp().get_min_size(); //wxSize(76*wxGetApp().em_unit(), 49*wxGetApp().em_unit());

    SetMinSize(min_size/*wxSize(760, 490)*/);
    SetSize(wxSize(FromDIP(1200), FromDIP(800)));

    Layout();

    update_title();

    // declare events
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ": mainframe received close_widow event";
        if (event.CanVeto() && m_plater->get_view3D_canvas3D()->get_gizmos_manager().is_in_editing_mode(true)) {
            // prevents to open the save dirty project dialog
            event.Veto();
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< "cancelled by gizmo in editing";
            return;
        }

        //BBS:
        //if (event.CanVeto() && !wxGetApp().check_and_save_current_preset_changes(_L("Application is closing"), _L("Closing Application while some presets are modified."))) {
        //    event.Veto();
        //    return;
        //}
        auto check = [](bool yes_or_no) {
            if (yes_or_no)
                return true;
            return wxGetApp().check_and_save_current_preset_changes(_L("Application is closing"), _L("Closing Application while some presets are modified."));
        };

        // BBS: close save project
        int result;
        if (event.CanVeto() && ((result = m_plater->close_with_confirm(check)) == wxID_CANCEL)) {
            event.Veto();
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< "cancelled by close_with_confirm selection";
            return;
        }
        if (event.CanVeto() && !wxGetApp().check_print_host_queue()) {
            event.Veto();
            return;
        }
        if (GLCanvas3D *canvas = m_plater->get_assmeble_canvas3D()) {
            canvas->close_project_and_save_assembly_steps_tree();
        }

    #if 0 // BBS
        //if (m_plater != nullptr) {
        //    int saved_project = m_plater->save_project_if_dirty(_L("Closing Application. Current project is modified."));
        //    if (saved_project == wxID_CANCEL) {
        //        event.Veto();
        //        return;
        //    }
        //    // check unsaved changes only if project wasn't saved
        //    else if (plater()->is_project_dirty() && saved_project == wxID_NO && event.CanVeto() &&
        //             (plater()->is_presets_dirty() && !wxGetApp().check_and_save_current_preset_changes(_L("Application is closing"), _L("Closing Application while some presets are modified.")))) {
        //        event.Veto();
        //        return;
        //    }
        //}
    #endif

        try {
            NetworkAgent* agent = GUI::wxGetApp().getAgent();
            if (agent) {
                json j;

                auto get_value = [&agent](const std::string& name) -> std::string {
                    std::string value = "";
                    agent->track_get_property(name, value);
                    if (value == "")
                        value = "0";

                    return value;
                };

                j["auto_orient"] = get_value("auto_orient");
                j["auto_arrange"] = get_value("auto_arrange");
                j["split_to_objects"] = get_value("split_to_objects");
                j["split_to_part"] = get_value("split_to_part");
                j["custom_height"] = get_value("custom_height");
                j["move"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Move));
                j["rotate"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Rotate));
                j["scale"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Scale));
                j["flatten"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Flatten));
                j["cut"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Cut));
                j["meshboolean"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::MeshBoolean));
                j["custom_support"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::FdmSupports));
                j["custom_fuzzyskin"]      = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::FuzzySkin));
                j["custom_seam"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Seam));
                j["text_shape"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Text));
                j["measure"]            = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Measure));
                j["assembly"]               = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::Assembly));
                j["color_painting"] = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::MmuSegmentation));
                j["brimears"]            = get_value(GLGizmosManager::convert_gizmo_type_to_string(GLGizmosManager::EType::BrimEars));
                j["assembly_view"] = get_value("assembly_view");
                j["assembly_view_export_pdf"] = get_value("assembly_view_export_pdf");
                j["assembly_view_export_markdown"] = get_value("assembly_view_export_markdown");
                j["assembly_view_export_mp4"] = get_value("assembly_view_export_mp4");

                agent->track_event("key_func", j.dump());

                j.clear();
                j["auto_arrange_duration"] = get_value("arrange_duration");
                j["custom_height_duration"] = get_value("layersediting_duration");
                j["move_duration"] = get_value("Move_duration");
                j["rotate_duration"] = get_value("Rotate_duration");
                j["scale_duration"] = get_value("Scale_duration");
                j["flatten_duration"] = get_value("Lay on face_duration");
                j["cut_duration"] = get_value("Cut_duration");
                j["brimears_duration"]         = get_value("Brimears_duration");
                j["meshboolean_duration"] = get_value("Mesh Boolean_duration");
                j["custom_support_duration"] = get_value("Supports Painting_duration");
                j["custom_seam_duration"] = get_value("Seam painting_duration");
                j["text_shape_duration"] = get_value("Text shape_duration");
                j["color_painting_duration"] = get_value("Color Painting_duration");
                j["fuzzyskin_painting_duration"] = get_value("FuzzySkin Painting_duration");
                j["assembly_duration"]         = get_value("Assemble_duration");
                j["measure_duration"]         = get_value("Measure_duration");
                j["assembly_view_duration"] = get_value("assembly_view_duration");

                agent->track_event("key_func_duration", j.dump());

                j.clear();
                j["default_menu"] = get_value("default_menu");
                j["object_menu"] = get_value("object_menu");
                j["part_menu"] = get_value("part_menu");
                j["multi_selection_menu"] = get_value("multi_selection_menu");
                j["plate_menu"] = get_value("plate_menu");
                j["assemble_object_menu"] = get_value("assemble_object_menu");
                j["assemble_multi_selection_menu"] = get_value("assemble_multi_selection_menu");
                agent->track_event("menu_click", j.dump());

                j.clear();
                j["device_page"] = get_value("select_device_page");
                j["status"] = get_value("status");
                j["sd_card"] = get_value("sd_card");
                j["HMS"] = get_value("HMS");
                j["update"] = get_value("update");
                agent->track_event("device_ctrl", j.dump());
            }
        }
        catch (...) {}

        MarkdownTip::ExitTip();

        m_plater->reset();
        this->shutdown();
        // propagate event

        wxGetApp().remove_mall_system_dialog();
        event.Skip();
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__<< ": mainframe finished process close_widow event";
    });

    //FIXME it seems this method is not called on application start-up, at least not on Windows. Why?
    // The same applies to wxEVT_CREATE, it is not being called on startup on Windows.
    Bind(wxEVT_ACTIVATE, [this](wxActivateEvent& event) {
        if (m_plater != nullptr && event.GetActive())
            m_plater->on_activate();
        event.Skip();
    });

// OSX specific issue:
// When we move application between Retina and non-Retina displays, The legend on a canvas doesn't redraw
// So, redraw explicitly canvas, when application is moved
//FIXME maybe this is useful for __WXGTK3__ as well?
#if __APPLE__
    Bind(wxEVT_MOVE, [](wxMoveEvent& event) {
        wxGetApp().plater()->get_current_canvas3D()->set_as_dirty();
        wxGetApp().plater()->get_current_canvas3D()->request_extra_frame();
        event.Skip();
    });
#endif

    update_ui_from_settings();    // FIXME (?)

    if (m_plater != nullptr) {
        // BBS
        update_slice_print_status(eEventSliceUpdate, true, true);

        // BBS: backup project
        if (wxGetApp().app_config->get("backup_switch") == "true") {
            std::string backup_interval;
            if (!wxGetApp().app_config->get("app", "backup_interval", backup_interval))
                backup_interval = "10";
            Slic3r::set_backup_interval(boost::lexical_cast<long>(backup_interval));
        } else {
            Slic3r::set_backup_interval(0);
        }
        Slic3r::set_backup_callback([this](int action) {
            if (action == 0) {
                wxPostEvent(this, wxCommandEvent(EVT_BACKUP_POST));
            }
            else if (action == 1) {
                if (!m_plater->up_to_date(false, true)) {
                    m_plater->export_3mf(m_plater->model().get_backup_path() + "/.3mf", SaveStrategy::Backup);
                    m_plater->up_to_date(true, true);
                }
            }
         });
        Bind(EVT_BACKUP_POST, [](wxCommandEvent& e) {
            Slic3r::run_backup_ui_tasks();
            });
;    }
    this->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &evt) {
        const int key_code = evt.GetKeyCode();
#ifdef __APPLE__
        if (evt.CmdDown() && (evt.GetKeyCode() == 'H')) {
            // Pass Cmd+H through to the macOS application menu so the
            // standard "Hide BambuStudio" action fires.  Without evt.Skip()
            // wxEVT_CHAR_HOOK consumes the event and macOS never sees it.
            evt.Skip();
            return;
        }
        if (evt.CmdDown() && !evt.ShiftDown() && (evt.GetKeyCode() == 'M')) {
            this->Iconize();
            return;
        }
        if (evt.CmdDown() && evt.GetKeyCode() == 'Q') { wxPostEvent(this, wxCloseEvent(wxEVT_CLOSE_WINDOW)); return;}
        if (evt.CmdDown() && evt.RawControlDown() && evt.GetKeyCode() == 'F') {
            EnableFullScreenView(true);
            if (IsFullScreen()) {
                ShowFullScreen(false);
            } else {
                ShowFullScreen(true);
            }
            return;}
#endif
        if (evt.CmdDown() && evt.GetKeyCode() == 'R') { if (m_slice_enable) { wxGetApp().plater()->update(true, true); wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SLICE_PLATE)); this->m_tabpanel->SetSelection(tpPreview); } return; }
        if (evt.CmdDown() && evt.ShiftDown() && evt.GetKeyCode() == 'G') {
            m_plater->apply_background_progress();
            m_print_enable = get_enable_print_status();
            m_print_btn->Enable(m_print_enable);
            if (m_print_enable) {
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_PLATE));
            }
            evt.Skip();
            return;
        }
        else if (evt.CmdDown() && evt.GetKeyCode() == 'G') { if (can_export_gcode()) { wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_SLICED_FILE)); } evt.Skip(); return; }
        if (evt.CmdDown() && evt.GetKeyCode() == 'J') { m_printhost_queue_dlg->Show(); return; }
        if (evt.CmdDown() && evt.GetKeyCode() == 'N') { new_project_tab(); return;}
        if (evt.CmdDown() && evt.GetKeyCode() == 'O') { open_project_tab(); return;}
        if (evt.CmdDown() && evt.ShiftDown() && evt.GetKeyCode() == 'S') { if (can_save_as()) m_plater->save_project(true); return;}
        else if (evt.CmdDown() && evt.GetKeyCode() == 'S') { if (can_save()) m_plater->save_project(); return;}
        if (evt.CmdDown() && evt.GetKeyCode() == 'F') {
            if (m_plater && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor || m_tabpanel->GetSelection() == TabPosition::tpPreview)) {
                m_plater->sidebar().can_search();
            }
        }
#ifdef __APPLE__
        if (evt.CmdDown() && evt.GetKeyCode() == ',')
#else
        if (evt.CmdDown() && evt.GetKeyCode() == 'P')
#endif
        {
            PreferencesDialog dlg(this);
            dlg.ShowModal();
#if ENABLE_GCODE_LINES_ID_IN_H_SLIDER
            if (dlg.seq_top_layer_only_changed() || dlg.seq_seq_top_gcode_indices_changed())
#else
            if (dlg.seq_top_layer_only_changed())
#endif // ENABLE_GCODE_LINES_ID_IN_H_SLIDER
                plater()->refresh_print();

            // Refresh recent list if time format changed
            if (dlg.use_12h_time_format_changed() && m_webview) {
                wxGetApp().CallAfter([this]() {
                    if (m_webview) {
                        m_webview->SendRecentList(-1);
                    }
                });
            }
            return;
        }

        if (evt.CmdDown() && evt.GetKeyCode() == 'I') {
            if (!can_add_models()) return;
            if (m_plater) { m_plater->add_file(); }
            return;
        }

        if (!evt.HasAnyModifiers() && (evt.GetKeyCode() == 'Z' || evt.GetKeyCode() == 'z')) {
            if (!should_skip_fit_camera_shortcut(m_plater))
                view_zoom_to_fit();
            else
                evt.Skip();
            return;
        }

        // Pass 3D view preset shortcuts directly to the current canvas. (Only 0-7 currently used but reserve 8 & 9 anyway.)
        if (evt.CmdDown() && ((key_code >= '0' && key_code <= '9') || (key_code >= WXK_NUMPAD0 && key_code <= WXK_NUMPAD9)) && m_plater) {
            if (auto *canvas = m_plater->canvas3D()) {
                if (auto *target = (wxWindow*)canvas->get_wxglcanvas()) {
                    wxKeyEvent e(evt);
                    e.SetEventType(wxEVT_KEY_UP);
                    e.SetEventObject(target);
                    wxPostEvent(target, e);
                }
            }
            return;
        }

        evt.Skip();
    });

    Bind(wxEVT_SHOW, [this](wxShowEvent &evt) {
        DeviceManager *manger = wxGetApp().getDeviceManager();
        if (manger) {
            evt.IsShown() ? manger->start_refresher() : manger->stop_refresher();
        }
    });
    Bind(wxEVT_IDLE, ([this](wxIdleEvent &e) {
             if (m_topbar && m_plater) {
                 m_topbar->EnableSaveItem(can_save());
                 m_topbar->EnableUndoItem(m_plater->can_undo());
                 m_topbar->EnableRedoItem(m_plater->can_redo());
             }
             // Keep the unsaved-changes "*" in the title in sync on all platforms (#9987).
             if (m_plater)
                 update_title();
         }));
#ifdef _MSW_DARK_MODE
    wxGetApp().UpdateDarkUIWin(this);
#endif // _MSW_DARK_MODE

    wxGetApp().persist_window_geometry(this, true);
    wxGetApp().persist_window_geometry(&m_settings_dialog, true);
}

#ifdef __WIN32__
// Orca: Fix maximized window overlaps taskbar when taskbar auto hide is enabled (#8085)
// Adopted from https://gist.github.com/MortenChristiansen/6463580
static void AdjustWorkingAreaForAutoHide(const HWND hWnd, MINMAXINFO *mmi)
{
    const auto taskbarHwnd = FindWindowA("Shell_TrayWnd", nullptr);
    if (!taskbarHwnd) { return; }
    const auto monitorContainingApplication = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONULL);
    const auto monitorWithTaskbarOnIt       = MonitorFromWindow(taskbarHwnd, MONITOR_DEFAULTTONULL);
    if (monitorContainingApplication != monitorWithTaskbarOnIt) { return; }
    APPBARDATA abd;
    abd.cbSize = sizeof(APPBARDATA);
    abd.hWnd   = taskbarHwnd;

    // Find if task bar has auto-hide enabled
    const auto uState = (UINT) SHAppBarMessage(ABM_GETSTATE, &abd);
    if ((uState & ABS_AUTOHIDE) != ABS_AUTOHIDE) { return; }

    RECT borderThickness;
    SetRectEmpty(&borderThickness);
    AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, 0);

    // Determine taskbar position
    SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    const auto &rc = abd.rc;
    if (rc.top == rc.left && rc.bottom > rc.right) {
        // Left
        const auto offset = borderThickness.left + 2;
        mmi->ptMaxPosition.x += offset;
        mmi->ptMaxTrackSize.x -= offset;
        mmi->ptMaxSize.x -= offset;
    } else if (rc.top == rc.left && rc.bottom < rc.right) {
        // Top
        const auto offset = borderThickness.top + 2;
        mmi->ptMaxPosition.y += offset;
        mmi->ptMaxTrackSize.y -= offset;
        mmi->ptMaxSize.y -= offset;
    } else if (rc.top > rc.left) {
        // Bottom
        const auto offset = borderThickness.bottom + 2;
        mmi->ptMaxSize.y -= offset;
        mmi->ptMaxTrackSize.y -= offset;
    } else {
        // Right
        const auto offset = borderThickness.right + 2;
        mmi->ptMaxSize.x -= offset;
        mmi->ptMaxTrackSize.x -= offset;
    }
}

WXLRESULT MainFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    /* When we have a custom titlebar in the window, we don't need the non-client area of a normal window
     * to be painted. In order to achieve this, we handle the "WM_NCCALCSIZE" which is responsible for the
     * size of non-client area of a window and set the return value to 0. Also we have to tell the
     * application to not paint this area on activate and deactivation events so we also handle
     * "WM_NCACTIVATE" message. */
    switch (nMsg) {
    case WM_NCACTIVATE: {
        /* Returning 0 from this message disable the window from receiving activate events which is not
        desirable. However When a visual style is not active (?) for this window, "lParam" is a handle to an
        optional update region for the nonclient area of the window. If this parameter is set to -1,
        DefWindowProc does not repaint the nonclient area to reflect the state change. */
        lParam = -1;
        break;
    }
    /* To remove the standard window frame, you must handle the WM_NCCALCSIZE message, specifically when
    its wParam value is TRUE and the return value is 0 */
    case WM_NCCALCSIZE:
        if (wParam) {
            HWND hWnd = GetHandle();
            /* Detect whether window is maximized or not. We don't need to change the resize border when win is
             *  maximized because all resize borders are gone automatically */
            WINDOWPLACEMENT wPos;
            // GetWindowPlacement fail if this member is not set correctly.
            wPos.length = sizeof(wPos);
            GetWindowPlacement(hWnd, &wPos);
            if (wPos.showCmd != SW_SHOWMAXIMIZED) {
                RECT borderThickness;
                SetRectEmpty(&borderThickness);
                AdjustWindowRectEx(&borderThickness, GetWindowLongPtr(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL);
                borderThickness.left *= -1;
                borderThickness.top *= -1;
                NCCALCSIZE_PARAMS *sz = reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);
                // Add 1 pixel to the top border to make the window resizable from the top border
                sz->rgrc[0].top += 1; // borderThickness.top;
                sz->rgrc[0].left += borderThickness.left;
                sz->rgrc[0].right -= borderThickness.right;
                sz->rgrc[0].bottom -= borderThickness.bottom;
                return 0;
            }
        }
        break;
    case WM_GETMINMAXINFO: {
        if (lParam) {
            HWND hWnd = GetHandle();
            auto mmi = (MINMAXINFO *) lParam;
            HandleGetMinMaxInfo(mmi);
            AdjustWorkingAreaForAutoHide(hWnd, mmi);
            return 0;
        }
        break;
    }
    case WM_ENTERSIZEMOVE:
        if (should_block_window_resize_for_assembly(m_plater))
            return 0;
        m_is_in_move_or_resize = true;
        break;
    case WM_EXITSIZEMOVE:
        m_is_in_move_or_resize = false;
        if (should_block_window_resize_for_assembly(m_plater))
            return 0;
        Refresh();
        Layout();
        wxQueueEvent(wxGetApp().plater(), new SimpleEvent(EVT_NOTICE_CHILDE_SIZE_CHANGED));
        break;
    }
    return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}

#endif

void  MainFrame::show_log_window()
{
    m_log_window = new wxLogWindow(this, _L("Logging"), true, false);
    m_log_window->Show();
}

//BBS GUI refactor: remove unused layout new/dlg
void MainFrame::update_layout()
{
    auto restore_to_creation = [this]() {
        auto clean_sizer = [](wxSizer* sizer) {
            while (!sizer->GetChildren().IsEmpty()) {
                sizer->Detach(0);
            }
        };

        // On Linux m_plater needs to be removed from m_tabpanel before to reparent it
        int plater_page_id = m_tabpanel->FindPage(m_plater);
        if (plater_page_id != wxNOT_FOUND)
            m_tabpanel->RemovePage(plater_page_id);

        if (m_plater->GetParent() != this)
            m_plater->Reparent(this);

        if (m_tabpanel->GetParent() != this)
            m_tabpanel->Reparent(this);

        plater_page_id = (m_plater_page != nullptr) ? m_tabpanel->FindPage(m_plater_page) : wxNOT_FOUND;
        if (plater_page_id != wxNOT_FOUND) {
            m_tabpanel->DeletePage(plater_page_id);
            m_plater_page = nullptr;
        }

        clean_sizer(m_main_sizer);
        clean_sizer(m_settings_dialog.GetSizer());

        if (m_settings_dialog.IsShown())
            m_settings_dialog.Close();

        m_tabpanel->Hide();
        m_plater->Hide();
        if (m_prepare_action_bar)
            m_prepare_action_bar->Hide();

        Layout();
    };

    //BBS GUI refactor: remove unused layout new/dlg
    //ESettingsLayout layout = wxGetApp().is_gcode_viewer() ? ESettingsLayout::GCodeViewer : ESettingsLayout::Old;
    ESettingsLayout layout =  ESettingsLayout::Old;

    if (m_layout == layout)
        return;

    wxBusyCursor busy;

    Freeze();

    // Remove old settings
    if (m_layout != ESettingsLayout::Unknown)
        restore_to_creation();

    ESettingsLayout old_layout = m_layout;
    m_layout = layout;

    // From the very beginning the Print settings should be selected
    //m_last_selected_tab = m_layout == ESettingsLayout::Dlg ? 0 : 1;
    m_last_selected_tab = 1;

    // Set new settings
    switch (m_layout)
    {
    case ESettingsLayout::Old:
    {
        m_plater->Reparent(m_tabpanel);
        m_tabpanel->InsertPage(tp3DEditor, m_plater, _L("Prepare"), std::string("tab_3d_active"), std::string("tab_3d_active"), false);
        m_tabpanel->InsertPage(tpPreview, m_plater, _L("Preview"), std::string("tab_preview_active"), std::string("tab_preview_active"), false);
        m_main_sizer->Add(m_tabpanel, 1, wxEXPAND | wxTOP, 0);
        m_main_sizer->Add(m_prepare_action_bar, 0, wxEXPAND);
        show_option(m_tabpanel->GetSelection() == tp3DEditor);

        m_tabpanel->Bind(wxCUSTOMEVT_NOTEBOOK_SEL_CHANGED, [this](wxCommandEvent& evt)
        {
            // jump to 3deditor under preview_only mode
            if (evt.GetId() == tp3DEditor){
                wxCommandEvent event(EVT_SWITCH_TO_PREPARE_TAB);
                wxPostEvent(m_plater, event);
                if(!preview_only_hint())
                    return;
            }
            else if (evt.GetId() == tpCalibration) {
                m_calibration->update_all();
            }
            evt.Skip();
        });

        m_plater->Show();
        m_tabpanel->Show();

        break;
    }
    case ESettingsLayout::GCodeViewer:
    {
        m_main_sizer->Add(m_plater, 1, wxEXPAND);
        //BBS: add bed exclude area
        m_plater->set_bed_shape({{0.0, 0.0}, {200.0, 0.0}, {200.0, 200.0}, {0.0, 200.0}}, {}, {}, 0.0, {}, {}, {}, {}, true);
        m_plater->get_collapse_toolbar().set_enabled(false);
        m_plater->enable_sidebar(false);
        m_plater->Show();
        break;
    }
    default:
        break;
    }

    //BBS GUI refactor: remove unused layout new/dlg
//#ifdef __APPLE__
//    // Using SetMinSize() on Mac messes up the window position in some cases
//    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
//    // So, if we haven't possibility to set MinSize() for the MainFrame,
//    // set the MinSize() as a half of regular  for the m_plater and m_tabpanel, when settings layout is in slNew mode
//    // Otherwise, MainFrame will be maximized by height
//    if (m_layout == ESettingsLayout::New) {
//        wxSize size = wxGetApp().get_min_size();
//        size.SetHeight(int(0.5 * size.GetHeight()));
//        m_plater->SetMinSize(size);
//        m_tabpanel->SetMinSize(size);
//    }
//#endif

#ifdef __APPLE__
    m_plater->sidebar().change_top_border_for_mode_sizer(m_layout != ESettingsLayout::Old);
#endif

    Layout();
    Thaw();
}

// Called when closing the application and when switching the application language.
void MainFrame::shutdown()
{
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "MainFrame::shutdown enter";
    // BBS: session file-tabs — persist tab entries and groups before teardown.
    if (m_project_tabbar)
        m_project_tabbar->SaveToConfig();
    // BBS: backup
    Slic3r::set_backup_callback(nullptr);
#ifdef _WIN32
	if (m_hDeviceNotify) {
		::UnregisterDeviceNotification(HDEVNOTIFY(m_hDeviceNotify));
		m_hDeviceNotify = nullptr;
	}
 	if (m_ulSHChangeNotifyRegister) {
        SHChangeNotifyDeregister(m_ulSHChangeNotifyRegister);
        m_ulSHChangeNotifyRegister = 0;
 	}
#endif // _WIN32

    if (m_plater != nullptr) {
        m_plater->get_ui_job_worker().cancel_all();
        m_plater->stop_jobs();

        // Unbinding of wxWidgets event handling in canvases needs to be done here because on MAC,
        // when closing the application using Command+Q, a mouse event is triggered after this lambda is completed,
        // causing a crash
        m_plater->unbind_canvas_event_handlers();

        // Cleanup of canvases' volumes needs to be done here or a crash may happen on some Linux Debian flavours
        m_plater->reset_canvas_volumes();
    }

    // Weird things happen as the Paint messages are floating around the windows being destructed.
    // Avoid the Paint messages by hiding the main window.
    // Also the application closes much faster without these unnecessary screen refreshes.
    // In addition, there were some crashes due to the Paint events sent to already destructed windows.
    this->Show(false);

    if (m_settings_dialog.IsShown())
        // call Close() to trigger call to lambda defined into GUI_App::persist_window_geometry()
        m_settings_dialog.Close();

    if (m_plater != nullptr) {
        // Stop the background thread (Windows and Linux).
        // Disconnect from a 3DConnextion driver (OSX).
        m_plater->get_mouse3d_controller().shutdown();
        // Store the device parameter database back to appconfig.
        m_plater->get_mouse3d_controller().save_config(*wxGetApp().app_config);
    }

    // stop agent
    NetworkAgent* agent = wxGetApp().getAgent();
    if (agent)
        agent->track_enable(false);

    // Stop the background thread of the removable drive manager, so that no new updates will be sent to the Plater.
    //wxGetApp().removable_drive_manager()->shutdown();
	//stop listening for messages from other instances
	wxGetApp().other_instance_message_handler()->shutdown(this);
    // Save the slic3r.ini.Usually the ini file is saved from "on idle" callback,
    // but in rare cases it may not have been called yet.
    wxGetApp().app_config->save();
//         if (m_plater)
//             m_plater->print = undef;
//         Slic3r::GUI::deregister_on_request_update_callback();

    // set to null tabs and a plater
    // to avoid any manipulations with them from App->wxEVT_IDLE after of the mainframe closing
    wxGetApp().tabs_list.clear();
    wxGetApp().model_tabs_list.clear();
    wxGetApp().shutdown();
    // BBS: why clear ?
    //wxGetApp().plater_ = nullptr;

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "MainFrame::shutdown exit";
}

void MainFrame::update_filament_tab_ui()
{
    wxGetApp().get_tab(Preset::Type::TYPE_FILAMENT)->reload_config();
    wxGetApp().get_tab(Preset::Type::TYPE_FILAMENT)->update_dirty();
    wxGetApp().get_tab(Preset::Type::TYPE_FILAMENT)->update_tab_ui();
}

void MainFrame::update_title()
{
    if (!m_plater)
        return;
    // Prepend "* " while the project has unsaved changes (#9987), on top of the project name
    // that is already shown: in the custom topbar on Windows, and in the native window title
    // on macOS/Linux (both set by Plater::priv::set_project_name).
    const wxString name  = m_plater->get_project_name();
    const bool     dirty = m_plater->is_project_dirty();
    // BBS: keep the active project tab's label + dirty dot in lock-step with the live
    // Plater document. Runs before the title-cache short-circuit so the dot always
    // tracks; gated until the startup reconcile so it never clobbers restored labels.
    // Suppressed during a tab switch: load_snapshot_from fires update_title() internally
    // while the OUTGOING tab is still active, which would otherwise rewrite that tab's
    // label to the incoming project and clear its dirty dot. switch_project_tab does an
    // explicit sync after SetActive() instead.
    if (m_project_tabs_ready && !m_project_tab_switching && m_project_tabbar &&
        m_project_tabbar->GetActive() >= 0) {
        m_project_tabbar->SetActiveTitle(name.IsEmpty() ? _L("Untitled") : name);
        m_project_tabbar->SetActiveDirty(dirty);
    }
    const wxString title = (dirty && !name.IsEmpty()) ? ("* " + name) : name;
    if (title == m_title_cache)
        return;
    m_title_cache = title;
#ifdef __WINDOWS__
    if (m_topbar)
        m_topbar->SetTitle(title);
    // Also reflect the "*" in the window/taskbar title, which set_project_name builds
    // as "<name> - BambuStudio".
    SetTitle(title + " - BambuStudio");
#else
    SetTitle(title);
#ifdef __APPLE__
    if (!title.IsEmpty())
        update_title_colour_after_set_title();
#endif
#endif
}

void MainFrame::show_calibration_button(bool show, bool is_BBL)
{
#ifdef __APPLE__
    bool shown = m_menubar->FindMenu(_L("Calibration")) != wxNOT_FOUND;
    if (shown == show)
        ;
    else if (show)
        m_menubar->Insert(3, m_calib_menu, wxString::Format("&%s", _L("Calibration")));
    else
        m_menubar->Remove(3);
#else
    topbar()->ShowCalibrationButton(show);
#endif
    show = is_BBL;
    // STUDIO-18116: locate the Calibration tab by its page pointer instead of relying on
    // the hardcoded `tpCalibration` enum index. The enum value (6) is calibrated for the
    // multi_machine-enabled layout (8 pages); when multi_machine is disabled the actual
    // notebook only has 7 pages and Calibration sits at index 5 while the Filament Manager
    // tab sits at index 6. Calling RemovePage(tpCalibration=6) in that situation removes
    // the Filament Manager tab instead, which is exactly the "no Filament Manager tab on
    // first launch" symptom users hit when no BBL preset is selected yet.
    int cal_page_id = m_tabpanel->FindPage(m_calibration);
    auto shown2 = cal_page_id != wxNOT_FOUND;
    if (shown2 == show)
        ;
    else if (show) {
        // Insert right before the Filament Manager tab (or append if it isn't there) so
        // the visual order stays Project / Calibration / Filament Manager regardless of
        // how many leading plater pages update_layout() has inserted.
        int fm_page_id = m_web_device ? m_tabpanel->FindPage(m_web_device) : wxNOT_FOUND;
        size_t insert_at = (fm_page_id != wxNOT_FOUND)
                               ? static_cast<size_t>(fm_page_id)
                               : m_tabpanel->GetPageCount();
        m_tabpanel->InsertPage(insert_at, m_calibration, _L("Calibration"),
                               std::string("tab_monitor_active"),
                               std::string("tab_monitor_active"), false);
    } else {
        m_tabpanel->RemovePage(static_cast<size_t>(cal_page_id));
    }
}

void MainFrame::update_title_colour_after_set_title()
{
#ifdef __APPLE__
    set_title_colour_after_set_title(GetHandle());
#endif
}

// ----------------------------------------------------------------------------
// BBS: session file-tabs orchestration (see ProjectTabBar.hpp / SHARED CONTRACT).
// The app keeps ONE live Plater document. Switching tabs snapshots the outgoing
// tab (only when dirty) to a per-tab temp .3mf and loads the target, reusing the
// shipped Backup/Restore round-trip (Plater::save_snapshot_to / load_snapshot_from).
// Switch latency is inherent (full deserialize + GL rebuild); undo history and the
// camera reset on switch — that is acceptable for this design.
// ----------------------------------------------------------------------------
namespace {
// RAII guard that serializes tab operations so a click landing mid-switch is ignored
// (mirrors Plater's m_loading_project re-entrancy guard).
struct TabOpGuard {
    bool& m_flag;
    explicit TabOpGuard(bool& f) : m_flag(f) { m_flag = true; }
    ~TabOpGuard() { m_flag = false; }
    TabOpGuard(const TabOpGuard&) = delete;
    TabOpGuard& operator=(const TabOpGuard&) = delete;
};

// A fresh, unique temp path under data_dir()/cache/project_tabs for one tab's snapshot.
std::string make_project_tab_snapshot_path()
{
    namespace fs = boost::filesystem;
    static std::atomic<uint64_t> s_counter{0};
    fs::path dir = fs::path(data_dir()) / "cache" / "project_tabs";
    boost::system::error_code ec;
    fs::create_directories(dir, ec);
    const std::string stamp = wxDateTime::UNow().GetValue().ToString().ToStdString();
    const uint64_t    seq   = s_counter.fetch_add(1);
    fs::path file = dir / (std::string("tab_") + stamp + "_" + std::to_string(seq) + ".3mf");
    return file.string();
}
} // namespace

bool MainFrame::save_active_tab_snapshot_if_dirty()
{
    if (!m_plater || !m_project_tabbar)
        return true;
    const int active = m_project_tabbar->GetActive();
    if (active < 0 || active >= m_project_tabbar->Count())
        return true;
    // Serialize ONLY the outgoing tab, and only when it actually has unsaved changes:
    // a clean tab is reproduced by reloading its file_path, so no snapshot is needed.
    if (!m_plater->is_project_dirty())
        return true;
    ProjectTab& t = m_project_tabbar->TabAt(active);
    if (t.snapshot_path.empty())
        t.snapshot_path = make_project_tab_snapshot_path();
    const bool ok = m_plater->save_snapshot_to(t.snapshot_path);
    if (ok)
        t.dirty = true;
    else
        BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << ": failed to snapshot outgoing project tab " << active;
    return ok;
}

void MainFrame::switch_project_tab(int target)
{
    if (!m_plater || !m_project_tabbar)
        return;
    if (m_project_tab_switching) // ignore a click that lands mid-switch
        return;
    if (target < 0 || target >= m_project_tabbar->Count())
        return;
    if (target == m_project_tabbar->GetActive())
        return;

    TabOpGuard guard(m_project_tab_switching);

    // p->reset() inside the load path already settles/stops any background slicing.
    save_active_tab_snapshot_if_dirty();

    ProjectTab&       tgt                  = m_project_tabbar->TabAt(target);
    const bool        loaded_from_snapshot = !tgt.snapshot_path.empty();
    const std::string load_path            = loaded_from_snapshot ? tgt.snapshot_path : tgt.file_path;
    if (!load_path.empty())
        m_plater->load_snapshot_from(load_path);
    else
        m_plater->new_project(/*skip_confirm=*/true, /*silent=*/true); // never-saved tab

    // Restore the tab's real on-disk identity. Loading from a temp snapshot leaves the
    // project filename pointing at the snapshot, which would misdirect Ctrl+S and the
    // window title and pollute Recent Projects; re-point it at the tab's real file.
    // (A never-saved dirty tab has no real file — its title stays the tab label; Save
    // then correctly behaves as Save As. Tracked as a followup.)
    if (loaded_from_snapshot && !tgt.file_path.empty())
        m_plater->set_project_filename(wxString::FromUTF8(tgt.file_path));

    m_project_tabbar->SetActive(target);
    // Explicit tab-bar sync: update_title()'s own sync is suppressed while
    // m_project_tab_switching is set (see update_title), so set the incoming tab's
    // label + dirty dot here from the tab model.
    m_project_tabbar->SetActiveTitle(tgt.title.IsEmpty() ? _L("Untitled") : tgt.title);
    m_project_tabbar->SetActiveDirty(tgt.dirty);
    update_title();
    m_project_tabbar->SaveToConfig();
}

void MainFrame::close_project_tab(int index)
{
    if (!m_plater || !m_project_tabbar)
        return;
    if (m_project_tab_switching)
        return;
    const int count = m_project_tabbar->Count();
    if (index < 0 || index >= count)
        return;

    const int active = m_project_tabbar->GetActive();

    TabOpGuard guard(m_project_tab_switching);

    if (index == active) {
        // Active tab: reuse the plater's unsaved-changes confirmation (same second-check
        // as the app-close path). Cancel aborts the close.
        auto check = [](bool yes_or_no) {
            if (yes_or_no)
                return true;
            return wxGetApp().check_and_save_current_preset_changes(
                _L("Close project tab"), _L("Closing a project tab while some presets are modified."));
        };
        if (m_plater->close_with_confirm(check) == wxID_CANCEL)
            return;
    } else {
        // Background tab with unsaved changes (its edits live only in the temp snapshot):
        // confirm before discarding them, since the live plater can't run its own
        // save-confirm on a project it doesn't currently hold.
        ProjectTab& t = m_project_tabbar->TabAt(index);
        if (t.dirty || !t.snapshot_path.empty()) {
            MessageDialog dlg(this, _L("This project tab has unsaved changes that will be lost. Close it anyway?"),
                              _L("Close project tab"), wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
            if (dlg.ShowModal() != wxID_YES)
                return;
        }
    }

    // Best-effort cleanup of this tab's temp snapshot.
    {
        ProjectTab& t = m_project_tabbar->TabAt(index);
        if (!t.snapshot_path.empty()) {
            boost::system::error_code ec;
            boost::filesystem::remove(boost::filesystem::path(t.snapshot_path), ec);
        }
    }

    m_project_tabbar->CloseTab(index);
    const int remaining = m_project_tabbar->Count();

    if (remaining == 0) {
        // Never leave the app tab-less: start a fresh Untitled tab.
        m_plater->new_project(/*skip_confirm=*/true, /*silent=*/true);
        wxString t = m_plater->get_project_name();
        if (t.IsEmpty())
            t = _L("Untitled");
        m_project_tabbar->AddTab(std::string(), t, /*activate=*/true);
        update_title();
    } else if (index == active) {
        // Closed the visible tab: activate a neighbour and load its document.
        int neighbour = index;
        if (neighbour >= remaining)
            neighbour = remaining - 1;
        ProjectTab&       n         = m_project_tabbar->TabAt(neighbour);
        const std::string load_path = n.snapshot_path.empty() ? n.file_path : n.snapshot_path;
        if (!load_path.empty())
            m_plater->load_snapshot_from(load_path);
        else
            m_plater->new_project(/*skip_confirm=*/true, /*silent=*/true);
        m_project_tabbar->SetActive(neighbour);
        update_title();
    } else {
        // Closed a background tab: keep the live document; fix the active index.
        const int new_active = (active > index) ? active - 1 : active;
        m_project_tabbar->SetActive(new_active);
    }
    m_project_tabbar->SaveToConfig();
}

void MainFrame::new_project_tab()
{
    if (!m_plater || !m_project_tabbar)
        return;
    if (m_project_tab_switching)
        return;
    if (!can_start_new_project())
        return;

    TabOpGuard guard(m_project_tab_switching);

    // Preserve the current tab, then open a fresh Untitled tab alongside it. The
    // outgoing document is already handled, so skip new_project's own confirm.
    save_active_tab_snapshot_if_dirty();
    if (m_plater->new_project(/*skip_confirm=*/true) == wxID_CANCEL)
        return;

    wxString t = m_plater->get_project_name();
    if (t.IsEmpty())
        t = _L("Untitled");
    m_project_tabbar->AddTab(std::string(), t, /*activate=*/true);
    update_title();
    m_project_tabbar->SaveToConfig();
}

void MainFrame::open_project_tab()
{
    if (!m_plater || !m_project_tabbar)
        return;
    if (m_project_tab_switching)
        return;
    if (!can_open_project())
        return;

    // Ask for the file first (app-modal dialog blocks the tab bar), then load it into
    // a new tab. Delegating to open_project_in_tab keeps the re-entrancy guard scoped
    // to the load rather than the dialog.
    wxString input_file;
    wxGetApp().load_project(this, input_file);
    if (input_file.IsEmpty())
        return;
    open_project_in_tab(input_file);
}

void MainFrame::open_project_in_tab(const wxString& filename)
{
    if (!m_plater || !m_project_tabbar)
        return;
    if (m_project_tab_switching)
        return;
    if (filename.IsEmpty())
        return;

    TabOpGuard guard(m_project_tab_switching);

    // Preserve the current tab, then load the file into the single live plater. We
    // already handled the outgoing document, so skip load_project's close confirmation.
    save_active_tab_snapshot_if_dirty();
    m_plater->load_project(filename, "-", nullptr, /*skip_close_confirmation=*/true);

    wxString t = m_plater->get_project_name();
    if (t.IsEmpty())
        t = _L("Untitled");
    m_project_tabbar->AddTab(into_u8(filename), t, /*activate=*/true);
    update_title();
    m_project_tabbar->SaveToConfig();
}

void MainFrame::reconcile_initial_project_tab()
{
    if (!m_plater || !m_project_tabbar)
        return;
    // From here on the active-tab label/dirty sync in update_title() is live.
    m_project_tabs_ready = true;

    int active = m_project_tabbar->GetActive();
    if (active < 0) {
        if (m_project_tabbar->Count() == 0) {
            update_title();
            return;
        }
        active = 0;
        m_project_tabbar->SetActive(0);
    }

    // If the app already established a real document at startup (command-line open,
    // recovery, etc.), keep it — update_title() below relabels the active tab to match.
    const bool live_has_file = !m_plater->get_project_filename().IsEmpty();
    if (!live_has_file) {
        // Fresh Untitled document: lazily restore the tab the user left on last session.
        ProjectTab&       t         = m_project_tabbar->TabAt(active);
        const std::string load_path = t.snapshot_path.empty() ? t.file_path : t.snapshot_path;
        if (!load_path.empty()) {
            boost::system::error_code ec;
            if (boost::filesystem::exists(boost::filesystem::path(load_path), ec)) {
                TabOpGuard guard(m_project_tab_switching);
                m_plater->load_snapshot_from(load_path);
            }
        }
    }
    update_title();
}

void MainFrame::show_option(bool show)
{
    if (!m_prepare_action_bar)
        return;

    const bool should_show = show && m_layout == ESettingsLayout::Old;
    if (should_show)
        update_prepare_action_bar_content();
    if (m_prepare_action_bar->IsShown() == should_show)
        return;

    m_prepare_action_bar->Show(should_show);
    if (m_main_sizer)
        m_main_sizer->Layout();
    Layout();
}

namespace {

// Stable child name so the two update paths can recover the second (detail)
// line of the two-tier print estimate without a new MainFrame member.
constexpr const char *PREPARE_ESTIMATE_DETAIL_NAME = "md3_prepare_estimate_detail";

// Roboto Mono 15/500 for the estimate's time line (MD3 Prepare kit line 1).
// Derived from the 14px mono preset so the platform's design-px -> point
// scaling is inherited rather than re-derived.
wxFont prepare_estimate_time_font()
{
    wxFont font = ::Label::Mono_14;
    if (font.IsOk())
        font.SetFractionalPointSize(font.GetFractionalPointSize() * 15.0 / 14.0);
    font.SetNumericWeight(500);
    return font;
}

// Two-tier estimate styling (MD3 Prepare kit §67-70): line 1 = print time in
// Roboto Mono 15/500 OnSurface; line 2 = weight / length in 11px
// OnSurfaceVariant. Applied both at build time and on Rescale/theme rebuild.
void style_prepare_estimate(wxStaticText *time_line, wxStaticText *detail_line)
{
    if (time_line) {
        time_line->SetFont(prepare_estimate_time_font());
        time_line->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    }
    if (detail_line) {
        detail_line->SetFont(::Label::Body_11);
        detail_line->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    }
}

} // namespace

void MainFrame::update_prepare_action_bar_content()
{
    if (!m_prepare_action_bar || !m_plater)
        return;

    int left_sidebar_width = 0;
    int right_sidebar_width = 0;
    Sidebar &sidebar = m_plater->sidebar();
    if (!m_plater->is_sidebar_collapsed() && sidebar.IsShown()) {
        const int sidebar_width = std::max(0, sidebar.GetSize().GetWidth());
        if (m_plater->get_sidebar_docking_state() == Sidebar::Left)
            left_sidebar_width = sidebar_width;
        else if (m_plater->get_sidebar_docking_state() == Sidebar::Right)
            right_sidebar_width = sidebar_width;
    }
    if (m_prepare_left_sidebar_spacer)
        m_prepare_left_sidebar_spacer->SetMinSize(wxSize(left_sidebar_width, 0));
    if (m_prepare_right_sidebar_spacer)
        m_prepare_right_sidebar_spacer->SetMinSize(wxSize(right_sidebar_width, 0));

    PartPlateList &plates = m_plater->get_partplate_list();
    PartPlate *plate = plates.get_plate_count() > 0 ? plates.get_curr_plate() : nullptr;
    if (m_prepare_plate_button) {
        if (plate) {
            const wxString plate_label = plate->get_plate_name().empty()
                ? wxString::Format(_L("Plate %d"), plates.get_curr_plate_index() + 1)
                : from_u8(plate->get_plate_name());
            m_prepare_plate_button->SetLabel(plate_label);
            m_prepare_plate_button->SetToolTip(plate_label);
            m_prepare_plate_button->Enable(true);
        } else {
            m_prepare_plate_button->SetLabel(wxEmptyString);
            m_prepare_plate_button->SetToolTip(wxEmptyString);
            m_prepare_plate_button->Enable(false);
        }
    }
    if (m_prepare_add_plate_button)
        m_prepare_add_plate_button->Enable(m_plater->can_add_plate());

    if (m_prepare_estimate_label) {
        // Two-tier estimate (MD3 Prepare kit §67-70): line 1 is the print time,
        // line 2 is the material weight / length ("23.4 g · 7.85 m").
        wxString time_text = _L("Not sliced");
        wxString detail_text;
        if (plate && plate->is_slice_result_valid()) {
            if (GCodeProcessorResult *result = plate->get_slice_result(); result != nullptr && !result->print_statistics.modes.empty()) {
                const float seconds = result->print_statistics.modes.front().time;
                double grams  = 0.0;
                double meters = 0.0;
                if (auto *print = plate->fff_print(); print != nullptr) {
                    grams  = print->print_statistics().total_weight;
                    meters = print->print_statistics().total_used_filament / 1000.0; // mm -> m
                }
                if (seconds > 0.0f) {
                    time_text = from_u8(short_time(get_time_dhms(seconds)));
                    if (grams > 0.0)
                        detail_text = wxString::Format("%.1f g", grams);
                    if (meters > 0.0) {
                        if (!detail_text.empty())
                            detail_text += wxString::FromUTF8(" \xC2\xB7 "); // middle dot
                        detail_text += wxString::Format("%.2f m", meters);
                    }
                }
            }
        }
        m_prepare_estimate_label->SetLabel(time_text);
        if (wxWindow *detail = m_prepare_action_bar->FindWindow(PREPARE_ESTIMATE_DETAIL_NAME))
            static_cast<wxStaticText *>(detail)->SetLabel(detail_text);
    }

    if (wxSizer *sizer = m_prepare_action_bar->GetSizer())
        sizer->Layout();
    m_prepare_action_bar->Layout();
    m_prepare_action_bar->Refresh(false);
}

void MainFrame::update_prepare_action_bar_style()
{
    if (!m_prepare_action_bar || !m_prepare_action_bar_divider)
        return;

    const int bar_height = FromDIP(MD3::Metrics::prepare_actions_height);
    const int divider_height = std::max(1, FromDIP(1));

    m_prepare_action_bar->SetMinSize(wxSize(-1, bar_height));
    m_prepare_action_bar->SetMaxSize(wxSize(-1, bar_height));
    const wxColour bar_bg = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    m_prepare_action_bar->SetBackgroundColour(bar_bg);

    // Re-seed every direct child's window background with the bar surface. The
    // children were created BEFORE the bar's themed background landed, so their
    // Create-time snapshots kept the stock light panel grey — in dark mode that
    // showed as white squares behind the rounded controls (expand button, the
    // dashed add-plate pill corners) and as light slabs behind the transparent
    // slice/print sub-panels. The divider and the vertical split rule are the
    // two children whose backgrounds ARE their visible content, so they keep
    // their OutlineVariant fills (re-applied just below).
    for (wxWindow *child : m_prepare_action_bar->GetChildren()) {
        if (child == m_prepare_action_bar_divider || child == m_prepare_split_line)
            continue;
        child->SetBackgroundColour(bar_bg);
    }

    m_prepare_action_bar_divider->SetMinSize(wxSize(-1, divider_height));
    m_prepare_action_bar_divider->SetMaxSize(wxSize(-1, divider_height));
    m_prepare_action_bar_divider->SetBackgroundColour(StateColor::semantic(MD3::Role::OutlineVariant));

    if (m_prepare_split_line) {
        // Re-apply the OutlineVariant tone and DPI-scaled geometry so the divider
        // stays theme-correct and crisp across monitor/DPI changes.
        m_prepare_split_line->SetBackgroundColour(StateColor::semantic(MD3::Role::OutlineVariant));
        m_prepare_split_line->SetMinSize(wxSize(FromDIP(1), FromDIP(22)));
    }
    if (m_prepare_expand_btn) {
        // Re-derive the ghost IconButton's neutral rest fill/geometry now that the
        // bar's SurfaceContainerLow background is applied (Rescale re-runs the MD3
        // styling), keeping it theme- and DPI-correct.
        m_prepare_expand_btn->Rescale();
    }

    if (m_prepare_plate_button) {
        const wxColour primary = StateColor::semantic(MD3::Role::Primary);
        // Active-plate chip (MD3 digest 3.2): SecondaryContainer fill,
        // OnSecondaryContainer text, 2px Primary border, 12px radius.
        const StateColor background(
            std::pair{StateColor::semantic(MD3::Role::PrimaryContainer), (int) StateColor::Hovered},
            std::pair{StateColor::semantic(MD3::Role::SecondaryContainer), (int) StateColor::Normal});
        m_prepare_plate_button->SetBackgroundColor(background);
        m_prepare_plate_button->SetBorderColor(StateColor(primary));
        m_prepare_plate_button->SetBorderWidth(FromDIP(2));
        m_prepare_plate_button->SetTextColor(StateColor(StateColor::semantic(MD3::Role::OnSecondaryContainer)));
        m_prepare_plate_button->SetCornerRadius(FromDIP(12));
        m_prepare_plate_button->SetMinSize(FromDIP(wxSize(96, 40)));
        m_prepare_plate_button->Rescale();
    }
    if (m_prepare_add_plate_button) {
        // Add-plate (MD3 digest 3.2): dashed 1px Outline border, transparent
        // (bar-surface) fill, 12px radius, OnSurfaceVariant glyph.
        const StateColor background(
            std::pair{StateColor::semantic(MD3::Role::SurfaceContainerHigh), (int) StateColor::Hovered},
            std::pair{StateColor::semantic(MD3::Role::SurfaceContainerLow), (int) StateColor::Normal});
        m_prepare_add_plate_button->SetBackgroundColor(background);
        m_prepare_add_plate_button->SetBorderColor(StateColor(StateColor::semantic(MD3::Role::Outline)));
        m_prepare_add_plate_button->SetBorderWidth(FromDIP(1));
        m_prepare_add_plate_button->SetBorderStyle(wxPENSTYLE_SHORT_DASH);
        m_prepare_add_plate_button->SetTextColor(StateColor(StateColor::semantic(MD3::Role::OnSurfaceVariant)));
        m_prepare_add_plate_button->SetCornerRadius(FromDIP(12));
        m_prepare_add_plate_button->SetMinSize(FromDIP(wxSize(40, 40)));
        m_prepare_add_plate_button->Rescale();
    }
    if (m_prepare_estimate_label) {
        // Two-tier print estimate (MD3 Prepare kit §67-70): time line in Roboto
        // Mono 15/500 OnSurface, weight/length line in 11px OnSurfaceVariant.
        wxStaticText *detail = nullptr;
        if (wxWindow *w = m_prepare_action_bar->FindWindow(PREPARE_ESTIMATE_DETAIL_NAME))
            detail = static_cast<wxStaticText *>(w);
        style_prepare_estimate(m_prepare_estimate_label, detail);
        m_prepare_estimate_label->SetMinSize(FromDIP(wxSize(118, -1)));
    }

    // wxSizer borders are pixel values, so refresh the horizontal inset when
    // the window crosses monitors with different DPI scales.
    if (wxSizer* sizer = m_prepare_action_bar->GetSizer()) {
        if (wxSizerItem* content = sizer->GetItem(static_cast<size_t>(1)); content && content->GetSizer()) {
            if (wxSizerItem *actions = content->GetSizer()->GetItem(static_cast<size_t>(1)))
                actions->SetBorder(FromDIP(MD3::Metrics::active().padding));
        }
        sizer->Layout();
    }

    update_prepare_action_bar_content();
    m_prepare_action_bar->Refresh(false);
    m_prepare_action_bar_divider->Refresh(false);
}

void MainFrame::init_tabpanel()
{
    // wxNB_NOPAGETHEME: Disable Windows Vista theme for the Notebook background. The theme performance is terrible on Windows 10
    // with multiple high resolution displays connected.
   // BBS
    m_prepare_action_bar = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    m_prepare_action_bar_divider = new wxPanel(m_prepare_action_bar, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    m_side_tools = create_side_tools(m_prepare_action_bar);

    auto* action_bar_sizer = new wxBoxSizer(wxVERTICAL);
    auto* action_row_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_prepare_left_sidebar_spacer = action_row_sizer->Add(0, 0, 0);
    action_row_sizer->Add(m_side_tools, 1, wxEXPAND | wxLEFT | wxRIGHT,
                          FromDIP(MD3::Metrics::active().padding));
    m_prepare_right_sidebar_spacer = action_row_sizer->Add(0, 0, 0);
    action_bar_sizer->Add(m_prepare_action_bar_divider, 0, wxEXPAND);
    action_bar_sizer->Add(action_row_sizer, 1, wxEXPAND);
    m_prepare_action_bar->SetSizer(action_bar_sizer);
    update_prepare_action_bar_style();
    m_prepare_action_bar->Hide();

    m_tabpanel = new Notebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, nullptr, wxNB_TOP | wxTAB_TRAVERSAL | wxNB_NOPAGETHEME);
    // Match the tab-bar (ButtonsListCtrl) Surface role; the strip repaints over it.
    m_tabpanel->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));

#ifndef __WXOSX__ // Don't call SetFont under OSX to avoid name cutting in ObjectList
    m_tabpanel->SetFont(Slic3r::GUI::wxGetApp().normal_font());
#endif
    m_tabpanel->Hide();
    m_settings_dialog.set_tabpanel(m_tabpanel);

    m_tabpanel->Bind(wxEVT_NOTEBOOK_PAGE_CHANGING, [this](wxBookCtrlEvent& e) {
        int old_sel = e.GetOldSelection();
        int new_sel = e.GetSelection();

        //if (bool test = false) {
        //    SupportRecommendDialog* dialog = new SupportRecommendDialog(nullptr, _L("Notice"));;

        //    wxArrayString params;
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));
        //    params.Add(wxString::FromUTF8("支撑独立层高：关闭"));

        //    std::vector<wxString> objects;
        //    for (int i = 0; i < 10; i++) {
        //        objects.push_back("ABCABCABCABC");
        //    }

        //    std::vector<std::tuple<int, wxColour, wxString>> mainMat;
        //    mainMat.push_back(std::make_tuple(2, *wxRED, "Bambu PLA Basic"));
        //    mainMat.push_back(std::make_tuple(5, *wxBLUE, "Bambu PLA Basic"));
        //    mainMat.push_back(std::make_tuple(3, *wxGREEN, "Bambu PLA Basic"));
        //    mainMat.push_back(std::make_tuple(1, *wxWHITE, "Bambu PLA Basic"));

        //    dialog->AddSupportComboCard(objects, mainMat, std::make_tuple(1, *wxWHITE, "Bambu PLA Basic"), params);
        //    dialog->AddSupportComboCard(objects, mainMat, std::make_tuple(1, *wxWHITE, "Bambu PLA Basic"), params);
        //    dialog->ShowModal();
        //}

        if (old_sel != wxNOT_FOUND &&
            new_sel != old_sel &&
            m_project != nullptr &&
            m_tabpanel->GetPage((size_t)old_sel) == m_project &&
            m_project->is_editing_page()) {
            MessageDialog dlg(this,
                              _L("The current page has unsaved changes. You can continue editing or choose to save/discard before leaving."),
                              _L("Save"),
                              wxYES_NO | wxCANCEL |wxCENTRE);
            int ret = dlg.ShowModal();
            if (ret == wxID_YES) {
                m_project->save_project();
            } else {
                e.Veto();
                return;
            }
        }
        if (wxGetApp().preset_bundle &&
            wxGetApp().preset_bundle->printers.get_edited_preset().is_bbl_vendor_preset(wxGetApp().preset_bundle) &&
            new_sel == tpMonitor) {
            if (!wxGetApp().getAgent()) {
                e.Veto();
                BOOST_LOG_TRIVIAL(info) << boost::format("skipped tab switch from %1% to %2%, lack of network plugins") %
                    old_sel % new_sel;
                if (m_plater) {
                    wxCommandEvent* evt = new wxCommandEvent(EVT_INSTALL_PLUGIN_HINT);
                    wxQueueEvent(m_plater, evt);
                }
                if (m_confirm_download_plugin_dlg == nullptr){
                    m_confirm_download_plugin_dlg = new SecondaryCheckDialog(this, wxID_ANY, _L("Install network plug-in"), SecondaryCheckDialog::ButtonStyle::ONLY_CONFIRM);
                    m_confirm_download_plugin_dlg->SetSize(wxSize(FromDIP(270), FromDIP(158)));
                    m_confirm_download_plugin_dlg->update_text(_L("Please Install network plug-in before log in."));
                    m_confirm_download_plugin_dlg->update_btn_label(_L("Install Network Plug-in"), "");

                    m_confirm_download_plugin_dlg->Bind(EVT_SECONDARY_CHECK_CONFIRM, [this](wxCommandEvent& e) {
                        this->m_confirm_download_plugin_dlg->Close();
                        wxGetApp().ShowDownNetPluginDlg();
                        return;
                        });
                }
                int xPos = GetRect().GetX() + (GetSize().x - m_confirm_download_plugin_dlg->GetSize().x) / 2;
                int yPos = GetRect().GetY() + (GetSize().y - m_confirm_download_plugin_dlg->GetSize().y) / 2;
                m_confirm_download_plugin_dlg->SetPosition(wxPoint(xPos, yPos));
                m_confirm_download_plugin_dlg->on_show();
            }
        }
        else {
            if (new_sel == tpMonitor && wxGetApp().preset_bundle != nullptr) {
                auto cfg = wxGetApp().preset_bundle->printers.get_edited_preset().config;
                wxString url;
                if (cfg.has("print_host_webui") && !cfg.opt_string("print_host_webui").empty()) {
                    url = cfg.opt_string("print_host_webui");
                } else if (cfg.has("print_host") && !cfg.opt_string("print_host").empty()) {
                    url = cfg.opt_string("print_host");
                }
                else {
                    ;
                }
                if (url.empty()) {
                    wxString url = wxString::Format("file://%s/web/device/missing_connection.html", from_u8(resources_dir()));
                    m_printer_view->load_url(url);
                }
            }
        }
    });

#ifdef __WXMSW__
    m_tabpanel->Bind(wxEVT_BOOKCTRL_PAGE_CHANGED, [this](wxBookCtrlEvent& e) {
#else
    m_tabpanel->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& e) {
#endif
        if (e.GetEventObject() != m_tabpanel){
            return;// The event maybe from child TabPanel
        }

        //BBS
        wxWindow* panel = m_tabpanel->GetCurrentPage();
        int sel = m_tabpanel->GetSelection();
        // The navigation strip is chrome rendered ABOVE the per-workspace
        // data-scheme scope, so its accent must ALWAYS stay the brand/seed value
        // (MD3 §4/§13). Only the workspace body/canvas adopts the Preview/Device
        // scheme; the tab bar never does, so it is pinned to Brand here rather
        // than switched with the selected page.
        m_tabpanel->GetBtnsListCtrl()->SetColorScheme(MD3::ColorScheme::Brand);
        //wxString page_text = m_tabpanel->GetPageText(sel);
        m_last_selected_tab = m_tabpanel->GetSelection();
        if (panel == m_plater) {
            if (sel == tp3DEditor) {
                wxPostEvent(m_plater, SimpleEvent(EVT_GLVIEWTOOLBAR_3D));
                m_param_panel->OnActivate();
            }
            else if (sel == tpPreview) {
                wxPostEvent(m_plater, SimpleEvent(EVT_GLVIEWTOOLBAR_PREVIEW));
                m_param_panel->OnActivate();
            }
        }

        //else if (panel == m_param_panel)
        //    m_param_panel->OnActivate();
        else if (panel == m_monitor) {
            //monitor
            NetworkAgent* agent = GUI::wxGetApp().getAgent();
            if (agent)
                agent->track_update_property("select_device_page", std::to_string(++select_device_page_count));
        }
        else if (panel == m_web_device) {
#if defined(__WXOSX__)
            // Defer hash navigation until after the notebook paints (macOS + WKWebView).
            CallAfter([this]() {
                if (m_web_device && m_tabpanel && m_tabpanel->GetCurrentPage() == m_web_device)
                    m_web_device->NavigateTo("/filament_manager", /*re_init=*/true);
            });
#else
            // Switching back to this tab: re-run init() to pick up changes.
            m_web_device->NavigateTo("/filament_manager", /*re_init=*/true);
#endif
        }

        // The primary Slice/Print actions belong to the Prepare workflow. Keep
        // the entire bar in sync with the selected page so programmatic tab
        // changes and layout rebuilds cannot leave individual controls behind.
        show_option(sel == tp3DEditor);
#if defined(__WXOSX__)
        // macOS root cause fix: suspend the Filament Manager WKWebView whenever it
        // is not the visible tab. Its live React SPA, if left mounted in a hidden
        // webview, keeps the CFRunLoop busy and starves wxEVT_IDLE app-wide, which
        // freezes the 3D canvas / tab switching on the prepare page and breaks the
        // language-switch GUI rebuild. Returning to the tab reloads it (NavigateTo).
        if (m_web_device && panel != m_web_device)
            m_web_device->Suspend();
#endif
#ifndef __APPLE__
        if (sel == tp3DEditor) {
            m_topbar->EnableUndoRedoItems();
        }
        else {
            m_topbar->DisableUndoRedoItems();
        }
#endif
#ifndef __WXGTK__
        // macOS: avoid moving first responder into WKWebView on Filament Manager (STUDIO-18111).
        if (panel
#if defined(__WXOSX__)
            && panel != m_web_device
#endif
        )
            panel->SetFocus();
#endif
    });

    if (wxGetApp().is_editor()) {
        m_webview         = new WebViewPanel(m_tabpanel);
        Bind(EVT_LOAD_URL, [this](wxCommandEvent &evt) {
            wxString url = evt.GetString();
            select_tab(MainFrame::tpHome);
            m_webview->load_url(url);
        });
        m_tabpanel->AddPage(m_webview, _L("Home"), "tab_home_active", "tab_home_active", false);
        m_tabpanel->SetPageToolTip(tpHome, _L("Home"));
        m_param_panel = new ParamsPanel(m_tabpanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT | wxTAB_TRAVERSAL);
    }

    m_plater = new Plater(this, this);
    m_plater->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceDim));
    m_plater->Hide();

    wxGetApp().plater_ = m_plater;

    // Keep the bottom action row aligned with the live docked sidebar and the
    // selected plate without hard-coding the Material sidebar width. These
    // events also cover sidebar collapse/restore and user resizing.
    m_plater->sidebar().Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
        CallAfter([this]() { update_prepare_action_bar_content(); });
        event.Skip();
    });
    m_plater->sidebar().Bind(wxEVT_SHOW, [this](wxShowEvent &event) {
        CallAfter([this]() { update_prepare_action_bar_content(); });
        event.Skip();
    });
    m_plater->Bind(EVT_GLCANVAS_PLATE_SELECT, [this](SimpleEvent &event) {
        CallAfter([this]() { update_prepare_action_bar_content(); });
        event.Skip();
    });

    create_preset_tabs();

        //BBS add pages
    m_monitor = new MonitorPanel(m_tabpanel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_monitor->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceDim));
    m_tabpanel->AddPage(m_monitor, _L("Device"), std::string("tab_monitor_active"), std::string("tab_monitor_active"), false);

    m_printer_view = new PrinterWebView(m_tabpanel);
    Bind(EVT_LOAD_PRINTER_URL, [this](wxCommandEvent &evt) {
        wxString url = evt.GetString();
        //select_tab(MainFrame::tpMonitor);
        m_printer_view->load_url(url);
    });
    m_printer_view->Hide();

    if (wxGetApp().is_enable_multi_machine()) {
        m_multi_machine = new MultiMachinePage(m_tabpanel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
        m_multi_machine->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceDim));
        // TODO: change the bitmap
        m_tabpanel->AddPage(m_multi_machine, _L("Multi-device"), std::string("tab_multi_active"), std::string("tab_multi_active"), false);
    }

    m_project = new ProjectPanel(m_tabpanel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_project->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceDim));
    m_tabpanel->AddPage(m_project, _L("Project"), std::string("tab_auxiliary_avtice"), std::string("tab_auxiliary_avtice"), false);

    m_calibration = new CalibrationPanel(m_tabpanel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_calibration->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceDim));
    m_tabpanel->AddPage(m_calibration, _L("Calibration"), std::string("tab_calibration_active"), std::string("tab_calibration_active"), false);

    if (!wxGetApp().is_fila_manager_disabled()) {
        m_web_device = new DeviceWebPage(m_tabpanel);
        m_tabpanel->AddPage(m_web_device, _L("Filament"), std::string("tab_filament_active"), std::string("tab_filament_active"), false);
    }

    // Settings is a navigation action rather than a synthetic notebook page.
    // This preserves every established TabPosition/page index while matching
    // the Material navigation model used by the main workspaces.
    m_tabpanel->AddNavigationAction(_L("Settings"), "settings", []() {
        wxGetApp().open_preferences();
    });

    if (m_plater) {
        // load initial config
        auto full_config = wxGetApp().preset_bundle->full_config();
        m_plater->on_config_change(full_config);

        // Show a correct number of filament fields.
        // nozzle_diameter is undefined when SLA printer is selected
        // BBS
        if (full_config.has("filament_colour")) {
            m_plater->on_filament_count_change(full_config.option<ConfigOptionStrings>("filament_colour")->values.size());
        }
    }
}


// OrcaSlicer
void MainFrame::show_device(bool bBBLPrinter) {
  if (m_tabpanel->GetPage(tpMonitor) != m_monitor &&
      m_tabpanel->GetPage(tpMonitor) != m_printer_view) {
    BOOST_LOG_TRIVIAL(error) << "Failed to find device tab";
    return;
  }
  if (bBBLPrinter) {
    if (m_tabpanel->GetPage(tpMonitor) != m_monitor) {
        m_printer_view->Show(false);
        m_monitor->Show(false);
        m_tabpanel->RemovePage(tpMonitor);
        m_tabpanel->InsertPage(tpMonitor, m_monitor, _L("Device"),
                             std::string("tab_monitor_active"),
                             std::string("tab_monitor_active"), false);
    }
  } else {
    if (m_tabpanel->GetPage(tpMonitor) != m_printer_view) {
        m_printer_view->Show(false);
        m_monitor->Show(false);
        m_tabpanel->RemovePage(tpMonitor);
        m_tabpanel->InsertPage(tpMonitor, m_printer_view, _L("Device"),
                          std::string("tab_monitor_active"),
                          std::string("tab_monitor_active"), false);
    }
  }
}


bool MainFrame::preview_only_hint()
{
    if (m_plater && (m_plater->only_gcode_mode() || (m_plater->using_exported_file()))) {
        BOOST_LOG_TRIVIAL(info) << boost::format("skipped tab switch from %1% to %2% in preview mode")%m_tabpanel->GetSelection() %tp3DEditor;

        ConfirmBeforeSendDialog confirm_dlg(this, wxID_ANY, _L("Warning"));
        confirm_dlg.Bind(EVT_SECONDARY_CHECK_CONFIRM, [this](wxCommandEvent& e) {
            preview_only_to_editor = true;
        });
        confirm_dlg.update_btn_label(_L("Yes"), _L("No"));
        auto filename = m_plater->get_preview_only_filename();

        confirm_dlg.update_text(filename + " " + _L("will be closed before creating a new model. Do you want to continue?"));
        confirm_dlg.on_show();
        if (preview_only_to_editor) {
            m_plater->new_project();
            preview_only_to_editor = false;
        }

        return false;
    }

    return true;
}

#ifdef WIN32
void MainFrame::register_win32_callbacks()
{
    //static GUID GUID_DEVINTERFACE_USB_DEVICE  = { 0xA5DCBF10, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED };
    //static GUID GUID_DEVINTERFACE_DISK        = { 0x53f56307, 0xb6bf, 0x11d0, 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b };
    //static GUID GUID_DEVINTERFACE_VOLUME      = { 0x71a27cdd, 0x812a, 0x11d0, 0xbe, 0xc7, 0x08, 0x00, 0x2b, 0xe2, 0x09, 0x2f };
    static GUID GUID_DEVINTERFACE_HID           = { 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 };

    // Register USB HID (Human Interface Devices) notifications to trigger the 3DConnexion enumeration.
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = { 0 };
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_HID;
    m_hDeviceNotify = ::RegisterDeviceNotification(this->GetHWND(), &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

// or register for file handle change?
//      DEV_BROADCAST_HANDLE NotificationFilter = { 0 };
//      NotificationFilter.dbch_size = sizeof(DEV_BROADCAST_HANDLE);
//      NotificationFilter.dbch_devicetype = DBT_DEVTYP_HANDLE;

    // Using Win32 Shell API to register for media insert / removal events.
    LPITEMIDLIST ppidl;
    if (SHGetSpecialFolderLocation(this->GetHWND(), CSIDL_DESKTOP, &ppidl) == NOERROR) {
        SHChangeNotifyEntry shCNE;
        shCNE.pidl       = ppidl;
        shCNE.fRecursive = TRUE;
        // Returns a positive integer registration identifier (ID).
        // Returns zero if out of memory or in response to invalid parameters.
        m_ulSHChangeNotifyRegister = SHChangeNotifyRegister(this->GetHWND(),        // Hwnd to receive notification
            SHCNE_DISKEVENTS,                                                       // Event types of interest (sources)
            SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED,
            //SHCNE_UPDATEITEM,                                                     // Events of interest - use SHCNE_ALLEVENTS for all events
            WM_USER_MEDIACHANGED,                                                   // Notification message to be sent upon the event
            1,                                                                      // Number of entries in the pfsne array
            &shCNE);                                                                // Array of SHChangeNotifyEntry structures that
                                                                                    // contain the notifications. This array should
                                                                                    // always be set to one when calling SHChnageNotifyRegister
                                                                                    // or SHChangeNotifyDeregister will not work properly.
        assert(m_ulSHChangeNotifyRegister != 0);    // Shell notification failed
    } else {
        // Failed to get desktop location
        assert(false);
    }

    {
        static constexpr int device_count = 1;
        RAWINPUTDEVICE devices[device_count] = { 0 };
        // multi-axis mouse (SpaceNavigator, etc.)
        devices[0].usUsagePage = 0x01;
        devices[0].usUsage = 0x08;
        if (! RegisterRawInputDevices(devices, device_count, sizeof(RAWINPUTDEVICE)))
            BOOST_LOG_TRIVIAL(error) << "RegisterRawInputDevices failed";
    }
}
#endif // _WIN32

void MainFrame::create_preset_tabs()
{
    wxGetApp().update_label_colours_from_appconfig();

    //BBS: GUI refactor
    //m_param_panel = new ParamsPanel(m_tabpanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT | wxTAB_TRAVERSAL);
    m_param_dialog = new ParamsDialog(m_plater);

    add_created_tab(new TabPrint(m_param_panel), "cog");
    add_created_tab(new TabPrintPlate(m_param_panel), "cog");
    add_created_tab(new TabPrintObject(m_param_panel), "cog");
    add_created_tab(new TabPrintPart(m_param_panel), "cog");
    add_created_tab(new TabPrintLayer(m_param_panel), "cog");
    add_created_tab(new TabFilament(m_param_dialog->panel()), "spool");
    /* BBS work around to avoid appearance bug */
    //add_created_tab(new TabSLAPrint(m_param_panel));
    //add_created_tab(new TabSLAMaterial(m_param_panel));
    add_created_tab(new TabPrinter(m_param_dialog->panel()), "printer");

    m_param_panel->rebuild_panels();
    m_param_dialog->panel()->rebuild_panels();
    //m_tabpanel->AddPage(m_param_panel, "Parameters", "notebook_presets_active");
    //m_tabpanel->InsertPage(tpSettings, m_param_panel, _L("Parameters"), std::string("cog"));
}

void MainFrame::add_created_tab(Tab* panel,  const std::string& bmp_name /*= ""*/)
{
    panel->create_preset_tab();

    if (panel->type() == Preset::TYPE_PLATE) {
        wxGetApp().tabs_list.pop_back();
        wxGetApp().plate_tab = panel;
    }
    // BBS: model config
    if (panel->type() == Preset::TYPE_MODEL) {
        wxGetApp().tabs_list.pop_back();
        wxGetApp().model_tabs_list.push_back(panel);
    }
}

bool MainFrame::is_active_and_shown_tab(wxPanel* panel)
{
    if (panel == m_param_panel)
        panel = m_plater;
    else
        return m_param_dialog->IsShown();

    if (m_tabpanel->GetCurrentPage() != panel)
        return false;
    return true;
}

bool MainFrame::can_start_new_project() const
{
    /*return m_plater && (!m_plater->get_project_filename(".3mf").IsEmpty() ||
                        GetTitle().StartsWith('*')||
                        wxGetApp().has_current_preset_changes() ||
                        !m_plater->model().objects.empty());*/
    return (m_plater && !m_plater->is_background_process_slicing());
}

bool MainFrame::can_open_project() const
{
    return (m_plater && !m_plater->is_background_process_slicing());
}

bool  MainFrame::can_add_models() const
{
    return (m_plater && !m_plater->is_background_process_slicing() && !m_plater->only_gcode_mode() && !m_plater->using_exported_file());
}

bool MainFrame::can_save() const
{
    return (m_plater != nullptr) &&
        !m_plater->get_view3D_canvas3D()->get_gizmos_manager().is_in_editing_mode(false) &&
        m_plater->is_project_dirty() && !m_plater->using_exported_file() && !m_plater->only_gcode_mode();
}

bool MainFrame::can_save_as() const
{
    return (m_plater != nullptr) &&
        !m_plater->get_view3D_canvas3D()->get_gizmos_manager().is_in_editing_mode(false) && !m_plater->using_exported_file() && !m_plater->only_gcode_mode();
}

void MainFrame::save_project()
{
    save_project_as(m_plater->get_project_filename(".3mf"));
}

bool MainFrame::save_project_as(const wxString& filename)
{
    if (m_plater == nullptr)
        return false;
    if (filename.empty())
        return m_plater->save_project(true) == wxID_YES;

    const std::filesystem::path previous_history_identity = m_plater->project_history_identity();
    m_plater->flush_project_history_pending("Project edit before save", true, true);

    // export_3mf returns a negative value on failure and a non-negative value
    // on success. Converting it directly to bool inverted both common cases
    // (0 became false, -1 became true).
    const int  export_result = m_plater->export_3mf(into_path(filename));
    const bool saved         = export_result >= 0;
    if (saved) {
//        wxGetApp().update_saved_preset_from_current_preset();
        m_plater->reset_project_dirty_after_save();
        m_plater->capture_saved_project_history(filename, previous_history_identity);
    }
    return saved;
}

void MainFrame::show_project_history()
{
    if (m_plater == nullptr)
        return;

    // list_versions is submitted to the same serialized worker as commits.
    // Flush and wait here so the dialog's list job is ordered after the latest
    // completed UI edit instead of presenting a stale HEAD.
    if (!m_plater->flush_project_history_pending("Project edit before opening version history", false, true)) {
        MessageDialog(this, _L("Version history is waiting for the current operation to finish. Try again when it completes."),
                      _L("Version history"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }

    ProjectHistoryDialog dialog(this, m_plater);
    if (dialog.ShowModal() != wxID_APPLY)
        return;

    const std::filesystem::path restored_snapshot = dialog.release_restored_snapshot();
    if (restored_snapshot.empty()) {
        MessageDialog(this, _L("The selected project version could not be prepared."),
                      _L("Version history"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }

    const bool restored = m_plater->restore_project_history_snapshot(restored_snapshot);

    std::error_code cleanup_error;
    std::filesystem::remove(restored_snapshot, cleanup_error);
    cleanup_error.clear();
    std::filesystem::remove(restored_snapshot.parent_path(), cleanup_error);

    if (!restored) {
        MessageDialog(this, _L("The selected version was not restored."),
                      _L("Version history"), wxOK | wxICON_INFORMATION).ShowModal();
    }
}

bool MainFrame::can_upload() const
{
    return true;
}

bool MainFrame::can_export_model() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

bool MainFrame::can_export_toolpaths() const
{
    return (m_plater != nullptr) && (m_plater->printer_technology() == ptFFF) && m_plater->is_preview_shown() && m_plater->is_preview_loaded() && m_plater->has_toolpaths_to_export();
}

bool MainFrame::can_export_supports() const
{
    if ((m_plater == nullptr) || (m_plater->printer_technology() != ptSLA) || m_plater->model().objects.empty())
        return false;

    bool can_export = false;
    const PrintObjects& objects = m_plater->sla_print().objects();
    for (const SLAPrintObject* object : objects)
    {
        if (object->has_mesh(slaposPad) || object->has_mesh(slaposSupportTree))
        {
            can_export = true;
            break;
        }
    }
    return can_export;
}

bool MainFrame::can_export_gcode() const
{
    if (m_plater == nullptr)
        return false;

    if (m_plater->model().objects.empty())
        return false;

    if (m_plater->is_export_gcode_scheduled())
        return false;

    // TODO:: add other filters
    PartPlateList &part_plate_list = m_plater->get_partplate_list();
    PartPlate *current_plate = part_plate_list.get_curr_plate();
    if (!current_plate->is_slice_result_ready_for_print())
        return false;

    return true;
}

bool MainFrame::can_export_all_gcode() const
{
    if (m_plater == nullptr)
        return false;

    if (m_plater->model().objects.empty())
        return false;

    if (m_plater->is_export_gcode_scheduled())
        return false;

    // TODO:: add other filters
    PartPlateList& part_plate_list = m_plater->get_partplate_list();
    return part_plate_list.is_all_slice_results_ready_for_print();
}

bool MainFrame::can_print_3mf() const
{
    if (m_plater && !m_plater->model().objects.empty()) {
        //
    }
    return true;
}

bool MainFrame::can_send_gcode() const
{
    if (m_plater && !m_plater->model().objects.empty())
    {
        auto cfg = wxGetApp().preset_bundle->printers.get_edited_preset().config;

        const auto *print_host_opt = cfg.option<ConfigOptionString>("print_host");
        if (! print_host_opt) return false;
        else return !print_host_opt->value.empty();
    }
    return true;
}

/*bool MainFrame::can_export_gcode_sd() const
{
    if (m_plater == nullptr)
        return false;

    if (m_plater->model().objects.empty())
        return false;

    if (m_plater->is_export_gcode_scheduled())
        return false;

    // TODO:: add other filters

    return wxGetApp().removable_drive_manager()->status().has_removable_drives;
}

bool MainFrame::can_eject() const
{
	return wxGetApp().removable_drive_manager()->status().has_eject;
}*/

bool MainFrame::can_slice() const
{
#ifdef SUPPORT_BACKGROUND_PROCESSING
    bool bg_proc = wxGetApp().app_config->get("background_processing") == "1";
    return (m_plater != nullptr) ? !m_plater->model().objects.empty() && !bg_proc : false;
#else
    return (m_plater != nullptr) ? !m_plater->model().objects.empty() : false;
#endif
}

bool MainFrame::can_change_view() const
{
    switch (m_layout)
    {
    default:                   { return false; }
    //BBS GUI refactor: remove unused layout new/dlg
    case ESettingsLayout::Old: {
        int page_id = m_tabpanel->GetSelection();
        return page_id != wxNOT_FOUND && dynamic_cast<const Slic3r::GUI::Plater*>(m_tabpanel->GetPage((size_t)page_id)) != nullptr;
    }
    case ESettingsLayout::GCodeViewer: { return true; }
    }
}

bool MainFrame::can_toggle_camera_fullscreen() const
{
    StatusPanel *status_panel = m_monitor ? m_monitor->get_status_panel() : nullptr;
    if (!status_panel) return false;
    if (status_panel->is_camera_fullscreen()) return true;
    return m_tabpanel && m_tabpanel->GetSelection() == TabPosition::tpMonitor && status_panel->can_show_camera_fullscreen();
}

void MainFrame::toggle_camera_fullscreen()
{
    StatusPanel *status_panel = m_monitor ? m_monitor->get_status_panel() : nullptr;
    if (!status_panel || !can_toggle_camera_fullscreen()) return;
    status_panel->toggle_camera_fullscreen();
}

bool MainFrame::can_clone() const {
    return can_select() && !m_plater->is_selection_empty();
}

bool MainFrame::can_select() const
{
    return (m_plater != nullptr) && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor) && !m_plater->model().objects.empty();
}

bool MainFrame::can_deselect() const
{
    return (m_plater != nullptr) && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor) && !m_plater->is_selection_empty();
}

bool MainFrame::can_delete() const
{
    return (m_plater != nullptr) && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor) && !m_plater->is_selection_empty();
}

bool MainFrame::can_delete_all() const
{
    return (m_plater != nullptr) && (m_tabpanel->GetSelection() == TabPosition::tp3DEditor) && !m_plater->model().objects.empty();
}

bool MainFrame::can_reslice() const
{
    return (m_plater != nullptr) && !m_plater->model().objects.empty();
}

wxBoxSizer* MainFrame::create_side_tools(wxWindow* parent)
{
    enable_multi_machine = wxGetApp().is_enable_multi_machine();
    int em = em_unit();
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

    m_prepare_plate_button = new Button(parent, _L("Plate 1"), "", wxNO_BORDER);
    m_prepare_add_plate_button = new Button(parent, "+", "", wxNO_BORDER);
    // Two-tier print estimate (MD3 Prepare kit §67-70): a stacked time line
    // (Roboto Mono 15/500) over a weight/length line (11px OnSurfaceVariant).
    m_prepare_estimate_label = new wxStaticText(parent, wxID_ANY, _L("Not sliced"),
                                                 wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    wxStaticText *m_prepare_estimate_detail = new wxStaticText(parent, wxID_ANY, wxEmptyString,
                                                 wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    m_prepare_estimate_detail->SetName(PREPARE_ESTIMATE_DETAIL_NAME);

    const wxColour primary = StateColor::semantic(MD3::Role::Primary);
    const wxColour outline = StateColor::semantic(MD3::Role::Outline);
    // Active-plate chip (MD3 digest 3.2): SecondaryContainer fill,
    // OnSecondaryContainer text, 2px Primary border, 12px radius. Restyled
    // authoritatively in update_prepare_action_bar_style(); mirrored here.
    const StateColor plate_background(
        std::pair{StateColor::semantic(MD3::Role::PrimaryContainer), (int) StateColor::Hovered},
        std::pair{StateColor::semantic(MD3::Role::SecondaryContainer), (int) StateColor::Normal});
    m_prepare_plate_button->SetBackgroundColor(plate_background);
    m_prepare_plate_button->SetBorderColor(StateColor(primary));
    m_prepare_plate_button->SetBorderWidth(FromDIP(2));
    m_prepare_plate_button->SetTextColor(StateColor(StateColor::semantic(MD3::Role::OnSecondaryContainer)));
    m_prepare_plate_button->SetCornerRadius(FromDIP(12));
    m_prepare_plate_button->SetMinSize(FromDIP(wxSize(96, 40)));
    // Leading grid_view glyph (MD3 Prepare kit §62-63), coloured by the chip's
    // OnSecondaryContainer text role. Button self-gates on MaterialIcon::available():
    // no glyph (label-only chip) when the Material Symbols face is unavailable.
    m_prepare_plate_button->SetGlyph(MaterialIcon::GridView, 20);

    // Add-plate (MD3 digest 3.2): dashed 1px Outline border, transparent
    // (bar-surface) fill, 12px radius, OnSurfaceVariant glyph.
    const StateColor add_background(
        std::pair{StateColor::semantic(MD3::Role::SurfaceContainerHigh), (int) StateColor::Hovered},
        std::pair{StateColor::semantic(MD3::Role::SurfaceContainerLow), (int) StateColor::Normal});
    m_prepare_add_plate_button->SetBackgroundColor(add_background);
    m_prepare_add_plate_button->SetBorderColor(StateColor(outline));
    m_prepare_add_plate_button->SetBorderWidth(FromDIP(1));
    m_prepare_add_plate_button->SetBorderStyle(wxPENSTYLE_SHORT_DASH);
    m_prepare_add_plate_button->SetTextColor(StateColor(StateColor::semantic(MD3::Role::OnSurfaceVariant)));
    m_prepare_add_plate_button->SetCornerRadius(FromDIP(12));
    m_prepare_add_plate_button->SetMinSize(FromDIP(wxSize(40, 40)));
    // Replace the literal '+' label with a 20px 'add' glyph (MD3 Prepare kit §64),
    // coloured by the button's OnSurfaceVariant text role. Capability-gated: the
    // '+' text stays as the fallback when the Material Symbols face is unavailable.
    if (MaterialIcon::available()) {
        m_prepare_add_plate_button->SetLabel(wxEmptyString);
        m_prepare_add_plate_button->SetGlyph(MaterialIcon::Add, 20);
    }

    style_prepare_estimate(m_prepare_estimate_label, m_prepare_estimate_detail);
    m_prepare_estimate_label->SetMinSize(FromDIP(wxSize(118, -1)));

    wxBoxSizer *estimate_col = new wxBoxSizer(wxVERTICAL);
    estimate_col->Add(m_prepare_estimate_label, 0, wxALIGN_RIGHT);
    estimate_col->Add(m_prepare_estimate_detail, 0, wxALIGN_RIGHT);

    m_prepare_plate_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (!m_plater)
            return;
        auto &plates = m_plater->get_partplate_list();
        wxMenu menu;
        std::vector<int> ids;
        ids.reserve(plates.get_plate_count());
        for (int index = 0; index < plates.get_plate_count(); ++index) {
            const int id = wxWindow::NewControlId();
            ids.push_back(id);
            PartPlate *plate = plates.get_plate(index);
            wxString label = plate != nullptr && !plate->get_plate_name().empty()
                ? from_u8(plate->get_plate_name())
                : wxString::Format(_L("Plate %d"), index + 1);
            wxMenuItem *item = menu.AppendRadioItem(id, label);
            item->Check(index == plates.get_curr_plate_index());
        }
        const int selected = m_prepare_plate_button->GetPopupMenuSelectionFromUser(menu);
        const auto found = std::find(ids.begin(), ids.end(), selected);
        if (found != ids.end()) {
            m_plater->select_plate(static_cast<int>(std::distance(ids.begin(), found)));
            update_prepare_action_bar_content();
        }
    });
    m_prepare_add_plate_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (m_plater && m_plater->canvas3D() && m_plater->canvas3D()->get_wxglcanvas()) {
            wxPostEvent(m_plater->canvas3D()->get_wxglcanvas(), SimpleEvent(EVT_GLTOOLBAR_ADD_PLATE));
        }
    });

    /*helio*/
    // MD3 divider: a 1px OutlineVariant vertical rule replaces the legacy raster
    // 'topbar_line' separator bitmap. It mirrors the action bar's own horizontal
    // divider (a thin panel filled with the OutlineVariant token), so it is
    // theme- and DPI-adaptive without any raster asset.
    m_prepare_split_line = new wxPanel(parent, wxID_ANY, wxDefaultPosition,
                                       wxSize(FromDIP(1), FromDIP(22)), wxBORDER_NONE);
    m_prepare_split_line->SetBackgroundColour(StateColor::semantic(MD3::Role::OutlineVariant));
    m_prepare_split_line->SetMinSize(wxSize(FromDIP(1), FromDIP(22)));

    // The Helio mark is a genuine brand asset (exempt): it stays a raster bitmap
    // and is never recoloured. The generic 'expand program' affordance, however,
    // is legacy raster chrome — when the Material Symbols face is available it
    // migrates to the borderless glyph IconButton below, and the raster
    // ExpandButton is retained only as the capability fallback.
    const bool action_icons_ok = MaterialIcon::available();
    expand_program_holder = new ExpandButtonHolder(parent);
    expand_program_holder->addExpandButton(expand_helio_id, "helio_icon_topbar");
    if (!action_icons_ok)
        expand_program_holder->addExpandButton(expand_program_id, "expand_program");
    expand_program_holder->Bind(wxEXPAND_LEFT_DOWN, [=](const wxCommandEvent& e) {

        if (e.GetInt() == expand_helio_id) {
            BOOST_LOG_TRIVIAL(info) << "Helio button clicked";
            Plater* plater = wxGetApp().plater();
            wxCommandEvent evt(EVT_HELIO_INPUT_DLG);
            evt.SetEventObject(plater);
            wxPostEvent(plater, evt);
        }

        if (e.GetInt() == expand_program_id) {
            ExpandCenterDialog dlg;
            dlg.ShowModal();
        }
        });

    if (wxGetApp().app_config->get("helio_enable") == "true") {
        expand_program_holder->ShowExpandButton(expand_helio_id, true);
    }
    else {
        expand_program_holder->ShowExpandButton(expand_helio_id, false);
    }

    // Set tooltip for Helio expand button
    expand_program_holder->SetExpandButtonRichTooltip(expand_helio_id, "monitor_speed", _L("Unlock faster, more reliable, warp-free prints with Helio Additive."));
    // Set tooltip for the raster program expand button (capability fallback only)
    if (!action_icons_ok)
        expand_program_holder->SetExpandButtonRichTooltip(expand_program_id, "monitor_speed", _L("Unlock faster, more reliable, warp-free prints with Helio Additive."));

    // Material Symbols expand affordance: a borderless ghost IconButton drawn with
    // an expand chevron (OnSurfaceVariant, hover SurfaceContainerHigh) that opens
    // the same ExpandCenterDialog. Replaces the raster 'expand_program' chrome.
    if (action_icons_ok) {
        m_prepare_expand_btn = new Button(parent, wxEmptyString);
        m_prepare_expand_btn->SetIconButton(Button::IconShape::Circle, 34);
        m_prepare_expand_btn->SetGlyph(MaterialIcon::ExpandLess);
        m_prepare_expand_btn->SetToolTip(_L("Unlock faster, more reliable, warp-free prints with Helio Additive."));
        m_prepare_expand_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            ExpandCenterDialog dlg;
            dlg.ShowModal();
        });
    }

    /*slice*/
    m_slice_select = eSlicePlate;
    m_print_select = ePrintPlate;

    auto slice_panel = new wxPanel(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxTRANSPARENT_WINDOW);
    auto print_panel = new wxPanel(parent,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxTRANSPARENT_WINDOW);

    m_slice_btn = new SideButton(slice_panel, _L("Slice plate"), "");
    // The kit has no dropdown carets, so the legacy raster 'sidebutton_dropdown'
    // glyph is dropped; the options segment survives as a functional pill (its
    // Material Symbol 'arrow_drop_down' is deferred to the icon wave).
    m_slice_option_btn = new SideButton(slice_panel, "", "");
    m_print_btn = new SideButton(print_panel, _L("Print plate"), "");
    m_print_option_btn = new SideButton(print_panel, "", "");

    auto slice_sizer = new wxBoxSizer(wxHORIZONTAL);
    slice_sizer->Add(m_slice_option_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    slice_sizer->Add(m_slice_btn, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    slice_panel->SetSizer(slice_sizer);

    auto print_sizer = new wxBoxSizer(wxHORIZONTAL);
    print_sizer->Add(m_print_option_btn, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    print_sizer->Add(m_print_btn, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(1));
    print_panel->SetSizer(print_sizer);

    update_side_button_style();
    // Leading Material Symbols glyphs (MD3 Prepare kit §71-72): Slice = deployed_code
    // (outlined action), Print = print. The vendored Material Symbols face is static
    // Outlined, so the Print 'filled' emphasis is carried by its Primary/OnPrimary
    // fill (set in update_side_button_style), not a FILL-axis swap. SideButton
    // self-gates on MaterialIcon::available(): the label alone shows when absent.
    m_slice_btn->SetLeadingGlyph(MaterialIcon::DeployedCode);
    m_print_btn->SetLeadingGlyph(MaterialIcon::Print);
    m_slice_option_btn->Enable();
    m_print_option_btn->Enable();
    sizer->Add(m_prepare_plate_button, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(FromDIP(8), 0, 0, 0, 0);
    sizer->Add(m_prepare_add_plate_button, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(0, 0, 1, wxEXPAND, 0);
    sizer->Add(estimate_col, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    sizer->Add(expand_program_holder, 0, wxALIGN_CENTER, 0);
    if (m_prepare_expand_btn)
        sizer->Add(m_prepare_expand_btn, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(FromDIP(4), 0, 0, 0, 0);
    sizer->Add(m_prepare_split_line, 0, wxALIGN_CENTER, 0);
    sizer->Add(FromDIP(6), 0, 0, 0, 0);
    sizer->Add(slice_panel, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(FromDIP(8), 0, 0, 0, 0);
    sizer->Add(print_panel, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(FromDIP(4), 0, 0, 0, 0);

    sizer->Layout();

    m_filament_group_popup = new FilamentGroupPopup(m_slice_btn);

    auto try_hover_pop_up = [this]() {
#ifdef __APPLE__
        if (!IsActive()) {
            return;
        }
#endif
        auto curr_plate = this->m_plater->get_partplate_list().get_curr_plate();

        // This popup used to sit below the navigation row. The action bar is
        // now at the bottom of the window, so anchor the popup above the Slice
        // control and clamp it to the active display before and after it refits.
        auto position_popup = [this]() {
            const wxSize popup_size = m_filament_group_popup->GetSize();
            wxPoint pos = m_slice_btn->ClientToScreen(wxPoint(0, 0));
            pos.x -= m_slice_option_btn->GetSize().x + FromDIP(228);
            pos.y -= popup_size.y + FromDIP(8);

            const wxRect display_area = wxDisplay(m_slice_btn).GetClientArea();
            pos.x = std::max(display_area.x,
                             std::min(pos.x, display_area.GetRight() - popup_size.x + 1));
            pos.y = std::max(display_area.y,
                             std::min(pos.y, display_area.GetBottom() - popup_size.y + 1));
            m_filament_group_popup->SetPosition(pos);
        };

        position_popup();
        m_filament_group_popup->tryPopup(m_plater, curr_plate, m_slice_select == eSliceAll);
        if (m_filament_group_popup->IsShown())
            position_popup();
    };

#ifndef __linux__
// in linux plateform, the pop up will taker over the mouse event and make the slice button cannot handle click event
    // this pannel is used to trigger hover when button is disabled
    slice_panel->Bind(wxEVT_ENTER_WINDOW, [this,try_hover_pop_up](auto& event) {
        if(!m_slice_option_pop_up || !m_slice_option_pop_up->IsShown())
            try_hover_pop_up();
        });

    slice_panel->Bind(wxEVT_LEAVE_WINDOW, [this](auto& event) {
        m_filament_group_popup->tryClose();
        });

    m_slice_btn->Bind(wxEVT_ENTER_WINDOW, [this, try_hover_pop_up](auto& event) {
        if (!m_slice_option_pop_up || !m_slice_option_pop_up->IsShown())
            try_hover_pop_up();
        });

    m_slice_btn->Bind(wxEVT_LEAVE_WINDOW, [this](auto& event) {
        m_filament_group_popup->tryClose();
        });
#endif

    m_slice_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
        {
            if (m_plater->is_background_process_update_scheduled())
                m_plater->update(false, true);

            m_plater->reset_check_status();
            if (!m_plater->check_ams_status(m_slice_select == eSliceAll))
                return;
            m_plater->set_slice_from_slice_btn(true);

            //this->m_plater->select_view_3D("Preview");
            m_plater->exit_gizmo();
            m_plater->update(true, true);

            bool slice = true;

            auto curr_plate = m_plater->get_partplate_list().get_curr_plate();
            #ifdef __linux__
                slice = try_pop_up_before_slice(m_slice_select == eSliceAll, m_plater, curr_plate, true);
            #else
                slice = try_pop_up_before_slice(m_slice_select == eSliceAll, m_plater, curr_plate, false);
            #endif

            bool model_fits     = false;
            bool validate_error = false;
            m_plater->validate_current_plate(model_fits, validate_error);

            if (slice && model_fits && !validate_error) {
                std::string printer_model = wxGetApp().preset_bundle->printers.get_edited_preset().config.opt_string("printer_model");
                std::unordered_set<std::string> printer_models = {"Bambu Lab H2D", "Bambu Lab H2D Pro", "Bambu Lab H2C"};
                int extruder_count = wxGetApp().preset_bundle->get_printer_extruder_count();
                if (extruder_count > 1 && printer_models.count(printer_model)) {
                    const std::string slice_video_key = dual_extruder_first_slice_video_app_config_key(printer_model);
                    const std::string slice_video_val = wxGetApp().app_config->get(slice_video_key);
                    if (slice_video_val.empty() || slice_video_val == "true" || slice_video_val == "1") {
                        MessageDialog dlg(this, _L("This is your first time slicing with the dual extruder machine.\nWould you like to watch a quick tutorial video?"), _L("First Guide"), wxYES_NO);
                        auto  res = dlg.ShowModal();
                        if (res == wxID_YES) {
                            play_dual_extruder_slice_video();
                            slice = false;
                        }
                        wxGetApp().app_config->set(slice_video_key, "false");
                    }

                    if ((wxGetApp().app_config->get("play_tpu_printing_video") == "true")) {
                        auto used_filaments = curr_plate->get_extruders();
                        std::transform(used_filaments.begin(), used_filaments.end(), used_filaments.begin(), [](auto i) {return i - 1; });
                        auto full_config = wxGetApp().preset_bundle->full_config();
                        auto filament_types = full_config.option<ConfigOptionStrings>("filament_type")->values;
                        if (std::any_of(used_filaments.begin(), used_filaments.end(), [filament_types](int idx) { return filament_types[idx] == "TPU"; })) {
                            MessageDialog dlg(this, _L("This is your first time printing tpu filaments with the dual extruder machine.\nWould you like to watch a quick tutorial video?"), _L("First Guide"), wxYES_NO);
                            auto  res = dlg.ShowModal();
                            if (res == wxID_YES) {
                                play_dual_extruder_print_tpu_video();
                                slice = false;
                            }
                            wxGetApp().app_config->set("play_tpu_printing_video", "false");
                        }
                    }
                }

                if (printer_model == "Bambu Lab H2S") {
                    if ((wxGetApp().app_config->get("prompt_for_brittle_filaments") == "true") ) {
                        auto used_filaments = curr_plate->get_extruders();
                        std::transform(used_filaments.begin(), used_filaments.end(), used_filaments.begin(), [](auto i) {return i - 1; });
                        auto full_config = wxGetApp().preset_bundle->full_config();
                        auto filament_types = full_config.option<ConfigOptionStrings>("filament_type")->values;
                        if (std::any_of(used_filaments.begin(), used_filaments.end(), [filament_types](int idx) { return filament_types[idx] == "PPA-CF" || filament_types[idx] == "PPS-CF"; })) {
                            MessageDialog dlg(this, _L("PPS-CF/PPA-CF is brittle and could break in bended PTFE tube above Toolhead. Please refer to Wiki before use. "), _L("Tips"), wxYES_NO);
                            auto  res = dlg.ShowModal();
                            if (res == wxID_YES) {
                                wxLaunchDefaultBrowser("https://e.bambulab.com/t?c=UC64kdlpHxN3Mb15");
                                slice = false;
                            }
                            wxGetApp().app_config->set("prompt_for_brittle_filaments", "false");
                        }
                    }
                }


                if (slice) {
                    if (m_slice_select == eSliceAll)
                        wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SLICE_ALL));
                    else if (m_slice_select == eSlicePlate)
                        wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SLICE_PLATE));
                    this->m_tabpanel->SetSelection(tpPreview);
                }
            }
        });

    m_print_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
        {
            //this->m_plater->select_view_3D("Preview");
            if (m_print_select == ePrintAll || m_print_select == ePrintPlate || m_print_select == ePrintMultiMachine)
            {
                m_plater->apply_background_progress();
                // check valid of print
                m_print_enable = get_enable_print_status();
                m_print_btn->Enable(m_print_enable);
                if (m_print_enable) {
                    if (m_print_select == ePrintAll)
                        wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_ALL));
                    if (m_print_select == ePrintPlate)
                        wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_PLATE));
                    if(m_print_select == ePrintMultiMachine)
                         wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_MULTI_MACHINE));
                }
            }
            else if (m_print_select == eExportGcode)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_GCODE));
            else if (m_print_select == eSendGcode)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SEND_GCODE));
            else if (m_print_select == eUploadGcode)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_UPLOAD_GCODE));
            else if (m_print_select == eExportSlicedFile)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_SLICED_FILE));
            else if (m_print_select == eExportAllSlicedFile)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_ALL_SLICED_FILE));
            else if (m_print_select == eSendToPrinter)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SEND_TO_PRINTER));
            else if (m_print_select == eSendToPrinterAll)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SEND_TO_PRINTER_ALL));
            else if (m_print_select == eSendMultiApp)
                wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_SEND_MULTI_APP));
            /* else if (m_print_select == ePrintMultiMachine)
                 wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_PRINT_MULTI_MACHINE));*/
        });

    m_slice_option_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
        {
            if(m_slice_option_pop_up)
                delete m_slice_option_pop_up;
            m_slice_option_pop_up = new SidePopup(this);
            SideButton* slice_all_btn = new SideButton(m_slice_option_pop_up, _L("Slice all"), "");
            slice_all_btn->SetCornerRadius(0);
            SideButton* slice_plate_btn = new SideButton(m_slice_option_pop_up, _L("Slice plate"), "");
            slice_plate_btn->SetCornerRadius(0);

            slice_all_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
                plater()->get_notification_manager()->close_notification_of_type(NotificationType::HelioSlicingError);
                m_slice_btn->SetLabel(_L("Slice all"));
                m_slice_select = eSliceAll;
                m_slice_enable = get_enable_slice_status();
                m_slice_btn->Enable(m_slice_enable);
                this->Layout();
                if(m_slice_option_pop_up)
                    m_slice_option_pop_up->Dismiss();
                });

            slice_plate_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
                plater()->get_notification_manager()->close_notification_of_type(NotificationType::HelioSlicingError);
                m_slice_btn->SetLabel(_L("Slice plate"));
                m_slice_select = eSlicePlate;
                m_slice_enable = get_enable_slice_status();
                m_slice_btn->Enable(m_slice_enable);
                this->Layout();
                if(m_slice_option_pop_up)
                    m_slice_option_pop_up->Dismiss();
                });

            m_slice_option_pop_up->append_button(slice_all_btn);
            m_slice_option_pop_up->append_button(slice_plate_btn);
            m_slice_option_pop_up->Popup(m_slice_btn);
        }
    );

    m_print_option_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
        {
            SidePopup* p = new SidePopup(this);

            if (wxGetApp().preset_bundle
                && !wxGetApp().preset_bundle->printers.get_edited_preset().is_bbl_vendor_preset(wxGetApp().preset_bundle)) {
                // ThirdParty Buttons
                SideButton* export_gcode_btn = new SideButton(p, _L("Export G-code file"), "");
                export_gcode_btn->SetCornerRadius(0);
                export_gcode_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Export G-code file"));
                    m_print_select = eExportGcode;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                // upload and print
                SideButton* send_gcode_btn = new SideButton(p, _L("Print"), "");
                send_gcode_btn->SetCornerRadius(0);
                send_gcode_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Print"));
                    m_print_select = eSendGcode;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                // upload only
                /*SideButton* upload_gcode_btn = new SideButton(p, _L("Send"), "");
                upload_gcode_btn->SetCornerRadius(0);
                upload_gcode_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Send"));
                    m_print_select = eUploadGcode;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });*/

                p->append_button(send_gcode_btn);
                //p->append_button(upload_gcode_btn);
                p->append_button(export_gcode_btn);
            }
            else {
                //Bambu Studio Buttons
                SideButton* print_plate_btn = new SideButton(p, _L("Print plate"), "");
                print_plate_btn->SetCornerRadius(0);

                SideButton* send_to_printer_btn = new SideButton(p, _L("Send"), "");
                send_to_printer_btn->SetCornerRadius(0);

                SideButton* export_sliced_file_btn = new SideButton(p, _L("Export plate sliced file"), "");
                export_sliced_file_btn->SetCornerRadius(0);

                SideButton* export_all_sliced_file_btn = new SideButton(p, _L("Export all sliced file"), "");
                export_all_sliced_file_btn->SetCornerRadius(0);

                print_plate_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Print plate"));
                    m_print_select = ePrintPlate;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                SideButton* print_all_btn = new SideButton(p, _L("Print all"), "");
                print_all_btn->SetCornerRadius(0);
                print_all_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Print all"));
                    m_print_select = ePrintAll;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                send_to_printer_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Send"));
                    m_print_select = eSendToPrinter;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                SideButton* send_to_printer_all_btn = new SideButton(p, _L("Send all"), "");
                send_to_printer_all_btn->SetCornerRadius(0);
                send_to_printer_all_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Send all"));
                    m_print_select = eSendToPrinterAll;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                export_sliced_file_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Export plate sliced file"));
                    m_print_select = eExportSlicedFile;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                export_all_sliced_file_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                    m_print_btn->SetLabel(_L("Export all sliced file"));
                    m_print_select = eExportAllSlicedFile;
                    m_print_enable = get_enable_print_status();
                    m_print_btn->Enable(m_print_enable);
                    this->Layout();
                    p->Dismiss();
                    });

                p->append_button(print_plate_btn);
                p->append_button(print_all_btn);
                p->append_button(send_to_printer_btn);
                p->append_button(send_to_printer_all_btn);
                p->append_button(export_sliced_file_btn);
                p->append_button(export_all_sliced_file_btn);


                if (check_bbl_farm_client_installed()) {
                    SideButton *send_to_multi_app_btn = new SideButton(p, _L("Send to Bambu Farm Manager Client"), "");
                    send_to_multi_app_btn->SetCornerRadius(0);
                    p->append_button(send_to_multi_app_btn);

                    send_to_multi_app_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent &) {
                        m_print_btn->SetLabel(_L("Send to BFMC"));
                        m_print_select = eSendMultiApp;
                        m_print_enable = get_enable_print_status();
                        m_print_btn->Enable(m_print_enable);
                        this->Layout();
                        p->Dismiss();
                    });
                }


                if (enable_multi_machine) {
                    SideButton* print_multi_machine_btn = new SideButton(p, _L("Send to Multi-device"), "");
                    print_multi_machine_btn->SetCornerRadius(0);
                    print_multi_machine_btn->Bind(wxEVT_BUTTON, [this, p](wxCommandEvent&) {
                        m_print_btn->SetLabel(_L("Send to Multi-device"));
                        m_print_select = ePrintMultiMachine;
                        m_print_enable = get_enable_print_status();
                        m_print_btn->Enable(m_print_enable);
                        this->Layout();
                        p->Dismiss();
                        });
                    p->append_button(print_multi_machine_btn);
                }
            }

            p->Popup(m_print_btn);
        }
    );

    /*
    Button * aux_btn = new Button(this, _L("Auxiliary"));
    aux_btn->SetBackgroundColour("#3B4446");
    aux_btn->Bind(wxEVT_BUTTON, [](auto e) {
        wxGetApp().sidebar().show_auxiliary_dialog();
    });
    sizer->Add(aux_btn, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 1 * em / 10);
    */
    // sizer->Add(FromDIP(19), 0, 0, 0, 0);

    return sizer;
}

bool MainFrame::get_enable_slice_status()
{
    bool enable = true;

    bool on_slicing = m_plater->is_background_process_slicing();
    if (on_slicing) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": on slicing, return false directly!");
        return false;
    }
    else if  (m_plater->only_gcode_mode() || m_plater->using_exported_file()) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": in gcode/exported 3mf mode, return false directly!");
        return false;
    }

    PartPlateList &part_plate_list = m_plater->get_partplate_list();
    PartPlate *current_plate = part_plate_list.get_curr_plate();

    if (m_slice_select == eSliceAll)
    {
        /*if (part_plate_list.is_all_slice_results_valid())
        {
            enable = false;
        }
        else if (!part_plate_list.is_all_plates_ready_for_slice())
        {
            enable = false;
        }*/
        //always enable slice_all button
        enable = true;
    }
    else if (m_slice_select == eSlicePlate)
    {
        if (current_plate->is_slice_result_valid())
        {
            enable = false;
        }
        else if (!current_plate->can_slice())
        {
            enable = false;
        }
    }

    if (enable && m_plater->sidebar().has_broken_mixed_filament())
        enable = false;

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": m_slice_select %1%, enable= %2% ")%m_slice_select %enable;
    return enable;
}

bool MainFrame::get_enable_print_status()
{
    bool enable = true;

    PartPlateList &part_plate_list = m_plater->get_partplate_list();
    PartPlate *current_plate = part_plate_list.get_curr_plate();
    bool is_all_plates = wxGetApp().plater()->get_preview_canvas3D()->is_all_plates_selected();
    if (m_print_select == ePrintAll)
    {
        if (!part_plate_list.is_all_slice_results_ready_for_print())
        {
            enable = false;
        }
    }
    else if (m_print_select == ePrintPlate)
    {
        if (!current_plate->is_slice_result_ready_for_print())
        {
            enable = false;
        }
        enable = enable && !is_all_plates;
    }
    else if (m_print_select == eExportGcode)
    {
        if (!current_plate->is_slice_result_valid())
        {
            enable = false;
        }
        enable = enable && !is_all_plates;
    }
    else if (m_print_select == eSendGcode)
    {
        if (!current_plate->is_slice_result_valid())
            enable = false;
        if (!can_send_gcode())
            enable = false;
        enable = enable && !is_all_plates;
    }
    else if (m_print_select == eUploadGcode)
    {
        if (!current_plate->is_slice_result_valid())
            enable = false;
        if (!can_send_gcode())
            enable = false;
        enable = enable && !is_all_plates;
    }
    else if (m_print_select == eExportSlicedFile)
    {
        if (!current_plate->is_slice_result_ready_for_export())
        {
            enable = false;
        }
        enable = enable && !is_all_plates;
	}
	else if (m_print_select == eSendToPrinter)
	{
		if (!current_plate->is_slice_result_ready_for_print())
		{
			enable = false;
		}
        enable = enable && !is_all_plates;
	}
    else if (m_print_select == eSendToPrinterAll)
    {
        if (!part_plate_list.is_all_slice_results_ready_for_print())
        {
            enable = false;
        }
    }
    else if (m_print_select == eExportAllSlicedFile)
    {
        if (!part_plate_list.is_all_slice_result_ready_for_export())
        {
            enable = false;
        }
    }
    else if (m_print_select == ePrintMultiMachine)
    {
        if (!current_plate->is_slice_result_ready_for_print())
        {
            enable = false;
        }
        enable = enable && !is_all_plates;
    }else if (m_print_select == eSendMultiApp) {
        if (!current_plate->is_slice_result_ready_for_print()) {
            enable = false;
        }
        enable = enable && !is_all_plates;
    }

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(": m_print_select %1%, enable= %2% ")%m_print_select %enable;

    return enable;
}

void MainFrame::update_side_button_style()
{
    using R = MD3::Role;

    // MD3 pill geometry: the Slice/Print action buttons are 44px tall with a
    // corner radius of height/2. Both are derived here (never cached) from the
    // current DPI so the on_dpi_changed -> update_side_button_style path
    // re-computes them after a monitor/DPI change instead of reusing a stale
    // radius.
    const int      btn_height = FromDIP(44);
    const double   pill       = MD3::Metrics::pill_radius(btn_height);
    const wxColour bar_bg     = StateColor::semantic(R::SurfaceContainerLow); // action-bar fill behind the pill corners

    const wxColour disabled_bg  = StateColor::semantic(R::SurfaceContainerHigh);
    // Disabled label tone: OnSurface blended ~45% over the ACTUAL disabled fill
    // (SurfaceContainerHigh), theme-aware. The previous ThemeColor::TextDisabled
    // pair rendered #6a6b73 on #2f3036 in dark mode (~1.7:1) — the "Slice plate"
    // / "Print plate" labels were unreadable when the plate was empty.
    auto blend = [](const wxColour &fg_c, const wxColour &bg_c, double t) -> wxColour {
        auto mix = [t](int f, int b) {
            int v = (int) (f * t + b * (1.0 - t) + 0.5);
            return (unsigned char) std::max(0, std::min(255, v));
        };
        return wxColour(mix(fg_c.Red(), bg_c.Red()), mix(fg_c.Green(), bg_c.Green()),
                        mix(fg_c.Blue(), bg_c.Blue()));
    };
    const wxColour disabled_txt = blend(StateColor::semantic(R::OnSurface), disabled_bg, 0.45);

    // Filled hover state layer: a subtle brighten of the Primary fill, matching
    // Widgets/Button.cpp applyMD3Style() (filled x1.06). Kept local so this file
    // does not depend on Button.cpp's internal linkage.
    auto brighten = [](const wxColour &c, double factor) -> wxColour {
        auto ch = [factor](unsigned char v) -> unsigned char {
            int r = (int) (v * factor + 0.5);
            if (r > 255) r = 255;
            if (r < 0)   r = 0;
            return (unsigned char) r;
        };
        return wxColour(ch(c.Red()), ch(c.Green()), ch(c.Blue()), c.Alpha());
    };

    // Slice -> MD3 outlined: SurfaceContainerHigh fill, 1px Outline ring,
    // OnSurface label (per ui-md3 Prepare.jsx: an outlined Button whose rest
    // background is sc-high). Hover lifts the fill to SurfaceContainerHighest.
    auto style_outlined = [&](SideButton *btn) {
        btn->SetBackgroundColor(StateColor(
            std::make_pair(StateColor::semantic(R::SurfaceContainerHigh),    (int) StateColor::Disabled),
            std::make_pair(StateColor::semantic(R::SurfaceContainerHighest), (int) StateColor::Hovered),
            std::make_pair(StateColor::semantic(R::SurfaceContainerHigh),    (int) StateColor::Normal)));
        btn->SetBorderColor(StateColor(
            std::make_pair(StateColor::semantic(R::OutlineVariant), (int) StateColor::Disabled),
            std::make_pair(StateColor::semantic(R::Outline),        (int) StateColor::Normal)));
        btn->SetForegroundColor(StateColor(
            std::make_pair(disabled_txt,                       (int) StateColor::Disabled),
            std::make_pair(StateColor::semantic(R::OnSurface), (int) StateColor::Normal)));
        btn->SetBottomColour(bar_bg);
    };

    // Print -> MD3 filled: Primary fill, OnPrimary label; the border tracks the
    // fill so no contrasting ring shows. The kit marks this button "elevated"
    // (elev-2); SideButton cannot draw the drop-shadow, tracked as a follow-up.
    auto style_filled = [&](SideButton *btn) {
        const wxColour fill  = StateColor::semantic(R::Primary);
        const wxColour hover = brighten(fill, 1.06);
        btn->SetBackgroundColor(StateColor(
            std::make_pair(disabled_bg, (int) StateColor::Disabled),
            std::make_pair(hover,       (int) StateColor::Hovered),
            std::make_pair(fill,        (int) StateColor::Normal)));
        btn->SetBorderColor(StateColor(
            std::make_pair(disabled_bg, (int) StateColor::Disabled),
            std::make_pair(hover,       (int) StateColor::Hovered),
            std::make_pair(fill,        (int) StateColor::Normal)));
        btn->SetForegroundColor(StateColor(
            std::make_pair(disabled_txt,                       (int) StateColor::Disabled),
            std::make_pair(StateColor::semantic(R::OnPrimary), (int) StateColor::Normal)));
        btn->SetBottomColour(bar_bg);
    };

    // The main action button and its options segment share one scheme so the
    // pair reads as a single split control. Both are fully-rounded pills
    // (layout_style 1) rather than the legacy flat-seam split; the former
    // 'sidebutton_dropdown' caret is gone (see the constructor above).
    auto layout_main = [&](SideButton *btn) {
        btn->SetLayoutStyle(1);
        btn->SetTextLayout(SideButton::EHorizontalOrientation::HO_Left, FromDIP(15));
        btn->SetCornerRadius(pill);
        btn->SetExtraSize(wxSize(FromDIP(38), FromDIP(10)));
        btn->SetMinSize(wxSize(-1, btn_height));
    };
    auto layout_option = [&](SideButton *btn) {
        btn->SetLayoutStyle(1);
        btn->SetTextLayout(SideButton::EHorizontalOrientation::HO_Center);
        btn->SetCornerRadius(pill);
        btn->SetExtraSize(wxSize(FromDIP(10), FromDIP(10)));
        btn->SetMinSize(wxSize(FromDIP(24), btn_height));
    };

    style_outlined(m_slice_btn);
    style_outlined(m_slice_option_btn);
    layout_main(m_slice_btn);
    layout_option(m_slice_option_btn);

    style_filled(m_print_btn);
    style_filled(m_print_option_btn);
    layout_main(m_print_btn);
    layout_option(m_print_option_btn);
}

void MainFrame::update_slice_print_status(SlicePrintEventType event, bool can_slice, bool can_print)
{
    bool enable_print = true, enable_slice = true;

    if (!can_slice)
    {
        if (m_slice_select == eSlicePlate)
            enable_slice = false;
    }
    if (!can_print)
        enable_print = false;


    //process print logic
    if (enable_print)
    {
        enable_print = get_enable_print_status();
    }

    //process slice logic
    if (enable_slice)
    {
        enable_slice = get_enable_slice_status();
    }

    bool old_slice_status = m_slice_btn->IsEnabled();

    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format(" m_slice_select %1%: can_slice= %2%, can_print %3%, enable_slice %4%, enable_print %5% ")%m_slice_select % can_slice %can_print %enable_slice %enable_print;
    m_print_btn->Enable(enable_print);
    m_slice_btn->Enable(enable_slice);
    m_slice_enable = enable_slice;
    m_print_enable = enable_print;

    /*for healio*/
    if (expand_program_holder) {
        expand_program_holder->updateExpandButtonBitmap(expand_helio_id, m_print_enable?"helio_icon_topbar":"helio_icon_topbar_disable");
        expand_program_holder->EnableExpandButton(expand_helio_id, m_print_enable);
    }


    if (!old_slice_status && enable_slice)
        m_plater->stop_helio_process();
        m_plater->reset_check_status();

    update_prepare_action_bar_content();
}


void MainFrame::on_dpi_changed(const wxRect& suggested_rect)
{
    wxGetApp().update_fonts(this);
    this->SetFont(this->normal_font());

#ifdef _MSW_DARK_MODE
    // update common mode sizer
    if (!wxGetApp().tabs_as_menu())
        dynamic_cast<Notebook*>(m_tabpanel)->Rescale();
#endif

#ifndef __APPLE__
    // BBS
    m_topbar->Rescale();
#endif

    // BBS: session file-tabs — re-fetch fonts + re-layout for the new DPI.
    if (m_project_tabbar)
        m_project_tabbar->Rescale();

    m_tabpanel->Rescale();

    update_side_button_style();
    m_slice_btn->Rescale();
    m_print_btn->Rescale();
    m_slice_option_btn->Rescale();
    m_print_option_btn->Rescale();
    expand_program_holder->msw_rescale();
    if (m_prepare_expand_btn)
        m_prepare_expand_btn->Rescale();
    update_prepare_action_bar_style();

    // update Plater
    wxGetApp().plater()->msw_rescale();

    // update Tabs
    //BBS GUI refactor: remove unused layout new/dlg
    //if (m_layout != ESettingsLayout::Dlg) // Do not update tabs if the Settings are in the separated dialog
    m_param_panel->msw_rescale();
    m_project->msw_rescale();
    m_monitor->msw_rescale();
    if (m_multi_machine)
        m_multi_machine->msw_rescale();
    m_calibration->msw_rescale();
    if (m_web_device)
        m_web_device->msw_rescale();

    // BBS
#if 0
    for (size_t id = 0; id < m_menubar->GetMenuCount(); id++)
        msw_rescale_menu(m_menubar->GetMenu(id));
#endif

    // Workarounds for correct Window rendering after rescale

    /* Even if Window is maximized during moving,
     * first of all we should imitate Window resizing. So:
     * 1. cancel maximization, if it was set
     * 2. imitate resizing
     * 3. set maximization, if it was set
     */
    const bool is_maximized = this->IsMaximized();
    if (is_maximized)
        this->Maximize(false);

    /* To correct window rendering (especially redraw of a status bar)
     * we should imitate window resizing.
     */
    const wxSize& sz = this->GetSize();
    this->SetSize(sz.x + 1, sz.y + 1);
    this->SetSize(sz);

    this->Maximize(is_maximized);
}

void MainFrame::on_sys_color_changed()
{
    wxBusyCursor wait;

    // update label colors in respect to the system mode
    wxGetApp().init_label_colours();

#ifndef __WINDOWS__
    wxGetApp().force_colors_update();
    wxGetApp().update_ui_from_settings();
#endif //__APPLE__

#ifdef __WXMSW__
    wxGetApp().UpdateDarkUI(m_tabpanel);
    if (m_prepare_action_bar)
        wxGetApp().UpdateDarkUI(m_prepare_action_bar);
 //   m_statusbar->update_dark_ui();
#ifdef _MSW_DARK_MODE
    // update common mode sizer
    if (!wxGetApp().tabs_as_menu())
        dynamic_cast<Notebook*>(m_tabpanel)->Rescale();
#endif
#endif

    // Keep the frame's own client fill on the caption surface for the new
    // theme (see the ctor note: uncovered client regions must never fall back
    // to the light APPWORKSPACE grey).
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));

    // BBS
    if (m_topbar)
        m_topbar->Rescale();
    m_tabpanel->Rescale();
    update_prepare_action_bar_style();
    // Re-resolve the Slice/Print MD3 tokens for the new light/dark scheme; they
    // are captured by value, so without this they would keep the old-theme
    // colours until the next DPI change.
    if (m_slice_btn)
        update_side_button_style();
    m_param_panel->msw_rescale();

    // update Plater
    wxGetApp().plater()->sys_color_changed();
    m_monitor->on_sys_color_changed();
    m_calibration->on_sys_color_changed();
    if (m_web_device) m_web_device->on_sys_color_changed();
    // update Tabs
    for (auto tab : wxGetApp().tabs_list)
        tab->sys_color_changed();
    for (auto tab : wxGetApp().model_tabs_list)
        tab->sys_color_changed();
    wxGetApp().plate_tab->sys_color_changed();

    MenuFactory::sys_color_changed(m_menubar);

    // update DiffPresetDialog from here, we're friends
    diff_dialog.on_sys_color_changed();

    WebView::RecreateAll();

    this->Refresh();
}

#ifdef _MSC_VER
    // \xA0 is a non-breaking space. It is entered here to spoil the automatic accelerators,
    // as the simple numeric accelerators spoil all numeric data entry.
static const wxString sep = "\t\xA0";
static const wxString sep_space = "\xA0";
#else
static const wxString sep = " - ";
static const wxString sep_space = "";
#endif

static wxMenu* generate_help_menu()
{
    wxMenu* helpMenu = new wxMenu();
#ifdef __WINDOWS__
    // shortcut key
    auto alt = GUI::shortkey_alt_prefix();
    append_menu_item(helpMenu, wxID_ANY, _L("Keyboard Shortcuts") + sep + "& Shift+" + alt +"?", _L("Show the list of the keyboard shortcuts"),
        [](wxCommandEvent&) { wxGetApp().keyboard_shortcuts(); });
#else
    append_menu_item(helpMenu, wxID_ANY, _L("Keyboard Shortcuts") + sep + "& Shift+?", _L("Show the list of the keyboard shortcuts"),
                     [](wxCommandEvent &) { wxGetApp().keyboard_shortcuts(); });
#endif
    // Show Beginner's Tutorial
    append_menu_item(helpMenu, wxID_ANY, _L("Setup Wizard"), _L("Setup Wizard"), [](wxCommandEvent &) {wxGetApp().ShowUserGuide();});

    helpMenu->AppendSeparator();
    // Open Config Folder
    append_menu_item(helpMenu, wxID_ANY, _L("Show Configuration Folder"), _L("Show Configuration Folder"),
        [](wxCommandEvent&) { Slic3r::GUI::desktop_open_datadir_folder(); });

    append_menu_item(helpMenu, wxID_ANY, _L("Show Tip of the Day"), _L("Show Tip of the Day"), [](wxCommandEvent&) {
        wxGetApp().plater()->get_dailytips()->open();
        wxGetApp().plater()->get_current_canvas3D()->set_as_dirty();
        });

    append_menu_item(helpMenu, wxID_ANY, _L("Report issue"), _L("Report issue"), [](wxCommandEvent &) {
        std::string url = "https://github.com/bambulab/BambuStudio/issues/new/choose";
        wxLaunchDefaultBrowser(url);
    });

    // Report a bug
    //append_menu_item(helpMenu, wxID_ANY, _L("Report Bug(TODO)"), _L("Report a bug of BambuStudio"),
    //    [](wxCommandEvent&) {
    //        //TODO
    //    });
    // Check New Version
    append_menu_item(helpMenu, wxID_ANY, _L("Check for Update"), _L("Check for Update"),
        [](wxCommandEvent&) {
            wxGetApp().check_new_version(true, 1);
        }, "", nullptr, []() {
            return true;
        });

    append_menu_item(helpMenu, wxID_ANY, _L("Check for Presets Update"), _L("Check for Presets Update"), [](wxCommandEvent &) {
        wxGetApp().check_config_updates_from_menu();
    });

     append_menu_item(helpMenu, wxID_ANY, _L("Open Network Test"), _L("Open Network Test"), [](wxCommandEvent&) {
            NetworkTestDialog dlg(wxGetApp().mainframe);
            dlg.ShowModal();
        });

    // About
#ifndef __APPLE__
    wxString about_title = wxString::Format(_L("&About %s"), SLIC3R_APP_FULL_NAME);
    append_menu_item(helpMenu, wxID_ANY, about_title, about_title,
            [](wxCommandEvent&) { Slic3r::GUI::about(); });
#endif

    return helpMenu;
}


static void add_common_publish_menu_items(wxMenu* publish_menu, MainFrame* mainFrame)
{
#ifndef __WINDOWS__
    append_menu_item(publish_menu, wxID_ANY, _L("Upload Models"), _L("Upload Models"),
        [](wxCommandEvent&) {
            if (!wxGetApp().getAgent()) {
                BOOST_LOG_TRIVIAL(info) << "publish: no agent";
                return;
            }

            json j;
            NetworkAgent* agent = GUI::wxGetApp().getAgent();
            if (agent)
                agent->track_event("enter_model_mall", j.dump());

            //if (GUI::wxGetApp().plater()->model().objects.empty()) return;
            wxGetApp().open_publish_page_dialog();
        });

    append_menu_item(publish_menu, wxID_ANY, _L("Download Models"), _L("Download Models"),
        [](wxCommandEvent&) {
            if (!wxGetApp().getAgent()) {
                BOOST_LOG_TRIVIAL(info) << "publish: no agent";
                return;
}

            //if (GUI::wxGetApp().plater()->model().objects.empty()) return;
            wxGetApp().open_mall_page_dialog();
        });
#endif
}

static void add_common_view_menu_items(wxMenu* view_menu, MainFrame* mainFrame, std::function<bool(void)> can_change_view)
{
    // The camera control accelerators are captured by GLCanvas3D::on_char().
    append_menu_item(view_menu, wxID_ANY, _L("Default View") + "\t" + ctrl + "0", _L("Default View"), [mainFrame](wxCommandEvent&) {
        mainFrame->select_view("plate");
        mainFrame->plater()->get_current_canvas3D()->zoom_to_bed();
        },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    //view_menu->AppendSeparator();
    //TRN To be shown in the main menu View->Top
    append_menu_item(view_menu, wxID_ANY, _L("Top") + "\t" + ctrl + "1", _L("Top View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("top"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    //TRN To be shown in the main menu View->Bottom
    append_menu_item(view_menu, wxID_ANY, _L("Bottom") + "\t" + ctrl + "2", _L("Bottom View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("bottom"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _L("Front") + "\t" + ctrl + "3", _L("Front View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("front"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _L("Rear") + "\t" + ctrl + "4", _L("Rear View"), [mainFrame](wxCommandEvent&) { mainFrame->select_view("rear"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _CTX(L_CONTEXT("Left", "Camera"), "Camera") + "\t" + ctrl + "5", _L("Left View"),[mainFrame](wxCommandEvent &) {mainFrame->select_view("left"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _CTX(L_CONTEXT("Right", "Camera"), "Camera") + "\t" + ctrl + "6", _L("Right View"),[mainFrame](wxCommandEvent &) { mainFrame->select_view("right"); },
        "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _CTX(L_CONTEXT("Isometric", "Camera"), "Camera") + "\t" + ctrl + "7", _L("Isometric View"), [mainFrame](wxCommandEvent &) { mainFrame->select_view("iso"); }, "",
        nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
#if !BBL_RELEASE_TO_PUBLIC
    append_menu_item(view_menu, wxID_ANY, _CTX(L_CONTEXT("Isometric", "Camera"), "Camera") + " 1", _L("Isometric View") + " 1",
        [mainFrame](wxCommandEvent &) { mainFrame->select_view("iso_1"); }, "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _CTX(L_CONTEXT("Isometric", "Camera"), "Camera") + " 2", _L("Isometric View") + " 2",
        [mainFrame](wxCommandEvent &) { mainFrame->select_view("iso_2"); },"", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
    append_menu_item(view_menu, wxID_ANY, _CTX(L_CONTEXT("Isometric", "Camera"), "Camera") + " 3", _L("Isometric View") + " 3",
        [mainFrame](wxCommandEvent &) { mainFrame->select_view("iso_3"); }, "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
#endif
#ifdef __APPLE__//In the macOS menu, single-key shortcuts are not supported, so this option is hidden temporarily. The actual functionality still works normally.
    append_menu_item(view_menu, wxID_ANY, _L("Fit Camera"), _L("Fit camera to scene or selected object."),
        [mainFrame](wxCommandEvent &) { mainFrame->view_zoom_to_fit(); }, "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
#else
    append_menu_item(view_menu, wxID_ANY, _L("Fit Camera") + "\t" "Z", _L("Fit camera to scene or selected object."),
        [mainFrame](wxCommandEvent &) { mainFrame->view_zoom_to_fit(); }, "", nullptr, [can_change_view]() { return can_change_view(); }, mainFrame);
#endif
}

void MainFrame::init_menubar_as_editor()
{
#ifdef __APPLE__
    m_menubar = new wxMenuBar();
#endif

    // File menu
    wxMenu* fileMenu = new wxMenu;
    {
#ifdef __APPLE__
        // New Window
        append_menu_item(fileMenu, wxID_ANY, _L("New Window"), _L("Start a new window"),
                         [](wxCommandEvent&) { start_new_slicer(); }, "", nullptr,
            [this] { return m_plater != nullptr && wxGetApp().app_config->get("app", "single_instance") == "false"; }, this);
#endif
        // New Project
        // BBS: session file-tabs — open in a NEW tab instead of replacing in place.
        append_menu_item(fileMenu, wxID_ANY, _L("New Project") + "\t" + ctrl + "N", _L("Start a new project"),
            [this](wxCommandEvent&) { new_project_tab(); }, "", nullptr,
            [this](){return can_start_new_project(); }, this);
        // Open Project

#ifndef __APPLE__
        append_menu_item(fileMenu, wxID_ANY, _L("Open Project") + dots + "\t" + ctrl + "O", _L("Open a project file"),
            [this](wxCommandEvent&) { open_project_tab(); }, "menu_open", nullptr,
            [this](){return can_open_project(); }, this);
#else
        append_menu_item(fileMenu, wxID_ANY, _L("Open Project") + dots + "\t" + ctrl + "O", _L("Open a project file"),
            [this](wxCommandEvent&) { open_project_tab(); }, "", nullptr,
            [this](){return can_open_project(); }, this);
#endif

        // Recent Project
        wxMenu* recent_projects_menu = new wxMenu();
        wxMenuItem* recent_projects_submenu = append_submenu(fileMenu, recent_projects_menu, wxID_ANY, _L("Recent projects"), "");
        m_recent_projects.UseMenu(recent_projects_menu);
        Bind(wxEVT_MENU, [this](wxCommandEvent& evt) {
            size_t file_id = evt.GetId() - wxID_FILE1;
            wxString filename = m_recent_projects.GetHistoryFile(file_id);
                open_recent_project(file_id, filename);
            }, wxID_FILE1, wxID_FILE1 + 49); // [5050, 5100)

        std::vector<std::string> recent_projects = wxGetApp().app_config->get_recent_projects();
        std::reverse(recent_projects.begin(), recent_projects.end());
        for (const std::string& project : recent_projects)
        {
            m_recent_projects.AddFileToHistory(from_u8(project));
        }
        m_recent_projects.LoadThumbnails();

        Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& evt) { evt.Enable(can_open_project() && (m_recent_projects.GetCount() > 0)); }, recent_projects_submenu->GetId());

        // BBS: close save project
#ifndef __APPLE__
        append_menu_item(fileMenu, wxID_ANY, _L("Save Project") + "\t" + ctrl + "S", _L("Save current project to file"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(); }, "menu_save", nullptr,
            [this](){return m_plater != nullptr && can_save(); }, this);
#else
        append_menu_item(fileMenu, wxID_ANY, _L("Save Project") + "\t" + ctrl + "S", _L("Save current project to file"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(); }, "", nullptr,
            [this](){return m_plater != nullptr && can_save(); }, this);
#endif


#ifndef __APPLE__
        append_menu_item(fileMenu, wxID_ANY, _L("Save Project as") + dots + "\t" + ctrl + "Shift+" + "S", _L("Save current project as"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(true); }, "menu_save", nullptr,
            [this](){return m_plater != nullptr && can_save_as(); }, this);
#else
        append_menu_item(fileMenu, wxID_ANY, _L("Save Project as") + dots + "\t" + ctrl + "Shift+" + "S", _L("Save current project as"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->save_project(true); }, "", nullptr,
            [this](){return m_plater != nullptr && can_save_as(); }, this);
#endif


        append_menu_item(fileMenu, wxID_ANY, _L("Version history") + dots,
            _L("Browse and restore local Git-backed project versions"),
            [this](wxCommandEvent&) { show_project_history(); }, "", nullptr,
            [this](){return m_plater != nullptr; }, this);

        // Open the current project's folder in the configured external editor
        // (Preferences > General > External editor). "custom" routes to the
        // user-picked executable; any other value resolves through the
        // detected-editor table with a sensible default fallback.
        append_menu_item(fileMenu, wxID_ANY, _L("Open in External Editor"),
            _L("Open the current project's folder in the configured external editor"),
            [this](wxCommandEvent&) {
                if (!m_plater) return;
                wxString proj = m_plater->get_project_filename(".3mf");
                if (proj.IsEmpty()) return;
                boost::filesystem::path folder = boost::filesystem::path(into_u8(proj)).parent_path();
                AppConfig *cfg = wxGetApp().app_config;
                wxString editor_exe;
                if (cfg->get("external_editor") == "custom") {
                    // An explicit Custom choice must never silently fall back to
                    // an auto-detected editor: honor the picked path or warn.
                    editor_exe = from_u8(cfg->get("external_editor_path"));
                } else {
                    FoundEditor editor = find_editor_or_default(cfg->get("external_editor"));
                    editor_exe = from_u8(editor.exe_path);
                }
                if (editor_exe.IsEmpty()) {
                    if (m_plater->get_notification_manager())
                        m_plater->get_notification_manager()->push_notification(
                            NotificationType::CustomNotification,
                            NotificationManager::NotificationLevel::WarningNotificationLevel,
                            _u8L("No external editor is configured. Choose one in Preferences > General > External editor."));
                    return;
                }
                open_in_external_editor(editor_exe, from_u8(folder.string()));
            }, "", nullptr,
            [this](){return m_plater != nullptr && !m_plater->get_project_filename(".3mf").IsEmpty(); }, this);


        fileMenu->AppendSeparator();

        // BBS
        wxMenu *import_menu = new wxMenu();
        append_menu_item(import_menu, wxID_ANY, _L("Import 3MF/STL/STEP/SVG/OBJ/AMF") + dots + "\t" + ctrl + "I", _L("Load a model"),
            [this](wxCommandEvent&) { if (m_plater) {
            m_plater->add_file();
        } }, "menu_import", nullptr,
            [this](){return can_add_models(); }, this);
        append_menu_item(import_menu, wxID_ANY, _L("Import Configs") + dots /*+ "\tCtrl+I"*/, _L("Load configs"),
            [this](wxCommandEvent&) { load_config_file(); }, "menu_import", nullptr,
            [this](){return true; }, this);

        append_submenu(fileMenu, import_menu, wxID_ANY, _L("Import"), "");


        wxMenu* export_menu = new wxMenu();
        // BBS export as STL
        append_menu_item(export_menu, wxID_ANY, _L("Export all objects as one STL") + dots, _L("Export all objects as one STL"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_stl(); }, "menu_export_stl", nullptr,
            [this](){return can_export_model(); }, this);
        append_menu_item(export_menu, wxID_ANY, _L("Export all objects as STLs") + dots, _L("Export all objects as STLs"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_stl(false, false, true); }, "menu_export_stl", nullptr,
            [this](){return can_export_model(); }, this);
        append_menu_item(export_menu, wxID_ANY, _L("Export Generic 3MF") + dots/* + "\tCtrl+G"*/, _L("Export 3mf file without using some 3mf-extensions"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_core_3mf(); }, "menu_export_sliced_file", nullptr,
            [this](){return can_export_model(); }, this);
        // BBS export .gcode.3mf
        append_menu_item(export_menu, wxID_ANY, _L("Export plate sliced file") + dots + "\t" + ctrl + "G", _L("Export current sliced file"),
            [this](wxCommandEvent&) { if (m_plater) wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_SLICED_FILE)); }, "menu_export_sliced_file", nullptr,
            [this](){return can_export_gcode(); }, this);

        append_menu_item(export_menu, wxID_ANY, _L("Export all plate sliced file") + dots/* + "\tCtrl+G"*/, _L("Export all plate sliced file"),
            [this](wxCommandEvent&) { if (m_plater) wxPostEvent(m_plater, SimpleEvent(EVT_GLTOOLBAR_EXPORT_ALL_SLICED_FILE)); }, "menu_export_sliced_file", nullptr,
            [this]() {return can_export_all_gcode(); }, this);

        append_menu_item(export_menu, wxID_ANY, _L("Export G-code") + dots/* + "\tCtrl+G"*/, _L("Export current plate as G-code"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->export_gcode(false); }, "menu_export_gcode", nullptr,
            [this]() {return can_export_gcode(); }, this);

        append_menu_item(export_menu, wxID_ANY, _L("Export toolpaths as OBJ") + dots, _L("Export toolpaths as OBJ"),
            [this](wxCommandEvent&) { if (m_plater != nullptr) m_plater->export_toolpaths_to_obj(); }, "menu_export_toolpaths", nullptr,
            [this]() {return can_export_toolpaths(); }, this);

        append_menu_item(
            export_menu, wxID_ANY, _L("Export Preset Bundle") + dots /* + "\tCtrl+E"*/, _L("Export current configuration to files"),
            [this](wxCommandEvent &) { export_config(); },
            "menu_export_config", nullptr,
            []() { return true; }, this);

        append_submenu(fileMenu, export_menu, wxID_ANY, _L("Export"), "");

        // Publish to MakerWorld
        append_menu_item(
            fileMenu, wxID_ANY, _L("Publish to MakerWorld"), _L("Publish to MakerWorld"),
            [this](wxCommandEvent &) {
                CallAfter([this] {
                    wxGetApp().open_publish_page_dialog();

                    if (!wxGetApp().getAgent()) {
                        BOOST_LOG_TRIVIAL(info) << "publish: no agent";
                        return;
                    }

                    // record
                    json          j;
                    NetworkAgent *agent = GUI::wxGetApp().getAgent();
                    if (agent) agent->track_event("enter_model_mall", j.dump());
                });
            },
            "", nullptr,
            [this](){ return wxGetApp().has_model_mall(); }, this);

        append_menu_item(
            fileMenu, wxID_ANY, _L("Batch Preset Management"), wxString::Format(_L("Batch Preset Management")),
            [this](wxCommandEvent &) {
                UserPresetsDialog dlg(this);
                dlg.ShowModal();
            },
            "", nullptr);

        fileMenu->AppendSeparator();

#ifndef __APPLE__
        append_menu_item(fileMenu, wxID_EXIT, _L("Quit"), wxString::Format(_L("Quit")),
            [this](wxCommandEvent&) { Close(false); }, "menu_exit", nullptr);
#else
        append_menu_item(fileMenu, wxID_EXIT, _L("Quit"), wxString::Format(_L("Quit")),
            [this](wxCommandEvent&) { Close(false); }, "", nullptr);
#endif
    }

    // Edit menu
    wxMenu* editMenu = nullptr;
    if (m_plater != nullptr)
    {
        editMenu = new wxMenu();
    #ifdef __APPLE__
        // Backspace sign
        wxString hotkey_delete = "\u232b";
    #else
        wxString hotkey_delete = "Del";
    #endif

    auto handle_key_event = [](wxKeyEvent& evt) {
        if (wxGetApp().imgui()->update_key_data(evt)) {
            wxGetApp().plater()->get_current_canvas3D()->render();
            return true;
        }
        return false;
    };
#ifndef __APPLE__
        // BBS undo
        append_menu_item(editMenu, wxID_ANY, _L("Undo") + "\t" + ctrl + "Z",
            _L("Undo"), [this](wxCommandEvent&) { m_plater->undo(); },
            "menu_undo", nullptr, [this](){return m_plater->can_undo(); }, this);
        // BBS redo
        append_menu_item(editMenu, wxID_ANY, _L("Redo") + "\t" + ctrl + "Y",
            _L("Redo"), [this](wxCommandEvent&) { m_plater->redo(); },
            "menu_redo", nullptr, [this](){return m_plater->can_redo(); }, this);
        editMenu->AppendSeparator();
        // BBS Cut TODO
        append_menu_item(editMenu, wxID_ANY, _L("Cut") + "\t" + ctrl + "X",
            _L("Cut selection to clipboard"), [this](wxCommandEvent&) {m_plater->cut_selection_to_clipboard(); },
            "menu_cut", nullptr, [this]() {return m_plater->can_copy_to_clipboard(); }, this);
        // BBS Copy
        append_menu_item(editMenu, wxID_ANY, _L("Copy") + "\t" + ctrl + "C",
            _L("Copy selection to clipboard"), [this](wxCommandEvent&) { m_plater->copy_selection_to_clipboard(); },
            "menu_copy", nullptr, [this](){return m_plater->can_copy_to_clipboard(); }, this);
        // BBS Paste
        append_menu_item(editMenu, wxID_ANY, _L("Paste") + "\t" + ctrl + "V",
            _L("Paste clipboard"), [this](wxCommandEvent&) { m_plater->paste_from_clipboard(); },
            "menu_paste", nullptr, [this](){return m_plater->can_paste_from_clipboard(); }, this);
        // BBS Delete selected
        append_menu_item(editMenu, wxID_ANY, _L("Delete selected") + "\tDelete",
            _L("Deletes the current selection"),[this](wxCommandEvent&) { m_plater->remove_selected(); },
            "menu_remove", nullptr, [this](){return can_delete(); }, this);
        //BBS: delete all
        append_menu_item(editMenu, wxID_ANY, _L("Delete all") + "\t" + ctrl + "Shift+" + "D",
            _L("Deletes all objects"),[this](wxCommandEvent&) { m_plater->delete_all_objects_from_model(); },
            "menu_remove", nullptr, [this](){return can_delete_all(); }, this);
        editMenu->AppendSeparator();
        // BBS Clone Selected
        append_menu_item(editMenu, wxID_ANY, _L("Clone selected") /*+ "\tCtrl+M"*/,
            _L("Clone copies of selections"),[this](wxCommandEvent&) {
                m_plater->clone_selection();
            },
            "menu_remove", nullptr, [this](){return can_clone(); }, this);
        editMenu->AppendSeparator();
#else
        // BBS undo
        append_menu_item(editMenu, wxID_ANY, _L("Undo") + "\t" + ctrl + "Z",
            _L("Undo"), [this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.SetControlDown(true);
                e.m_keyCode = 'Z';
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->undo(); },
            "", nullptr, [this](){return m_plater->can_undo(); }, this);
        // BBS redo
        append_menu_item(editMenu, wxID_ANY, _L("Redo") + "\t" + ctrl + "Y",
            _L("Redo"), [this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.SetControlDown(true);
                e.m_keyCode = 'Y';
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->redo(); },
            "", nullptr, [this](){return m_plater->can_redo(); }, this);
        editMenu->AppendSeparator();
        // BBS Cut TODO
        append_menu_item(editMenu, wxID_ANY, _L("Cut") + "\t" + ctrl + "X",
            _L("Cut selection to clipboard"), [this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.SetControlDown(true);
                e.m_keyCode = 'X';
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->cut_selection_to_clipboard(); },
            "", nullptr, [this]() {return m_plater->can_copy_to_clipboard(); }, this);
        // BBS Copy
        append_menu_item(editMenu, wxID_ANY, _L("Copy") + "\t" + ctrl + "C",
            _L("Copy selection to clipboard"), [this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.SetControlDown(true);
                e.m_keyCode = 'C';
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->copy_selection_to_clipboard(); },
            "", nullptr, [this](){return m_plater->can_copy_to_clipboard(); }, this);
        // BBS Paste
        append_menu_item(editMenu, wxID_ANY, _L("Paste") + "\t" + ctrl + "V",
            _L("Paste clipboard"), [this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.SetControlDown(true);
                e.m_keyCode = 'V';
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->paste_from_clipboard(); },
            "", nullptr, [this](){return m_plater->can_paste_from_clipboard(); }, this);
#if 0
        // BBS Delete selected
        append_menu_item(editMenu, wxID_ANY, _L("Delete selected") + "\tBackSpace",
            _L("Deletes the current selection"),[this](wxCommandEvent&) {
                m_plater->remove_selected();
            },
            "", nullptr, [this](){return can_delete(); }, this);
#endif
        //BBS: delete all
        append_menu_item(editMenu, wxID_ANY, _L("Delete all") + "\t" + ctrl + "Shift+" + "D",
            _L("Deletes all objects"),[this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.SetShiftDown(true);
                e.SetControlDown(true);
                e.m_keyCode = 'D';
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->delete_all_objects_from_model(); },
            "", nullptr, [this](){return can_delete_all(); }, this);
        editMenu->AppendSeparator();
        // BBS Clone Selected
        append_menu_item(editMenu, wxID_ANY, _L("Clone selected") + "\t" + ctrl + "K",
            _L("Clone copies of selections"),[this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.SetControlDown(true);
                e.m_keyCode = 'M';
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->clone_selection();
            },
            "", nullptr, [this](){return can_clone(); }, this);
        editMenu->AppendSeparator();
#endif

        // BBS Select All
        append_menu_item(editMenu, wxID_ANY, _L("Select all") + "\t" + ctrl + "A",
            _L("Selects all objects"), [this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.SetControlDown(true);
                e.m_keyCode = 'A';
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->select_all(); },
            "", nullptr, [this](){return can_select(); }, this);
        // BBS Deslect All
        append_menu_item(editMenu, wxID_ANY, _L("Deselect all") + "\tEsc",
            _L("Deselects all objects"), [this, handle_key_event](wxCommandEvent&) {
                wxKeyEvent e;
                e.SetEventType(wxEVT_KEY_DOWN);
                e.m_keyCode = WXK_ESCAPE;
                if (handle_key_event(e)) {
                    return;
                }
                m_plater->deselect_all(); },
            "", nullptr, [this](){return can_deselect(); }, this);
        //editMenu->AppendSeparator();
        //append_menu_check_item(editMenu, wxID_ANY, _L("Show Model Mesh(TODO)"),
        //    _L("Display triangles of models"), [this](wxCommandEvent& evt) {
        //        wxGetApp().app_config->set_bool("show_model_mesh", evt.GetInt() == 1);
        //    }, nullptr, [this]() {return can_select(); }, [this]() { return wxGetApp().app_config->get("show_model_mesh").compare("true") == 0; }, this);
        //append_menu_check_item(editMenu, wxID_ANY, _L("Show Model Shadow(TODO)"), _L("Display shadow of objects"),
        //    [this](wxCommandEvent& evt) {
        //        wxGetApp().app_config->set_bool("show_model_shadow", evt.GetInt() == 1);
        //    }, nullptr, [this]() {return can_select(); }, [this]() { return wxGetApp().app_config->get("show_model_shadow").compare("true") == 0; }, this);
        //editMenu->AppendSeparator();
        //append_menu_check_item(editMenu, wxID_ANY, _L("Show Printable Box(TODO)"), _L("Display printable box"),
        //    [this](wxCommandEvent& evt) {
        //        wxGetApp().app_config->set_bool("show_printable_box", evt.GetInt() == 1);
        //    }, nullptr, [this]() {return can_select(); }, [this]() { return wxGetApp().app_config->get("show_printable_box").compare("true") == 0; }, this);
    }

    // BBS

    // Object-specific commands are also surfaced as their own top-level menu in
    // the Material title bar.  Keep the established Plater actions and update-UI
    // predicates so keyboard shortcuts and selection state behave exactly as in
    // the existing Edit menu.
    wxMenu* objectsMenu = nullptr;
    if (m_plater != nullptr) {
        objectsMenu = new wxMenu();
        append_menu_item(objectsMenu, wxID_ANY, _L("Add model") + dots, _L("Load a model"),
            [this](wxCommandEvent&) { m_plater->add_file(); }, "menu_import", nullptr,
            [this]() { return can_add_models(); }, this);
        objectsMenu->AppendSeparator();
        append_menu_item(objectsMenu, wxID_ANY, _L("Clone selected"), _L("Clone copies of selections"),
            [this](wxCommandEvent&) { m_plater->clone_selection(); }, "menu_copy", nullptr,
            [this]() { return can_clone(); }, this);
        append_menu_item(objectsMenu, wxID_ANY, _L("Delete selected"), _L("Deletes the current selection"),
            [this](wxCommandEvent&) { m_plater->remove_selected(); }, "menu_remove", nullptr,
            [this]() { return can_delete(); }, this);
        append_menu_item(objectsMenu, wxID_ANY, _L("Delete all"), _L("Deletes all objects"),
            [this](wxCommandEvent&) { m_plater->delete_all_objects_from_model(); }, "menu_remove", nullptr,
            [this]() { return can_delete_all(); }, this);
        objectsMenu->AppendSeparator();
        append_menu_item(objectsMenu, wxID_ANY, _L("Select all"), _L("Selects all objects"),
            [this](wxCommandEvent&) { m_plater->select_all(); }, "", nullptr,
            [this]() { return can_select(); }, this);
        append_menu_item(objectsMenu, wxID_ANY, _L("Deselect all"), _L("Deselects all objects"),
            [this](wxCommandEvent&) { m_plater->deselect_all(); }, "", nullptr,
            [this]() { return can_deselect(); }, this);
    }

    //publish menu

    /*if (m_plater) {
        publishMenu = new wxMenu();
        add_common_publish_menu_items(publishMenu, this);
        publishMenu->AppendSeparator();
    }*/

    // View menu
    wxMenu* viewMenu = nullptr;
    if (m_plater) {
        viewMenu = new wxMenu();
        add_common_view_menu_items(viewMenu, this, std::bind(&MainFrame::can_change_view, this));
        viewMenu->AppendSeparator();

        wxMenuItem *camera_fullscreen_item = append_menu_item(
            viewMenu, wxID_ANY, _L("Enter Camera Full Screen"), _L("Show the Device camera in full screen"),
            [this](wxCommandEvent &) { toggle_camera_fullscreen(); }, "", nullptr);
        Bind(wxEVT_UPDATE_UI, [this, camera_fullscreen_item](wxUpdateUIEvent &evt) {
            StatusPanel *status_panel = m_monitor ? m_monitor->get_status_panel() : nullptr;
            const bool active = status_panel && status_panel->is_camera_fullscreen();
            camera_fullscreen_item->SetItemLabel(active ? _L("Exit Camera Full Screen") : _L("Enter Camera Full Screen"));
            evt.Enable(can_toggle_camera_fullscreen());
        }, camera_fullscreen_item->GetId());
        viewMenu->AppendSeparator();

        //BBS perspective view
        wxWindowID camera_id_base = wxWindow::NewControlId(int(wxID_CAMERA_COUNT));
        auto perspective_item = append_menu_radio_item(viewMenu, wxID_CAMERA_PERSPECTIVE + camera_id_base, _L("Use Perspective View"), _L("Use Perspective View"),
            [this](wxCommandEvent&) {
                wxGetApp().app_config->set_bool("use_perspective_camera", true);
                wxGetApp().update_ui_from_settings();
            }, nullptr);
        //BBS orthogonal view
        auto orthogonal_item = append_menu_radio_item(viewMenu, wxID_CAMERA_ORTHOGONAL + camera_id_base, _L("Use Orthogonal View"), _L("Use Orthogonal View"),
            [this](wxCommandEvent&) {
                wxGetApp().app_config->set_bool("use_perspective_camera", false);
                wxGetApp().update_ui_from_settings();
            }, nullptr);
        if (wxGetApp().app_config->get("use_perspective_camera").compare("true") == 0)
            viewMenu->Check(wxID_CAMERA_PERSPECTIVE + camera_id_base, true);
        else
            viewMenu->Check(wxID_CAMERA_ORTHOGONAL + camera_id_base, true);
        viewMenu->AppendSeparator();
        append_menu_check_item(
            viewMenu, wxID_ANY, _L("Show 3D Navigator"), _L("Show 3D navigator in Prepare and Preview scene"),
            [this](wxCommandEvent &) {
                wxGetApp().toggle_show_3d_navigator();
                m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT));
            },
            this, [this]() { return m_tabpanel->GetSelection() == TabPosition::tp3DEditor || m_tabpanel->GetSelection() == TabPosition::tpPreview; },
            [this]() { return wxGetApp().show_3d_navigator(); }, this);
        append_menu_item(
            viewMenu, wxID_ANY, _L("Reset Window Layout") + "\t" + ctrl + "W", _L("Reset to default window layout"),
            [this](wxCommandEvent &) { m_plater->reset_window_layout(); }, "", this,
            [this]() { return (m_tabpanel->GetSelection() == TabPosition::tp3DEditor || m_tabpanel->GetSelection() == TabPosition::tpPreview) && m_plater->is_sidebar_enabled(); },
            this);
        viewMenu->AppendSeparator();
        append_menu_check_item(viewMenu, wxID_ANY, _L("Show Labels by Layer") + "\t" + ctrl + "E", _L("Show Labels of printing by layer in 3D scene"),
            [this](wxCommandEvent&) { m_plater->show_view3D_layer_labels(!m_plater->are_view3D_layer_labels_shown()); m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT)); }, this,
            [this]() { return m_plater->is_view3D_shown(); }, [this]() { return m_plater->are_view3D_layer_labels_shown(); }, this);
        append_menu_check_item(viewMenu, wxID_ANY, _L("Show Labels by Object") + "\t" + ctrl + "Shift+" + "E", _L("Show Labels of printing by object in 3D scene"),
            [this](wxCommandEvent&) { m_plater->show_view3D_object_labels(!m_plater->are_view3D_object_labels_shown()); m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT)); }, this,
            [this]() { return m_plater->is_view3D_shown(); }, [this]() { return m_plater->are_view3D_object_labels_shown(); }, this);

        append_menu_check_item(viewMenu, wxID_ANY, _L("Show &Overhang") + "\t" + ctrl + "L", _L("Show object overhang highlight in 3D scene"),
            [this](wxCommandEvent &) {
                m_plater->show_view3D_overhang(!m_plater->is_view3D_overhang_shown());
                m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT));
            },
            this, [this]() { return m_plater->is_view3D_shown(); }, [this]() { return m_plater->is_view3D_overhang_shown(); }, this);
        viewMenu->AppendSeparator();
        append_menu_item(
            viewMenu, wxID_ANY, _L("Set 3DConnexion"), _L("Set 3DConnexion mouse"),
            [this](wxCommandEvent &) {
#ifdef _WIN32
                if (wxGetApp().app_config->get("use_legacy_3DConnexion") == "true") {
#endif //_WIN32
                    Mouse3DController &controller = wxGetApp().plater()->get_mouse3d_controller();
                    controller.show_settings_dialog(!controller.is_settings_dialog_shown());
#ifdef _WIN32
                }
#endif //_WIN32
            },  "", nullptr, [this]() {
                Mouse3DController &controller = wxGetApp().plater()->get_mouse3d_controller();
                auto               tab_index  = (MainFrame::TabPosition) dynamic_cast<Notebook *>(wxGetApp().tab_panel())->GetSelection();
                auto is_3d_view = tab_index == MainFrame::TabPosition::tp3DEditor || tab_index == MainFrame::TabPosition::tpPreview;
                return is_3d_view && controller.connected();
            },
            this);
        /*viewMenu->AppendSeparator();
        append_menu_check_item(viewMenu, wxID_ANY, _L("Show &Wireframe") + "\tCtrl+Shift+Enter", _L("Show wireframes in 3D scene"),
            [this](wxCommandEvent&) { m_plater->toggle_show_wireframe(); m_plater->get_current_canvas3D()->post_event(SimpleEvent(wxEVT_PAINT)); }, this,
            [this]() { return m_plater->is_wireframe_enabled(); }, [this]() { return m_plater->is_show_wireframe(); }, this);*/

        //viewMenu->AppendSeparator();
        ////BBS orthogonal view
        //append_menu_check_item(viewMenu, wxID_ANY, _L("Show Edges(TODO)"), _L("Show Edges"),
        //    [this](wxCommandEvent& evt) {
        //        wxGetApp().app_config->set("show_build_edges", evt.GetInt() == 1 ? "true" : "false");
        //    }, nullptr, [this]() {return can_select(); }, [this]() {
        //        std::string show_build_edges = wxGetApp().app_config->get("show_build_edges");
        //        return show_build_edges.compare("true") == 0;
        //    }, this);
    }

    //TODO remove
    //auto config_wizard_name = _(ConfigWizard::name(true) + "(Debug)");
    //const auto config_wizard_tooltip = from_u8((boost::format(_utf8(L("Run %s"))) % config_wizard_name).str());
    //auto config_item = new wxMenuItem(m_topbar->GetTopMenu(), ConfigMenuWizard + config_id_base, config_wizard_name, config_wizard_tooltip);
#ifdef __APPLE__
    wxWindowID bambu_studio_id_base = wxWindow::NewControlId(int(2));
    wxMenu* parent_menu = m_menubar->OSXGetAppleMenu();
    //auto preference_item = new wxMenuItem(parent_menu, BambuStudioMenuPreferences + bambu_studio_id_base, _L("Preferences") + "\tCtrl+,", "");

        std::string app_items[] = {
            L("Services"),
            L("Hide BambuStudio"),
            L("Hide Others"),
            L("Show All"),
            L("Quit BambuStudio")
        };
#endif
    //auto printer_item = new wxMenuItem(parent_menu, ConfigMenuPrinter + config_id_base, _L("Printer"), "");
    //auto language_item = new wxMenuItem(parent_menu, ConfigMenuLanguage + config_id_base, _L("Switch Language"), "");
//    parent_menu->Bind(wxEVT_MENU, [this, config_id_base](wxEvent& event) {
//        switch (event.GetId() - config_id_base) {
//        //case ConfigMenuLanguage:
//        //{
//        //    /* Before change application language, let's check unsaved changes on 3D-Scene
//        //     * and draw user's attention to the application restarting after a language change
//        //     */
//        //    {
//        //        // the dialog needs to be destroyed before the call to switch_language()
//        //        // or sometimes the application crashes into wxDialogBase() destructor
//        //        // so we put it into an inner scope
//        //        wxString title = _L("Language selection");
//        //        wxMessageDialog dialog(nullptr,
//        //            _L("Switching the language requires application restart.\n") + "\n\n" +
//        //            _L("Do you want to continue?"),
//        //            title,
//        //            wxICON_QUESTION | wxOK | wxCANCEL);
//        //        if (dialog.ShowModal() == wxID_CANCEL)
//        //            return;
//        //    }
//
//        //    wxGetApp().switch_language();
//        //    break;
//        //}
//        //case ConfigMenuWizard:
//        //{
//        //    wxGetApp().run_wizard(ConfigWizard::RR_USER);
//        //    break;
//        //}
//        case ConfigMenuPrinter:
//        {
//            wxGetApp().params_dialog()->Popup();
//            wxGetApp().get_tab(Preset::TYPE_PRINTER)->restore_last_select_item();
//            break;
//        }
//        case ConfigMenuPreferences:
//        {
//            CallAfter([this] {
//                PreferencesDialog dlg(this);
//                dlg.ShowModal();
//#if ENABLE_GCODE_LINES_ID_IN_H_SLIDER
//                if (dlg.seq_top_layer_only_changed() || dlg.seq_seq_top_gcode_indices_changed())
//#else
//                if (dlg.seq_top_layer_only_changed())
//#endif // ENABLE_GCODE_LINES_ID_IN_H_SLIDER
//                    plater()->refresh_print();
//#if ENABLE_CUSTOMIZABLE_FILES_ASSOCIATION_ON_WIN
//#ifdef _WIN32
//                /*
//                if (wxGetApp().app_config()->get("associate_3mf") == "true")
//                    wxGetApp().associate_3mf_files();
//                if (wxGetApp().app_config()->get("associate_stl") == "true")
//                    wxGetApp().associate_stl_files();
//                /*if (wxGetApp().app_config()->get("associate_step") == "true")
//                    wxGetApp().associate_step_files();*/
//#endif // _WIN32
//#endif
//            });
//            break;
//        }
//        default:
//            break;
//        }
//    });

#ifdef __APPLE__
    wxString about_title = wxString::Format(_L("&About %s"), SLIC3R_APP_FULL_NAME);
    //auto about_item = new wxMenuItem(parent_menu, BambuStudioMenuAbout + bambu_studio_id_base, about_title, "");
        //parent_menu->Bind(wxEVT_MENU, [this, bambu_studio_id_base](wxEvent& event) {
        //    switch (event.GetId() - bambu_studio_id_base) {
        //        case BambuStudioMenuAbout:
        //            Slic3r::GUI::about();
        //            break;
        //        case BambuStudioMenuPreferences:
        //            CallAfter([this] {
        //                PreferencesDialog dlg(this);
        //                dlg.ShowModal();
        //#if ENABLE_GCODE_LINES_ID_IN_H_SLIDER
        //                if (dlg.seq_top_layer_only_changed() || dlg.seq_seq_top_gcode_indices_changed())
        //#else
        //                if (dlg.seq_top_layer_only_changed())
        //#endif // ENABLE_GCODE_LINES_ID_IN_H_SLIDER
        //                    plater()->refresh_print();
        //            });
        //            break;
        //        default:
        //            break;
        //    }
        //});
    //parent_menu->Insert(0, about_item);
    append_menu_item(
        parent_menu, wxID_ANY, _L(about_title), "",
        [this](wxCommandEvent &) { Slic3r::GUI::about();},
        "", nullptr, []() { return true; }, this, 0);
    append_menu_item(
        parent_menu, wxID_ANY, _L("Preferences") + "\t" + ctrl + ",", "",
        [this](wxCommandEvent &) {
            PreferencesDialog dlg(this);
            dlg.ShowModal();
#if ENABLE_GCODE_LINES_ID_IN_H_SLIDER
            if (dlg.seq_top_layer_only_changed() || dlg.seq_seq_top_gcode_indices_changed())
#else
            if (dlg.seq_top_layer_only_changed())
#endif
                plater()->refresh_print();

            // Refresh recent list if time format changed
            if (dlg.use_12h_time_format_changed() && m_webview) {
                wxGetApp().CallAfter([this]() {
                    if (m_webview) {
                        m_webview->SendRecentList(-1);
                    }
                });
            }
        },
        "", nullptr, []() { return true; }, this, 1);
    //parent_menu->Insert(1, preference_item);
#endif
    // Help menu
    auto helpMenu = generate_help_menu();

#ifndef __APPLE__
    // Preferences remains an ordinary menu item; only the menu presentation
    // moved from the legacy catch-all dropdown to the top-level Edit control.
    if (editMenu) {
        editMenu->AppendSeparator();
        append_menu_item(
            editMenu, wxID_ANY, _L("Preferences") + "\t" + ctrl + "P", "",
            [this](wxCommandEvent &) {
                PreferencesDialog dlg(this);
                dlg.ShowModal();
#if ENABLE_GCODE_LINES_ID_IN_H_SLIDER
                if (dlg.seq_top_layer_only_changed() || dlg.seq_seq_top_gcode_indices_changed())
#else
                if (dlg.seq_top_layer_only_changed())
#endif
                    plater()->refresh_print();

                // Refresh recent list if time format changed.
                if (dlg.use_12h_time_format_changed() && m_webview) {
                    wxGetApp().CallAfter([this]() {
                        if (m_webview)
                            m_webview->SendRecentList(-1);
                    });
                }
            },
            "", nullptr, []() { return true; }, this);
    }

    m_topbar->SetTopMenus(fileMenu, editMenu, viewMenu, objectsMenu, helpMenu);

     // OrcaSlicer calibrations
    {
        append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Temperature"), _L("Temperature Calibration"),
            [this](wxCommandEvent&) {
                if (!m_temp_calib_dlg)
                    m_temp_calib_dlg = new Temp_Calibration_Dlg((wxWindow*)this, wxID_ANY, m_plater);
                m_temp_calib_dlg->ShowModal();
            }, "", nullptr,
            [this]() {return m_plater->is_view3D_shown();; }, this);
        auto flowrate_menu = new wxMenu();
        append_menu_item(
            flowrate_menu, wxID_ANY, _L("Coarse"), _L("Flow rate test - Coarse"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(1); }, "", nullptr,
            [this]() {return m_plater->is_view3D_shown();; }, this);
        append_menu_item(flowrate_menu, wxID_ANY, _L("Fine"), _L("Flow rate test - Fine"),
            [this](wxCommandEvent&) { if (m_plater) m_plater->calib_flowrate(2); }, "", nullptr,
            [this]() {return m_plater->is_view3D_shown();; }, this);
        m_topbar->GetCalibMenu()->AppendSubMenu(flowrate_menu, _L("Flow rate"));
        append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Pressure advance"), _L("Pressure advance"),
            [this](wxCommandEvent&) {
                //if (!m_pa_calib_dlg)
                m_pa_calib_dlg = new PA_Calibration_Dlg((wxWindow*)this, wxID_ANY, m_plater);
                m_pa_calib_dlg->ShowModal();
            }, "", nullptr,
            [this]() {return m_plater->is_view3D_shown();; }, this);
        append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Retraction test"), _L("Retraction test"),
            [this](wxCommandEvent&) {
                if (!m_retraction_calib_dlg)
                    m_retraction_calib_dlg = new Retraction_Test_Dlg((wxWindow*)this, wxID_ANY, m_plater);
                m_retraction_calib_dlg->ShowModal();
            }, "", nullptr,
            [this]() {return m_plater->is_view3D_shown();; }, this);

        // Advance calibrations
        auto advance_menu = new wxMenu(); // sub menu
        {
            append_menu_item(
                advance_menu, wxID_ANY, _L("Max flowrate"), _L("Max flowrate"),
                [this](wxCommandEvent &) {
                    if (!m_vol_test_dlg) m_vol_test_dlg = new MaxVolumetricSpeed_Test_Dlg((wxWindow *) this, wxID_ANY, m_plater);
                    m_vol_test_dlg->ShowModal();
                },
                "", nullptr,
                [this]() {
                    return m_plater->is_view3D_shown();
                    ;
                },
                this);

            append_menu_item(
                advance_menu, wxID_ANY, _L("VFA"), _L("VFA"),
                [this](wxCommandEvent &) {
                    if (!m_vfa_test_dlg) m_vfa_test_dlg = new VFA_Test_Dlg((wxWindow *) this, wxID_ANY, m_plater);
                    m_vfa_test_dlg->ShowModal();
                },
                "", nullptr,
                [this]() {
                    return m_plater->is_view3D_shown();
                    ;
                },
                this);
            m_topbar->GetCalibMenu()->AppendSubMenu(advance_menu, _L("More..."));
        }

        // help
        append_menu_item(m_topbar->GetCalibMenu(), wxID_ANY, _L("Tutorial"), _L("Calibration help"),
            [this](wxCommandEvent&) {
                try {
                    json js;
                    js["cali_type"] = "third_cali_tutorial";
                    NetworkAgent *agent   = GUI::wxGetApp().getAgent();
                    if (agent) agent->track_event("third_cali", js.dump());
                } catch (...) {}
                wxLaunchDefaultBrowser("https://wiki.bambulab.com/en/bambu-studio/Calibration", wxBROWSER_NEW_WINDOW);
            }, "", nullptr,
            [this]() {return m_plater->is_view3D_shown();; }, this);

    }
#else
    m_menubar->Append(fileMenu, wxString::Format("&%s", _L("File")));
    if (editMenu)
        m_menubar->Append(editMenu, wxString::Format("&%s", _L("Edit")));
    if (viewMenu)
        m_menubar->Append(viewMenu, wxString::Format("&%s", _L("View")));
    if (objectsMenu)
        m_menubar->Append(objectsMenu, wxString::Format("&%s", _L("Objects")));
    //if (publishMenu)
    //    m_menubar->Append(publishMenu, wxString::Format("&%s", _L("3D Models")));

        // SoftFever calibrations
    m_calib_menu = new wxMenu();

    // Temp
    append_menu_item(
        m_calib_menu, wxID_ANY, _L("Temperature"), _L("Temperature"),
        [this](wxCommandEvent &) {
            if (!m_temp_calib_dlg)
                m_temp_calib_dlg = new Temp_Calibration_Dlg((wxWindow *) this, wxID_ANY, m_plater);
            m_temp_calib_dlg->ShowModal();
        },
        "", nullptr,
        [this]() {
            return m_plater->is_view3D_shown();
            ;
        },
        this);

    // Flowrate
    auto flowrate_menu = new wxMenu();
    append_menu_item(
        flowrate_menu, wxID_ANY, _L("Coarse"), _L("Flow rate test - Coarse"),
        [this](wxCommandEvent &) {
            if (m_plater) m_plater->calib_flowrate(1);
        },
        "", nullptr,
        [this]() {
            return m_plater->is_view3D_shown();
            ;
        },
        this);
    append_menu_item(
        flowrate_menu, wxID_ANY, _L("Fine"), _L("Flow rate test - Fine"),
        [this](wxCommandEvent &) {
            if (m_plater) m_plater->calib_flowrate(2);
        },
        "", nullptr,
        [this]() {
            return m_plater->is_view3D_shown();
            ;
        },
        this);
    append_submenu(m_calib_menu, flowrate_menu, wxID_ANY, _L("Flow rate"), _L("Flow rate"), "", [this]() {
        return m_plater->is_view3D_shown();
        ;
    });

    // PA
    append_menu_item(
        m_calib_menu, wxID_ANY, _L("Pressure advance"), _L("Pressure advance"),
        [this](wxCommandEvent &) {
            m_pa_calib_dlg = new PA_Calibration_Dlg((wxWindow *) this, wxID_ANY, m_plater);
            m_pa_calib_dlg->ShowModal();
        },
        "", nullptr,
        [this]() {
            return m_plater->is_view3D_shown();
            ;
        },
        this);

    // Retraction
    append_menu_item(
        m_calib_menu, wxID_ANY, _L("Retraction test"), _L("Retraction test"),
        [this](wxCommandEvent &) {
            if (!m_retraction_calib_dlg) m_retraction_calib_dlg = new Retraction_Test_Dlg((wxWindow *) this, wxID_ANY, m_plater);
            m_retraction_calib_dlg->ShowModal();
        },
        "", nullptr,
        [this]() {
            return m_plater->is_view3D_shown();
            ;
        },
        this);

    // Advance calibrations
    auto advance_menu = new wxMenu();
    append_menu_item(
        advance_menu, wxID_ANY, _L("Max flowrate"), _L("Max flowrate"),
        [this](wxCommandEvent &) {
            if (!m_vol_test_dlg) m_vol_test_dlg = new MaxVolumetricSpeed_Test_Dlg((wxWindow *) this, wxID_ANY, m_plater);
            m_vol_test_dlg->ShowModal();
        },
        "", nullptr,
        [this]() {
            return m_plater->is_view3D_shown();
            ;
        },
        this);
    append_menu_item(
        advance_menu, wxID_ANY, _L("VFA"), _L("VFA"),
        [this](wxCommandEvent &) {
            if (!m_vfa_test_dlg) m_vfa_test_dlg = new VFA_Test_Dlg((wxWindow *) this, wxID_ANY, m_plater);
            m_vfa_test_dlg->ShowModal();
        },
        "", nullptr,
        [this]() {
            return m_plater->is_view3D_shown();
            ;
        },
        this);

    append_submenu(m_calib_menu, advance_menu, wxID_ANY, _L("More..."), _L("More calibrations"), "", [this]() {
        return m_plater->is_view3D_shown();
        ;
    });
    // help
    append_menu_item(
        m_calib_menu, wxID_ANY, _L("Tutorial"), _L("Calibration help"),
        [this](wxCommandEvent &) {
            try {
                json js;
                js["cali_type"] = "third_cali_tutorial";
                NetworkAgent *agent = GUI::wxGetApp().getAgent();
                if (agent) agent->track_event("third_cali", js.dump());
            } catch (...) {}
            wxLaunchDefaultBrowser("https://wiki.bambulab.com/en/bambu-studio/Calibration", wxBROWSER_NEW_WINDOW);
        }, "", nullptr,
        [this]() {
            return m_plater->is_view3D_shown();
            ;
        },
        this);

    m_menubar->Append(new wxMenu(), L("Window"));
    std::string window_items[] = {
        L("Minimize"),
        L("Zoom"),
        L("Tile Window to Left of Screen"),
        L("Tile Window to Right of Screen"),
        L("Replace Tiled Window"),
        L("Remove Window from Set"),
        L("Bring All to Front")
    };

    if (helpMenu)
        m_menubar->Append(helpMenu, wxString::Format("&%s", _L("Help")));
    SetMenuBar(m_menubar);

#endif

#ifdef _MSW_DARK_MODE
    if (wxGetApp().tabs_as_menu())
        m_menubar->EnableTop(6, false);
#endif

#ifdef __APPLE__
    // This fixes a bug on Mac OS where the quit command doesn't emit window close events
    // wx bug: https://trac.wxwidgets.org/ticket/18328
    wxMenu* apple_menu = m_menubar->OSXGetAppleMenu();
    if (apple_menu != nullptr) {
        apple_menu->Bind(wxEVT_MENU, [this](wxCommandEvent &) {
            Close();
        }, wxID_EXIT);
    }
#endif // __APPLE__
}

void MainFrame::set_max_recent_count(int max)
{
    max = max < 0 ? 0 : max > 999 ? 999 : max;
    size_t count = m_recent_projects.GetCount();
    m_recent_projects.SetMaxFiles(max);
    if (count != m_recent_projects.GetCount()) {
        count = m_recent_projects.GetCount();
        std::vector<std::string> recent_projects;
        for (size_t i = 0; i < count; ++i) {
            recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
        }
        wxGetApp().app_config->set_recent_projects(recent_projects);
        wxGetApp().app_config->save();
        m_webview->SendRecentList(-1);
    }
}

void MainFrame::open_menubar_item(const wxString& menu_name,const wxString& item_name)
{
    if (m_menubar == nullptr)
        return;
    // Get menu object from menubar
    int     menu_index = m_menubar->FindMenu(menu_name);
    wxMenu* menu       = m_menubar->GetMenu(menu_index);
    if (menu == nullptr) {
        BOOST_LOG_TRIVIAL(error) << "Mainframe open_menubar_item function couldn't find menu: " << menu_name;
        return;
    }
    // Get item id from menu
    int     item_id   = menu->FindItem(item_name);
    if (item_id == wxNOT_FOUND)
    {
        // try adding three dots char
        item_id = menu->FindItem(item_name + dots);
    }
    if (item_id == wxNOT_FOUND)
    {
        BOOST_LOG_TRIVIAL(error) << "Mainframe open_menubar_item function couldn't find item: " << item_name;
        return;
    }
    // wxEVT_MENU will trigger item
    wxPostEvent((wxEvtHandler*)menu, wxCommandEvent(wxEVT_MENU, item_id));
}

void MainFrame::init_menubar_as_gcodeviewer()
{
    //BBS do not show gcode viewer mebu
#if 0
    wxMenu* fileMenu = new wxMenu;
    {
        append_menu_item(fileMenu, wxID_ANY, _L("&Open G-code") + dots + "\tCtrl+O", _L("Open a G-code file"),
            [this](wxCommandEvent&) { if (m_plater != nullptr) m_plater->load_gcode(); }, "open", nullptr,
            [this]() {return m_plater != nullptr; }, this);
#ifdef __APPLE__
        append_menu_item(fileMenu, wxID_ANY, _L("Re&load from Disk") + dots + "\tCtrl+Shift+R",
            _L("Reload the plater from disk"), [this](wxCommandEvent&) { m_plater->reload_gcode_from_disk(); },
            "", nullptr, [this]() { return !m_plater->get_last_loaded_gcode().empty(); }, this);
#else
        append_menu_item(fileMenu, wxID_ANY, _L("Re&load from Disk") + sep + "F5",
            _L("Reload the plater from disk"), [this](wxCommandEvent&) { m_plater->reload_gcode_from_disk(); },
            "", nullptr, [this]() { return !m_plater->get_last_loaded_gcode().empty(); }, this);
#endif // __APPLE__
        fileMenu->AppendSeparator();
        append_menu_item(fileMenu, wxID_ANY, _L("Export &Toolpaths as OBJ") + dots, _L("Export toolpaths as OBJ"),
            [this](wxCommandEvent&) { if (m_plater != nullptr) m_plater->export_toolpaths_to_obj(); }, "export_plater", nullptr,
            [this]() {return can_export_toolpaths(); }, this);
        append_menu_item(fileMenu, wxID_ANY, _L("Open &Studio") + dots, _L("Open Studio"),
            [](wxCommandEvent&) { start_new_slicer(); }, "", nullptr,
            []() {return true; }, this);
        fileMenu->AppendSeparator();
        append_menu_item(fileMenu, wxID_EXIT, _L("&Quit"), wxString::Format(_L("Quit %s"), SLIC3R_APP_NAME),
            [this](wxCommandEvent&) { Close(false); });
    }

    // View menu
    wxMenu* viewMenu = nullptr;
    if (m_plater != nullptr) {
        viewMenu = new wxMenu();
        add_common_view_menu_items(viewMenu, this, std::bind(&MainFrame::can_change_view, this));
    }

    // helpmenu
    auto helpMenu = generate_help_menu();

    m_menubar = new wxMenuBar();
    m_menubar->Append(fileMenu, _L("&File"));
    if (viewMenu != nullptr) m_menubar->Append(viewMenu, _L("&View"));
    // Add additional menus from C++
    wxGetApp().add_config_menu(m_menubar);
    m_menubar->Append(helpMenu, _L("&Help"));
    SetMenuBar(m_menubar);

#ifdef __APPLE__
    // This fixes a bug on Mac OS where the quit command doesn't emit window close events
    // wx bug: https://trac.wxwidgets.org/ticket/18328
    wxMenu* apple_menu = m_menubar->OSXGetAppleMenu();
    if (apple_menu != nullptr) {
        apple_menu->Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            Close();
            }, wxID_EXIT);
    }
#endif // __APPLE__
#endif
}

void MainFrame::update_menubar()
{
    if (wxGetApp().is_gcode_viewer())
        return;

    const bool is_fff = plater()->printer_technology() == ptFFF;
}

void MainFrame::update_calibration_button_status()
{
    Preset &printer_preset = wxGetApp().preset_bundle->printers.get_edited_preset();
    bool isBBL = printer_preset.is_bbl_vendor_preset(wxGetApp().preset_bundle);
    bool is_multi_extruder = wxGetApp().preset_bundle->get_printer_extruder_count() > 1;
    // Show calibration Menu for BBL printers if Develop Mode is on.
    bool show_calibration = (!isBBL || wxGetApp().app_config->get("developer_mode") == "true");
    wxGetApp().mainframe->show_calibration_button(show_calibration, isBBL);
}

void MainFrame::reslice_now()
{
    if (m_plater)
        m_plater->reslice();
}

struct ConfigsOverwriteConfirmDialog : MessageDialog
{
    ConfigsOverwriteConfirmDialog(wxWindow *parent, wxString name, bool exported)
        : MessageDialog(parent,
                        wxString::Format(exported ? _L("A file exists with the same name: %s, do you want to override it.") :
                                                  _L("A config exists with the same name: %s, do you want to override it."),
                                         name),
                        _L(exported ? "Overwrite file" : "Overwrite config"),
                        wxYES_NO | wxNO_DEFAULT)
    {
        add_button(wxID_YESTOALL, false, _L("Yes to All"));
        add_button(wxID_NOTOALL, false, _L("No to All"));
    }
};

void MainFrame::export_config()
{
    ExportConfigsDialog export_configs_dlg(nullptr);
    export_configs_dlg.ShowModal();
    return;

    // Generate a cummulative configuration for the selected print, filaments and printer.
    wxDirDialog dlg(this, _L("Choose a directory"),
        from_u8(!m_last_config.IsEmpty() ? get_dir_name(m_last_config) : wxGetApp().app_config->get_last_dir()), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    wxString path;
    if (dlg.ShowModal() == wxID_OK)
        path = dlg.GetPath();
    if (!path.IsEmpty()) {
        // Export the config bundle.
        wxGetApp().app_config->update_config_dir(into_u8(path));
        try {
            auto files = wxGetApp().preset_bundle->export_current_configs(into_u8(path), [this](std::string const & name) {
                    ConfigsOverwriteConfirmDialog dlg(this, from_u8(name), true);
                    int res = dlg.ShowModal();
                    int ids[]{wxID_NO, wxID_YES, wxID_NOTOALL, wxID_YESTOALL};
                    return std::find(ids, ids + 4, res) - ids;
            }, false);
            if (!files.empty())
                m_last_config = from_u8(files.back());
            MessageDialog dlg(this, wxString::Format(_L_PLURAL("There is %d config exported. (Only non-system configs)",
                "There are %d configs exported. (Only non-system configs)", files.size()), files.size()),
                              _L("Export result"), wxOK);
            dlg.ShowModal();
        } catch (const std::exception &ex) {
            show_error(this, ex.what());
        }
    }
}

// Load a config file containing a Print, Filament & Printer preset.
void MainFrame::load_config_file()
{
    //BBS do not load config file
 //   if (!wxGetApp().check_and_save_current_preset_changes(_L("Loading profile file"), "", false))
 //       return;
    wxFileDialog dlg(this, _L("Select profile to load:"),
        !m_last_config.IsEmpty() ? get_dir_name(m_last_config) : wxGetApp().app_config->get_last_dir(),
        "config.json", "Config files (*.json;*.zip;*.bbscfg;*.bbsflmt)|*.json;*.zip;*.bbscfg;*.bbsflmt", wxFD_OPEN | wxFD_MULTIPLE | wxFD_FILE_MUST_EXIST);
     wxArrayString files;
    if (dlg.ShowModal() != wxID_OK)
        return;
    dlg.GetPaths(files);
    std::vector<std::string> cfiles;
    for (auto file : files) {
        cfiles.push_back(into_u8(file));
        m_last_config = file;
    }
    bool update = false;
    wxGetApp().preset_bundle->import_presets(cfiles, [this](std::string const & name) {
            ConfigsOverwriteConfirmDialog dlg(this, from_u8(name), false);
            int           res = dlg.ShowModal();
            int           ids[]{wxID_NO, wxID_YES, wxID_NOTOALL, wxID_YESTOALL};
            return std::find(ids, ids + 4, res) - ids;
        },
        ForwardCompatibilitySubstitutionRule::Enable);
    if (!cfiles.empty()) {
        wxGetApp().app_config->update_config_dir(get_dir_name(cfiles.back()));
        wxGetApp().load_current_presets();
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " presets has been import,and size is" << cfiles.size();
        NetworkAgent* agent = wxGetApp().getAgent();
        if (agent) {
#if !BBL_RELEASE_TO_PUBLIC
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " user is: " << agent->get_user_id();
#endif
        }
    }
    wxGetApp().preset_bundle->update_compatible(PresetSelectCompatibleType::Always);
    update_side_preset_ui();
    MessageDialog dlg2(this, wxString::Format(_L_PLURAL("There is %d config imported. (Only non-system and compatible configs)",
        "There are %d configs imported. (Only non-system and compatible configs)", cfiles.size()), cfiles.size()),
                        _L("Import result"), wxOK);
    dlg2.ShowModal();
}

// Load a config file containing a Print, Filament & Printer preset from command line.
bool MainFrame::load_config_file(const std::string &path)
{
    try {
        ConfigSubstitutions config_substitutions = wxGetApp().preset_bundle->load_config_file(path, ForwardCompatibilitySubstitutionRule::Enable);
        if (!config_substitutions.empty())
            show_substitutions_info(config_substitutions, path);
    } catch (const std::exception &ex) {
        show_error(this, ex.what());
        return false;
    }
    wxGetApp().load_current_presets();
    return true;
}

//BBS: export current config bundle as BBL default reference
//void MainFrame::export_current_configbundle()
//{
    // BBS do not export profile
   // if (!wxGetApp().check_and_save_current_preset_changes(_L("Exporting current profile bundle"),
   //     _L("Some presets are modified and the unsaved changes will not be exported into profile bundle."), false, true))
   //     return;

   // // validate current configuration in case it's dirty
   // auto err = wxGetApp().preset_bundle->full_config().validate();
   // if (! err.empty()) {
   //     show_error(this, err);
   //     return;
   // }
   // // Ask user for a file name.
   // wxFileDialog dlg(this, _L("Save BBL Default bundle as:"),
   //     !m_last_config.IsEmpty() ? get_dir_name(m_last_config) : wxGetApp().app_config->get_last_dir(),
   //     "BBL_config_bundle.ini",
   //     file_wildcards(FT_INI), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
   // wxString file;
   // if (dlg.ShowModal() == wxID_OK)
   //     file = dlg.GetPath();
   // if (!file.IsEmpty()) {
   //     // Export the config bundle.
   //     wxGetApp().app_config->update_config_dir(get_dir_name(file));
   //     try {
   //         wxGetApp().preset_bundle->export_current_configbundle(file.ToUTF8().data());
   //     } catch (const std::exception &ex) {
			//show_error(this, ex.what());
   //     }
   // }
//}

//BBS: export all the system preset configs to seperate files
/*void MainFrame::export_system_configs()
{
    // Ask user for a file name.
    wxDirDialog dlg(this, _L("choose a directory"),
        !m_last_config.IsEmpty() ? get_dir_name(m_last_config) : wxGetApp().app_config->get_last_dir(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    wxString path;
    if (dlg.ShowModal() == wxID_OK)
        path = dlg.GetPath();
    if (!path.IsEmpty()) {
        // Export the config bundle.
        wxGetApp().app_config->update_config_dir(path.ToStdString());
        try {
            wxGetApp().preset_bundle->export_system_configs(path.ToUTF8().data());
        } catch (const std::exception &ex) {
            show_error(this, ex.what());
        }
    }
}*/

//void MainFrame::export_configbundle(bool export_physical_printers /*= false*/)
//{
////    ; //BBS do not export config bundle
//}

// Loading a config bundle with an external file name used to be used
// to auto - install a config bundle on a fresh user account,
// but that behavior was not documented and likely buggy.
//void MainFrame::load_configbundle(wxString file/* = wxEmptyString, const bool reset_user_profile*/)
//{
//    ; //BBS do not import config bundle
//}

// Load a provied DynamicConfig into the Print / Filament / Printer tabs, thus modifying the active preset.
// Also update the plater with the new presets.
void MainFrame::load_config(const DynamicPrintConfig& config)
{
	PrinterTechnology printer_technology = wxGetApp().preset_bundle->printers.get_edited_preset().printer_technology();
	const auto       *opt_printer_technology = config.option<ConfigOptionEnum<PrinterTechnology>>("printer_technology");
	if (opt_printer_technology != nullptr && opt_printer_technology->value != printer_technology) {
		printer_technology = opt_printer_technology->value;
		this->plater()->set_printer_technology(printer_technology);
	}
#if 0
	for (auto tab : wxGetApp().tabs_list)
		if (tab->supports_printer_technology(printer_technology)) {
			if (tab->type() == Slic3r::Preset::TYPE_PRINTER)
				static_cast<TabPrinter*>(tab)->update_pages();
			tab->load_config(config);
		}
    if (m_plater)
        m_plater->on_config_change(config);
#else
	// Load the currently selected preset into the GUI, update the preset selection box.
    //FIXME this is not quite safe for multi-extruder printers,
    // as the number of extruders is not adjusted for the vector values.
    // (see PresetBundle::update_multi_material_filament_presets())
    // Better to call PresetBundle::load_config() instead?
    for (auto tab : wxGetApp().tabs_list)
        if (tab->supports_printer_technology(printer_technology)) {
            // Only apply keys, which are present in the tab's config. Ignore the other keys.
			for (const std::string &opt_key : tab->get_config()->diff(config))
				// Ignore print_settings_id, printer_settings_id, filament_settings_id etc.
				if (! boost::algorithm::ends_with(opt_key, "_settings_id"))
					tab->get_config()->option(opt_key)->set(config.option(opt_key));
        }

    wxGetApp().load_current_presets();
#endif
}

//BBS: GUI refactor
void MainFrame::select_tab(wxPanel* panel)
{
    if (!panel)
        return;
    if (panel == m_param_panel) {
        panel = m_plater;
    } else if (dynamic_cast<ParamsPanel*>(panel)) {
        wxGetApp().params_dialog()->Popup();
        return;
    }
    int page_idx = m_tabpanel->FindPage(panel);
    if (page_idx == tp3DEditor && m_tabpanel->GetSelection() == tpPreview)
        return;
    //BBS GUI refactor: remove unused layout new/dlg
    /*if (page_idx != wxNOT_FOUND && m_layout == ESettingsLayout::Dlg)
        page_idx++;*/
    select_tab(size_t(page_idx));
}

//BBS
void MainFrame::jump_to_monitor(std::string dev_id)
{
    m_tabpanel->SetSelection(tpMonitor);
    if (!dev_id.empty()) {
        ((MonitorPanel*)m_monitor)->select_machine(dev_id);
    }
}

void MainFrame::jump_to_multipage()
{
    m_tabpanel->SetSelection(tpMultiDevice);
    ((MultiMachinePage*)m_multi_machine)->jump_to_send_page();
}


//BBS GUI refactor: remove unused layout new/dlg
void MainFrame::select_tab(size_t tab/* = size_t(-1)*/)
{
    //bool tabpanel_was_hidden = false;

    // Controls on page are created on active page of active tab now.
    // We should select/activate tab before its showing to avoid an UI-flickering
    auto select = [this, tab](bool was_hidden) {
        // when tab == -1, it means we should show the last selected tab
        //BBS GUI refactor: remove unused layout new/dlg
        //size_t new_selection = tab == (size_t)(-1) ? m_last_selected_tab : (m_layout == ESettingsLayout::Dlg && tab != 0) ? tab - 1 : tab;
        size_t new_selection = tab == (size_t)(-1) ? m_last_selected_tab : tab;

        if (m_tabpanel->GetSelection() != (int)new_selection)
            m_tabpanel->SetSelection(new_selection);
#ifdef _MSW_DARK_MODE
        /*if (wxGetApp().tabs_as_menu()) {
            if (Tab* cur_tab = dynamic_cast<Tab*>(m_tabpanel->GetPage(new_selection)))
                update_marker_for_tabs_menu((m_layout == ESettingsLayout::Old ? m_menubar : m_settings_dialog.menubar()), cur_tab->title(), m_layout == ESettingsLayout::Old);
            else if (tab == 0 && m_layout == ESettingsLayout::Old)
                m_plater->get_current_canvas3D()->render();
        }*/
#endif
        if (tab == MainFrame::tp3DEditor && m_layout == ESettingsLayout::Old)
            m_plater->canvas3D()->render();
        else if (was_hidden) {
            Tab* cur_tab = dynamic_cast<Tab*>(m_tabpanel->GetPage(new_selection));
            if (cur_tab)
                cur_tab->OnActivate();
        }
    };

    select(false);
}

void MainFrame::request_select_tab(TabPosition pos)
{
    wxCommandEvent* evt = new wxCommandEvent(EVT_SELECT_TAB);
    evt->SetInt(pos);
    wxQueueEvent(this, evt);
}

int MainFrame::get_calibration_curr_tab() {
    if (m_calibration)
        return m_calibration->get_tabpanel()->GetSelection();
    return -1;
}

// Set a camera direction, zoom to all objects.
void MainFrame::select_view(const std::string& direction)
{
     if (m_plater)
         m_plater->select_view(direction);
}

void MainFrame::view_zoom_to_fit() const
{
    if (GLCanvas3D *canvas = m_plater ? m_plater->canvas3D() : nullptr)
        canvas->zoom_to_fit();
}

// #ys_FIXME_to_delete
void MainFrame::on_presets_changed(SimpleEvent &event)
{
    auto *tab = dynamic_cast<Tab*>(event.GetEventObject());
    wxASSERT(tab != nullptr);
    if (tab == nullptr) {
        return;
    }

    // Update preset combo boxes(Print settings, Filament, Material, Printer) from their respective tabs.
    auto presets = tab->get_presets();
    if (m_plater != nullptr && presets != nullptr) {

        // FIXME: The preset type really should be a property of Tab instead
        Slic3r::Preset::Type preset_type = tab->type();
        if (preset_type == Slic3r::Preset::TYPE_INVALID) {
            wxASSERT(false);
            return;
        }

        m_plater->on_config_change(*tab->get_config());

        m_plater->sidebar().update_presets(preset_type);
    }
}

// #ys_FIXME_to_delete
void MainFrame::on_value_changed(wxCommandEvent& event)
{
    auto *tab = dynamic_cast<Tab*>(event.GetEventObject());
    wxASSERT(tab != nullptr);
    if (tab == nullptr)
        return;

    auto opt_key = event.GetString();
    if (m_plater) {
        m_plater->on_config_change(*tab->get_config()); // propagate config change events to the plater
        if (opt_key == "extruders_count") {
            auto value = event.GetInt();
            m_plater->on_filament_count_change(value);
        }
    }
}

void MainFrame::on_config_changed(DynamicPrintConfig* config) const
{
    if (m_plater)
        m_plater->on_config_change(*config); // propagate config change events to the plater
}

void MainFrame::set_print_button_to_default(PrintSelectType select_type)
{
    if (select_type == PrintSelectType::ePrintPlate) {
        m_print_btn->SetLabel(_L("Print plate"));
        m_print_select = ePrintPlate;
        if (m_print_enable)
            m_print_enable = get_enable_print_status();
        m_print_btn->Enable(m_print_enable);
        this->Layout();
    } else if (select_type == PrintSelectType::eSendGcode) {
        m_print_btn->SetLabel(_L("Print"));
        m_print_select = eSendGcode;
        if (m_print_enable)
            m_print_enable = get_enable_print_status() && can_send_gcode();
        m_print_btn->Enable(m_print_enable);
        this->Layout();
    } else {
        //unsupport
        return;
    }
}

void MainFrame::add_to_recent_projects(const wxString& filename)
{
    if (wxFileExists(filename))
    {
        m_recent_projects.AddFileToHistory(filename);
        std::vector<std::string> recent_projects;
        size_t count = m_recent_projects.GetCount();
        for (size_t i = 0; i < count; ++i)
        {
            recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
        }
        wxGetApp().app_config->set_recent_projects(recent_projects);
        wxGetApp().app_config->save();
        m_webview->SendRecentList(0);
    }
}

std::wstring MainFrame::FileHistory::GetThumbnailUrl(int index) const
{
    if (m_thumbnails[index].empty()) return L"";
    std::wstringstream wss;
    wss << L"data:image/png;base64,";
    wss << wxBase64Encode(m_thumbnails[index].data(), m_thumbnails[index].size());
    return wss.str();
}

void MainFrame::FileHistory::AddFileToHistory(const wxString &file)
{
    if (this->m_fileMaxFiles == 0)
        return;
    wxFileHistory::AddFileToHistory(file);
    if (m_load_called)
        m_thumbnails.push_front(bbs_3mf_get_thumbnail(into_u8(file).c_str()));
    else
        m_thumbnails.push_front("");
}

void MainFrame::FileHistory::RemoveFileFromHistory(size_t i)
{
    if (i >= m_thumbnails.size()) // FIX zero max
        return;
    wxFileHistory::RemoveFileFromHistory(i);
    m_thumbnails.erase(m_thumbnails.begin() + i);
}

size_t MainFrame::FileHistory::FindFileInHistory(const wxString & file)
{
    return m_fileHistory.Index(file);
}

void MainFrame::FileHistory::LoadThumbnails()
{
    tbb::parallel_for(tbb::blocked_range<size_t>(0, GetCount()), [this](tbb::blocked_range<size_t> range) {
        for (size_t i = range.begin(); i < range.end(); ++i) {
            auto thumbnail = bbs_3mf_get_thumbnail(into_u8(GetHistoryFile(i)).c_str());
            if (!thumbnail.empty()) {
                m_thumbnails[i] = thumbnail;
            }
        }
    });
    m_load_called = true;
}

inline void MainFrame::FileHistory::SetMaxFiles(int max)
{
    m_fileMaxFiles  = max;
    size_t numFiles = m_fileHistory.size();
    while (numFiles > m_fileMaxFiles)
        RemoveFileFromHistory(--numFiles);
}

void MainFrame::get_recent_projects(boost::property_tree::wptree &tree, int images)
{
    for (size_t i = 0; i < m_recent_projects.GetCount(); ++i) {
        boost::property_tree::wptree item;
        std::wstring proj = m_recent_projects.GetHistoryFile(i).ToStdWstring();
        item.put(L"project_name", proj.substr(proj.find_last_of(L"/\\") + 1));
        item.put(L"path", proj);
        boost::system::error_code ec;
        std::time_t t = boost::filesystem::last_write_time(proj, ec);
        if (!ec) {
            std::tm* local_tm = std::localtime(&t);
            if (local_tm != nullptr)
            {
                bool use_12h_format = wxGetApp().app_config->get("use_12h_time_format") == "true";

                // Format date and time: YYYY-MM-DD HH:MM[:SS][AM/PM]
                std::wstringstream time_stream;
                time_stream << std::setw(4) << std::setfill(L'0') << (local_tm->tm_year + 1900) << L"-"
                        << std::setw(2) << std::setfill(L'0') << (local_tm->tm_mon + 1) << L"-"
                        << std::setw(2) << std::setfill(L'0') << local_tm->tm_mday << L" "
                        << from_u8(Slic3r::format_time_hm(local_tm, use_12h_format));
                item.put(L"time", time_stream.str());

            }
            else
            {
                std::wstring time = wxDateTime(t).FormatISOCombined(' ').ToStdWstring();
                item.put(L"time", time);
            }
            if (i <= images) {
                auto thumbnail = m_recent_projects.GetThumbnailUrl(i);
                if (!thumbnail.empty()) item.put(L"image", thumbnail);
            }
        } else {
            item.put(L"time", _L("File is missing"));
        }
        tree.push_back({L"", item});
    }
}

void MainFrame::open_recent_project(size_t file_id, wxString const & filename)
{
    if (file_id == size_t(-1)) {
        file_id = m_recent_projects.FindFileInHistory(filename);
    }
    if (wxFileExists(filename)) {
        CallAfter([this, filename] {
            if (wxGetApp().can_load_project()) {
                // BBS: session file-tabs — open the recent project in a new tab.
                open_project_in_tab(filename);
            }
        });
    }
    else
    {
        MessageDialog msg(this, _L("The project is no longer available."), _L("Error"), wxOK | wxYES_DEFAULT);
        if (msg.ShowModal() == wxID_YES)
        {
            m_recent_projects.RemoveFileFromHistory(file_id);
            std::vector<std::string> recent_projects;
            size_t count = m_recent_projects.GetCount();
            for (size_t i = 0; i < count; ++i)
            {
                recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
            }
            wxGetApp().app_config->set_recent_projects(recent_projects);
            wxGetApp().app_config->save();
            m_webview->SendRecentList(-1);
        }
    }
}

void MainFrame::remove_recent_project(size_t file_id, wxString const &filename)
{
    if (file_id == size_t(-1)) {
        if (filename.IsEmpty())
            while (m_recent_projects.GetCount() > 0)
                m_recent_projects.RemoveFileFromHistory(0);
        else
            file_id = m_recent_projects.FindFileInHistory(filename);
    }
    if (file_id != size_t(-1))
        m_recent_projects.RemoveFileFromHistory(file_id);
    std::vector<std::string> recent_projects;
    size_t count = m_recent_projects.GetCount();
    for (size_t i = 0; i < count; ++i)
    {
        recent_projects.push_back(into_u8(m_recent_projects.GetHistoryFile(i)));
    }
    wxGetApp().app_config->set_recent_projects(recent_projects);
    wxGetApp().app_config->save();
    m_webview->SendRecentList(-1);
}

void MainFrame::load_url(wxString url)
{
    BOOST_LOG_TRIVIAL(trace) << "load_url";
    auto evt = new wxCommandEvent(EVT_LOAD_URL, this->GetId());
    evt->SetString(url);
    wxQueueEvent(this, evt);
}

void MainFrame::load_printer_url(wxString url)
{
    BOOST_LOG_TRIVIAL(trace) << "load_printer_url";
    auto evt = new wxCommandEvent(EVT_LOAD_PRINTER_URL, this->GetId());
    evt->SetString(url);
    wxQueueEvent(this, evt);
}

void MainFrame::load_printer_url()
{
    PresetBundle &preset_bundle = *wxGetApp().preset_bundle;
    if (preset_bundle.printers.get_edited_preset().is_bbl_vendor_preset(&preset_bundle))
        return;

    auto cfg = preset_bundle.printers.get_edited_preset().config;
    wxString url =
        cfg.opt_string("print_host_webui").empty() ? cfg.opt_string("print_host") : cfg.opt_string("print_host_webui");
    if (!url.empty()) {
        if (!url.Lower().starts_with("http"))
            url = wxString::Format("http://%s", url);

        load_printer_url(url);
    }
}

bool MainFrame::is_printer_view() const { return m_tabpanel->GetSelection() == TabPosition::tpMonitor; }


void MainFrame::refresh_plugin_tips()
{
    if (m_webview != nullptr)
        m_webview->ShowNetpluginTip();
}

void MainFrame::RunScript(wxString js)
{
    if (m_webview != nullptr)
        m_webview->RunScript(js);
}

void MainFrame::RunScriptLeft(wxString js)
{
    if (m_webview != nullptr)
        m_webview->RunScriptLeft(js);
}

void MainFrame::technology_changed()
{
    // upadte DiffDlg
    diff_dialog.update_presets();

    // update menu titles
    PrinterTechnology pt = plater()->printer_technology();
    if (int id = m_menubar->FindMenu(pt == ptFFF ? _omitL("Material Settings") : _L("Filament Settings")); id != wxNOT_FOUND)
        m_menubar->SetMenuLabel(id, pt == ptSLA ? _omitL("Material Settings") : _L("Filament Settings"));
}


//
// Called after the Preferences dialog is closed and the program settings are saved.
// Update the UI based on the current preferences.
void MainFrame::update_ui_from_settings()
{
    if (m_plater)
        m_plater->update_ui_from_settings();
    for (auto tab: wxGetApp().tabs_list)
        tab->update_ui_from_settings();
}


void MainFrame::show_sync_dialog()
{
    SimpleEvent* evt = new SimpleEvent(EVT_SYNC_CLOUD_PRESET);
    wxQueueEvent(this, evt);
}

bool MainFrame::check_bbl_farm_client_installed()
{
#ifdef WIN32
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Bambulab\\Bambu Farm Manager Client"), 0, KEY_READ, &hKey);
    LONG result_backup = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("HKEY_CLASSES_ROOT\\bambu-farm-client\\shell\\open\\command"), 0, KEY_READ, &hKey);

    if (result == ERROR_SUCCESS || result_backup == ERROR_SUCCESS) {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "Bambu Farm Manager Client found.";
        RegCloseKey(hKey);
        return true;
    } else {
        BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "Bambu Farm Manager Client Not found.";
        return false;
    }

#else
    return false;
#endif
}

void MainFrame::update_side_preset_ui()
{
    // select last preset
    for (auto tab : wxGetApp().tabs_list) {
        tab->update_tab_ui();
    }

    //BBS: update the preset
    m_plater->sidebar().update_presets(Preset::TYPE_PRINTER);
    m_plater->sidebar().update_presets(Preset::TYPE_FILAMENT);


    //take off multi machine
    if(m_multi_machine){m_multi_machine->clear_page();}
}

void MainFrame::on_select_default_preset(SimpleEvent& evt)
{
    MessageDialog dialog(this,
                    _L("Do you want to synchronize your personal data from Bambu Cloud? \n"
                        "It contains the following information:\n"
                        "1. The Process presets\n"
                        "2. The Filament presets\n"
                        "3. The Printer presets"),
                    _L("Synchronization"),
                    wxCENTER |
                    wxYES_DEFAULT | wxYES_NO |
                    wxICON_INFORMATION);

    /* get setting list */
    NetworkAgent* agent = wxGetApp().getAgent();
    switch ( dialog.ShowModal() )
    {
        case wxID_YES: {
            wxGetApp().app_config->set_bool("sync_user_preset", true);
            wxGetApp().start_sync_user_preset(true);
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " sync_user_preset: true";
            break;
        }
        case wxID_NO:
            wxGetApp().app_config->set_bool("sync_user_preset", false);
            wxGetApp().stop_sync_user_preset();
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " sync_user_preset: false";
            break;
        default:
            break;
    }

    update_side_preset_ui();
}

std::string MainFrame::get_base_name(const wxString &full_name, const char *extension) const
{
    boost::filesystem::path filename = boost::filesystem::path(full_name.wx_str()).filename();
    if (extension != nullptr)
		filename = filename.replace_extension(extension);
    return filename.string();
}

std::string MainFrame::get_dir_name(const wxString &full_name) const
{
    return boost::filesystem::path(into_u8(full_name)).parent_path().string();
}


// ----------------------------------------------------------------------------
// SettingsDialog
// ----------------------------------------------------------------------------

SettingsDialog::SettingsDialog(MainFrame* mainframe)
:DPIDialog(NULL, wxID_ANY, wxString(SLIC3R_APP_NAME) + " - " + _L("Settings"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE, "settings_dialog"),
//: DPIDialog(mainframe, wxID_ANY, wxString(SLIC3R_APP_NAME) + " - " + _L("Settings"), wxDefaultPosition, wxDefaultSize,
//        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX, "settings_dialog"),
    m_main_frame(mainframe)
{
    if (wxGetApp().is_gcode_viewer())
        return;

#if defined(__WXMSW__)
    // ys_FIXME! temporary workaround for correct font scaling
    // Because of from wxWidgets 3.1.3 auto rescaling is implemented for the Fonts,
    // From the very beginning set dialog font to the wxSYS_DEFAULT_GUI_FONT
    this->SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
#else
    this->SetFont(wxGetApp().normal_font());
    this->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
#endif // __WXMSW__

    // Load the icon either from the exe, or from the ico file.
#if _WIN32
    {
        TCHAR szExeFileName[MAX_PATH];
        GetModuleFileName(nullptr, szExeFileName, MAX_PATH);
        SetIcon(wxIcon(szExeFileName, wxBITMAP_TYPE_ICO));
    }
#else
    SetIcon(wxIcon(var("BambuStudio_128px.png"), wxBITMAP_TYPE_PNG));
#endif // _WIN32

    //just hide the Frame on closing
    this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& evt) { this->Hide(); });

#ifdef _MSW_DARK_MODE
    if (wxGetApp().tabs_as_menu()) {
        // menubar
        //m_menubar = new wxMenuBar();
        //add_tabs_as_menu(m_menubar, mainframe, this);
        //this->SetMenuBar(m_menubar);
    }
#endif

    // initialize layout
    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->SetSizeHints(this);
    SetSizer(sizer);
    Fit();

    const wxSize min_size = wxSize(85 * em_unit(), 50 * em_unit());
#ifdef __APPLE__
    // Using SetMinSize() on Mac messes up the window position in some cases
    // cf. https://groups.google.com/forum/#!topic/wx-users/yUKPBBfXWO0
    SetSize(min_size);
#else
    SetMinSize(min_size);
    SetSize(GetMinSize());
#endif
    Layout();
}

void SettingsDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    if (wxGetApp().is_gcode_viewer())
        return;

    const int& em = em_unit();
    const wxSize& size = wxSize(85 * em, 50 * em);

    // BBS
    m_tabpanel->Rescale();

    // update Tabs
    for (auto tab : wxGetApp().tabs_list)
        tab->msw_rescale();

    SetMinSize(size);
    Fit();
    Refresh();
}


} // GUI
} // Slic3r
