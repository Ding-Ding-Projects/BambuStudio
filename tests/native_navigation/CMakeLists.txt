# This source provides its own Catch main and tests the header-only navigation
# model. It does not link the native UI or require wxWidgets.
add_executable(native_navigation_tests navigation_layout_tests.cpp)
target_compile_features(native_navigation_tests PRIVATE cxx_std_17)
target_include_directories(native_navigation_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/tests"
    "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(native_navigation_tests PRIVATE test_common)
set_property(TARGET native_navigation_tests PROPERTY FOLDER "tests")
add_test(NAME native_navigation_tests COMMAND native_navigation_tests ${CATCH_EXTRA_ARGS})
