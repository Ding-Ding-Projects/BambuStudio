if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(AXIS_HEADER "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/AxisCtrlButton.hpp")
set(AXIS_SOURCE "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/AxisCtrlButton.cpp")
set(CAPSULE_HEADER "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/CapsuleButton.hpp")
set(CAPSULE_SOURCE "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/CapsuleButton.cpp")
set(FAN_HEADER "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/FanControl.hpp")
set(FAN_SOURCE "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/FanControl.cpp")

foreach(REQUIRED_FILE IN ITEMS
        "${AXIS_HEADER}" "${AXIS_SOURCE}"
        "${CAPSULE_HEADER}" "${CAPSULE_SOURCE}"
        "${FAN_HEADER}" "${FAN_SOURCE}")
    if(NOT EXISTS "${REQUIRED_FILE}")
        message(FATAL_ERROR "Missing native accessibility source: ${REQUIRED_FILE}")
    endif()
endforeach()

file(READ "${AXIS_HEADER}" AXIS_HEADER_TEXT)
file(READ "${AXIS_SOURCE}" AXIS_SOURCE_TEXT)
file(READ "${CAPSULE_HEADER}" CAPSULE_HEADER_TEXT)
file(READ "${CAPSULE_SOURCE}" CAPSULE_SOURCE_TEXT)
file(READ "${FAN_HEADER}" FAN_HEADER_TEXT)
file(READ "${FAN_SOURCE}" FAN_SOURCE_TEXT)

function(require_token SOURCE_TEXT TOKEN DESCRIPTION)
    string(FIND "${SOURCE_TEXT}" "${TOKEN}" TOKEN_OFFSET)
    if(TOKEN_OFFSET EQUAL -1)
        message(FATAL_ERROR "Missing ${DESCRIPTION}: ${TOKEN}")
    endif()
endfunction()

foreach(AXIS_HEADER_TOKEN IN ITEMS
        "AcceptsFocusFromKeyboard() const override"
        "AccessibilityNameForChild"
        "AccessibilityActivate"
        "MSWWindowProc")
    require_token("${AXIS_HEADER_TEXT}" "${AXIS_HEADER_TOKEN}" "Axis keyboard/accessibility declaration")
endforeach()
foreach(AXIS_SOURCE_TOKEN IN ITEMS
        "SetAccessible(new AxisCtrlButtonAccessible"
        "GetChildCount"
        "GetChild(int child_id"
        "HitTest(const wxPoint& point"
        "GetLocation(wxRect& location"
        "GetFocus(int *child_id"
        "wxROLE_SYSTEM_GROUPING"
        "wxROLE_SYSTEM_PUSHBUTTON"
        "Move Y up %d mm"
        "Move X left %d mm"
        "Home XY axes"
        "Move X right %d mm"
        "Move Y down %d mm"
        "wxACC_STATE_SYSTEM_FOCUSABLE"
        "wxACC_STATE_SYSTEM_FOCUSED"
        "wxACC_STATE_SYSTEM_PRESSED"
        "wxACC_EVENT_OBJECT_STATECHANGE"
        "AccessibilityHasCurrentChild()"
        "AccessibilityCurrentChildId()"
        "wxPanelNameStr"
        "wxACC_STATE_SYSTEM_UNAVAILABLE"
        "wxACC_STATE_SYSTEM_INVISIBLE"
        "case WXK_UP"
        "case WXK_LEFT"
        "case WXK_RIGHT"
        "case WXK_DOWN"
        "case WXK_HOME"
        "DLGC_WANTARROWS"
        "event.SetInt(position)")
    require_token("${AXIS_SOURCE_TEXT}" "${AXIS_SOURCE_TOKEN}" "Axis directional action contract")
endforeach()

foreach(CAPSULE_HEADER_TOKEN IN ITEMS
        "AcceptsFocusFromKeyboard() const override"
        "AccessibilityActivate"
        "OnKeyDown"
        "OnKeyUp"
        "MSWWindowProc")
    require_token("${CAPSULE_HEADER_TEXT}" "${CAPSULE_HEADER_TOKEN}" "Capsule keyboard/accessibility declaration")
endforeach()
foreach(CAPSULE_SOURCE_TOKEN IN ITEMS
        "SetAccessible(new CapsuleButtonAccessible"
        "GetChildCount"
        "*child_count = 0"
        "wxROLE_SYSTEM_RADIOBUTTON"
        "wxACC_STATE_SYSTEM_CHECKED"
        "wxACC_EVENT_OBJECT_STATECHANGE"
        "key != WXK_SPACE"
        "key != WXK_RETURN"
        "key != WXK_NUMPAD_ENTER"
        "wxCommandEvent click_event(wxEVT_BUTTON, GetId())"
        "click_event.SetEventObject(this)"
        "DisableFocusFromKeyboard"
        "DLGC_WANTMESSAGE")
    require_token("${CAPSULE_SOURCE_TEXT}" "${CAPSULE_SOURCE_TOKEN}" "Capsule radio/selection contract")
endforeach()

foreach(FAN_HEADER_TOKEN IN ITEMS
        "get_fan_speeds() const"
        "AcceptsFocusFromKeyboard() const override"
        "AccessibilityStep"
        "SetAccessibleName"
        "on_key_down"
        "MSWWindowProc")
    require_token("${FAN_HEADER_TEXT}" "${FAN_HEADER_TOKEN}" "Fan stepper keyboard/accessibility declaration")
endforeach()
foreach(FAN_SOURCE_TOKEN IN ITEMS
        "SetAccessible(new FanOperateAccessible"
        "wxROLE_SYSTEM_SPINBUTTON"
        "wxPanelNameStr"
        "get_fan_speeds() * 10"
        "wxACC_STATE_SYSTEM_FOCUSABLE"
        "wxACC_STATE_SYSTEM_FOCUSED"
        "wxACC_STATE_SYSTEM_UNAVAILABLE"
        "wxACC_STATE_SYSTEM_INVISIBLE"
        "wxACC_EVENT_OBJECT_NAMECHANGE"
        "wxACC_EVENT_OBJECT_VALUECHANGE"
        "set_fan_speeds(m_current_speeds + 1)"
        "set_fan_speeds(m_current_speeds - 1)"
        "case WXK_LEFT"
        "case WXK_DOWN"
        "case WXK_RIGHT"
        "case WXK_UP"
        "case WXK_NUMPAD_SUBTRACT"
        "case WXK_NUMPAD_ADD"
        "add_fan_speeds()"
        "decrease_fan_speeds()"
        "post_event(wxCommandEvent(EVT_FAN_ADD))"
        "post_event(wxCommandEvent(EVT_FAN_DEC))"
        "post_event(wxCommandEvent(EVT_FAN_SWITCH_ON))"
        "post_event(wxCommandEvent(EVT_FAN_SWITCH_OFF))"
        "SetAccessibleName(wxString::Format(_L(\"%s fan speed\"), name))"
        "DLGC_WANTARROWS")
    require_token("${FAN_SOURCE_TEXT}" "${FAN_SOURCE_TOKEN}" "Fan stepper name/value/state contract")
endforeach()

string(FIND "${FAN_SOURCE_TEXT}" "GetDefaultAction(int child_id" FAN_DEFAULT_ACTION_OFFSET)
if(NOT FAN_DEFAULT_ACTION_OFFSET EQUAL -1)
    message(FATAL_ERROR "Fan spin-button value control must not claim a push-button default action")
endif()

message(STATUS "Native Axis, Capsule, and Fan accessibility contract passed")
