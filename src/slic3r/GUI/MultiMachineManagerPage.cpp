#include "MultiMachineManagerPage.hpp"
#include "Bulk/BulkActionPlan.hpp"
#include "Bulk/BulkActionPreviewDialog.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "NotificationManager.hpp"
#include "Plater.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/SearchField.hpp"

#include "DeviceCore/DevManager.h"
#include "Widgets/Label.hpp"

#include <algorithm>
#include <fstream>

#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/wrapsizer.h>

namespace Slic3r {
namespace GUI {

namespace {

// Logical size of the kit CheckBox glyph plus the gap to the icon tile; the
// card header columns shift right by this much so nothing overlaps the box.
constexpr int CARD_CHECKBOX_PX  = 20;
constexpr int CARD_CHECKBOX_GAP = 8;

std::string json_escape(const std::string& in)
{
    std::string out;
    out.reserve(in.size() + 8);
    for (const unsigned char c : in) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

std::string csv_escape(const std::string& in)
{
    if (in.find_first_of(",\"\r\n") == std::string::npos)
        return in;
    std::string out = "\"";
    for (const char c : in) {
        if (c == '"') out += "\"\""; else out.push_back(c);
    }
    out += "\"";
    return out;
}

} // namespace

MultiMachineItem::MultiMachineItem(wxWindow* parent, MachineObject* obj)
    : DeviceItem(parent, obj)
{
    // Background matches the farm scroll surface so the rounded-card gutters blend;
    // the card fill (SurfaceContainerLow) is painted inside doRender().
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    SetMinSize(wxSize(FromDIP(DEVICE_CARD_WIDTH), FromDIP(DEVICE_CARD_HEIGHT)));
    SetMaxSize(wxSize(FromDIP(DEVICE_CARD_WIDTH), FromDIP(DEVICE_CARD_HEIGHT)));

    // Bulk-selection checkbox, vertically centred on the 44px icon tile row.
    // It is a real child control so it is keyboard-reachable and carries its
    // own accessible name; the card paint leaves its column free.
    m_check = new CheckBox(this);
    m_check->SetColorScheme(MD3::ColorScheme::Device);
    m_check->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    {
        const int pad  = FromDIP(16);
        const int tile = FromDIP(44);
        m_check->SetPosition(wxPoint(pad, pad + (tile - FromDIP(CARD_CHECKBOX_PX)) / 2));
    }
    m_check->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent&) {
        m_selected = m_check->GetValue();
        update_accessible_name();
        Refresh();
        if (m_on_toggle)
            m_on_toggle(this, wxGetKeyState(WXK_SHIFT));
    });
    update_accessible_name();

