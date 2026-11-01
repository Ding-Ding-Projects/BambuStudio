# Material 3 dynamic colour (MD3::Dynamic): pure colour maths, no wxWidgets.
# The reference data is generated from Google's material-color-utilities by
# generate_reference.mjs in this directory.
add_executable(md3_dynamic_color_tests
    md3_dynamic_color_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Widgets/MD3DynamicColor.cpp)
target_include_directories(md3_dynamic_color_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(md3_dynamic_color_tests PRIVATE cxx_std_17)
target_link_libraries(md3_dynamic_color_tests test_common)
set_property(TARGET md3_dynamic_color_tests PROPERTY FOLDER "tests")

add_test(md3_dynamic_color_tests md3_dynamic_color_tests "${CATCH_EXTRA_ARGS}")
