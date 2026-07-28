#include "SafetyOptionsDialog.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "libslic3r/Utils.hpp"
#include "Widgets/SwitchButton.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/StateColor.hpp"
#include "MsgDialog.hpp"

#include "DeviceCore/DevConfig.h"
#include "DeviceCore/DevExtruderSystem.h"
#include "DeviceCore/DevNozzleSystem.h"
#include "DeviceCore/DevPrintOptions.h"

#include <algorithm>

namespace Slic3r { namespace GUI {

namespace {

// ui-md3 containment/Snackbar geometry, in logical px: 12px vertical padding,
// 16px leading / 10px trailing inset, 12px gaps, a 20px leading mark and a 28px
// dismiss target carrying a 16px close mark. Every one is FromDIP'd at use.
constexpr int kToastPadV     = 12;
constexpr int kToastPadLeft  = 16;
constexpr int kToastPadRight = 10;
constexpr int kToastGap      = 12;
constexpr int kToastIconPx   = 20;
constexpr int kToastCloseBox = 28;
constexpr int kToastClosePx  = 16;
constexpr int kToastMargin   = 16; // inset kept from the hosting dialog's edges

} // namespace

SafetyOptionsDialog::SafetyOptionsDialog(wxWindow* parent)
    : DPIDialog(parent, wxID_ANY, _L("Safety Options"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    this->SetDoubleBuffered(true);
    std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    SetSize(FromDIP(480),FromDIP(320));

    m_scrollwindow = new wxScrolledWindow(this, wxID_ANY);
    m_scrollwindow->SetScrollRate(0, FromDIP(10));
    m_scrollwindow->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    m_scrollwindow->SetMinSize(wxSize(FromDIP(480), wxDefaultCoord));
    m_scrollwindow->SetMaxSize(wxSize(FromDIP(480), wxDefaultCoord));

    auto m_options_sizer = create_settings_group(m_scrollwindow);
    m_options_sizer->SetMinSize(wxSize(FromDIP(460), wxDefaultCoord));

    m_scrollwindow->SetSizer(m_options_sizer);

    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(m_scrollwindow, 1, wxEXPAND);
    this->SetSizer(mainSizer);

    m_options_sizer->Fit(m_scrollwindow);
    m_scrollwindow->FitInside();

    this->Layout();

    m_cb_open_door->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &evt) {
        if (m_cb_open_door->GetValue()) {
            if (obj) { obj->command_set_door_open_check(MachineObject::DOOR_OPEN_CHECK_ENABLE_WARNING); }
        } else {
            if (obj) { obj->command_set_door_open_check(MachineObject::DOOR_OPEN_CHECK_DISABLE); }
        }
        evt.Skip();
    });

    m_open_door_switch_board->Bind(wxCUSTOMEVT_SWITCH_POS, [this](wxCommandEvent &evt)
    {
        if (evt.GetInt() == 0)
        {
            if (obj) { obj->command_set_door_open_check(MachineObject::DOOR_OPEN_CHECK_ENABLE_PAUSE_PRINT); }
        }
        else if (evt.GetInt() == 1)
        {
            if (obj) { obj->command_set_door_open_check(MachineObject::DOOR_OPEN_CHECK_ENABLE_WARNING); }
        }
        evt.Skip();
    });

    m_cb_idel_heating_protection->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &evt) {
        if (obj)
        {
            obj->GetPrintOptions()->command_xcam_control_idelheatingprotect_detector(m_cb_idel_heating_protection->GetValue());
        }
        else
        {
            BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << "obj is empty";
        }
            evt.Skip();
    });

    auto toast_click = [this](wxMouseEvent &e) {
        if (m_idel_protect_unavailable) {
            show_idel_heating_toast(_L("Unavailable while heating maintenance function is on."));
        }
        e.Skip();
    };
    m_text_idel_heating_protection->Bind(wxEVT_LEFT_DOWN, toast_click);
    m_text_idel_heating_protection_caption->Bind(wxEVT_LEFT_DOWN, toast_click);
    m_idel_heating_container->Bind(wxEVT_LEFT_DOWN, toast_click);

    m_idel_heating_toast_timer.SetOwner(this);
    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { dismiss_idel_heating_toast(); },
         m_idel_heating_toast_timer.GetId());

    wxGetApp().UpdateDlgDarkUI(this);
    MD3DialogCaption::Adopt(this);
}

