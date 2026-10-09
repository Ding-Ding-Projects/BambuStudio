# Pure-model tests for the shared collapsible filter and statistics container.
# CollapsibleFilterState.hpp has no wxWidgets dependency, so this target links
# only the shared Catch2 wiring and stays independently configurable.
add_executable(collapsible_filters_tests
    collapsible_filters_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Widgets/CollapsibleFilterState.hpp
)

target_include_directories(collapsible_filters_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(collapsible_filters_tests PRIVATE cxx_std_17)
target_link_libraries(collapsible_filters_tests PRIVATE test_common)
set_property(TARGET collapsible_filters_tests PROPERTY FOLDER "tests")

add_test(NAME collapsible_filters_tests COMMAND collapsible_filters_tests "${CATCH_EXTRA_ARGS}")
