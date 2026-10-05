target_sources(libslic3r_gui PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/SecurityPanels.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/SecurityPanels.hpp")
target_link_libraries(libslic3r_gui PRIVATE libslic3r_local_security)
