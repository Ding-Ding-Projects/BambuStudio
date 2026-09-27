# Renamable app display name: validation/sanitization rules (Catch2, no wx
# dependency) plus a source-contract guard that identity-bound code paths never
# read the display-name key.
add_executable(app_display_name_tests
    app_display_name_tests_main.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/AppDisplayName.cpp
)
target_include_directories(app_display_name_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(app_display_name_tests test_common)
set_property(TARGET app_display_name_tests PROPERTY FOLDER "tests")

add_test(app_display_name_tests app_display_name_tests "${CATCH_EXTRA_ARGS}")

add_test(
    NAME app_display_name_identity_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/app_display_name_identity_contract.cmake"
)
