if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(side_button_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SideButton.cpp")
set(side_button_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SideButton.hpp")
set(side_popup_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SideMenuPopup.cpp")
set(side_popup_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/SideMenuPopup.hpp")
set(link_label_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/Widgets/LinkLabel.cpp")
set(main_frame_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/MainFrame.cpp")

foreach(source_file IN ITEMS
        "${side_button_cpp}"
        "${side_button_hpp}"
        "${side_popup_cpp}"
        "${side_popup_hpp}"
        "${link_label_cpp}"
        "${main_frame_cpp}")
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
    endif()
endforeach()

message(STATUS "Native shared-controls accessibility contract passed")
