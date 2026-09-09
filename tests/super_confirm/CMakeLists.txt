# Pure state-machine tests for the destructive-action super confirmation gate.
# The header under test has no wxWidgets dependency, so this target links only
# the shared Catch2 wiring and stays independently configurable.
add_executable(super_confirm_tests
    super_confirm_tests.cpp
)

target_include_directories(super_confirm_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(super_confirm_tests test_common)
set_property(TARGET super_confirm_tests PROPERTY FOLDER "tests")

add_test(super_confirm_tests super_confirm_tests "${CATCH_EXTRA_ARGS}")