    Bind(wxEVT_PAINT, &MultiMachineItem::paintEvent, this);
    Bind(wxEVT_ENTER_WINDOW, &MultiMachineItem::OnEnterWindow, this);
    Bind(wxEVT_LEAVE_WINDOW, &MultiMachineItem::OnLeaveWindow, this);
    Bind(wxEVT_LEFT_DOWN, &MultiMachineItem::OnLeftDown, this);
    Bind(wxEVT_MOTION, &MultiMachineItem::OnMove, this);
    Bind(EVT_MULTI_DEVICE_VIEW, [this, obj](auto& e) {
        wxGetApp().mainframe->jump_to_monitor(obj->get_dev_id());
        if (wxGetApp().mainframe->m_monitor->get_status_panel()->get_media_play_ctrl()) {
            wxGetApp().mainframe->m_monitor->get_status_panel()->get_media_play_ctrl()->jump_to_play();
        }
    });
    wxGetApp().UpdateDarkUIWin(this);
}

void MultiMachineItem::SetSelected(bool selected)
{
    if (m_selected == selected && m_check && m_check->GetValue() == selected)
        return;
    m_selected = selected;
    if (m_check)
        m_check->SetValue(selected);
    update_accessible_name();
    Refresh();
}

void MultiMachineItem::update_accessible_name()
{
    const wxString name = obj_ ? wxString::FromUTF8(obj_->get_dev_name()) : wxString();
    // TRN: Accessible name of a device card; %1$s device name, %2$s "selected" or "not selected".
    SetName(wxString::Format(_L("%s, %s"), name, m_selected ? _L("selected") : _L("not selected")));
    if (m_check)
        // TRN: Accessible name of the per-device selection checkbox; %s is the device name.
        m_check->SetName(wxString::Format(_L("Select %s"), name));
}

void MultiMachineItem::OnEnterWindow(wxMouseEvent& evt)
{
    m_hover = true;
    Refresh();
}

void MultiMachineItem::OnLeaveWindow(wxMouseEvent& evt)
{
    m_hover = false;
    Refresh();
}

void MultiMachineItem::OnLeftDown(wxMouseEvent& evt)
{
    // The whole card is the click-through affordance to the device monitor (the
    // legacy anatomy hit-tested a far-right "View" button region that no longer
    // exists in the card layout). Still posts the same EVT_MULTI_DEVICE_VIEW so
    // the monitor jump / media playback wiring is preserved.
    post_event(wxCommandEvent(EVT_MULTI_DEVICE_VIEW));
    evt.Skip();
}

void MultiMachineItem::OnMove(wxMouseEvent& evt)
{
    SetCursor(wxCURSOR_HAND);
    evt.Skip();
}

void MultiMachineItem::paintEvent(wxPaintEvent& evt)
{
    wxPaintDC dc(this);
    render(dc);
}

void MultiMachineItem::render(wxDC& dc)
{
#ifdef __WXMSW__
    wxSize     size = GetSize();
    wxMemoryDC memdc;
    wxBitmap   bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    memdc.Blit({ 0, 0 }, size, &dc, { 0, 0 });

    {
        wxGCDC dc2(memdc);
        doRender(dc2);
    }

    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    doRender(dc);
#endif
}

void MultiMachineItem::DrawTextWithEllipsis(wxDC& dc, const wxString& text, int maxWidth, int left, int top) {
    wxSize size = GetSize();
    wxFont font = dc.GetFont();

    wxSize textSize = dc.GetTextExtent(text);
    dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    int textWidth = textSize.GetWidth();

    if (textWidth > maxWidth) {
        wxString truncatedText = text;
        int ellipsisWidth = dc.GetTextExtent("...").GetWidth();
        int numChars = text.length();

        for (int i = numChars - 1; i >= 0; --i) {
            truncatedText = text.substr(0, i) + "...";
            int truncatedWidth = dc.GetTextExtent(truncatedText).GetWidth();

            if (truncatedWidth <= maxWidth - ellipsisWidth) {
                break;
            }
        }

        if (top == 0) {
            dc.DrawText(truncatedText, left, (size.y - textSize.y) / 2);
        }
        else {
            dc.DrawText(truncatedText, left, (size.y - textSize.y) / 2 - top);
        }

    }
    else {
        if (top == 0) {
            dc.DrawText(text, left, (size.y - textSize.y) / 2);
        }
        else {
            dc.DrawText(text, left, (size.y - textSize.y) / 2 - top);
        }
    }
}

void MultiMachineItem::doRender(wxDC& dc)
{
    // MD3 device-farm card (ui-md3 Multi.jsx): a single Card per device with a
    // printer-icon tile + name/model, a status dot, a camera-thumbnail
    // placeholder, and a progress bar. All geometry is DPI-scaled via FromDIP.
    const wxSize size = GetSize();

    const int pad     = FromDIP(16);
    const int innerW  = size.x - 2 * pad;
    const int glyphOk = MaterialIcon::available();

    // ---- Card surface + interactive hover border (Card.jsx: sc-low fill,
    // 1px outline-variant, primary on hover, r16) ----
    dc.SetPen(wxPen(StateColor::semantic((m_hover || m_selected) ? MD3::Role::Primary : MD3::Role::OutlineVariant)));
    dc.SetBrush(wxBrush(StateColor::semantic(m_selected ? MD3::Role::SecondaryContainer : MD3::Role::SurfaceContainerLow)));
    dc.DrawRoundedRectangle(0, 0, size.x - 1, size.y - 1, FromDIP(16));

    if (!obj_)
        return;

    // The checkbox child sits at the far left of the header row; every header
    // column starts after it.
    const int checkW = FromDIP(CARD_CHECKBOX_PX) + FromDIP(CARD_CHECKBOX_GAP);

    // Local ellipsizing text draw (top-left anchored, unlike the vertically
    // centered DrawTextWithEllipsis used by the legacy row).
    auto draw_elided = [&](const wxString& text, const wxColour& colour, int x, int y, int maxWidth) {
        dc.SetTextForeground(colour);
        wxString out = text;
        if (dc.GetTextExtent(out).GetWidth() > maxWidth) {
            const int ellipsisW = dc.GetTextExtent("...").GetWidth();
            for (int i = (int) text.length() - 1; i >= 0; --i) {
                out = text.substr(0, i) + "...";
                if (dc.GetTextExtent(out).GetWidth() <= maxWidth - ellipsisW)
                    break;
            }
        }
        dc.DrawText(out, x, y);
        return dc.GetTextExtent(out).GetWidth();
    };

    // ---- Header: icon tile + name/model + status dot ----
    const int tile = FromDIP(44);
    const int headTop = pad;
    // icon tile (r12 sc-highest + print glyph 26 on-surface-variant)
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHighest)));
    dc.DrawRoundedRectangle(pad + checkW, headTop, tile, tile, FromDIP(12));
    if (glyphOk) {
        MaterialIcon::drawCentered(dc, MaterialIcon::Print, FromDIP(26),
            StateColor::semantic(MD3::Role::OnSurfaceVariant), wxRect(pad + checkW, headTop, tile, tile));
    }

    // status dot + text (right-aligned within the header row)
    wxString statusText = get_state_device();
    MD3::Role dotRole = MD3::Role::Primary;
    if (!obj_->is_online()) {
        statusText = _L("Offline");
        dotRole    = MD3::Role::Error;
    } else if (state_device == 2) {
        dotRole = MD3::Role::Error;
    } else if (state_device == 0 || state_device == 7) {
        dotRole = MD3::Role::Outline;
    }
    dc.SetFont(Label::Body_12);
    const int dotSize  = FromDIP(8);
    const int statusTW = dc.GetTextExtent(statusText).GetWidth();
    const int statusRight = size.x - pad;
    const int statusTextX = statusRight - statusTW;
    const int dotX = statusTextX - FromDIP(6) - dotSize;
    const int headCenterY = headTop + tile / 2;
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(dotRole)));
    dc.DrawEllipse(dotX, headCenterY - dotSize / 2, dotSize, dotSize);
    dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    dc.DrawText(statusText, statusTextX, headCenterY - dc.GetTextExtent(statusText).GetHeight() / 2);

    // name (14/600) + model/task sub-line (11.5 on-surface-variant), ellipsized
    const int textX    = pad + checkW + tile + FromDIP(12);
    const int textMaxW = (dotX - FromDIP(8)) - textX;
    wxString dev_name = wxString::FromUTF8(obj_->get_dev_name());
    // Sub-line surfaces the running job when printing (as the legacy row did),
    // otherwise the printer model per the kit.
    wxString sub_line = wxString::FromUTF8(obj_->printer_type);
    if (obj_->is_in_printing() && !obj_->subtask_name.empty())
        sub_line = GUI::from_u8(obj_->subtask_name);
    dc.SetFont(Label::Head_14);
    const int nameH = dc.GetTextExtent(dev_name).GetHeight();
    dc.SetFont(Label::Body_12);
    const int subH = dc.GetTextExtent(sub_line).GetHeight();
    const int blockH = nameH + FromDIP(2) + subH;
    int ty = headTop + (tile - blockH) / 2;
    if (ty < headTop) ty = headTop;
    dc.SetFont(Label::Head_14);
    draw_elided(dev_name, StateColor::semantic(MD3::Role::OnSurface), textX, ty, textMaxW > 0 ? textMaxW : innerW);
    dc.SetFont(Label::Body_12);
    draw_elided(sub_line, StateColor::semantic(MD3::Role::OnSurfaceVariant), textX, ty + nameH + FromDIP(2), textMaxW > 0 ? textMaxW : innerW);

    // ---- Camera-thumbnail placeholder (r12 sc-highest + videocam glyph).
    // Real device thumbnails are DATA; this is the idle placeholder. ----
    const int camTop = headTop + tile + FromDIP(12);
    const int camH   = FromDIP(84);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHighest)));
    dc.DrawRoundedRectangle(pad, camTop, innerW, camH, FromDIP(12));
    if (glyphOk) {
        MaterialIcon::drawCentered(dc, MaterialIcon::Videocam, FromDIP(30),
            StateColor::semantic(MD3::Role::Outline), wxRect(pad, camTop, innerW, camH));
    }

    // ---- Progress caption + bar (preserve the legacy progress binding) ----
    const int barH   = FromDIP(8);
    const int barY   = size.y - pad - barH;
    const int capY   = barY - FromDIP(4) - FromDIP(15);
    float progress   = 0.0f;
    wxString caption;
    wxColour captionColour = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    if (state_device > 2 && state_device < 7) {
        if (obj_->get_curr_stage() == _L("Printing") && obj_->subtask_) {
            progress = static_cast<float>(obj_->subtask_->task_progress) / 100.0f;
            caption  = wxString::Format("%d", obj_->subtask_->task_progress) + "%  |  " + get_left_time(obj_->mc_left_time);
            captionColour = StateColor::semantic(MD3::Role::Primary);
        } else {
            caption = obj_->get_curr_stage();
        }
    }
    if (!caption.IsEmpty()) {
        dc.SetFont(Label::Mono_12);
        draw_elided(caption, captionColour, pad, capY, innerW);
    }
    // track
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHighest)));
    dc.DrawRoundedRectangle(pad, barY, innerW, barH, FromDIP(4));
    if (progress > 0.0f) {
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Primary)));
        int fillW = std::max(barH, static_cast<int>(innerW * progress));
        dc.DrawRoundedRectangle(pad, barY, fillW, barH, FromDIP(4));
    }
}

