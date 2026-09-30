#include "wxExtensions.hpp"
#include "Widgets/MD3ScrolledWindow.hpp"

#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <memory>

#include <wx/clipbrd.h>
#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/graphics.h>
#include <wx/stattext.h>
#include <boost/algorithm/string/replace.hpp>

/* mac need the macro while including <boost/stacktrace.hpp>*/
#ifdef  __APPLE__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <boost/stacktrace.hpp>

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "GUI_ObjectList.hpp"
#include "I18N.hpp"
#include "GUI_Utils.hpp"
#include "Plater.hpp"
#include "../Utils/MacDarkMode.hpp"
#include "BitmapComboBox.hpp"
#include "Widgets/MD3ColorPicker.hpp"
#include "Widgets/MD3Menu.hpp"
#include "Widgets/StaticBox.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "../Utils/WxFontUtils.hpp"
#include "FilamentBitmapUtils.hpp"
#include "../Utils/ColorSpaceConvert.hpp"
#ifndef __linux__
// msw_menuitem_bitmaps is used for MSW and OSX
static std::map<int, std::string> msw_menuitem_bitmaps;
#ifdef __WXMSW__
void msw_rescale_menu(wxMenu* menu)
{
	struct update_icons {
		static void run(wxMenuItem* item) {
			const auto it = msw_menuitem_bitmaps.find(item->GetId());
			if (it != msw_menuitem_bitmaps.end()) {
				const wxBitmap& item_icon = create_menu_bitmap(it->second);
				if (item_icon.IsOk())
					item->SetBitmap(item_icon);
			}
			if (item->IsSubMenu())
				for (wxMenuItem *sub_item : item->GetSubMenu()->GetMenuItems())
					update_icons::run(sub_item);
		}
	};

	for (wxMenuItem *item : menu->GetMenuItems())
		update_icons::run(item);
}
#endif /* __WXMSW__ */
#endif /* no __WXGTK__ */

void enable_menu_item(wxUpdateUIEvent& evt, std::function<bool()> const cb_condition, wxMenuItem* item, wxWindow* win)
{
    const bool enable = cb_condition();
    evt.Enable(enable);

#ifdef __WXOSX__
    const auto it = msw_menuitem_bitmaps.find(item->GetId());
    if (it != msw_menuitem_bitmaps.end())
    {
        const wxBitmap& item_icon = create_scaled_bitmap(it->second, win, 16, !enable);
        if (item_icon.IsOk())
            item->SetBitmap(item_icon);
    }
#endif // __WXOSX__
}

wxMenuItem* append_menu_item(wxMenu* menu, int id, const wxString& string, const wxString& description,
    std::function<void(wxCommandEvent& event)> cb, const wxBitmap& icon, wxEvtHandler* event_handler,
    std::function<bool()> const cb_condition, wxWindow* parent, int insert_pos/* = wxNOT_FOUND*/)
{
    if (id == wxID_ANY)
        id = wxNewId();

    auto *item = new wxMenuItem(menu, id, string, description);
    if (icon.IsOk()) {
        item->SetBitmap(icon);
    }
    if (insert_pos == wxNOT_FOUND)
        menu->Append(item);
    else
        menu->Insert(insert_pos, item);

#ifdef __WXMSW__
    if (event_handler != nullptr && event_handler != menu)
        event_handler->Bind(wxEVT_MENU, cb, id);
    else
#endif // __WXMSW__
        menu->Bind(wxEVT_MENU, cb, id);

    if (parent) {
        parent->Bind(wxEVT_UPDATE_UI, [cb_condition, item, parent](wxUpdateUIEvent& evt) {
            enable_menu_item(evt, cb_condition, item, parent); }, id);
    }

    return item;
}

wxMenuItem* append_menu_item(wxMenu* menu, int id, const wxString& string, const wxString& description,
    std::function<void(wxCommandEvent& event)> cb, const std::string& icon, wxEvtHandler* event_handler,
    std::function<bool()> const cb_condition, wxWindow* parent, int insert_pos/* = wxNOT_FOUND*/)
{
    if (id == wxID_ANY)
        id = wxNewId();

    const wxBitmap& bmp = !icon.empty() ? create_menu_bitmap(icon) : wxNullBitmap;   // FIXME: pass window ptr
//#ifdef __WXMSW__
#ifndef __WXGTK__
    if (bmp.IsOk())
        msw_menuitem_bitmaps[id] = icon;
#endif /* __WXMSW__ */

    return append_menu_item(menu, id, string, description, cb, bmp, event_handler, cb_condition, parent, insert_pos);
}

wxMenuItem* append_submenu(wxMenu* menu, wxMenu* sub_menu, int id, const wxString& string, const wxString& description, const std::string& icon,
    std::function<bool()> const cb_condition, wxWindow* parent, int insert_pos)
{
    if (id == wxID_ANY)
        id = wxNewId();

    wxMenuItem* item = new wxMenuItem(menu, id, string, description, wxITEM_NORMAL, sub_menu);
    if (!icon.empty()) {
        item->SetBitmap(create_menu_bitmap(icon));    // FIXME: pass window ptr
//#ifdef __WXMSW__
#ifndef __WXGTK__
        msw_menuitem_bitmaps[id] = icon;
#endif /* __WXMSW__ */
    }

    if (insert_pos == wxNOT_FOUND)
        menu->Append(item);
    else
        menu->Insert(insert_pos, item);

    if (parent) {
        parent->Bind(wxEVT_UPDATE_UI, [cb_condition, item, parent](wxUpdateUIEvent& evt) {
            enable_menu_item(evt, cb_condition, item, parent); }, id);
    }

    return item;
}

wxMenuItem* append_menu_radio_item(wxMenu* menu, int id, const wxString& string, const wxString& description,
    std::function<void(wxCommandEvent& event)> cb, wxEvtHandler* event_handler)
{
    if (id == wxID_ANY)
        id = wxNewId();

    wxMenuItem* item = menu->AppendRadioItem(id, string, description);

#ifdef __WXMSW__
    if (event_handler != nullptr && event_handler != menu)
        event_handler->Bind(wxEVT_MENU, cb, id);
    else
#endif // __WXMSW__
        menu->Bind(wxEVT_MENU, cb, id);

    return item;
}

