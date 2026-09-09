add_executable(changelog_tests
    changelog_tests.cpp
)

target_include_directories(changelog_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(changelog_tests PRIVATE
    CHANGELOG_TEST_RESOURCE=R"\(${PROJECT_SOURCE_DIR}/resources/changelog/changelog.json\)"
)
target_link_libraries(changelog_tests test_common libslic3r_changelog)
set_property(TARGET changelog_tests PROPERTY FOLDER "tests")

add_test(changelog_tests changelog_tests "${CATCH_EXTRA_ARGS}")
