# Pure C++17 tests for the shared bulk-action library: the BulkSelection
# multi-select model, the rename-by-pattern engine and the BulkActionPlan
# counts. None of the headers under test pull in wxWidgets, so this target
# links only the shared Catch2 wiring.
add_executable(bulk_actions_tests
    bulk_actions_tests.cpp
)

target_include_directories(bulk_actions_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(bulk_actions_tests test_common)
set_property(TARGET bulk_actions_tests PROPERTY FOLDER "tests")

add_test(bulk_actions_tests bulk_actions_tests "${CATCH_EXTRA_ARGS}")
