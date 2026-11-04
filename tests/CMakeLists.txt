# TODO Add individual tests as executables in separate directories
# add_subirectory(<testcase>)

set(TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
file(TO_NATIVE_PATH "${TEST_DATA_DIR}" TEST_DATA_DIR)

add_library(Catch2 INTERFACE)
list (APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules/Catch2)
target_include_directories(Catch2 INTERFACE ${CMAKE_CURRENT_LIST_DIR})
add_library(Catch2::Catch2 ALIAS Catch2)
if (APPLE)
	# OSX builds targeting OSX 10.9 do not support new std::uncought_exception()
	# see https://github.com/catchorg/Catch2/issues/1218
	target_compile_definitions(Catch2 INTERFACE -DCATCH_CONFIG_NO_CPP17_UNCAUGHT_EXCEPTIONS)
endif()
include(Catch)

set(CATCH_EXTRA_ARGS "" CACHE STRING "Extra arguments for catch2 test suites.")

add_library(test_common INTERFACE)
target_compile_definitions(test_common INTERFACE TEST_DATA_DIR=R"\(${TEST_DATA_DIR}\)" CATCH_CONFIG_FAST_COMPILE)
target_link_libraries(test_common INTERFACE Catch2::Catch2)

# libslic3r references OpenSSL, libcurl and a few GUI-layer symbols (Http, encryption, nanosvg)
# that are never reached by the tests; let the linker leave those unresolved.
target_link_libraries(test_common INTERFACE libcurl OpenSSL::SSL OpenSSL::Crypto)
if (MSVC)
    target_link_options(test_common INTERFACE /FORCE:UNRESOLVED)
endif()

if (APPLE)
    target_link_libraries(test_common INTERFACE "-liconv -framework IOKit" "-framework CoreFoundation" -lc++)
endif()

# Tests of GUI-layer code. wxWidgets include paths and definitions are applied at directory scope
# in src/, so they are not propagated by linking libslic3r_gui alone.
add_library(test_gui INTERFACE)
get_directory_property(_src_include_dirs DIRECTORY ${PROJECT_SOURCE_DIR}/src INCLUDE_DIRECTORIES)
get_directory_property(_src_compile_defs DIRECTORY ${PROJECT_SOURCE_DIR}/src COMPILE_DEFINITIONS)
target_include_directories(test_gui INTERFACE ${_src_include_dirs} ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(test_gui INTERFACE ${_src_compile_defs})
target_link_libraries(test_gui INTERFACE test_common libslic3r_gui libslic3r)
if (MSVC)
    target_link_libraries(test_gui INTERFACE Setupapi.lib)
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(deviceweb)
add_subdirectory(native_shared_controls)
add_subdirectory(native_gui)
add_subdirectory(native_navigation)
add_subdirectory(non_blocking_notifications)
add_subdirectory(notification_center)
add_subdirectory(home_assistant)
add_subdirectory(native_accessibility)
add_subdirectory(sidebar_filament_combos)
add_subdirectory(libnest2d)
add_subdirectory(language_mode)
add_subdirectory(personal_vocabulary)
add_subdirectory(personal_modes)
add_subdirectory(feature_services)
add_subdirectory(md3_menu)
add_subdirectory(command_palette)
add_subdirectory(app_display_name)
add_subdirectory(app_logo)
add_subdirectory(dim_sum)
add_subdirectory(local_converter_copy)
add_subdirectory(appearance)
add_subdirectory(blank_editor_presets)
add_subdirectory(tab_strip)
add_subdirectory(collapsible_filters)
add_subdirectory(project_history)
add_subdirectory(profile_archive)
add_subdirectory(history_search)
add_subdirectory(workspace)
add_subdirectory(super_confirm)
add_subdirectory(changelog)
add_subdirectory(model_creator)
add_subdirectory(ollama_suite_model)
add_subdirectory(print_workflow)
add_subdirectory(export_everything)
add_subdirectory(bounded_regex)
add_subdirectory(regex_analysis)
add_subdirectory(color_spaces)
add_subdirectory(libslic3r)
add_subdirectory(slic3rutils)
add_subdirectory(fff_print)
add_subdirectory(fila_manager)
add_subdirectory(filament_group)
add_subdirectory(nozzle_inventory)
add_subdirectory(libslic3r_utils)

# This suite was not kept in sync with libslic3r upstream and no longer compiles.
# It is left out of the default build; build the target explicitly to work on reviving it.
add_subdirectory(sla_print EXCLUDE_FROM_ALL)

add_subdirectory(cpp17 EXCLUDE_FROM_ALL)    # does not have to be built all the time
# add_subdirectory(example)
add_subdirectory(settings_drafts)

add_subdirectory(quiet_prompts)
