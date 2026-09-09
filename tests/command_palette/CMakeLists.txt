add_executable(command_palette_tests
    command_palette_tests_main.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/CommandPaletteIndex.cpp
)

# wxWidgets is discovered under src/, whose directory-scoped variables are not
# visible from this sibling tests/ tree. The index module only needs wx/accel.h
# (wxAcceleratorEntry is a plain value type), so discover core+base here.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SLIC3R_WX_STABLE)
    find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
else()
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
endif()
include(${wxWidgets_USE_FILE})

target_include_directories(command_palette_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
# The completeness guards read Preferences.cpp and docs/features from the
# checked-in tree, so the index cannot drift from what actually ships.
target_compile_definitions(command_palette_tests PRIVATE
    COMMAND_PALETTE_TEST_SOURCE_DIR=R"\(${PROJECT_SOURCE_DIR}\)"
)
target_link_libraries(command_palette_tests test_common ${wxWidgets_LIBRARIES} PNG::PNG)
set_property(TARGET command_palette_tests PROPERTY FOLDER "tests")

add_test(command_palette_tests command_palette_tests "${CATCH_EXTRA_ARGS}")
