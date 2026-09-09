# Scheduled settings model (schema, calendar matching, precedence, API contract,
# override/restore arithmetic) and the Home Assistant settings-sync deny-list and
# payload builder. Both halves are header-only and wx-free, so this suite links
# only Catch2 and nlohmann::json.
add_executable(scheduled_settings_tests
    scheduled_settings_tests_main.cpp
    home_assistant_settings_sync_tests.cpp
)
target_include_directories(scheduled_settings_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(scheduled_settings_tests PRIVATE cxx_std_17)
target_link_libraries(scheduled_settings_tests PRIVATE test_common)
set_property(TARGET scheduled_settings_tests PROPERTY FOLDER "tests")

add_test(NAME scheduled_settings_tests COMMAND scheduled_settings_tests "${CATCH_EXTRA_ARGS}")