wxMenuItem* append_menu_check_item(wxMenu* menu, int id, const wxString& string, const wxString& description,
    std::function<void(wxCommandEvent & event)> cb, wxEvtHandler* event_handler,
    std::function<bool()> const enable_condition, std::function<bool()> const check_condition, wxWindow* parent)
{
    if (id == wxID_ANY)
        id = wxNewId();

    wxMenuItem* item = menu->AppendCheckItem(id, string, description);

#ifdef __WXMSW__
    if (event_handler != nullptr && event_handler != menu)
        event_handler->Bind(wxEVT_MENU, cb, id);
    else
#endif // __WXMSW__
        menu->Bind(wxEVT_MENU, cb, id);

    if (parent)
        parent->Bind(wxEVT_UPDATE_UI, [enable_condition, check_condition](wxUpdateUIEvent& evt)
            {
                evt.Enable(enable_condition());
                evt.Check(check_condition());
            }, id);

    return item;
}

const unsigned int wxCheckListBoxComboPopup::DefaultWidth = 200;
const unsigned int wxCheckListBoxComboPopup::DefaultHeight = 200;

bool wxCheckListBoxComboPopup::Create(wxWindow* parent)
{
    return wxCheckListBox::Create(parent, wxID_HIGHEST + 1, wxPoint(0, 0));
}

wxWindow* wxCheckListBoxComboPopup::GetControl()
{
    return this;
}

void wxCheckListBoxComboPopup::SetStringValue(const wxString& value)
{
    m_text = value;
}

wxString wxCheckListBoxComboPopup::GetStringValue() const
{
    return m_text;
}

wxSize wxCheckListBoxComboPopup::GetAdjustedSize(int minWidth, int prefHeight, int maxHeight)
{
    // set width dinamically in dependence of items text
    // and set height dinamically in dependence of items count

    wxComboCtrl* cmb = GetComboCtrl();
    if (cmb != nullptr) {
        wxSize size = GetComboCtrl()->GetSize();

        unsigned int count = GetCount();
        if (count > 0) {
            int max_width = size.x;
            for (unsigned int i = 0; i < count; ++i) {
                max_width = std::max(max_width, 60 + GetTextExtent(GetString(i)).x);
            }
            size.SetWidth(max_width);
            size.SetHeight(count * cmb->GetCharHeight());
        }
        else
            size.SetHeight(DefaultHeight);

        return size;
    }
    else
        return wxSize(DefaultWidth, DefaultHeight);
}

void wxCheckListBoxComboPopup::OnKeyEvent(wxKeyEvent& evt)
{
    // filters out all the keys which are not working properly
    switch (evt.GetKeyCode())
    {
    case WXK_LEFT:
    case WXK_UP:
    case WXK_RIGHT:
    case WXK_DOWN:
    case WXK_PAGEUP:
    case WXK_PAGEDOWN:
    case WXK_END:
    case WXK_HOME:
    case WXK_NUMPAD_LEFT:
    case WXK_NUMPAD_UP:
    case WXK_NUMPAD_RIGHT:
    case WXK_NUMPAD_DOWN:
    case WXK_NUMPAD_PAGEUP:
    case WXK_NUMPAD_PAGEDOWN:
    case WXK_NUMPAD_END:
    case WXK_NUMPAD_HOME:
    {
        break;
    }
    default:
    {
        evt.Skip();
        break;
    }
    }
}

void wxCheckListBoxComboPopup::OnCheckListBox(wxCommandEvent& evt)
{
    // forwards the checklistbox event to the owner wxComboCtrl

    if (m_check_box_events_status == OnCheckListBoxFunction::FreeToProceed )
    {
        wxComboCtrl* cmb = GetComboCtrl();
        if (cmb != nullptr) {
            wxCommandEvent event(wxEVT_CHECKLISTBOX, cmb->GetId());
            event.SetEventObject(cmb);
            cmb->ProcessWindowEvent(event);
        }
    }

    evt.Skip();

    #ifndef _WIN32  // events are sent differently on OSX+Linux vs Win (more description in header file)
        if ( m_check_box_events_status == OnCheckListBoxFunction::RefuseToProceed )
            // this happens if the event was resent by OnListBoxSelection - next call to OnListBoxSelection is due to user clicking the text, so the function should
            // explicitly change the state on the checkbox
            m_check_box_events_status = OnCheckListBoxFunction::WasRefusedLastTime;
        else
            // if the user clicked the checkbox square, this event was sent before OnListBoxSelection was called, so we don't want it to resend it
            m_check_box_events_status = OnCheckListBoxFunction::RefuseToProceed;
    #endif
}

void wxCheckListBoxComboPopup::OnListBoxSelection(wxCommandEvent& evt)
{
    // transforms list box item selection event into checklistbox item toggle event

    int selId = GetSelection();
    if (selId != wxNOT_FOUND)
    {
        #ifndef _WIN32
            if (m_check_box_events_status == OnCheckListBoxFunction::RefuseToProceed)
        #endif
                Check((unsigned int)selId, !IsChecked((unsigned int)selId));

        m_check_box_events_status = OnCheckListBoxFunction::FreeToProceed; // so the checkbox reacts to square-click the next time

        SetSelection(wxNOT_FOUND);
        wxCommandEvent event(wxEVT_CHECKLISTBOX, GetId());
        event.SetInt(selId);
        event.SetEventObject(this);
        ProcessEvent(event);
    }
}


// ***  wxDataViewTreeCtrlComboPopup  ***

const unsigned int wxDataViewTreeCtrlComboPopup::DefaultWidth = 270;
const unsigned int wxDataViewTreeCtrlComboPopup::DefaultHeight = 200;
const unsigned int wxDataViewTreeCtrlComboPopup::DefaultItemHeight = 22;

bool wxDataViewTreeCtrlComboPopup::Create(wxWindow* parent)
{
	return wxDataViewTreeCtrl::Create(parent, wxID_ANY/*HIGHEST + 1*/, wxPoint(0, 0), wxDefaultSize/*wxSize(270, -1)*/, wxDV_NO_HEADER);
}
/*
wxSize wxDataViewTreeCtrlComboPopup::GetAdjustedSize(int minWidth, int prefHeight, int maxHeight)
{
	// matches owner wxComboCtrl's width
	// and sets height dinamically in dependence of contained items count
	wxComboCtrl* cmb = GetComboCtrl();
	if (cmb != nullptr)
	{
		wxSize size = GetComboCtrl()->GetSize();
		if (m_cnt_open_items > 0)
			size.SetHeight(m_cnt_open_items * DefaultItemHeight);
		else
			size.SetHeight(DefaultHeight);

		return size;
	}
	else
		return wxSize(DefaultWidth, DefaultHeight);
}
*/
void wxDataViewTreeCtrlComboPopup::OnKeyEvent(wxKeyEvent& evt)
{
	// filters out all the keys which are not working properly
	if (evt.GetKeyCode() == WXK_UP)
	{
		return;
	}
	else if (evt.GetKeyCode() == WXK_DOWN)
	{
		return;
	}
	else
	{
		evt.Skip();
		return;
	}
}

