add_executable(tab_strip_tests
    tab_strip_tests_main.cpp
)

# TabStripModel.hpp is header-only and depends on wx core/base plus the
# vendored nlohmann/json, so this target mirrors tests/md3_menu: wxWidgets is
# rediscovered here because the src/ tree's directory-scoped wx variables are
# not visible from tests/.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SLIC3R_WX_STABLE)
    find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
else()
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
endif()
include(${wxWidgets_USE_FILE})

target_include_directories(tab_strip_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tab_strip_tests test_common ${wxWidgets_LIBRARIES} PNG::PNG)
set_property(TARGET tab_strip_tests PROPERTY FOLDER "tests")

add_test(tab_strip_tests tab_strip_tests "${CATCH_EXTRA_ARGS}")
