if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(status_panel "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/StatusPanel.cpp")
set(media_panel "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/MediaFilePanel.cpp")
set(ams_setting_header "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/AMSSetting.hpp")
set(ams_setting "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/AMSSetting.cpp")
set(camera_hud "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/CameraHUD.cpp")
set(switch_button "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SwitchButton.cpp")
file(READ "${status_panel}" status_source)
file(READ "${media_panel}" media_source)
file(READ "${ams_setting_header}" ams_header_source)
file(READ "${ams_setting}" ams_source)
file(READ "${camera_hud}" camera_source)
file(READ "${switch_button}" switch_source)

# Active-print actions are icon-only: they must be keyboard reachable and expose
# a synchronized tooltip/name, without recreating an unchanged native tooltip.
foreach(focusable_button "m_button_pause_resume" "m_button_abort")
    if(NOT status_source MATCHES "${focusable_button}->SetCanFocus\\(true\\)")
        message(FATAL_ERROR "Active-print ${focusable_button} must remain keyboard focusable")
    endif()
    if(status_source MATCHES "${focusable_button}->SetCanFocus\\(false\\)")
        message(FATAL_ERROR "Active-print ${focusable_button} must not be removed from tab traversal")
    endif()
endforeach()
if(NOT status_source MATCHES "if \\(button->GetToolTipText\\(\\) != label\\)[\r\n ]*button->SetToolTip\\(label\\)")
    message(FATAL_ERROR "Print-action tooltip updates must be skipped when text is unchanged")
endif()
if(NOT status_source MATCHES "if \\(button->GetName\\(\\) != label\\)[\r\n ]*button->SetName\\(label\\)")
    message(FATAL_ERROR "Print-action accessible names must stay synchronized without redundant updates")
endif()
if(NOT status_source MATCHES "m_button_pause_resume, _L\\(\"Pause\"\\)")
    message(FATAL_ERROR "The initial pause action must announce Pause")
endif()
if(NOT status_source MATCHES "m_button_abort, _L\\(\"Stop\"\\)")
    message(FATAL_ERROR "The icon-only abort action must announce Stop")
endif()
foreach(state_contract
        "pause_disable|Pause"
        "resume_disable|Resume"
        "resume|Resume"
        "pause|Pause")
    string(REPLACE "|" ";" state_fields "${state_contract}")
    list(GET state_fields 0 state_name)
    list(GET state_fields 1 action_name)
    if(NOT status_source MATCHES "type == \"${state_name}\"[\r\n ]*\\)[^{]*\\{[^{]*m_button_pause_resume->SetGlyph\\([^;]+;[\r\n ]*set_button_action_label\\(m_button_pause_resume, _L\\(\"${action_name}\"\\)\\)")
        message(FATAL_ERROR "Print state ${state_name} must announce ${action_name}")
    endif()
endforeach()

# Every media toolbar button here has visible text. Keep that native label as its
# accessible name; action-oriented detail belongs in the tooltip only.
foreach(visible_button
        "m_button_timelapse|Timelapse"
        "m_button_video|Video"
        "m_button_model|Model"
        "m_button_refresh|Refresh"
        "m_button_delete|Delete"
        "m_button_download|Download"
        "m_button_management|Select"
        "m_button_select_all|Select All"
        "m_button_year|Year"
        "m_button_month|Month"
        "m_button_all|All Files")
    string(REPLACE "|" ";" button_fields "${visible_button}")
    list(GET button_fields 0 button_name)
    list(GET button_fields 1 visible_label)
    if(NOT media_source MATCHES "${button_name}[\r\n ]*= new ::Button\\([^;]*_L\\(\"${visible_label}\"\\)")
        message(FATAL_ERROR "${button_name} must retain its visible label '${visible_label}'")
    endif()
    if(media_source MATCHES "${button_name}->SetName\\(")
        message(FATAL_ERROR "${button_name} must not replace its visible label with a tooltip-style accessible name")
    endif()
endforeach()
if(media_source MATCHES "set_button_action_label")
    message(FATAL_ERROR "Visible-label media buttons must not use the icon-only naming helper")
endif()
if(NOT media_source MATCHES "m_button_model->SetToolTip\\(_L\\(\"Switch to 3mf model files\\.\"\\)\\)")
    message(FATAL_ERROR "The Model tooltip must target m_button_model")
endif()
if(NOT media_source MATCHES "m_button_management->SetLabel\\(selecting \\? _L\\(\"Cancel\"\\) : _L\\(\"Select\"\\)\\)[\r\n ]*;?[\r\n ]*m_button_management->SetToolTip\\(selecting \\? _L\\(\"Finish managing files\\.\"\\) : _L\\(\"Batch manage files\\.\"\\)\\)")
    message(FATAL_ERROR "The management button must keep its dynamic visible label while updating tooltip detail")