SafetyOptionsDialog::~SafetyOptionsDialog()
{
}

void SafetyOptionsDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    Fit();
}

void SafetyOptionsDialog::update_options(MachineObject* obj_)
{
    if (!obj_) return;

    updateOpenDoorCheck(obj_);
    updateIdelHeatingProtect(obj_);

    this->Freeze();
    this->Thaw();
    Layout();
}

void SafetyOptionsDialog::updateOpenDoorCheck(MachineObject *obj) {
    if (!obj || !obj->support_door_open_check()) {
        m_cb_open_door->Hide();
        m_text_open_door->Hide();
        m_open_door_switch_board->Hide();
        return;
    }

    if (obj->get_door_open_check_state() != MachineObject::DOOR_OPEN_CHECK_DISABLE) {
        m_cb_open_door->SetValue(true);
        m_open_door_switch_board->Enable();

        if (obj->get_door_open_check_state() == MachineObject::DOOR_OPEN_CHECK_ENABLE_WARNING) {
            m_open_door_switch_board->updateState("left");
            m_open_door_switch_board->Refresh();
        } else if (obj->get_door_open_check_state() == MachineObject::DOOR_OPEN_CHECK_ENABLE_PAUSE_PRINT) {
            m_open_door_switch_board->updateState("right");
            m_open_door_switch_board->Refresh();
        }

    } else {
        m_cb_open_door->SetValue(false);
        m_open_door_switch_board->Disable();
    }

    m_cb_open_door->Show();
    m_text_open_door->Show();
    m_open_door_switch_board->Show();
}

void SafetyOptionsDialog::updateIdelHeatingProtect(MachineObject *obj)
{
    if (obj->GetPrintOptions()->GetDetectionOption(PrintOptionEnum::Idle_Heating_Protect_Detection)->is_support_detect) {
        m_idel_heating_container->Show();
    } else {
        m_idel_heating_container->Hide();
        m_idel_protect_unavailable = false;
        return;
    }

    if (obj->GetPrintOptions()->GetDetectionOption(PrintOptionEnum::Idle_Heating_Protect_Detection)->current_detect_value == 2)
    {
        m_cb_idel_heating_protection->SetValue(false);
        m_cb_idel_heating_protection->Enable(false);
        // Outline is the register's disabled-text tone (StatusPanel's
        // device_disabled_text_color); the old #AAAAAA was a light-only literal
        // that stayed pale grey on the dark surface.
        m_text_idel_heating_protection->SetForegroundColour(StateColor::semantic(MD3::Role::Outline));
        m_text_idel_heating_protection_caption->SetForegroundColour(StateColor::semantic(MD3::Role::Outline));
        m_cb_idel_heating_protection->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
        m_idel_protect_unavailable = true;
    } else {
        m_cb_idel_heating_protection->Enable(true);
        m_cb_idel_heating_protection->SetValue(obj->GetPrintOptions()->GetDetectionOption(PrintOptionEnum::Idle_Heating_Protect_Detection)->current_detect_value);
        m_text_idel_heating_protection->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        m_text_idel_heating_protection_caption->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        m_cb_idel_heating_protection->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        m_idel_protect_unavailable = false;
    }
}

