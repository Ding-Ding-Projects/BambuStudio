if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(webview_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/WebViewDialog.cpp")
set(webview_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/WebViewDialog.hpp")
set(disconnect_html "${BAMBU_SOURCE_DIR}/resources/web/homepage3/disconnect.html")
file(READ "${webview_cpp}" cpp_source)
file(READ "${webview_hpp}" hpp_source)
file(READ "${disconnect_html}" disconnect_source)

if(cpp_source MATCHES "MakeDisconnectUrl|LoadURL\\([^\\r\\n]*disconnect\\.html")
    message(FATAL_ERROR "Cloud failures must not replace a usable WebView with disconnect.html")
endif()
if(hpp_source MATCHES "MakeDisconnectUrl")
    message(FATAL_ERROR "The legacy disconnect navigation API must remain retired")
endif()

foreach(required_symbol
        "ShowCloudPageFailure"
        "ClearCloudPageFailure"
        "OnCloudPageRetry"
        "m_info->AddButton"
        "m_info->ShowMessage"
        "wxWEBVIEW_NAV_ERR_CONNECTION"
        "wxWEBVIEW_NAV_ERR_NOT_FOUND"
        "HomeWebFailureKind::CloudAuthentication")
    if(NOT cpp_source MATCHES "${required_symbol}")
        message(FATAL_ERROR "Missing retained-page failure integration: ${required_symbol}")
    endif()
endforeach()

foreach(required_message
        "Network unavailable\\. The current page remains open\\."
        "Cloud sign-in could not be completed\\. The current page remains open\\."
        "The requested cloud page was not found\\. The current page remains open\\.")
    if(NOT cpp_source MATCHES "${required_message}")
        message(FATAL_ERROR "Missing truthful cloud failure message: ${required_message}")
    endif()
endforeach()

if(NOT disconnect_source MATCHES "<main[^>]*role=\"alert\"[^>]*aria-live=\"assertive\"")
    message(FATAL_ERROR "The compatibility disconnect document must announce its error accessibly")
endif()
if(NOT disconnect_source MATCHES "<button[^>]*id=\"WarnBtn\"[^>]*type=\"button\"")
    message(FATAL_ERROR "The compatibility retry control must be a semantic button")
endif()

message(STATUS "Home WebView retained-page failure contract passed")