endif()
foreach(focusable_button
        "m_button_timelapse"
        "m_button_video"
        "m_button_model"
        "m_button_refresh"
        "m_button_delete"
        "m_button_download"
        "m_button_management"
        "m_button_select_all")
    if(media_source MATCHES "${focusable_button}->SetCanFocus\\(false\\)")
        message(FATAL_ERROR "Media toolbar ${focusable_button} must not be removed from tab traversal")
    endif()
endforeach()
function(require_source_block source_var start_marker end_marker required_text error_message)
    string(FIND "${${source_var}}" "${start_marker}" block_start)
    string(FIND "${${source_var}}" "${end_marker}" block_end)
    if(block_start EQUAL -1 OR block_end EQUAL -1 OR block_end LESS block_start)
        message(FATAL_ERROR "Could not isolate source block: ${error_message}")
    endif()
    math(EXPR block_length "${block_end} - ${block_start}")
    string(SUBSTRING "${${source_var}}" ${block_start} ${block_length} source_block)
    string(FIND "${source_block}" "${required_text}" required_offset)
    if(required_offset EQUAL -1)
        message(FATAL_ERROR "${error_message}")
    endif()
endfunction()
require_source_block(
    media_source
    "for (auto b : {m_button_timelapse, m_button_video, m_button_model})"
    "wxBoxSizer *type_sizer"
    "b->SetCanFocus(true)"
    "Media type buttons must remain keyboard focusable")
if(NOT media_source MATCHES "m_button_refresh->SetCanFocus\\(true\\)")
    message(FATAL_ERROR "Media refresh action must remain keyboard focusable")
endif()
require_source_block(
    media_source
    "for (auto b : {m_button_delete, m_button_download, m_button_management, m_button_select_all})"
    "m_button_delete->SetBorderColorNormal"
    "b->SetCanFocus(true)"
    "Media management buttons must remain keyboard focusable")

# AMSSetting owns the type panel through the wx parent hierarchy. Non-decision
# feedback no longer needs a retained dialog pointer or an EndModal side effect.
if(ams_header_source MATCHES "m_setting_dlg" OR ams_source MATCHES "m_setting_dlg")
    message(FATAL_ERROR "AMSSettingTypePanel must not retain the dead dialog pointer")
endif()
if(NOT ams_header_source MATCHES "explicit AMSSettingTypePanel\\(wxWindow\\* parent\\)")
    message(FATAL_ERROR "AMSSettingTypePanel must rely on its wx parent for lifetime")
endif()
if(NOT ams_source MATCHES "new AMSSettingTypePanel\\(m_panel_body\\)")
    message(FATAL_ERROR "AMSSetting must parent the type panel to m_panel_body")
endif()
if(NOT ams_source MATCHES "SetSelection\\(part->GetCurrentFirmwareIdxSel\\(\\)\\);[\r\n ]*show_info\\(this, _L\\(\"The printer is busy and cannot switch AMS type\\.\"\\)")
    message(FATAL_ERROR "AMS busy feedback must restore the selection before notifying")
endif()
if(NOT ams_source MATCHES "SetSelection\\(part->GetCurrentFirmwareIdxSel\\(\\)\\);[\r\n ]*warning_catcher\\(this, _L\\(\"Please unload all filament before switching\\.\"\\)\\)")
    message(FATAL_ERROR "AMS unload feedback must restore the selection before notifying")
endif()
if(NOT ams_source MATCHES "MessageDialog dlg\\(this, _L\\(\"AMS type switching needs firmware update[^\"]*\"\\)[^;]*;[\r\n ]*dlg.SetButtonLabel\\(wxID_OK, _L\\(\"Confirm\"\\)\\);[\r\n ]*int rtn = dlg.ShowModal\\(\\)")
    message(FATAL_ERROR "The genuine AMS firmware-switch decision must remain modal")
endif()

# Both animation paths use the shared policy and snap to a stable endpoint.
foreach(source_var camera_source switch_source)
    if(NOT ${source_var} MATCHES "#include \"MD3Motion.hpp\"")
        message(FATAL_ERROR "${source_var} must include the shared reduced-motion policy")
    endif()
endforeach()
if(NOT camera_source MATCHES "m_live && IsShownOnScreen\\(\\) && !MD3::Motion::reduced\\(\\)")
    message(FATAL_ERROR "Camera LIVE pulse must not start under reduced motion")
