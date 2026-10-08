# Pure-model tests for the regex builder's structure, capability and
# backtracking-risk analysis. The header under test is wxWidgets-free and
# Boost-free; the worker-backed probes live in bounded_regex_tests.
add_executable(regex_analysis_tests
    regex_analysis_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Widgets/RegexAnalysis.hpp
)
target_include_directories(regex_analysis_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(regex_analysis_tests PRIVATE cxx_std_17)
target_link_libraries(regex_analysis_tests PRIVATE test_common)
set_property(TARGET regex_analysis_tests PROPERTY FOLDER "tests")

add_test(NAME regex_analysis_tests COMMAND regex_analysis_tests "${CATCH_EXTRA_ARGS}")