wxBoxSizer* SafetyOptionsDialog::create_settings_group(wxWindow* parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    //Open Door Detection
    wxBoxSizer* line_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_cb_open_door = new CheckBox(parent);
    m_text_open_door = new Label(parent, _L("Open Door Detection"));
    m_text_open_door->SetFont(Label::Body_14);
    m_open_door_switch_board = new SwitchBoard(parent, _L("Notification"), _L("Pause printing"), wxSize(FromDIP(200), FromDIP(26)));
    m_open_door_switch_board->Disable();
    line_sizer->AddSpacer(FromDIP(5));
    line_sizer->Add(m_cb_open_door, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(5));
    line_sizer->Add(m_text_open_door, 1, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(5));

    sizer->Add(line_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(18));
    sizer->Add(m_open_door_switch_board, 0, wxLEFT, FromDIP(65));
    line_sizer->Add(FromDIP(10), 0, 0, 0);
    sizer->Add(0, 0, 0, wxTOP, FromDIP(15));

    //Idel Heating Protect
    m_idel_heating_container = new wxPanel(parent, wxID_ANY);
    m_idel_heating_container->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    wxBoxSizer* idel_container_sizer = new wxBoxSizer(wxVERTICAL);

    line_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_cb_idel_heating_protection = new CheckBox(m_idel_heating_container);
    m_text_idel_heating_protection = new Label(m_idel_heating_container, _L("Idle Heating Protection"));
    m_text_idel_heating_protection->SetFont(Label::Body_14);
    line_sizer->AddSpacer(FromDIP(5));
    line_sizer->Add(m_cb_idel_heating_protection, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(5));
    line_sizer->Add(m_text_idel_heating_protection, 1, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(5));
    idel_container_sizer->Add(line_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(18));

    line_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_text_idel_heating_protection_caption = new Label(m_idel_heating_container, _L("Stops heating automatically after 5 mins of idle to ensure safety."));
    m_text_idel_heating_protection_caption->SetFont(Label::Body_12);
    m_text_idel_heating_protection_caption->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    line_sizer->AddSpacer(FromDIP(20));
    line_sizer->Add(m_text_idel_heating_protection_caption, 1, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(5));
    idel_container_sizer->Add(line_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));

    m_idel_heating_container->SetSizer(idel_container_sizer);
    sizer->Add(m_idel_heating_container, 0, wxEXPAND);

    return sizer;
}

void SafetyOptionsDialog::update_machine_obj(MachineObject *obj_)
{
    obj = obj_;
}

bool SafetyOptionsDialog::Show(bool show)
{
    if (show) {
        wxGetApp().UpdateDlgDarkUI(this);
        CentreOnParent();
    }
    return DPIDialog::Show(show);
}

// ---------------------------------------------------------------------------
// SafetyOptionToast -- MD3 snackbar
// ---------------------------------------------------------------------------

SafetyOptionToast::SafetyOptionToast(wxWindow *parent, const wxString &text)
    : wxPopupWindow(parent, wxBORDER_NONE | wxFRAME_SHAPED)
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::InverseSurface));

    // LB_PROPAGATE_MOUSE_EVENT so a click on the message itself still reaches the
    // card's dismiss handler instead of being swallowed by the static text.
    m_label = new Label(this, Label::Body_13, text, LB_PROPAGATE_MOUSE_EVENT);
    m_label->SetBackgroundColour(StateColor::semantic(MD3::Role::InverseSurface));
    m_label->SetForegroundColour(StateColor::semantic(MD3::Role::InverseOn));

    // Chrome = leading inset + mark + gap ... gap + dismiss target + trailing
    // inset. Everything but the message run.
    const int chrome = FromDIP(kToastPadLeft) + FromDIP(kToastIconPx) + FromDIP(kToastGap) +
                       FromDIP(kToastGap) + FromDIP(kToastCloseBox) + FromDIP(kToastPadRight);

    // A long translation (bilingual mode is the worst case) must not push the
    // card past the dialog that hosts it, so the message wraps at the widest run
    // the dialog can actually show.
    if (parent) {
        const int avail = parent->GetClientSize().x - 2 * FromDIP(kToastMargin) - chrome;
        if (avail > 0 && m_label->GetBestSize().x > avail)
            m_label->Wrap(avail);
    }

    const wxSize text_size = m_label->GetBestSize();
    const int    content_h = std::max(text_size.y, std::max(FromDIP(kToastIconPx), FromDIP(kToastCloseBox)));
    const wxSize card(chrome + text_size.x, content_h + 2 * FromDIP(kToastPadV));

    SetSize(card);
    m_label->SetSize(FromDIP(kToastPadLeft) + FromDIP(kToastIconPx) + FromDIP(kToastGap),
                     (card.y - text_size.y) / 2, text_size.x, text_size.y);

    ApplyShape();
    Bind(wxEVT_PAINT, &SafetyOptionToast::OnPaint, this);
}

