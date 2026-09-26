add_executable(model_creator_tests
    model_creator_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/ModelCreator/SceneSpec.cpp
)
target_include_directories(model_creator_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(model_creator_tests test_common)
set_property(TARGET model_creator_tests PROPERTY FOLDER "tests")
add_test(model_creator_tests model_creator_tests "${CATCH_EXTRA_ARGS}")
