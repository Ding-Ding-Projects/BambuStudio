# Pure-model tests for the local file converter's funny-level voice ladders.
# The header under test is wxWidgets-free, so this target needs only Catch2.
add_executable(local_converter_copy_tests
    local_converter_copy_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/LocalConverter/LocalConverterCopy.hpp
)
target_include_directories(local_converter_copy_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(local_converter_copy_tests PRIVATE cxx_std_17)
target_link_libraries(local_converter_copy_tests PRIVATE test_common)
set_property(TARGET local_converter_copy_tests PROPERTY FOLDER "tests")

add_test(NAME local_converter_copy_tests COMMAND local_converter_copy_tests "${CATCH_EXTRA_ARGS}")
