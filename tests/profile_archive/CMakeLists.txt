if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SLIC3R_WX_STABLE)
    find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
else()
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
endif()
include(${wxWidgets_USE_FILE})
add_executable(profile_archive_tests profile_archive_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/ConfigProfileArchive.cpp)

if(SLIC3R_STATIC)
    # Match libslic3r_gui and the static wxWidgets build's assertion ABI.
    # This does not disable Catch2 assertions.
    target_compile_definitions(profile_archive_tests PRIVATE wxDEBUG_LEVEL=0)
endif()
target_include_directories(profile_archive_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(profile_archive_tests PRIVATE test_common ${wxWidgets_LIBRARIES} PNG::PNG)
set_property(TARGET profile_archive_tests PROPERTY FOLDER "tests")
add_test(profile_archive_tests profile_archive_tests "${CATCH_EXTRA_ARGS}")