void wxDataViewTreeCtrlComboPopup::OnDataViewTreeCtrlSelection(wxCommandEvent& evt)
{
	wxComboCtrl* cmb = GetComboCtrl();
	auto selected = GetItemText(GetSelection());
	cmb->SetText(selected);
}

// edit tooltip : change Slic3r to SLIC3R_APP_KEY
// Temporary workaround for localization
void edit_tooltip(wxString& tooltip)
{
    tooltip.Replace("Slic3r", SLIC3R_APP_KEY, true);
}

bool copy_text_to_clipboard(const wxString& text)
{
    if (text.IsEmpty() || !wxTheClipboard->Open())
        return false;

    const bool copied = wxTheClipboard->SetData(new wxTextDataObject(text));
    wxTheClipboard->Close();
    return copied;
}

void enable_static_text_copy_menu(wxStaticText* label)
{
    if (!label)
        return;

    // wxEVT_CONTEXT_MENU is not delivered by MSW static controls, so drive the
    // menu from the raw right-click instead.
    label->Bind(wxEVT_RIGHT_UP, [label](wxMouseEvent& evt) {
        const wxString value = label->GetLabel();
        if (value.IsEmpty() || value == "-") {
            evt.Skip();
            return;
        }

        wxMenu menu;
        const int copy_id = wxWindow::NewControlId();
        menu.Append(copy_id, _L("Copy"));
        menu.Bind(wxEVT_MENU, [value](wxCommandEvent&) { copy_text_to_clipboard(value); }, copy_id);
        // The Material menu, like every other context menu; the native one does not follow the theme or the language modes.
        MD3::PopupMenu(label, &menu, label->ClientToScreen(evt.GetPosition()));
    });
}

/* Function for rescale of buttons in Dialog under MSW if dpi is changed.
 * btn_ids - vector of buttons identifiers
 */
void msw_buttons_rescale(wxDialog* dlg, const int em_unit, const std::vector<int>& btn_ids)
{
    const wxSize& btn_size = wxSize(-1, int(2.5f * em_unit + 0.5f));

    for (int btn_id : btn_ids) {
        // There is a case [FirmwareDialog], when we have wxControl instead of wxButton
        // so let casting everything to the wxControl
        wxControl* btn = static_cast<wxControl*>(dlg->FindWindowById(btn_id, dlg));
        if (btn)
            btn->SetMinSize(btn_size);
    }
}

/* Function for getting of em_unit value from correct parent.
 * In most of cases it is m_em_unit value from GUI_App,
 * but for DPIDialogs it's its own value.
 * This value will be used to correct rescale after moving between
 * Displays with different HDPI */
int em_unit(wxWindow* win)
{
    if (win)
    {
        wxTopLevelWindow *toplevel = Slic3r::GUI::find_toplevel_parent(win);
        Slic3r::GUI::DPIDialog* dlg = dynamic_cast<Slic3r::GUI::DPIDialog*>(toplevel);
        if (dlg)
            return dlg->em_unit();
        Slic3r::GUI::DPIFrame* frame = dynamic_cast<Slic3r::GUI::DPIFrame*>(toplevel);
        if (frame)
            return frame->em_unit();
    }

    return Slic3r::GUI::wxGetApp().em_unit();
}

int mode_icon_px_size()
{
#ifdef __APPLE__
    return 10;
#else
    return 12;
#endif
}

wxBitmap create_menu_bitmap(const std::string& bmp_name)
{
    return create_scaled_bitmap(bmp_name, nullptr, 16, false, "", true);
}

static std::unordered_set<std::string> s_bmps_not_found;

// win is used to get a correct em_unit value
// It's important for bitmaps of dialogs.
// if win == nullptr, em_unit value of MainFrame will be used
wxBitmap create_scaled_bitmap(  const std::string& bmp_name_in,
                                wxWindow *win/* = nullptr*/,
                                const int px_cnt/* = 16*/,
                                const bool grayscale/* = false*/,
                                const std::string& new_color/* = std::string()*/, // color witch will used instead of orange
                                const bool menu_bitmap/* = false*/,
                                const bool resize/* = false*/,
                                const bool bitmap2/* = false*/,
                                const vector<std::string>& array_new_color/* = vector<std::string>*/)//used for semi transparent material)
{
    static Slic3r::GUI::BitmapCache cache;

    /* An empty name means the caller failed to resolve the icon (eg. missing printer config).
       Do not throw in this case, a missing icon must not take down the whole process */
    if (bmp_name_in.empty() || bmp_name_in == ".png") {
        BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << ": empty bitmap name";
        return wxNullBitmap;
    }

    if (bitmap2) {
        return create_scaled_bitmap2(bmp_name_in, cache, win, px_cnt, grayscale, resize, array_new_color);
    }
    unsigned int width = 0;
    unsigned int height = (unsigned int) (win->FromDIP(px_cnt) + 0.5f);

    std::string bmp_name = bmp_name_in;
    boost::replace_last(bmp_name, ".png", "");

    bool dark_mode =
#ifdef _WIN32
    menu_bitmap ? Slic3r::GUI::check_dark_mode() :
#endif
    Slic3r::GUI::wxGetApp().dark_mode();

    // Try loading an SVG first, then PNG if SVG is not found:
    wxBitmap *bmp = cache.load_svg(bmp_name, width, height, grayscale, dark_mode, new_color, resize ? em_unit(win) * 0.1f : 0.f);
    if (bmp == nullptr) {
        bmp = cache.load_png(bmp_name, width, height, grayscale, resize ? win->FromDIP(10) * 0.1f : 0.f);
    }

    if (bmp == nullptr) {

        /*stacktrace is time-consuming, optimize it*/
        if (s_bmps_not_found.count(bmp_name) == 0) {
            BOOST_LOG_TRIVIAL(error) << "Could not load bitmap: " << boost::stacktrace::stacktrace();
            s_bmps_not_found.emplace(bmp_name);
        }

        // Neither SVG nor PNG has been found, raise error
        throw Slic3r::RuntimeError("Could not load bitmap: " + bmp_name);
    }

    return *bmp;
}

