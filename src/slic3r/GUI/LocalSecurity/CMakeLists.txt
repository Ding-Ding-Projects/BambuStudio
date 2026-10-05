target_sources(libslic3r_gui PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/SecurityPanels.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SecurityPanels.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/PairingQr.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PairingQr.hpp")
target_link_libraries(libslic3r_gui libslic3r_local_security)
if(TARGET libslic3r_identity_history)
    target_sources(libslic3r_gui PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/IdentityHistoryPanel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/IdentityHistoryPanel.hpp")
    target_link_libraries(libslic3r_gui libslic3r_identity_history)
endif()
