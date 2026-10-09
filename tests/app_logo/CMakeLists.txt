# App logo customization: the header-only core (presets, rendering, bounded
# BMP primitive, settings round trip) and the selection model the app chrome
# uses (shipped mark, stored value, chrome targets, render plans). Neither
# target needs wxWidgets.
add_executable(app_logo_core_tests test_logo.cpp)
target_include_directories(app_logo_core_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(app_logo_core_tests PRIVATE cxx_std_17)
set_property(TARGET app_logo_core_tests PROPERTY FOLDER "tests")
add_test(NAME app_logo_core COMMAND app_logo_core_tests)

add_executable(app_logo_tests
    app_logo_tests_main.cpp
    ${PROJECT_SOURCE_DIR}/src/libslic3r/AppLogo/Logo.hpp
)
target_include_directories(app_logo_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(app_logo_tests PRIVATE cxx_std_17)
target_link_libraries(app_logo_tests PRIVATE test_common)
set_property(TARGET app_logo_tests PROPERTY FOLDER "tests")
add_test(NAME app_logo_tests COMMAND app_logo_tests "${CATCH_EXTRA_ARGS}")