void MultiMachineItem::post_event(wxCommandEvent&& event)
{
    event.SetEventObject(this);
    event.SetString(obj_->get_dev_id());
    event.SetInt(state_selected);
    wxPostEvent(this, event);
}

void MultiMachineItem::DoSetSize(int x, int y, int width, int height, int sizeFlags /*= wxSIZE_AUTO*/)
{
    wxWindow::DoSetSize(x, y, width, height, sizeFlags);
}

wxString MultiMachineItem::get_left_time(int mc_left_time)
{
    // update gcode progress
    std::string left_time;
    wxString    left_time_text = _L("N/A");

    try {
        left_time = get_bbl_monitor_time_dhm(mc_left_time);
    }
    catch (...) {
        ;
    }

    if (!left_time.empty()) left_time_text = wxString::Format("-%s", left_time);
    return left_time_text;
}


MultiMachineManagerPage::MultiMachineManagerPage(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    m_main_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_main_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_main_sizer = new wxBoxSizer(wxVERTICAL);

    StateColor head_bg(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Outline), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Normal)
    );

    //edit prints
    auto m_btn_bg_enable = StateColor(
        std::pair<wxColour, int>(ThemeColor::BrandGreenPressed, StateColor::Pressed),
        std::pair<wxColour, int>(ThemeColor::BrandGreenHovered, StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Primary), StateColor::Normal)
    );


    // ---- MD3 farm toolbar (ui-md3 Multi.jsx): 'Device farm' title + live
    // SearchField + the Edit-printers flow as a filled (Primary) kit button.
    // Replaces the legacy right-aligned Edit-only row. The sort toggles below
    // (m_table_head_panel) stay functional. ----
    auto* toolbar_sizer = new wxBoxSizer(wxHORIZONTAL);

    auto* farm_title = new Label(m_main_panel, _L("Device farm"));
    farm_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    farm_title->SetFont(Label::Head_20);

    m_search = new SearchField(m_main_panel, _L("Search devices"));
    m_search->SetColorScheme(MD3::ColorScheme::Device);
    m_search->SetMinSize(wxSize(FromDIP(240), FromDIP(40)));
    m_search->SetMaxSize(wxSize(FromDIP(340), FromDIP(40)));
    // Live name/type filter of the card grid: reset to the first page and rebuild
    // so paging tracks the filtered set (see refresh_user_device).
    m_search->SetOnQuery([this](const wxString& kw) {
        // Store the raw query; case handling is delegated to the shared matcher.
        m_search_filter = kw;
        m_search_filter.Trim(true).Trim(false);
        m_current_page = 0;
        refresh_user_device();
    });
    // Re-run the name/type filter (from page 0) when the regex / case /
    // whole-word chrome toggles.
    m_search->SetOnRegexToggle([this](bool) {
        m_current_page = 0;
        refresh_user_device();
    });

    m_button_edit = new Button(m_main_panel, _L("Edit Printers"));
    m_button_edit->SetVariant(Button::Variant::Filled);
    m_button_edit->SetFont(Label::Body_12);
    m_button_edit->SetCornerRadius(FromDIP(18));
    m_button_edit->SetMinSize(wxSize(FromDIP(120), FromDIP(40)));
    m_button_edit->SetMaxSize(wxSize(FromDIP(150), FromDIP(40)));

    m_button_edit->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) {
        MultiMachinePickPage dlg;
        dlg.ShowModal();
        refresh_user_device();
        evt.Skip();
    });

    toolbar_sizer->Add(farm_title, 0, wxALIGN_CENTER_VERTICAL, 0);
    toolbar_sizer->Add(m_search, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(16));
    toolbar_sizer->AddStretchSpacer(1);
    toolbar_sizer->Add(m_button_edit, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(16));

    // Bulk-selection strip: page / all-matches / invert / clear, the counts
    // label, and the bulk actions over the selected devices.
    //   "Send to selected..." is not wired: the multi-machine send dialog
    //   (SendMultiMachinePage) has no entry point that accepts a preselected
    //   device set, so there is nothing to hand the selection to.
    //   "Remove selected from list" is not wired: this page has no single
    //   remove/unbind action of its own (membership is edited in the
    //   Edit Printers picker), so there is no per-device action to run in bulk.
    auto* bulk_sizer = new wxBoxSizer(wxHORIZONTAL);
    const auto make_bulk_button = [this](const wxString& text, Button::Variant variant) {
        auto* b = new Button(m_main_panel, text);
        b->SetVariant(variant);
        b->SetButtonSize(Button::Size::Small);
        b->SetColorScheme(MD3::ColorScheme::Device);
        return b;
    };
    m_button_select_page = make_bulk_button(_L("Select this page"), Button::Variant::Tonal);
    m_button_select_all  = make_bulk_button(_L("Select all matches"), Button::Variant::Tonal);
    m_button_invert      = make_bulk_button(_L("Invert selection"), Button::Variant::Tonal);
    m_button_clear       = make_bulk_button(_L("Clear"), Button::Variant::Text);
    m_button_export      = make_bulk_button(_L("Export selected..."), Button::Variant::Outlined);
    m_button_select_page->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { select_page(); });
    m_button_select_all->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { select_all_matches(); });
    m_button_invert->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { invert_selection(); });
    m_button_clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { clear_selection(); });
    m_button_export->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { bulk_export(); });
    // TRN: Tooltip of the device-farm "Export selected..." bulk action.
    m_button_export->SetToolTip(_L("Writes the selected devices (name, id, model, status, task, progress) to a JSON or CSV file"));
    m_bulk_counts = new Label(m_main_panel, Label::Body_12, wxEmptyString);
    m_bulk_counts->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    bulk_sizer->Add(m_button_select_page, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    bulk_sizer->Add(m_button_select_all, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    bulk_sizer->Add(m_button_invert, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    bulk_sizer->Add(m_button_clear, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    bulk_sizer->Add(m_bulk_counts, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    bulk_sizer->Add(m_button_export, 0, wxALIGN_CENTER_VERTICAL, 0);

    Bind(wxEVT_CHAR_HOOK, &MultiMachineManagerPage::on_char_hook, this);

    // Sort strip: no longer pinned to the fixed farm width; it spans fluidly
    // (added wxEXPAND below) with the two functional sort toggles left-packed.
    m_table_head_panel = new wxPanel(m_main_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_table_head_panel->SetMinSize(wxSize(-1, FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_table_head_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    m_table_head_sizer = new wxBoxSizer(wxHORIZONTAL);

    m_printer_name = new Button(m_table_head_panel, _L("Device Name"), "toolbar_double_directional_arrow", wxNO_BORDER, ICON_SINGLE_SIZE);
    m_printer_name->SetBackgroundColor(head_bg);
    m_printer_name->SetFont(Label::Head_11);
    m_printer_name->SetCornerRadius(0);
    m_printer_name->SetMinSize(wxSize(FromDIP(DEVICE_LEFT_DEV_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_printer_name->SetMaxSize(wxSize(FromDIP(DEVICE_LEFT_DEV_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_printer_name->SetCenter(false);
    m_printer_name->Bind(wxEVT_ENTER_WINDOW, [&](wxMouseEvent& evt) {
        SetCursor(wxCURSOR_HAND);
        });
    m_printer_name->Bind(wxEVT_LEAVE_WINDOW, [&](wxMouseEvent& evt) {
        SetCursor(wxCURSOR_ARROW);
        });
    m_printer_name->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& evt) {
        device_dev_name_big = !device_dev_name_big;
        auto sortcb = [this](ObjState s1, ObjState s2) {
            return device_dev_name_big ? s1.state_dev_name > s2.state_dev_name : s1.state_dev_name < s2.state_dev_name;
        };
        this->m_sort.set_role(sortcb, SortItem::SR_MACHINE_NAME, device_dev_name_big);
        this->refresh_user_device();
    });


    m_task_name = new Button(m_table_head_panel, _L("Task Name"), "", wxNO_BORDER, ICON_SINGLE_SIZE);
    m_task_name->SetBackgroundColor(StateColor::semantic(MD3::Role::SurfaceContainer));
    m_task_name->SetFont(Label::Head_11);
    m_task_name->SetCornerRadius(0);
    m_task_name->SetMinSize(wxSize(FromDIP(DEVICE_LEFT_DEV_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_task_name->SetMaxSize(wxSize(FromDIP(DEVICE_LEFT_DEV_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_task_name->SetCenter(false);



    m_status = new Button(m_table_head_panel, _L("Device Status"), "toolbar_double_directional_arrow", wxNO_BORDER, ICON_SINGLE_SIZE);
    m_status->SetBackgroundColor(head_bg);
    m_status->SetFont(Label::Head_11);
    m_status->SetCornerRadius(0);
    m_status->SetMinSize(wxSize(FromDIP(DEVICE_LEFT_PRO_INFO), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_status->SetMaxSize(wxSize(FromDIP(DEVICE_LEFT_PRO_INFO), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_status->SetCenter(false);
    m_status->Bind(wxEVT_ENTER_WINDOW, [&](wxMouseEvent& evt) {
        SetCursor(wxCURSOR_HAND);
        });
    m_status->Bind(wxEVT_LEAVE_WINDOW, [&](wxMouseEvent& evt) {
        SetCursor(wxCURSOR_ARROW);
        });
    m_status->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& evt) {
        device_state_big = !device_state_big;
        auto sortcb = [this](ObjState s1, ObjState s2) {
            return device_state_big ? s1.state_device > s2.state_device : s1.state_device < s2.state_device;
            };
        this->m_sort.set_role(sortcb, SortItem::SortRule::SR_MACHINE_STATE, device_state_big);
        this->refresh_user_device();
    });


    m_action = new Button(m_table_head_panel, _L("Actions"), "", wxNO_BORDER, ICON_SINGLE_SIZE, false);
    m_action->SetBackgroundColor(StateColor::semantic(MD3::Role::SurfaceContainer));
    m_action->SetFont(Label::Head_11);
    m_action->SetCornerRadius(0);
    m_action->SetMinSize(wxSize(FromDIP(DEVICE_LEFT_PRO_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_action->SetMaxSize(wxSize(FromDIP(DEVICE_LEFT_PRO_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_action->SetCenter(false);


    m_table_head_sizer->AddSpacer(FromDIP(DEVICE_LEFT_PADDING_LEFT));
    m_table_head_sizer->Add(m_printer_name, 0, wxALIGN_CENTER_VERTICAL, 0);
    m_table_head_sizer->Add(m_task_name, 0, wxALIGN_CENTER_VERTICAL, 0);
    m_table_head_sizer->Add(m_status, 0, wxALIGN_CENTER_VERTICAL, 0);
    m_table_head_sizer->Add(m_action, 0, wxLEFT, 0);

    m_table_head_panel->SetSizer(m_table_head_sizer);
    // Card grid has no columns: keep the two functional sort toggles (Device
    // Name / Device Status drive m_sort) but hide the non-interactive column
    // labels so the strip reads as a sort bar rather than a table header. The
    // widgets stay allocated (msw_rescale references them).
    m_task_name->Hide();
    m_action->Hide();
    m_table_head_panel->Layout();

    m_tip_text = new Label(m_main_panel, wxEmptyString, wxALIGN_CENTER);
    m_tip_text->SetLabel(_L("Please select the devices you would like to manage here (up to 6 devices)"));
    m_tip_text->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_tip_text->SetFont(::Label::Head_20);
    m_tip_text->Wrap(-1);

    m_button_add = new Button(m_main_panel, _L("Add"));
    m_button_add->SetVariant(Button::Variant::Filled);
    m_button_add->SetFont(Label::Body_12);
    m_button_add->SetCornerRadius(FromDIP(18));
    m_button_add->SetMinSize(wxSize(FromDIP(90), FromDIP(36)));
    m_button_add->SetMaxSize(wxSize(FromDIP(90), FromDIP(36)));

    m_button_add->Bind(wxEVT_BUTTON, [this](wxCommandEvent& evt) {
        MultiMachinePickPage dlg;
        dlg.ShowModal();
        refresh_user_device();
        evt.Skip();
    });

    m_machine_list = new wxScrolledWindow(m_main_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_machine_list->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_machine_list->SetScrollRate(0, 5);
    // Width is fluid (min = one card column, no max pin) so the grid host fills
    // the panel via wxEXPAND and the wrap sizer reflows the cards; the viewport
    // height stays fixed so overflow scrolls as before.
    m_machine_list->SetMinSize(wxSize(FromDIP(DEVICE_CARD_WIDTH + 2 * DEVICE_CARD_GAP), 10 * FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_machine_list->SetMaxSize(wxSize(-1, 10 * FromDIP(DEVICE_ITEM_MAX_HEIGHT)));

    // Responsive card grid: a wrap sizer reflows the device cards across the
    // available width (list -> grid anatomy per ui-md3 Multi.jsx). Held here as
    // the base wxBoxSizer* member; wxWrapSizer derives from wxBoxSizer.
    m_sizer_machine_list = new wxWrapSizer(wxHORIZONTAL);
    m_machine_list->SetSizer(m_sizer_machine_list);
    // Re-wrap the grid whenever the fluid host is resized (window narrows/widens).
    m_machine_list->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        if (m_sizer_machine_list) {
            m_sizer_machine_list->Layout();
            m_machine_list->FitInside();
        }
        e.Skip();
    });
    m_machine_list->Layout();

    // add flipping page
    StateColor ctrl_bg(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Outline), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerLowest), StateColor::Normal)
    );

    // Pagination strip spans fluidly (added wxEXPAND below); its internal sizer
    // keeps the flip controls right-aligned as before.
    m_flipping_panel = new wxPanel(m_main_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_flipping_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));

    m_flipping_page_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_page_sizer = new wxBoxSizer(wxVERTICAL);
    btn_last_page = new Button(m_flipping_panel, "", "go_last_plate", 0, FromDIP(20));
    btn_last_page->SetMinSize(wxSize(FromDIP(20), FromDIP(20)));
    btn_last_page->SetMaxSize(wxSize(FromDIP(20), FromDIP(20)));
    btn_last_page->SetBackgroundColor(head_bg);
    btn_last_page->Bind(wxEVT_LEFT_DOWN, [&](wxMouseEvent& evt) {
        evt.Skip();
        if (m_current_page == 0)
            return;
        btn_last_page->Enable(false);
        btn_next_page->Enable(false);
        start_timer();
        m_current_page--;
        if (m_current_page < 0)
            m_current_page = 0;
        refresh_user_device();
        update_page_number();
    });
    st_page_number = new Label(m_flipping_panel, wxEmptyString);
    btn_next_page = new Button(m_flipping_panel, "", "go_next_plate", 0, FromDIP(20));
    btn_next_page->SetMinSize(wxSize(FromDIP(20), FromDIP(20)));
    btn_next_page->SetMaxSize(wxSize(FromDIP(20), FromDIP(20)));
    btn_next_page->SetBackgroundColor(head_bg);
    btn_next_page->Bind(wxEVT_LEFT_DOWN, [&](wxMouseEvent& evt) {
        evt.Skip();
        if (m_current_page == m_total_page - 1)
            return;
        btn_last_page->Enable(false);
        btn_next_page->Enable(false);
        start_timer();
        m_current_page++;
        if (m_current_page > m_total_page - 1)
            m_current_page = m_total_page - 1;
        refresh_user_device();
        update_page_number();
    });

    m_page_num_input = new ::TextInput(m_flipping_panel, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(50), -1), wxTE_PROCESS_ENTER);
    StateColor input_bg(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Disabled), std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerLowest), StateColor::Enabled));
    m_page_num_input->SetBackgroundColor(input_bg);
    m_page_num_input->GetTextCtrl()->SetValue("1");
    wxTextValidator validator(wxFILTER_DIGITS);
    m_page_num_input->GetTextCtrl()->SetValidator(validator);
    m_page_num_input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [&](wxCommandEvent& e) {
        page_num_enter_evt();
    });

    m_page_num_enter = new Button(m_flipping_panel, _("Go"));
    m_page_num_enter->SetMinSize(wxSize(FromDIP(25), FromDIP(25)));
    m_page_num_enter->SetMaxSize(wxSize(FromDIP(25), FromDIP(25)));
    m_page_num_enter->SetBackgroundColor(ctrl_bg);
    m_page_num_enter->SetCornerRadius(FromDIP(5));
    m_page_num_enter->Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](auto& evt) {
        page_num_enter_evt();
    });

    m_flipping_page_sizer->Add(0, 0, 1, wxEXPAND, 0);
    m_flipping_page_sizer->Add(btn_last_page, 0, wxALIGN_CENTER, 0);
    m_flipping_page_sizer->Add(st_page_number, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(5));
    m_flipping_page_sizer->Add(btn_next_page, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(5));
    m_flipping_page_sizer->Add(m_page_num_input, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(20));
    m_flipping_page_sizer->Add(m_page_num_enter, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, FromDIP(5));
    m_page_sizer->Add(m_flipping_page_sizer, 0, wxALIGN_CENTER_HORIZONTAL, FromDIP(5));
    m_flipping_panel->SetSizer(m_page_sizer);
    m_flipping_panel->Layout();

    m_main_sizer->AddSpacer(FromDIP(16));
    m_main_sizer->Add(toolbar_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(4));
    m_main_sizer->AddSpacer(FromDIP(8));
    m_main_sizer->Add(bulk_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(4));
    m_main_sizer->AddSpacer(FromDIP(12));
    m_main_sizer->Add(m_table_head_panel, 0, wxEXPAND, 0);
    m_main_sizer->Add(m_tip_text, 0, wxEXPAND | wxTOP, FromDIP(50));
    m_main_sizer->Add(m_button_add, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(16));
    m_main_sizer->Add(m_machine_list, 0, wxEXPAND | wxTOP, FromDIP(8));
    m_main_sizer->Add(m_flipping_panel, 0, wxEXPAND | wxTOP, FromDIP(8));
    m_main_panel->SetSizer(m_main_sizer);
    m_main_panel->Layout();
    page_sizer = new wxBoxSizer(wxVERTICAL);
    page_sizer->Add(m_main_panel, 1, wxALL | wxEXPAND, FromDIP(25));

    SetSizer(page_sizer);
    Layout();
    Fit();

    Bind(wxEVT_TIMER, &MultiMachineManagerPage::on_timer, this);
    update_bulk_controls();
}

// ---- Bulk selection -------------------------------------------------------

void MultiMachineManagerPage::on_item_toggled(MultiMachineItem* item, bool shift)
{
    if (!item || !item->obj_) return;
    const std::string id = item->obj_->get_dev_id();
    if (shift && !m_bulk_anchor.empty() && item->IsSelected()) {
        // Shift-click: select the inclusive run between the anchor and this
        // card in the current page order.
        m_bulk.select_range(m_page_ids, m_bulk_anchor, id);
    } else {
        m_bulk.set(id, item->IsSelected());
    }
    m_bulk_anchor = id;
    apply_selection_to_items();
    update_bulk_controls();
}

void MultiMachineManagerPage::select_page()
{
    m_bulk.select_page(m_page_ids);
    apply_selection_to_items();
    update_bulk_controls();
}

void MultiMachineManagerPage::select_all_matches()
{
    m_bulk.select_all_matches(m_match_ids);
    apply_selection_to_items();
    update_bulk_controls();
}

void MultiMachineManagerPage::invert_selection()
{
    m_bulk.invert(m_match_ids);
    apply_selection_to_items();
    update_bulk_controls();
}

void MultiMachineManagerPage::clear_selection()
{
    m_bulk.clear();
    m_bulk_anchor.clear();
    apply_selection_to_items();
    update_bulk_controls();
}

void MultiMachineManagerPage::apply_selection_to_items()
{
    for (MultiMachineItem* item : m_device_items)
        if (item && item->obj_)
            item->SetSelected(m_bulk.contains(item->obj_->get_dev_id()));
}

void MultiMachineManagerPage::update_bulk_controls()
{
    if (!m_bulk_counts) return;
    const int page    = static_cast<int>(m_page_ids.size());
    const int matches = static_cast<int>(m_match_ids.size());
    // TRN: %d is how many device cards are on the current page.
    m_button_select_page->SetLabel(wxString::Format(_L("Select this page (%d)"), page));
    m_button_select_page->SetToolTip(_L("Selects every device card on this page") + " (Ctrl+A)");
    // TRN: %d is how many devices match the search across every page.
    m_button_select_all->SetLabel(wxString::Format(_L("Select all %d matches"), matches));
    m_button_select_all->SetToolTip(_L("Selects every device matching the search, on every page") + " (Ctrl+Shift+A)");
    m_button_invert->SetToolTip(_L("Inverts the selection across every matching device") + " (Ctrl+I)");
    m_button_clear->SetToolTip(_L("Clears the selection"));

    const std::size_t selected = m_bulk.size();
    const std::size_t on_page  = m_bulk.count_within(m_page_ids);
    // TRN: %d devices are selected in total.
    wxString counts = wxString::Format(_L("%d selected"), static_cast<int>(selected));
    if (selected > on_page)
        // TRN: %d selected devices sit on other pages (or are hidden by the search).
        counts += " " + wxString::Format(_L("(%d on other pages)"), static_cast<int>(selected - on_page));
    m_bulk_counts->SetLabel(counts);

    m_button_select_page->Enable(page > 0);
    m_button_select_all->Enable(matches > 0);
    m_button_invert->Enable(matches > 0);
    m_button_clear->Enable(selected > 0);
    m_button_export->Enable(selected > 0);
    m_main_panel->Layout();
}

void MultiMachineManagerPage::on_char_hook(wxKeyEvent& event)
{
    const bool ctrl  = event.ControlDown() || event.RawControlDown();
    const bool shift = event.ShiftDown();
    // Leave the search field's own editing shortcuts alone.
    wxWindow* focus = wxWindow::FindFocus();
    const bool editing = focus && dynamic_cast<wxTextCtrl*>(focus) != nullptr;
    if (ctrl && !event.AltDown() && !editing) {
        if (event.GetKeyCode() == 'A') {
            if (shift) select_all_matches(); else select_page();
            return;
        }
        if (event.GetKeyCode() == 'I' && !shift) {
            invert_selection();
            return;
        }
    }
    event.Skip();
}

void MultiMachineManagerPage::bulk_export()
{
    // Selected devices in display order; devices no longer in the farm are
    // dropped by retain() in refresh_user_device, so every id resolves.
    std::vector<std::string> all_ids;
    all_ids.reserve(m_user_machines.size());
    for (const auto& kv : m_user_machines) all_ids.push_back(kv.first);
    const std::vector<std::string> selected = m_bulk.ordered_within(all_ids);
    if (selected.empty()) return;

    // TRN: Title of the file picker for exporting selected devices.
    wxFileDialog picker(this, _L("Export selected devices"), wxEmptyString, "devices.json",
                        "JSON (*.json)|*.json|CSV (*.csv)|*.csv", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (picker.ShowModal() != wxID_OK) return;
    wxFileName target(picker.GetPath());
    const bool csv = picker.GetFilterIndex() == 1 || target.GetExt().Lower() == "csv";
    if (target.GetExt().IsEmpty()) target.SetExt(csv ? "csv" : "json");

    struct Row
    {
        std::string name, id, model, status, task;
        int         progress{ -1 };
    };
    std::vector<Row>     rows;
    Bulk::BulkActionPlan plan;
    plan.action = _u8L("Export devices");
    // TRN: %s is the export file name.
    plan.consequence = std::string(wxString::Format(csv ? _L("The selected devices are written as CSV rows to %s. Devices are not changed.")
                                                        : _L("The selected devices are written as a JSON list to %s. Devices are not changed."),
                                                    target.GetFullName()).ToUTF8());
    for (const std::string& id : selected) {
        auto it = m_user_machines.find(id);
        if (it == m_user_machines.end() || !it->second) {
            plan.items.push_back(Bulk::BulkItem::skipped(id, _u8L("device is no longer in the farm")));
            continue;
        }
        MachineObject* obj = it->second;
        Row row;
        row.name   = obj->get_dev_name();
        row.id     = id;
        row.model  = obj->printer_type;
        row.status = obj->is_online() ? obj->print_status : std::string("OFFLINE");
        row.task   = obj->subtask_name;
        if (obj->is_in_printing() && obj->subtask_) row.progress = obj->subtask_->task_progress;
        plan.items.push_back(Bulk::BulkItem::changed(row.name, row.model + " / " + row.status));
        rows.push_back(std::move(row));
    }
    if (!Bulk::BulkActionPreviewDialog::Run(this, plan)) return;

    const int of = static_cast<int>(m_user_machines.size());
    std::ofstream out(target.GetFullPath().ToStdWstring(), std::ios::binary | std::ios::trunc);
    if (!out) {
        // TRN: %s is the export file path.
        wxGetApp().plater()->get_notification_manager()->push_notification(
            NotificationType::CustomNotification, NotificationManager::NotificationLevel::ErrorNotificationLevel,
            std::string(wxString::Format(_L("Could not write %s"), target.GetFullPath()).ToUTF8()));
        return;
    }
    if (csv) {
        // Comment header states the selection scope; UTF-8, LF line endings.
        out << "# " << rows.size() << " of " << of << " devices selected\n";
        out << "name,dev_id,model,status,task,progress\n";
        for (const Row& r : rows) {
            out << csv_escape(r.name) << ',' << csv_escape(r.id) << ',' << csv_escape(r.model) << ',' << csv_escape(r.status)
                << ',' << csv_escape(r.task) << ',';
            if (r.progress >= 0) out << r.progress;
            out << '\n';
        }
    } else {
        out << "{\n  \"schema\": \"bambustudio.device-export/1\",\n  \"encoding\": \"UTF-8\",\n";
        out << "  \"selected\": " << rows.size() << ",\n  \"of\": " << of << ",\n  \"devices\": [\n";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const Row& r = rows[i];
            out << "    {\"name\": \"" << json_escape(r.name) << "\", \"dev_id\": \"" << json_escape(r.id) << "\", \"model\": \""
                << json_escape(r.model) << "\", \"status\": \"" << json_escape(r.status) << "\", \"task\": \"" << json_escape(r.task)
                << "\", \"progress\": ";
            if (r.progress >= 0) out << r.progress; else out << "null";
            out << "}" << (i + 1 < rows.size() ? "," : "") << "\n";
        }
        out << "  ]\n}\n";
    }
    out.close();
    // TRN: %1$d devices were exported of %2$d in the farm, to file %3$s.
    wxGetApp().plater()->get_notification_manager()->push_notification(
        std::string(wxString::Format(_L("Exported %d of %d devices to %s"), static_cast<int>(rows.size()), of, target.GetFullName()).ToUTF8()));
}

void MultiMachineManagerPage::update_page()
{
    for (int i = 0; i < m_device_items.size(); i++) {
        m_device_items[i]->sync_state();
        m_device_items[i]->Refresh();
    }
}

void MultiMachineManagerPage::refresh_user_device(bool clear)
{
    m_sizer_machine_list->Clear(true);
    m_device_items.clear();
    m_page_ids.clear();
    m_match_ids.clear();

    if (clear) {
        m_user_machines.clear();
        m_bulk.clear();
        update_bulk_controls();
        return;
    }

    Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    auto all_machine = dev->get_my_cloud_machine_list();
    auto user_machine = std::map<std::string, MachineObject*>();

    //selected machine
    for (int i = 0; i < PICK_DEVICE_MAX; i++) {
        auto dev_id = wxGetApp().app_config->get("multi_devices", std::to_string(i));

        if (all_machine.count(dev_id) > 0) {
            user_machine[dev_id] = all_machine[dev_id];
        }
    }


    const int total_selected = static_cast<int>(user_machine.size());
    m_user_machines = user_machine;
    {
        std::vector<std::string> live;
        live.reserve(user_machine.size());
        for (const auto& kv : user_machine) live.push_back(kv.first);
        m_bulk.retain_listed(live);
    }

    // Full state list for the selected devices.
    m_state_objs.clear();
    for (auto it = user_machine.begin(); it != user_machine.end(); ++it) {
        sync_state(it->second);
    }

    // Live farm-search filter (device name / printer type), applied BEFORE
    // paging so the page count and flipping controls track the visible set. An
    // empty query keeps every selected device.
    std::vector<ObjState> filtered;
    filtered.reserve(m_state_objs.size());
    if (m_search_filter.IsEmpty()) {
        filtered = m_state_objs;
    } else {
        // Route both the name and type match through the shared MD3 matcher so
        // the field's regex / case / whole-word chrome drives filtering. Invalid
        // half-typed regex matches everything (never blanks the grid).
        const bool regex         = m_search && m_search->IsRegexEnabled();
        const bool caseSensitive = m_search && m_search->IsCaseSensitive();
        const bool wholeWord     = m_search && m_search->IsWholeWord();
        const bool multiline     = m_search && m_search->IsMultiline();
        SearchField::MatchPass match_pass(m_search_filter, regex, caseSensitive, wholeWord, multiline);
        for (const auto& st : m_state_objs) {
            wxString name = wxString::FromUTF8(st.state_dev_name);
            wxString type;
            auto mit = user_machine.find(st.dev_id);
            if (mit != user_machine.end() && mit->second)
                type = wxString::FromUTF8(mit->second->printer_type);
            if (match_pass.matches(name) || (!type.IsEmpty() && match_pass.matches(type)))
                filtered.push_back(st);
        }
    }

    //sort
    if (m_sort.rule != SortItem::SortRule::SR_None) {
        std::sort(filtered.begin(), filtered.end(), m_sort.get_machine_call_back());
    }
    for (const ObjState& st : filtered) m_match_ids.push_back(st.dev_id);

    // Pagination is driven by the FILTERED count; keep the current page in range
    // as filtering shrinks the result set.
    m_total_count = static_cast<int>(filtered.size());
    double result = static_cast<double>(m_total_count) / m_count_page_item;
    m_total_page = std::ceil(result);
    if (m_total_page <= 0)
        m_current_page = 0;
    else if (m_current_page > m_total_page - 1)
        m_current_page = m_total_page - 1;

    std::vector<ObjState> sort_devices = extractRange(filtered, m_current_page * m_count_page_item, (m_current_page + 1) * m_count_page_item - 1 );
    std::vector<std::string> subscribe_list;

    for (auto i = 0; i < sort_devices.size(); ++i) {
        auto dev_id = sort_devices[i].dev_id;

        auto machine = user_machine[dev_id];

        MultiMachineItem* di = new MultiMachineItem(m_machine_list, machine);
        di->SetSelected(m_bulk.contains(dev_id));
        di->SetOnToggle([this](MultiMachineItem* item, bool shift) { on_item_toggled(item, shift); });
        m_page_ids.push_back(dev_id);
        m_device_items.push_back(di);
        // Fixed-size cards separated by a uniform gutter (the wxALL border is the
        // half-gutter); no wxEXPAND so cards keep their card width and wrap.
        m_sizer_machine_list->Add(m_device_items[i], 0, wxALL, FromDIP(DEVICE_CARD_GAP));

        subscribe_list.push_back(dev_id);
    }

    dev->subscribe_device_list(subscribe_list);

    // Empty states: "search excluded everything" (hint only, no Add) is distinct
    // from "nothing selected yet" (the original tip + Add affordance). Both _L.
    const bool has_matches = !m_device_items.empty();
    if (!has_matches && total_selected > 0 && !m_search_filter.IsEmpty()) {
        m_tip_text->SetLabel(_L("No devices match your search."));
        m_tip_text->Wrap(-1);
        m_tip_text->Show(true);
        m_button_add->Show(false);
    } else if (!has_matches) {
        m_tip_text->SetLabel(_L("Please select the devices you would like to manage here (up to 6 devices)"));
        m_tip_text->Wrap(-1);
        m_tip_text->Show(true);
        m_button_add->Show(true);
    } else {
        m_tip_text->Show(false);
        m_button_add->Show(false);
    }

    update_page_number();
    m_flipping_panel->Show(m_total_page > 1);
    update_bulk_controls();
    m_sizer_machine_list->Layout();
    m_machine_list->FitInside();
    Layout();
}

std::vector<ObjState> MultiMachineManagerPage::extractRange(const std::vector<ObjState>& source, int start, int end) {
    std::vector<ObjState> result;

    if (start < 0 || start > end || source.size() <= 0) {
        return result;
    }

    if ( end >= source.size() ) {
        end = source.size();
    }

    auto startIter = source.begin() + start;
    auto endIter = source.begin() + end;
    result.assign(startIter, endIter);
    return result;
}

void MultiMachineManagerPage::sync_state(MachineObject* obj_)
{
    ObjState state_obj;

    if (obj_) {
        state_obj.dev_id = obj_->get_dev_id();
        state_obj.state_dev_name = obj_->get_dev_name();

        if (obj_->print_status == "IDLE") {
            state_obj.state_device = 0;
        }
        else if (obj_->print_status == "FINISH") {
            state_obj.state_device = 1;
        }
        else if (obj_->print_status == "FAILED") {
            state_obj.state_device = 2;
        }
        else if (obj_->print_status == "RUNNING") {
            state_obj.state_device = 3;
        }
        else if (obj_->print_status == "PAUSE") {
            state_obj.state_device = 4;
        }
        else if (obj_->print_status == "PREPARE") {
            state_obj.state_device = 5;
        }
        else if (obj_->print_status == "SLICING") {
            state_obj.state_device = 6;
        }
        else {
            state_obj.state_device = 7;
        }
    }
    m_state_objs.push_back(state_obj);
}

bool MultiMachineManagerPage::Show(bool show)
{
    if (show) {
        refresh_user_device();
    }
    else {
        Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
        if (dev) {
            dev->subscribe_device_list(std::vector<std::string>());
        }
    }
    return wxPanel::Show(show);
}

void MultiMachineManagerPage::start_timer()
{
    if (m_flipping_timer) {
        m_flipping_timer->Stop();
    }
    else {
        m_flipping_timer = new wxTimer();
    }

    m_flipping_timer->SetOwner(this);
    m_flipping_timer->Start(1000);
    wxPostEvent(this, wxTimerEvent());
}

void MultiMachineManagerPage::update_page_number()
{
    double result = static_cast<double>(m_total_count) / m_count_page_item;
    m_total_page = std::ceil(result);

    wxString number = wxString(std::to_string(m_current_page + 1)) + " / " + wxString(std::to_string(m_total_page));
    st_page_number->SetLabel(number);
}

void MultiMachineManagerPage::on_timer(wxTimerEvent& event)
{
    m_flipping_timer->Stop();
    if (btn_last_page)
        btn_last_page->Enable(true);
    if (btn_next_page)
        btn_next_page->Enable(true);
}

void MultiMachineManagerPage::clear_page()
{

}

void MultiMachineManagerPage::page_num_enter_evt()
{
    btn_last_page->Enable(false);
    btn_next_page->Enable(false);
    start_timer();
    auto value = m_page_num_input->GetTextCtrl()->GetValue();
    long page_num = 0;
    if (value.ToLong(&page_num)) {
        if (page_num > m_total_page)
            m_current_page = m_total_page - 1;
        else if (page_num < 1)
            m_current_page = 0;
        else
            m_current_page = page_num - 1;
    }
    refresh_user_device();
    update_page_number();
}

void MultiMachineManagerPage::msw_rescale()
{
    m_printer_name->Rescale();
    m_printer_name->SetMinSize(wxSize(FromDIP(DEVICE_LEFT_DEV_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_printer_name->SetMaxSize(wxSize(FromDIP(DEVICE_LEFT_DEV_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_task_name->Rescale();
    m_task_name->SetMinSize(wxSize(FromDIP(DEVICE_LEFT_DEV_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_task_name->SetMaxSize(wxSize(FromDIP(DEVICE_LEFT_DEV_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_status->Rescale();
    m_status->SetMinSize(wxSize(FromDIP(DEVICE_LEFT_PRO_INFO), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_status->SetMaxSize(wxSize(FromDIP(DEVICE_LEFT_PRO_INFO), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_action->Rescale();
    m_action->SetMinSize(wxSize(FromDIP(DEVICE_LEFT_PRO_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_action->SetMaxSize(wxSize(FromDIP(DEVICE_LEFT_PRO_NAME), FromDIP(DEVICE_ITEM_MAX_HEIGHT)));
    m_button_add->Rescale();
    m_button_add->SetMinSize(wxSize(FromDIP(90), FromDIP(36)));
    m_button_add->SetMaxSize(wxSize(FromDIP(90), FromDIP(36)));

    btn_last_page->Rescale();
    btn_last_page->SetMinSize(wxSize(FromDIP(20), FromDIP(20)));
    btn_last_page->SetMaxSize(wxSize(FromDIP(20), FromDIP(20)));
    btn_next_page->Rescale();
    btn_next_page->SetMinSize(wxSize(FromDIP(20), FromDIP(20)));
    btn_next_page->SetMaxSize(wxSize(FromDIP(20), FromDIP(20)));
    m_page_num_enter->Rescale();
    m_page_num_enter->SetMinSize(wxSize(FromDIP(25), FromDIP(25)));
    m_page_num_enter->SetMaxSize(wxSize(FromDIP(25), FromDIP(25)));

    m_button_edit->Rescale();
    m_button_edit->SetMinSize(wxSize(FromDIP(120), FromDIP(40)));
    m_button_edit->SetMaxSize(wxSize(FromDIP(150), FromDIP(40)));

    if (m_search) {
        m_search->Rescale();
        m_search->SetMinSize(wxSize(FromDIP(240), FromDIP(40)));
        m_search->SetMaxSize(wxSize(FromDIP(340), FromDIP(40)));
    }
    for (Button* b : {m_button_select_page, m_button_select_all, m_button_invert, m_button_clear, m_button_export})
        if (b) b->Rescale();
    for (MultiMachineItem* item : m_device_items)
        if (item && item->m_check) item->m_check->Rescale();


    for (const auto& item : m_device_items) {
        item->Refresh();
    }

    Fit();
    Layout();
    Refresh();
}

} // namespace GUI
} // namespace Slic3r