wxBitmap create_scaled_bitmap2(const std::string& bmp_name_in, Slic3r::GUI::BitmapCache& cache, wxWindow* win/* = nullptr*/ ,
    const int px_cnt/* = 16*/, const bool grayscale/* = false*/ , const bool resize/* = false*/ ,
    const vector<std::string>& array_new_color/* = vector<std::string>()*/) // color witch will used instead of orange
{
    unsigned int width = 0;
    unsigned int height = (unsigned int)(win->FromDIP(px_cnt) + 0.5f);

    std::string bmp_name = bmp_name_in;
    boost::replace_last(bmp_name, ".png", "");

    wxBitmap* bmp = cache.load_svg2(bmp_name, width, height, grayscale, false, array_new_color, resize ? em_unit(win) * 0.1f : 0.f);
    if (bmp == nullptr) {
        /*stacktrace is time-consuming, optimize it*/
        if (s_bmps_not_found.count(bmp_name) == 0) {
            BOOST_LOG_TRIVIAL(error) << "Could not load bitmap: " << boost::stacktrace::stacktrace();
            s_bmps_not_found.emplace(bmp_name);
        }

        throw Slic3r::RuntimeError("Could not load bitmap: " + bmp_name);
    }
    return *bmp;
}


wxBitmap* get_default_extruder_color_icon(bool thin_icon/* = false*/)
{
    static Slic3r::GUI::BitmapCache bmp_cache;

    const double em = Slic3r::GUI::wxGetApp().em_unit();
    const int icon_width = lround((thin_icon ? 2 : 4.5) * em);
    const int icon_height = lround(2 * em);
    bool dark_mode = Slic3r::GUI::wxGetApp().dark_mode();

    wxClientDC cdc((wxWindow*)Slic3r::GUI::wxGetApp().mainframe);
    wxMemoryDC dc(&cdc);
    dc.SetFont(::Label::Body_12);

    wxString label = _L("default");
    std::string bitmap_key = std::string("default_color") + "-h" + std::to_string(icon_height) + "-w" + std::to_string(icon_width)
        + "-i" + label.ToStdString();

    wxBitmap* bitmap = bmp_cache.find(bitmap_key);
    if (bitmap == nullptr) {
        // Paint the color icon.
            //Slic3r::GUI::BitmapCache::parse_color(color, rgb);
            // there is no neede to scale created solid bitmap
        wxColor clr(255, 255, 255, 0);
        bitmap = bmp_cache.insert(bitmap_key, wxBitmap(icon_width, icon_height));
        dc.SelectObject(*bitmap);
        dc.SetBackground(wxBrush(clr));
        dc.Clear();
        dc.SetBrush(wxBrush(clr));
        dc.SetPen(*wxGREY_PEN);
        auto size = dc.GetTextExtent(wxString(label));
        dc.SetTextForeground(clr.GetLuminance() < 0.51 ? *wxWHITE : *wxBLACK);
        dc.DrawText(label, (icon_width - size.x) / 2, (icon_height - size.y) / 2);
        dc.SelectObject(wxNullBitmap);
    }

    return bitmap;
}

std::vector<wxBitmap*> get_extruder_color_icons(bool thin_icon/* = false*/, bool rounded_ring/* = false*/)
{
    // Create the bitmap with color bars.
    std::vector<wxBitmap*> bmps;
    std::vector<std::string> filaments_color_info = Slic3r::GUI::wxGetApp().plater()->get_filament_colors_render_info();
    std::vector<std::string> ctype = Slic3r::GUI::wxGetApp().plater()->get_filament_color_render_type();

    bool multi_color_valid = Slic3r::GUI::wxGetApp().plater()->is_color_size_equal();

    if (multi_color_valid && !filaments_color_info.empty() && !ctype.empty() && ctype.size() == filaments_color_info.size()) {
        std::vector<std::vector<std::string>> readable_color_info = read_color_pack(filaments_color_info);
        /* It's supposed that standard size of an icon is 36px*16px for 100% scaled display.
         * So set sizes for solid_colored icons used for filament preset
         * and scale them in respect to em_unit value
         */
        const double em          = Slic3r::GUI::wxGetApp().em_unit();
        // MD3 swatch: the kit's 28x28 reference size overrides the legacy
        // 2*em thin-icon sizing when rounded_ring is requested.
        const int    icon_width  = rounded_ring ? lround(2.8 * em) : lround((thin_icon ? 2 : 4.4) * em);
        const int    icon_height = rounded_ring ? lround(2.8 * em) : lround(2 * em);

        int index = 0;
        for (const auto &colors : readable_color_info) {
            auto label = std::to_string(++index);
            bool is_gradient = ctype[index-1] == "0";
            if (colors.size() == 1) {
                bmps.push_back(get_extruder_color_icon(colors[0], label, icon_width, icon_height, rounded_ring));
            } else {
                bmps.push_back(get_extruder_color_icon(colors, is_gradient, label, icon_width, icon_height));
            }
        }
    } else {
        std::vector<std::string> colors = Slic3r::GUI::wxGetApp().plater()->get_extruder_colors_from_plater_config();
        if (colors.empty()) return bmps;

        const double em          = Slic3r::GUI::wxGetApp().em_unit();
        const int    icon_width  = rounded_ring ? lround(2.8 * em) : lround((thin_icon ? 2 : 4.4) * em);
        const int    icon_height = rounded_ring ? lround(2.8 * em) : lround(2 * em);
        int index = 0;
        for (const auto &color : colors) {
            auto label = std::to_string(++index);
            bmps.push_back(get_extruder_color_icon(color, label, icon_width, icon_height, rounded_ring));
        }
    }
    return bmps;


}

std::vector<std::vector<std::string>> read_color_pack(std::vector<std::string> color_pack) {
    std::vector<std::vector<std::string>> color_info;
    for (const std::string &color : color_pack) {
        std::vector<std::string> colors;
        colors = Slic3r::split_string(color, ' ');
        color_info.push_back(colors);
    }
    return color_info;
}

wxColourData show_sys_picker_dialog(wxWindow *parent, const wxColourData &clr_data)
{
    wxColourData data = clr_data;
    const wxColour picked = pick_filament_color(parent, clr_data.GetColour(), _L("Please choose the filament colour"));
    if (picked.IsOk())
        data.SetColour(picked);
    return data;
}

void md3_style_data_view(wxDataViewCtrl *view)
{
    view->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    view->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    view->SetFont(::Label::Body_13);
    view->SetRowHeight(view->FromDIP(32));
    view->SetAlternateRowColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    wxItemAttr header;
    header.SetTextColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    header.SetFont(::Label::Head_13);
    view->SetHeaderAttr(header);
}