// The 12px snackbar corners come from a shaped window region rather than from
// painting alone: the card floats over whatever is behind it, so the corner
// pixels have to be cut out of the window itself. Same mask-bitmap -> wxRegion
// route the shaped MD3Dialog shell uses.
void SafetyOptionToast::ApplyShape()
{
    const wxSize size = GetSize();
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
        return;

    wxBitmap mask(size.GetWidth(), size.GetHeight(), 32);
    {
        wxMemoryDC dc;
        dc.SelectObject(mask);
        dc.SetBackground(wxBrush(wxColour(0, 0, 0)));
        dc.Clear();
        dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRoundedRectangle(0, 0, size.GetWidth(), size.GetHeight(), FromDIP(MD3::Metrics::radius_rail));
        dc.SelectObject(wxNullBitmap);
    }

    wxRegion region(mask, wxColour(0, 0, 0));
    if (region.IsOk())
        SetShape(region);
}

void SafetyOptionToast::OnPaint(wxPaintEvent &evt)
{
    wxPaintDC    dc(this);
    const wxRect rect = GetClientRect();

    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::InverseSurface)));
    dc.DrawRoundedRectangle(rect, FromDIP(MD3::Metrics::radius_rail));

    // The leading mark and the close mark are painted, not hosted as child
    // controls: an MSW popup window never takes focus, so a real button here
    // would be a control the keyboard can never reach. The whole card is the
    // click target instead, and the close mark says so.
    const int icon_box = FromDIP(kToastIconPx);
    MaterialIcon::drawCentered(dc, MaterialIcon::Info, kToastIconPx,
                               StateColor::semantic(MD3::Role::InversePrimary),
                               wxRect(FromDIP(kToastPadLeft), (rect.height - icon_box) / 2, icon_box, icon_box));

    const int close_box = FromDIP(kToastCloseBox);
    MaterialIcon::drawCentered(dc, MaterialIcon::Close, kToastClosePx,
                               StateColor::semantic(MD3::Role::InverseOn),
                               wxRect(rect.width - FromDIP(kToastPadRight) - close_box,
                                      (rect.height - close_box) / 2, close_box, close_box));

    evt.Skip();
}

// ---------------------------------------------------------------------------

void SafetyOptionsDialog::show_idel_heating_toast(const wxString &text)
{
    dismiss_idel_heating_toast();

    m_idel_heating_toast = new SafetyOptionToast(this, text);

    // Click anywhere on the card to dismiss it early -- the toast is purely
    // informational, so it must never be the thing standing in the user's way.
    auto dismiss = [this](wxMouseEvent &e) {
        dismiss_idel_heating_toast();
        e.Skip();
    };
    m_idel_heating_toast->Bind(wxEVT_LEFT_DOWN, dismiss);
    m_idel_heating_toast->SetCursor(wxCURSOR_HAND);

    wxRect  anchor = m_text_idel_heating_protection->GetScreenRect();
    wxPoint pos    = anchor.GetBottomLeft();
    pos.y += FromDIP(40);

    m_idel_heating_toast->Move(pos);
    m_idel_heating_toast->Show(true);

    // Start a one-shot timer for 3 seconds to dismiss
    m_idel_heating_toast_timer.Stop();
    m_idel_heating_toast_timer.StartOnce(3000);
}

void SafetyOptionsDialog::dismiss_idel_heating_toast()
{
    m_idel_heating_toast_timer.Stop();
    if (!m_idel_heating_toast)
        return;

    // wxPopupWindow::Destroy() deletes immediately, and this runs from the
    // toast's own click handler as well as from the timer, so hide now and reap
    // on the next event-loop turn. The toast is a wx child of the dialog, so a
    // dialog torn down before then takes the toast with it.
    SafetyOptionToast *toast = m_idel_heating_toast;
    m_idel_heating_toast     = nullptr;
    toast->Show(false);
    CallAfter([toast] { toast->Destroy(); });
}

}} // namespace Slic3r::GUI