add_executable(language_mode_tests
    language_mode_tests_main.cpp
    human_date_tests.cpp
    funny_level_tests.cpp
    bilingual_registry_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/LanguageMode.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/PersonalVocabulary.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/BilingualRegistry.cpp
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

if(SLIC3R_STATIC)
    # Match libslic3r_gui and the static wxWidgets build's assertion ABI.
    # This does not disable Catch2 assertions.
    target_compile_definitions(language_mode_tests PRIVATE wxDEBUG_LEVEL=0)
endif()

target_include_directories(language_mode_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
if(WIN32)
    # Match the production catalog output, including the generated Cantonese
    # catalog that is deliberately absent from the source resource directory.
    set(LANGUAGE_MODE_TEST_CATALOG_DIR "${CMAKE_BINARY_DIR}/fork-i18n")
    add_dependencies(language_mode_tests fork_catalogs)
else()
    set(LANGUAGE_MODE_TEST_CATALOG_DIR "${PROJECT_SOURCE_DIR}/resources/i18n")
endif()
target_compile_definitions(language_mode_tests PRIVATE
    LANGUAGE_MODE_TEST_I18N_DIR=R"\(${LANGUAGE_MODE_TEST_CATALOG_DIR}\)"
)
target_link_libraries(language_mode_tests test_common ${wxWidgets_LIBRARIES} PNG::PNG)
set_property(TARGET language_mode_tests PROPERTY FOLDER "tests")

add_test(language_mode_tests language_mode_tests "${CATCH_EXTRA_ARGS}")

# Pure-model tests for the funny-level step of the first-run setup guide. The
# header under test is wxWidgets-free, so this target needs only Catch2 and the
# bundled nlohmann json header from src/.
add_executable(first_run_disclosure_tests
    first_run_disclosure_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/FirstRunFunnyDisclosure.hpp
)
target_include_directories(first_run_disclosure_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(first_run_disclosure_tests PRIVATE cxx_std_17)
target_link_libraries(first_run_disclosure_tests PRIVATE test_common)
set_property(TARGET first_run_disclosure_tests PROPERTY FOLDER "tests")

add_test(NAME first_run_disclosure_tests COMMAND first_run_disclosure_tests "${CATCH_EXTRA_ARGS}")
