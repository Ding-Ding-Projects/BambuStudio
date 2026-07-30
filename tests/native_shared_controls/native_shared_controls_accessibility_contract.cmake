if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(side_button_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SideButton.cpp")
set(side_button_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SideButton.hpp")
set(side_popup_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SideMenuPopup.cpp")
set(side_popup_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SideMenuPopup.hpp")
set(link_label_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/LinkLabel.cpp")
set(switch_button_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SwitchButton.cpp")
set(switch_button_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SwitchButton.hpp")
set(ams_control_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/AMSControl.cpp")
set(ams_control_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/AMSControl.hpp")
set(main_frame_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/MainFrame.cpp")
set(safety_options_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/SafetyOptionsDialog.cpp")
set(print_options_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/PrintOptionsDialog.cpp")
set(ams_mapping_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/AmsMappingPopup.cpp")
set(status_panel_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/StatusPanel.cpp")

foreach(source_file IN ITEMS
        "${side_button_cpp}"
        "${side_button_hpp}"
        "${side_popup_cpp}"
        "${side_popup_hpp}"
        "${link_label_cpp}"
        "${switch_button_cpp}"
        "${switch_button_hpp}"
        "${ams_control_cpp}"
        "${ams_control_hpp}"
        "${main_frame_cpp}"
        "${safety_options_cpp}"
        "${print_options_cpp}"
        "${ams_mapping_cpp}"
        "${status_panel_cpp}")
    file(READ "${source_file}" source)
    get_filename_component(source_name "${source_file}" NAME)

    if(source_name STREQUAL "SideButton.cpp")
        foreach(required_symbol
                "SetAccessible\\(new SideButtonAccessible"
                "EVT_KEY_DOWN\\(SideButton::keyDown\\)"
                "EVT_KEY_UP\\(SideButton::keyUp\\)"
                "EVT_MOUSE_CAPTURE_LOST\\(SideButton::mouseCaptureLost\\)"
                "if \\(!IsEnabled\\(\\) || !IsShown\\(\\)\\)"
                "HandleAsNavigationKey\\(event\\)"
                "CreateKeyEvent\\(wxEVT_KEY_DOWN, w_param, l_param\\)"
                "WM_GETDLGCODE")
            if(NOT source MATCHES "${required_symbol}")
                message(FATAL_ERROR "SideButton keyboard/accessibility contract is missing: ${required_symbol}")
            endif()
        endforeach()
    elseif(source_name STREQUAL "SideButton.hpp")
        foreach(required_symbol
                "AcceptsFocusFromKeyboard\\(\\) const override"
                "AccessibilityActivate\\(\\)"
                "MSWWindowProc")
            if(NOT source MATCHES "${required_symbol}")
                message(FATAL_ERROR "SideButton API contract is missing: ${required_symbol}")
            endif()
        endforeach()
    elseif(source_name STREQUAL "SideMenuPopup.cpp")
        foreach(required_symbol
                "Bind\\(wxEVT_CHAR_HOOK, &SidePopup::keyDown"
                "focusBoundaryButton\\(true\\)"
                "restoreInvokerFocus\\(\\)"
                "case WXK_ESCAPE"
                "case WXK_HOME"
                "case WXK_END")
            if(NOT source MATCHES "${required_symbol}")
                message(FATAL_ERROR "SidePopup keyboard contract is missing: ${required_symbol}")
            endif()
        endforeach()
    elseif(source_name STREQUAL "SideMenuPopup.hpp")
        if(NOT source MATCHES "wxWeakRef<wxWindow>")
            message(FATAL_ERROR "SidePopup must keep a weak reference to its invoker")
        endif()
    elseif(source_name STREQUAL "LinkLabel.cpp")
        foreach(required_symbol
                "SetAccessible\\(new LinkLabelAccessible"
                "WXK_RETURN"
                "WXK_SPACE"
                "HandleAsNavigationKey\\(event\\)"
                "wxROLE_SYSTEM_LINK")
            if(NOT source MATCHES "${required_symbol}")
                message(FATAL_ERROR "LinkLabel keyboard/accessibility contract is missing: ${required_symbol}")
            endif()
        endforeach()
    elseif(source_name STREQUAL "SwitchButton.cpp")
        foreach(required_text
                "SetAccessible(new Accessible(this));"
                "Bind(wxEVT_KEY_DOWN, &SwitchBoard::on_key_down, this);"
                "Bind(wxEVT_KEY_UP, &SwitchBoard::on_key_up, this);"
                "Bind(wxEVT_SET_FOCUS, &SwitchBoard::on_focus, this);"
                "Bind(wxEVT_KILL_FOCUS, &SwitchBoard::on_focus, this);"
                "*child_count = 2;"
                "wxROLE_SYSTEM_GROUPING"
                "wxROLE_SYSTEM_RADIOBUTTON"
                "wxACC_STATE_SYSTEM_CHECKED"
                "wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override"
                "wxAccStatus DoDefaultAction(int child_id) override"
                "void SwitchBoard::activateSegment(bool left)"
                "void SwitchBoard::on_key_down(wxKeyEvent &evt)"
                "void SwitchBoard::on_key_up(wxKeyEvent &evt)"
                "case WXK_UP:"
                "case WXK_DOWN:"
                "case WXK_NUMPAD_ENTER:"
                "case WXK_TAB:"
                "if (m_keyboard_pressed_key == WXK_NONE) {"
                "m_keyboard_pressed_key = evt.GetKeyCode();"
                "if (m_keyboard_pressed_key == evt.GetKeyCode()) {"
                "m_keyboard_pressed_key = WXK_NONE;"
                "if (evt.GetEventType() == wxEVT_KILL_FOCUS)"
                "HandleAsNavigationKey(evt);"
                "SetFocus();"
                "bool SwitchBoard::Enable(bool enable)"
                "const bool changed = wxWindow::Enable(enable);"
                "wxSize SwitchBoard::DoGetBestSize() const"
                "GetTextExtent(leftLabel"
                "GetTextExtent(rightLabel"
                "SetMinSize(DoGetBestSize());"
                "wxACC_EVENT_OBJECT_VALUECHANGE"
                "wxACC_EVENT_OBJECT_NAMECHANGE, this, wxOBJID_CLIENT, 1"
                "wxACC_EVENT_OBJECT_NAMECHANGE, this, wxOBJID_CLIENT, 2"
                "wxCommandEvent event(wxCUSTOMEVT_SWITCH_POS, GetId());"
                "event.SetEventObject(this);"
                "event.SetInt(static_cast<int>(switch_left));"
                "wxPostEvent(this, event);"
                "WM_GETDLGCODE"
                "DLGC_WANTARROWS")
            string(FIND "${source}" "${required_text}" required_text_index)
            if(required_text_index EQUAL -1)
                message(FATAL_ERROR "SwitchBoard keyboard/accessibility contract is missing: ${required_text}")
            endif()
        endforeach()
        foreach(forbidden_text
                "new Accessible(this); // wxWindow owns the accessible object."
                "SetMaxSize(size);"
                "void SwitchBoard::Enable()"
                "void SwitchBoard::Disable()"
                "wxCommandEvent event(wxCUSTOMEVT_SWITCH_POS);"
                "is_enable")
            string(FIND "${source}" "${forbidden_text}" forbidden_text_index)
            if(NOT forbidden_text_index EQUAL -1)
                message(FATAL_ERROR "SwitchBoard retains a stale accessibility/layout contract: ${forbidden_text}")
            endif()
        endforeach()
    elseif(source_name STREQUAL "SwitchButton.hpp")
        foreach(required_text
                "bool Enable(bool enable = true) override;"
                "bool AcceptsFocus() const override;"
                "bool AcceptsFocusFromKeyboard() const override;"
                "wxSize DoGetBestSize() const override;"
                "class Accessible;"
                "friend class Accessible;"
                "void activateSegment(bool left);"
                "void on_key_down(wxKeyEvent& evt);"
                "void on_key_up(wxKeyEvent& evt);"
                "void on_focus(wxFocusEvent& evt);"
                "int m_keyboard_pressed_key = WXK_NONE;"
                "MSWWindowProc")
            string(FIND "${source}" "${required_text}" required_text_index)
            if(required_text_index EQUAL -1)
                message(FATAL_ERROR "SwitchBoard API contract is missing: ${required_text}")
            endif()
        endforeach()
        string(FIND "${source}" "is_enable" stale_enable_state_index)
        if(NOT stale_enable_state_index EQUAL -1)
            message(FATAL_ERROR "SwitchBoard must use wxWindow's enabled state")
        endif()
    elseif(source_name STREQUAL "AMSControl.cpp")
        foreach(required_text
                "m_button_ams_setting->Bind(wxEVT_COMMAND_BUTTON_CLICKED"
                "void AMSControl::on_ams_setting_click(wxCommandEvent &event)")
            string(FIND "${source}" "${required_text}" required_text_index)
            if(required_text_index EQUAL -1)
                message(FATAL_ERROR "AMS settings button command-event contract is missing: ${required_text}")
            endif()
        endforeach()
        string(FIND "${source}" "void AMSControl::on_ams_setting_click(wxMouseEvent" stale_mouse_handler_index)
        if(NOT stale_mouse_handler_index EQUAL -1)
            message(FATAL_ERROR "AMS settings button must not retain the obsolete mouse-event handler")
        endif()
    elseif(source_name STREQUAL "AMSControl.hpp")
        string(FIND "${source}" "void on_ams_setting_click(wxCommandEvent &event);" command_handler_index)
        if(command_handler_index EQUAL -1)
            message(FATAL_ERROR "AMSControl API must declare its settings command-event handler")
        endif()
        string(FIND "${source}" "on_ams_setting_click(wxMouseEvent" stale_mouse_declaration_index)
        if(NOT stale_mouse_declaration_index EQUAL -1)
            message(FATAL_ERROR "AMSControl API must not declare the obsolete settings mouse-event handler")
        endif()
    elseif(source_name STREQUAL "MainFrame.cpp")
        foreach(required_symbol
                "m_slice_option_btn->SetName\\(_L\\(\\\"Slice options\\\"\\)\\)"
                "m_print_option_btn->SetName\\(_L\\(\\\"Print options\\\"\\)\\)"
                "m_slice_option_btn->MoveAfterInTabOrder\\(m_slice_btn\\)"
                "m_print_option_btn->MoveAfterInTabOrder\\(m_print_btn\\)"
                "m_slice_option_pop_up->Popup\\(m_slice_option_btn\\)"
                "p->Popup\\(m_print_option_btn\\)")
            if(NOT source MATCHES "${required_symbol}")
                message(FATAL_ERROR "MainFrame split-button contract is missing: ${required_symbol}")
            endif()
        endforeach()
    elseif(source_name STREQUAL "SafetyOptionsDialog.cpp")
        string(FIND "${source}" "sizer->Add(m_open_door_switch_board, 0, wxEXPAND" switch_expand_index)
        if(switch_expand_index EQUAL -1)
            message(FATAL_ERROR "Safety Options must allow its SwitchBoard to grow")
        endif()
    elseif(source_name STREQUAL "PrintOptionsDialog.cpp")
        foreach(required_text
                "sizer->Add(purify_air_switch_board, 0, wxEXPAND"
                "sizer->Add(open_door_switch_board, 0, wxEXPAND")
            string(FIND "${source}" "${required_text}" switch_expand_index)
            if(switch_expand_index EQUAL -1)
                message(FATAL_ERROR "Print Options SwitchBoard growth contract is missing: ${required_text}")
            endif()
        endforeach()
    elseif(source_name STREQUAL "AmsMappingPopup.cpp")
        string(FIND "${source}" "SetMaxSize(wxSize(FromDIP(445), -1));" fixed_dialog_max_index)
        if(NOT fixed_dialog_max_index EQUAL -1)
            message(FATAL_ERROR "AMS replacement dialog must not cap translated content at 445 DIP")
        endif()
    elseif(source_name STREQUAL "StatusPanel.cpp")
        string(FIND "${source}" "bSizer_control->Add(m_ams_rack_switch, 0, wxEXPAND" rack_switch_expand_index)
        if(rack_switch_expand_index EQUAL -1)
            message(FATAL_ERROR "Status panel AMS/Hotends SwitchBoard must grow with its control column")
        endif()
        string(FIND "${source}" "panel->SetMaxSize(wxSize(FromDIP(143), -1));" fixed_extruder_max_index)
        if(NOT fixed_extruder_max_index EQUAL -1)
            message(FATAL_ERROR "Status panel extruder card must not cap translated SwitchBoard labels")
        endif()
    endif()
endforeach()

message(STATUS "Native shared-controls accessibility contract passed")
