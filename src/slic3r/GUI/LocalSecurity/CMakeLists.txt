target_sources(libslic3r_gui PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/SecurityPanels.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SecurityPanels.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/PairingQr.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/PairingQr.hpp")
target_link_libraries(libslic3r_gui libslic3r_local_security)
# FeatureServices/LocalSecurityServices.cpp always constructs the identity
# history service and its panel, so the GUI cannot be built without them.
# The history library exists whenever the bundled libgit2 package is found,
# which the top-level project already requires.
if(NOT TARGET libslic3r_identity_history)
    message(FATAL_ERROR "The local security panels require libslic3r_identity_history, which is built only when the libgit2::libgit2package target is available")
endif()
target_sources(libslic3r_gui PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/IdentityHistoryPanel.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/IdentityHistoryPanel.hpp")
target_link_libraries(libslic3r_gui libslic3r_identity_history)
