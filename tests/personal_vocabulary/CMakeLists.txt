if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SLIC3R_WX_STABLE)
    find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
else()
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
endif()
include(${wxWidgets_USE_FILE})

add_executable(personal_vocabulary_tests personal_vocabulary_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/PersonalVocabulary.cpp)
add_executable(personal_vocabulary_probe personal_vocabulary_probe.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/PersonalVocabulary.cpp)
foreach(target personal_vocabulary_tests personal_vocabulary_probe)
    if(SLIC3R_STATIC)
        # Match libslic3r_gui and the static wxWidgets build's assertion ABI.
        # This does not disable Catch2 assertions.
        target_compile_definitions(${target} PRIVATE wxDEBUG_LEVEL=0)
    endif()
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE ${wxWidgets_LIBRARIES} PNG::PNG)
    set_property(TARGET ${target} PROPERTY FOLDER "tests")
endforeach()
target_link_libraries(personal_vocabulary_tests PRIVATE test_common)
add_test(personal_vocabulary_tests personal_vocabulary_tests "${CATCH_EXTRA_ARGS}")

add_test(NAME personal_vocabulary_native_contract
    COMMAND ${CMAKE_COMMAND} -DBAMBU_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/native_display_contract.cmake)
