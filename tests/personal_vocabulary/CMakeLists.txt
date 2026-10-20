if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SLIC3R_WX_STABLE)
    find_package(wxWidgets 3.0 REQUIRED COMPONENTS core base)
else()
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
endif()
include(${wxWidgets_USE_FILE})

add_executable(personal_vocabulary_tests personal_vocabulary_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/PersonalVocabulary.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/PersonalVocabularySchema.cpp)
add_executable(personal_vocabulary_probe personal_vocabulary_probe.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/PersonalVocabulary.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/PersonalVocabularySchema.cpp)
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

# The file contract on its own: no wxWidgets and no Catch2, so the same source
# also builds with a plain compiler (see the command in the test file).
add_executable(personal_vocabulary_schema_test personal_vocabulary_schema_test.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/PersonalVocabularySchema.cpp)
target_include_directories(personal_vocabulary_schema_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(personal_vocabulary_schema_test PRIVATE cxx_std_17)
set_property(TARGET personal_vocabulary_schema_test PROPERTY FOLDER "tests")
add_test(NAME personal_vocabulary_schema_test COMMAND personal_vocabulary_schema_test)

add_test(NAME personal_vocabulary_native_contract
    COMMAND ${CMAKE_COMMAND} -DBAMBU_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/native_display_contract.cmake)