// Both formats occur in the list: "r,g,b,a" from color_to_string() and
// "#RRGGBB" from older builds.
static wxColour parse_custom_color(const std::string &text)
{
    if (text.empty())
        return wxNullColour;
    return text.find(',') != std::string::npos ? string_to_wxColor(text) : wxColour(text);
}

static bool same_rgb(const wxColour &a, const wxColour &b)
{
    return a.Red() == b.Red() && a.Green() == b.Green() && a.Blue() == b.Blue();
}

std::vector<wxColour> recent_custom_colors()
{
    std::vector<wxColour> colors;
    for (const std::string &text : Slic3r::GUI::wxGetApp().app_config->get_custom_color_from_config()) {
        const wxColour color = parse_custom_color(text);
        if (color.IsOk() && color.Alpha() != wxALPHA_TRANSPARENT)
            colors.push_back(color);
    }
    return colors;
}

void remember_custom_color(const wxColour &color)
{
    if (!color.IsOk() || color.Alpha() == wxALPHA_TRANSPARENT)
        return;
    // The pick goes first and the older entries follow, without the pick's own
    // earlier entry, up to the 16 slots the system dialog had. A short list is
    // written short, so no reader has to parse empty slots.
    std::vector<std::string> recents { color_to_string(color) };
    for (const std::string &previous : Slic3r::GUI::wxGetApp().app_config->get_custom_color_from_config()) {
        const wxColour parsed = parse_custom_color(previous);
        if (!parsed.IsOk() || same_rgb(parsed, color))
            continue;
        if ((int) recents.size() >= CUSTOM_COLOR_COUNT)
            break;
        recents.push_back(previous);
    }
    Slic3r::GUI::wxGetApp().app_config->save_custom_color_to_config(recents);
}

wxColour pick_filament_color(wxWindow *parent, const wxColour &initial, const wxString &title)
{
    MD3ColorPickerDialog::Options options;
    options.recent  = recent_custom_colors();
    options.opacity = false;
    options.title   = title;
    MD3ColorPickerDialog dialog(parent, initial, options);
    if (dialog.ShowModal() != wxID_OK)
        return wxNullColour;
    const wxColour picked = dialog.GetColour();
    remember_custom_color(picked);
    return picked;
}

wxBitmap *get_extruder_color_icon(std::vector<std::string> colors, bool is_gradient, std::string label, int icon_width, int icon_height){

    static Slic3r::GUI::BitmapCache bmp_cache;

    // build cache key, include all color info
    std::string bitmap_key = "";
    for (const auto& color : colors) {
        bitmap_key += color + "_";
    }
    bitmap_key += "h" + std::to_string(icon_height) + "-w" + std::to_string(icon_width) + "-i" + label;

    wxBitmap *bitmap = bmp_cache.find(bitmap_key);
    if (bitmap == nullptr) {

        std::vector<wxColour> wx_colors;
        for (const auto& color_str : colors) {
            wx_colors.push_back(wxColour(color_str));
        }
        if (wx_colors.empty()) {
            wx_colors.push_back(wxColour("#636363")); // default color if no colors provided
        }

        // create filament bitmap in multi color
        wxBitmap base_bitmap = Slic3r::GUI::create_filament_bitmap(wx_colors, wxSize(icon_width, icon_height), is_gradient);

        if (!base_bitmap.IsOk()) {
            // if create failed, return nullptr
            return nullptr;
        }

        // add text label directly on base_bitmap
        if (!label.empty()) {
#ifndef __WXMSW__
            wxMemoryDC dc(base_bitmap);
#else
            wxClientDC cdc((wxWindow *) Slic3r::GUI::wxGetApp().mainframe);
            wxMemoryDC dc(&cdc);
            dc.SelectObject(base_bitmap);
#endif

            // Ensure no background contamination
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(*wxTRANSPARENT_PEN);

            dc.SetFont(::Label::Body_12);
            Slic3r::GUI::WxFontUtils::get_suitable_font_size(icon_height - 2, dc);

            auto size = dc.GetTextExtent(wxString(label));

            // Set transparent background mode for text rendering
            dc.SetBackgroundMode(wxTRANSPARENT);

            // draw text with black border effect
            int text_x = (icon_width - size.x) / 2;
            int text_y = (icon_height - size.y) / 2;

            // Draw very thin border with lighter color and fewer directions
            dc.SetTextForeground(wxColor("262E30")); // Semi-transparent dark gray
            dc.DrawText(label, text_x - 1, text_y);     // Left
            dc.DrawText(label, text_x + 1, text_y);     // Right
            dc.DrawText(label, text_x, text_y - 1);     // Up
            dc.DrawText(label, text_x, text_y + 1);     // Down

            // Draw main white text on top
            dc.SetTextForeground(*wxWHITE);
            dc.DrawText(label, text_x, text_y);

            dc.SelectObject(wxNullBitmap);
        }

        // cache result
        bitmap = bmp_cache.insert(bitmap_key, base_bitmap);
    }
    return bitmap;
}

