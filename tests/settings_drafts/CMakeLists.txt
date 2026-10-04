add_executable(settings_drafts_tests
    settings_drafts_tests.cpp
    ../../src/slic3r/GUI/SettingsDraftStore.cpp
)
target_link_libraries(settings_drafts_tests test_common libslic3r)
set_property(TARGET settings_drafts_tests PROPERTY FOLDER "tests")
add_test(settings_drafts_tests settings_drafts_tests "${CATCH_EXTRA_ARGS}")