endif()
if(NOT camera_source MATCHES "!m_live \\|\\| !IsShownOnScreen\\(\\) \\|\\| MD3::Motion::reduced\\(\\)")
    message(FATAL_ERROR "Camera LIVE pulse timer must stop when reduced motion becomes active")
endif()
if(NOT camera_source MATCHES "m_pulse_timer.Stop\\(\\);[\r\n ]*m_phase = 0\\.0")
    message(FATAL_ERROR "Camera LIVE pulse must snap to steady opacity when stopped")
endif()
# Camera HUD icon chips are true keyboard and assistive push buttons. Keep the
# existing LEFT_DOWN event route so StatusPanel's mouse handlers and popup
# positioning contract remain unchanged for keyboard/default actions too.
foreach(required_text
        "SetAccessible(new CameraHUDChipAccessible(this))"
        "wxROLE_SYSTEM_PUSHBUTTON"
        "wxACC_STATE_SYSTEM_FOCUSABLE"
        "wxACC_STATE_SYSTEM_FOCUSED"
        "wxACC_STATE_SYSTEM_PRESSED"
        "wxACC_STATE_SYSTEM_UNAVAILABLE"
        "wxACC_STATE_SYSTEM_INVISIBLE"
        "wxACC_EVENT_OBJECT_FOCUS"
        "wxACC_EVENT_OBJECT_NAMECHANGE"
        "key_code == WXK_SPACE"
        "key_code == WXK_RETURN"
        "key_code == WXK_NUMPAD_ENTER"
        "bool CameraHUD::CameraHUDChip::AcceptsFocusFromKeyboard() const"
        "void CameraHUD::CameraHUDChip::AccessibilityActivate()"
        "wxMouseEvent event(wxEVT_LEFT_DOWN)"
        "gc->SetPen(wxPen(CameraHUD::FocusRing()"
        "return DLGC_WANTMESSAGE")
    string(FIND "${camera_source}" "${required_text}" required_offset)
    if(required_offset EQUAL -1)
        message(FATAL_ERROR "Camera HUD chip keyboard/accessibility contract missing: ${required_text}")
    endif()
endforeach()
foreach(required_text
        "m_camera_fullscreen_button->SetName(fullscreen_name)"
        "m_setting_button->SetName(settings_name)")
    string(FIND "${status_source}" "${required_text}" required_offset)
    if(required_offset EQUAL -1)
        message(FATAL_ERROR "Camera HUD icon-only controls need explicit localized names: ${required_text}")
    endif()
endforeach()

# Windows high contrast replaces HUD chrome, text, disabled content, status, and
# focus colours with the current system palette and suppresses decorative pulse.
foreach(required_text
        "SPI_GETHIGHCONTRAST"
        "HCF_HIGHCONTRASTON"
        "wxSYS_COLOUR_WINDOW"
        "wxSYS_COLOUR_WINDOWTEXT"
        "wxSYS_COLOUR_BTNFACE"
        "wxSYS_COLOUR_HIGHLIGHT"
        "wxSYS_COLOUR_GRAYTEXT"
        "HighContrastActive() ? 255 : alpha"
        "MD3::Motion::reduced() || HighContrastActive()")
    string(FIND "${camera_source}" "${required_text}" required_offset)
    if(required_offset EQUAL -1)
        message(FATAL_ERROR "Camera HUD high-contrast contract missing: ${required_text}")
    endif()
endforeach()
foreach(required_text
        "CameraHUD::HighContrastActive()"
        "wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)"
        "wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)"
        "indicator->SetBackgroundColour(hud_bg)")
    string(FIND "${status_source}" "${required_text}" required_offset)
    if(required_offset EQUAL -1)
        message(FATAL_ERROR "Camera status indicators must follow high-contrast HUD colours: ${required_text}")
    endif()
endforeach()
foreach(required_text
        "m_anim_target = GetValue() ? 1.0 : 0.0;"
        "if (MD3::Motion::reduced())"
        "m_anim_timer.Stop();"
        "m_anim = m_anim_target;"
        "update();")
    require_source_block(
        switch_source
        "void SwitchButton::startAnim()"
        "void SwitchButton::onAnimTick"
        "${required_text}"
        "Switch startAnim must snap to its stable target under reduced motion")
endforeach()
foreach(required_text
        "if (MD3::Motion::reduced())"
        "m_anim_timer.Stop();"
        "m_anim = m_anim_target;"
        "update();")
    require_source_block(
        switch_source
        "void SwitchButton::onAnimTick"
        "SwitchBoard::SwitchBoard"
        "${required_text}"
        "An active switch animation must snap if reduced motion becomes enabled")
endforeach()

message(STATUS "Native GUI accessibility contract passed")
