add_executable(md3_menu_tests
    md3_menu_tests_main.cpp
)

# MD3MenuModel.hpp is header-only and depends on wx core/base only, so this
# target mirrors tests/language_mode: wxWidgets is rediscovered here because
# the src/ tree's directory-scoped wx variables are not visible from tests/.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SLIC3R_WX_STABLE)
    find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
else()
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
endif()
include(${wxWidgets_USE_FILE})

target_include_directories(md3_menu_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(md3_menu_tests test_common ${wxWidgets_LIBRARIES} PNG::PNG)
set_property(TARGET md3_menu_tests PROPERTY FOLDER "tests")

add_test(md3_menu_tests md3_menu_tests "${CATCH_EXTRA_ARGS}")