wxBitmap *get_extruder_color_icon(std::string color, std::string label, int icon_width, int icon_height, bool rounded_ring /* = false */)
{
    static Slic3r::GUI::BitmapCache bmp_cache;

    std::string bitmap_key = color + "-h" + std::to_string(icon_height) + "-w" + std::to_string(icon_width) + "-i" + label + (rounded_ring ? "-r" : "");

    wxBitmap *bitmap = bmp_cache.find(bitmap_key);
    if (bitmap == nullptr) {
        wxColour clr(color);
        wxBitmap base_bitmap = Slic3r::GUI::create_filament_bitmap({clr}, wxSize(icon_width, icon_height), false);
        if (!base_bitmap.IsOk())
            return nullptr;

        if (rounded_ring || !label.empty()) {
#ifndef __WXMSW__
            wxMemoryDC dc(base_bitmap);
#else
            wxClientDC cdc((wxWindow *) Slic3r::GUI::wxGetApp().mainframe);
            wxMemoryDC dc(&cdc);
            dc.SelectObject(base_bitmap);
#endif
            dc.SetFont(::Label::Body_12);
            Slic3r::GUI::WxFontUtils::get_suitable_font_size(icon_height - 2, dc);

            if (rounded_ring) {
                // MD3 filament-row swatch (prepare/filament-rows-preset-combobox-not-
                // inforow): r8 rounded corners + a 1px inset OutlineVariant ring,
                // geometry only -- the colour data itself is unchanged. Radius is
                // proportional to MD3::Metrics::radius_tiny (8) at the kit's 28px
                // reference size, so callers passing a different DPI-scaled size
                // still get a correctly-scaled corner. Corners outside the rounded
                // rect are filled with the row's own SurfaceContainerHighest token
                // (rather than relying on per-pixel alpha, which the wxMemoryDC path
                // selected above does not reliably round-trip on every platform).
                const wxColour row_bg = StateColor::semantic(MD3::Role::SurfaceContainerHighest);
                dc.SetBackground(wxBrush(row_bg));
                dc.Clear();
                const double radius = icon_height * static_cast<double>(MD3::Metrics::radius_tiny) / 28.0;
                std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
                if (gc) {
                    gc->SetPen(*wxTRANSPARENT_PEN);
                    gc->SetBrush(wxBrush(clr.Alpha() == 0 ? *wxWHITE : clr));
                    gc->DrawRoundedRectangle(0.5, 0.5, icon_width - 1.0, icon_height - 1.0, radius);
                    const bool     dark = Slic3r::GUI::wxGetApp().dark_mode();
                    const wxColour ring = MD3::resolve(MD3::Role::OutlineVariant, dark);
                    gc->SetPen(wxPen(ring, 1));
                    gc->SetBrush(*wxTRANSPARENT_BRUSH);
                    gc->DrawRoundedRectangle(0.5, 0.5, icon_width - 1.0, icon_height - 1.0, radius);
                }
                if (!label.empty()) {
                    auto size = dc.GetTextExtent(wxString(label));
                    dc.SetBackgroundMode(wxTRANSPARENT);
                    dc.SetTextForeground(clr.Alpha() != 0 && clr.GetLuminance() < 0.51 ? *wxWHITE : *wxBLACK);
                    dc.DrawText(label, (icon_width - size.x) / 2, (icon_height - size.y) / 2);
                }
            } else {
                // Upstream's plain swatch label: transparent text background, text
                // colour by the swatch's luminance.
                dc.SetBackgroundMode(wxTRANSPARENT);
                auto size = dc.GetTextExtent(wxString(label));
                if (clr.Alpha() == 0)
                    dc.SetTextForeground(*wxBLACK);
                else
                    dc.SetTextForeground(clr.GetLuminance() < 0.51 ? *wxWHITE : *wxBLACK);
                dc.DrawText(label, (icon_width - size.x) / 2, (icon_height - size.y) / 2);
            }
            dc.SelectObject(wxNullBitmap);
        }

        bitmap = bmp_cache.insert(bitmap_key, base_bitmap);
    }
    return bitmap;
}

void apply_extruder_selector(Slic3r::GUI::BitmapComboBox** ctrl,
                             wxWindow* parent,
                             const std::string& first_item/* = ""*/,
                             wxPoint pos/* = wxDefaultPosition*/,
                             wxSize size/* = wxDefaultSize*/,
                             bool use_thin_icon/* = false*/)
{
    std::vector<wxBitmap*> icons = get_extruder_color_icons(use_thin_icon);

    if (!*ctrl) {
        *ctrl = new Slic3r::GUI::BitmapComboBox(parent, wxID_ANY, wxEmptyString, pos, size, 0, nullptr, wxCB_READONLY);
        Slic3r::GUI::wxGetApp().UpdateDarkUI(*ctrl);
    }
    else
    {
        (*ctrl)->SetPosition(pos);
        (*ctrl)->SetMinSize(size);
        (*ctrl)->SetSize(size);
        (*ctrl)->Clear();
    }
    if (first_item.empty())
        (*ctrl)->Hide();    // to avoid unwanted rendering before layout (ExtruderSequenceDialog)

    if (icons.empty() && !first_item.empty()) {
        (*ctrl)->Append(_(first_item), wxNullBitmap);
        return;
    }

    // For ObjectList we use short extruder name (just a number)
    const bool use_full_item_name = dynamic_cast<Slic3r::GUI::ObjectList*>(parent) == nullptr;

    int i = 0;
    wxString str = _(L("Extruder"));
    for (wxBitmap* bmp : icons) {
        if (i == 0) {
            if (!first_item.empty())
                (*ctrl)->Append(_(first_item), *bmp);
            ++i;
        }

        (*ctrl)->Append(use_full_item_name
                        ? Slic3r::GUI::from_u8((boost::format("%1% %2%") % str % i).str())
                        : wxString::Format("%d", i), *bmp);
        ++i;
    }
    (*ctrl)->SetSelection(0);
}

// ----------------------------------------------------------------------------
// MenuWithSeparators
// ----------------------------------------------------------------------------

void MenuWithSeparators::DestroySeparators()
{
    if (m_separator_frst) {
        Destroy(m_separator_frst);
        m_separator_frst = nullptr;
    }

    if (m_separator_scnd) {
        Destroy(m_separator_scnd);
        m_separator_scnd = nullptr;
    }
}

void MenuWithSeparators::SetFirstSeparator()
{
    m_separator_frst = this->AppendSeparator();
}

void MenuWithSeparators::SetSecondSeparator()
{
    m_separator_scnd = this->AppendSeparator();
}

// ----------------------------------------------------------------------------
// BambuBitmap
// ----------------------------------------------------------------------------
ScalableBitmap::ScalableBitmap( wxWindow *parent,
                                const std::string& icon_name/* = ""*/,
                                const int px_cnt/* = 16*/,
                                const bool grayscale/* = false*/,
                                const bool resize/* = false*/,
                                const bool bitmap2/* = false*/,
                                const std::vector<std::string>& new_color/* = vector<std::string>*/) :
    m_parent(parent), m_icon_name(icon_name),
    m_px_cnt(px_cnt), m_grayscale(grayscale), m_resize(resize) // BBS: support resize by fill border
{
    m_bmp = create_scaled_bitmap(icon_name, parent, px_cnt, m_grayscale, new_color.empty() ? std::string() : new_color.front(), false, resize, bitmap2, new_color);
    if (px_cnt == 0) {
        m_px_cnt = m_bmp.GetHeight(); // scale
        unsigned int height = (unsigned int) (parent->FromDIP(m_px_cnt) + 0.5f);
        if (height != GetBmpHeight())
            msw_rescale();
    }
}

wxSize ScalableBitmap::GetBmpSize() const
{
#ifdef __APPLE__
    return m_bmp.GetScaledSize();
#else
    return m_bmp.GetSize();
#endif
}

