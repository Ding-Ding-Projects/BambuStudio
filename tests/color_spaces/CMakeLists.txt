# Pure header-only colour maths (MD3::Color); no wxWidgets needed.
add_executable(color_spaces_tests color_spaces_tests.cpp)
target_include_directories(color_spaces_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(color_spaces_tests PRIVATE cxx_std_17)
target_link_libraries(color_spaces_tests test_common)
set_property(TARGET color_spaces_tests PROPERTY FOLDER "tests")

add_test(color_spaces_tests color_spaces_tests "${CATCH_EXTRA_ARGS}")
