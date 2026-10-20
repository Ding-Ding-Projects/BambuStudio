add_executable(appearance_tests
    appearance_tests_main.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Appearance/ElementStyle.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Appearance/ElementMotion.cpp
)

# ElementStyle.cpp depends on wx core/base and the vendored nlohmann::json only,
# so this target mirrors tests/md3_menu: wxWidgets is rediscovered here because
# the src/ tree's directory-scoped wx variables are not visible from tests/.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SLIC3R_WX_STABLE)
    find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
else()
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
endif()
include(${wxWidgets_USE_FILE})

target_include_directories(appearance_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(appearance_tests test_common ${wxWidgets_LIBRARIES} PNG::PNG)
set_property(TARGET appearance_tests PROPERTY FOLDER "tests")

add_test(appearance_tests appearance_tests "${CATCH_EXTRA_ARGS}")

add_executable(style_metrics_tests style_metrics_standalone.cpp)
target_include_directories(style_metrics_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(style_metrics_tests PRIVATE cxx_std_17)
add_test(NAME style_metrics_tests COMMAND style_metrics_tests)

# The per-element motion model (Animation and transition tab) is pure C++17
# plus the bundled nlohmann json header, so it needs neither wx nor Catch2.
add_executable(element_motion_tests element_motion_standalone.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Appearance/ElementMotion.cpp)
target_include_directories(element_motion_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(element_motion_tests PRIVATE cxx_std_17)
add_test(NAME element_motion_tests COMMAND element_motion_tests)