wxSize ScalableBitmap::GetBmpSize(const wxBitmap &bmp)
{
#ifdef __APPLE__
    return bmp.GetScaledSize();
#else
    return bmp.GetSize();
#endif
}

int ScalableBitmap::GetBmpWidth() const
{
    if (!m_bmp.IsOk())
        return 0;
#ifdef __APPLE__
    return m_bmp.GetScaledWidth();
#else
    return m_bmp.GetWidth();
#endif
}

int ScalableBitmap::GetBmpHeight() const
{
    if (!m_bmp.IsOk())
        return 0;
#ifdef __APPLE__
    return m_bmp.GetScaledHeight();
#else
    return m_bmp.GetHeight();
#endif
}


void ScalableBitmap::msw_rescale()
{
    // A bitmap wrapped from a ready wxBitmap has no resource to reload.
    if (m_icon_name.empty()) return;
    // BBS: support resize by fill border
    m_bmp = create_scaled_bitmap(m_icon_name, m_parent, m_px_cnt, m_grayscale, std::string(), false, m_resize);
}

// ----------------------------------------------------------------------------
// ScalableButton
// ----------------------------------------------------------------------------

ScalableButton::ScalableButton( wxWindow *          parent,
                                wxWindowID          id,
                                const std::string&  icon_name /*= ""*/,
                                const wxString&     label /* = wxEmptyString*/,
                                const wxSize&       size /* = wxDefaultSize*/,
                                const wxPoint&      pos /* = wxDefaultPosition*/,
                                long                style /*= wxBU_EXACTFIT | wxNO_BORDER*/,
                                bool                use_default_disabled_bitmap/* = false*/,
                                int                 bmp_px_cnt/* = 16*/) :
    Button(parent, label, wxString(), 0, 0, id),
    m_parent(parent),
    m_current_icon_name(icon_name),
    m_use_default_disabled_bitmap (use_default_disabled_bitmap),
    m_px_cnt(bmp_px_cnt)
{
    if (pos != wxDefaultPosition)
        Move(pos);

    init_style(label, size, style);

    if (!icon_name.empty()) {
        apply_bitmap(create_scaled_bitmap(icon_name, parent, m_px_cnt));
        update_disabled_bitmap(true);
    }

    if (size != wxDefaultSize)
    {
        const int em = em_unit(parent);
        m_width = size.x * 10 / em;
        m_height= size.y * 10 / em;
        SetMinSize(size);
    }
    // A window made without a size starts at its minimum, as the native button started at its best size.
    SetSize(GetEffectiveMinSize());
}


ScalableButton::ScalableButton( wxWindow *          parent,
                                wxWindowID          id,
                                const ScalableBitmap&  bitmap,
                                const wxString&     label /*= wxEmptyString*/,
                                long                style /*= wxBU_EXACTFIT | wxNO_BORDER*/) :
    Button(parent, label, wxString(), 0, 0, id),
    m_parent(parent),
    m_current_icon_name(bitmap.name()),
    m_px_cnt(bitmap.px_cnt())
{
    init_style(label, wxDefaultSize, style);
    apply_bitmap(bitmap.bmp());
    update_disabled_bitmap(false);
    SetSize(GetEffectiveMinSize());
}

void ScalableButton::init_style(const wxString& label, const wxSize& size, long style)
{
    m_restyling = true;
    if (label.IsEmpty()) {
        // The flat icon button: the kit icon button, which washes on hover and rings on
        // focus, a little larger than the icon so the wash reads around it. A size the
        // caller gave wins over the icon size, as it did for the native button.
        int container = m_px_cnt + 6;
        if (size.x > 0 && size.y > 0)
            container = std::max(ToDIP(size.x), ToDIP(size.y));
        SetIconButton(container > 36 ? Button::IconShape::Square : Button::IconShape::Circle, container);
    } else {
        SetButtonSize(Button::Size::Small);
        SetVariant(Button::Variant::Outlined);
        if (style & wxBU_LEFT)
            SetCenter(false);
    }
    m_restyling = false;
}

void ScalableButton::apply_bitmap(const wxBitmap& bitmap)
{
    m_bitmap = bitmap;
    m_restyling = true;
    SetIconBitmap(bitmap);
    m_restyling = false;
    reassert_style();
}

// The bitmap drawn while the button is disabled: the one the caller supplied, else the
// greyscale icon when the caller asked for the default one, else the normal bitmap made
// disabled, which is what the native button did for every bitmap it was given.
void ScalableButton::update_disabled_bitmap(bool from_icon_name)
{
    if (m_has_explicit_disabled)
        return;
    wxBitmap disabled;
    if (from_icon_name && m_use_default_disabled_bitmap && !m_current_icon_name.empty())
        disabled = create_scaled_bitmap(m_current_icon_name, m_parent, m_px_cnt, true);
    else if (m_bitmap.IsOk())
        disabled = m_bitmap.ConvertToDisabled();
    SetIconBitmapDisabled(disabled);
}

// The window colour shows outside the rounded shape and the resting fill inside it; both
// are the surface behind a flat icon button.
void ScalableButton::set_surface(const wxColour& colour)
{
    Button::SetBackgroundColour(colour);
    background_color.setColorForStates(colour, StateColor::Normal);
    Refresh();
}

// Each time the kit restyles the button it re-derives the colours, the radius, the padding
// and the minimum size from the parent and its size tier. Put back what a caller asked for.
void ScalableButton::reassert_style()
{
    if (m_backdrop.IsOk())
        set_surface(StateColor::isDarkMode() ? StateColor::darkModeColorFor(m_backdrop) : StateColor::lightModeColorFor(m_backdrop));
    if (m_min_size != wxDefaultSize)
        Button::SetMinSize(m_min_size);
}

void ScalableButton::SetMinSize(const wxSize& size)
{
    m_min_size = size;
    Button::SetMinSize(size);
}

bool ScalableButton::SetBackgroundColour(const wxColour& colour)
{
    const bool changed = Button::SetBackgroundColour(colour);
    if (colour.IsOk()) {
        // The kit's own restyle passes the parent colour through here; only a colour a
        // caller named is the surface to keep.
        if (!m_restyling)
            m_backdrop = colour;
        if (background_color.setColorForStates(colour, StateColor::Normal))
            Refresh();
    }
    return changed;
}

void ScalableButton::SetBitmap(const wxBitmap& bitmap, wxDirection /*dir*/)
{
    apply_bitmap(bitmap);
    update_disabled_bitmap(false);
}

