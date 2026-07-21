add_executable(language_mode_tests
    language_mode_tests_main.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/LanguageMode.cpp
)

# wxWidgets is discovered under src/, whose directory-scoped variables and
# usage requirements are not visible from this sibling tests/ tree. Discover
# the two components used by LanguageMode directly here so this focused test
# target remains independently configurable.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SLIC3R_WX_STABLE)
    find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
else()
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
endif()
include(${wxWidgets_USE_FILE})

target_include_directories(language_mode_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(language_mode_tests PRIVATE
    LANGUAGE_MODE_TEST_I18N_DIR=R"\(${PROJECT_SOURCE_DIR}/resources/i18n\)"
)
target_link_libraries(language_mode_tests test_common ${wxWidgets_LIBRARIES} PNG::PNG)
set_property(TARGET language_mode_tests PROPERTY FOLDER "tests")

add_test(language_mode_tests language_mode_tests "${CATCH_EXTRA_ARGS}")