void ScalableButton::SetBitmapDisabled(const wxBitmap& bitmap)
{
    m_has_explicit_disabled = bitmap.IsOk();
    if (m_has_explicit_disabled)
        SetIconBitmapDisabled(bitmap);
    else
        update_disabled_bitmap(false);
}

void ScalableButton::SetBitmap_(const ScalableBitmap& bmp)
{
    SetBitmap(bmp.bmp());
    m_current_icon_name = bmp.name();
}

bool ScalableButton::SetBitmap_(const std::string& bmp_name)
{
    if (m_current_icon_name == bmp_name)
    {
        return true;
    }

    m_current_icon_name = bmp_name;
    if (m_current_icon_name.empty())
        return false;

    apply_bitmap(create_scaled_bitmap(m_current_icon_name, m_parent, m_px_cnt));
    update_disabled_bitmap(true);
    return true;
}

void ScalableButton::SetBitmapDisabled_(const ScalableBitmap& bmp)
{
    SetBitmapDisabled(bmp.bmp());
    m_disabled_icon_name = bmp.name();
}

int ScalableButton::GetBitmapHeight()
{
#ifdef __APPLE__
    return GetBitmap().GetScaledHeight();
#else
    return GetBitmap().GetHeight();
#endif
}

void ScalableButton::UseDefaultBitmapDisabled()
{
    m_use_default_disabled_bitmap = true;
    m_has_explicit_disabled = false;
    update_disabled_bitmap(true);
}

void ScalableButton::msw_rescale()
{
    if (!m_current_icon_name.empty()) {
        apply_bitmap(create_scaled_bitmap(m_current_icon_name, m_parent, m_px_cnt));
        if (!m_disabled_icon_name.empty())
            SetIconBitmapDisabled(create_scaled_bitmap(m_disabled_icon_name, m_parent, m_px_cnt));
        else
            update_disabled_bitmap(true);
    }

    // Derives the kit radius, padding, height and the parent surface colour again, for
    // the current scale and theme.
    m_restyling = true;
    Rescale();
    m_restyling = false;

    if (m_width > 0 || m_height>0)
    {
        const int em = em_unit(m_parent);
        m_min_size = wxSize(m_width > 0 ? m_width * em / 10 : -1, m_height > 0 ? m_height * em / 10 : -1);
    }
    reassert_style();
}


// ----------------------------------------------------------------------------
// BlinkingBitmap
// ----------------------------------------------------------------------------

BlinkingBitmap::BlinkingBitmap(wxWindow* parent, const std::string& icon_name) :
    wxStaticBitmap(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(int(1.6 * Slic3r::GUI::wxGetApp().em_unit()), -1))
{
    bmp = ScalableBitmap(parent, icon_name);
}

void BlinkingBitmap::msw_rescale()
{
    bmp.msw_rescale();
    this->SetSize(bmp.GetBmpSize());
    this->SetMinSize(bmp.GetBmpSize());
}

void BlinkingBitmap::invalidate()
{
    this->SetBitmap(wxNullBitmap);
}

void BlinkingBitmap::activate()
{
    this->SetBitmap(bmp.bmp());
    show = true;
}

void BlinkingBitmap::blink()
{
    show = !show;
    this->SetBitmap(show ? bmp.bmp() : wxNullBitmap);
}


wxIMPLEMENT_CLASS(ImageTransientPopup,PopupWindow);

wxBEGIN_EVENT_TABLE(ImageTransientPopup,PopupWindow)
    EVT_MOUSE_EVENTS( ImageTransientPopup::OnMouse )
    EVT_SIZE( ImageTransientPopup::OnSize )
    EVT_SET_FOCUS( ImageTransientPopup::OnSetFocus )
    EVT_KILL_FOCUS( ImageTransientPopup::OnKillFocus )
wxEND_EVENT_TABLE()

ImageTransientPopup::ImageTransientPopup( wxWindow *parent, bool scrolled, wxBitmap bmp)
                     :PopupWindow( parent,
                                              wxBORDER_NONE |
                                              wxPU_CONTAINS_CONTROLS )
{
    m_panel = new MD3ScrolledWindow( this, wxID_ANY );
    m_panel->SetBackgroundColour( *wxLIGHT_GREY );

    // Keep this code to verify if mouse events work, they're required if
    // you're making a control like a combobox where the items are highlighted
    // under the cursor, the m_panel is set focus in the Popup() function
    m_panel->Bind(wxEVT_MOTION, &ImageTransientPopup::OnMouse, this);

    m_image = new wxStaticBitmap(m_panel,
        wxID_ANY, bmp);

    wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
    topSizer->Add(m_image, 1, wxCENTRE | wxALL | wxEXPAND, 0);
    topSizer->SetMinSize(300, 300);

    if ( scrolled )
    {
        // Add a big window to ensure that scrollbars are shown when we set the
        // panel size to a lesser size below.
        topSizer->Add(new wxPanel(m_panel, wxID_ANY, wxDefaultPosition,
                                  wxSize(600, 600)));
    }

    m_panel->SetSizer( topSizer );
    if ( scrolled )
    {
        // Set the fixed size to ensure that the scrollbars are shown.
        m_panel->SetSize(300, 300);

        // And also actually enable them.
        m_panel->SetScrollRate(10, 10);
    }
    else
    {
        // Use the fitting size for the panel if we don't need scrollbars.
        topSizer->Fit(m_panel);
    }

    SetClientSize(m_panel->GetSize());
}

ImageTransientPopup::~ImageTransientPopup()
{
}

void ImageTransientPopup::SetImage(wxBitmap bmp)
{
    m_image->SetBitmap(bmp);
    m_panel->Layout();
}

void ImageTransientPopup::Popup(wxWindow* WXUNUSED(focus))
{
    PopupWindow::Popup();
}

void ImageTransientPopup::OnDismiss()
{
    PopupWindow::OnDismiss();
}

bool ImageTransientPopup::ProcessLeftDown(wxMouseEvent& event)
{
    return PopupWindow::ProcessLeftDown(event);
}
bool ImageTransientPopup::Show( bool show )
{
    return PopupWindow::Show(show);
}

void ImageTransientPopup::OnSize(wxSizeEvent &event)
{
    event.Skip();
}

void ImageTransientPopup::OnSetFocus(wxFocusEvent &event)
{
    event.Skip();
}

void ImageTransientPopup::OnKillFocus(wxFocusEvent &event)
{
    event.Skip();
}

void ImageTransientPopup::OnMouse(wxMouseEvent &event)
{
    event.Skip();
}




