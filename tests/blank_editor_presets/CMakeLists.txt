# Pure-model tests for the start-from presets blank editors offer. The model
# and the shipped defaults table are wxWidgets-free, so this target needs only
# Catch2 and the bundled nlohmann json header from src/.
add_executable(blank_editor_presets_tests
    blank_editor_presets_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Presets/BlankEditorPresets.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Presets/BlankEditorPresets.hpp
    ${PROJECT_SOURCE_DIR}/src/libslic3r/PresentationDefaults.hpp
)
target_include_directories(blank_editor_presets_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(blank_editor_presets_tests PRIVATE cxx_std_17)
target_link_libraries(blank_editor_presets_tests PRIVATE test_common)
set_property(TARGET blank_editor_presets_tests PROPERTY FOLDER "tests")

add_test(NAME blank_editor_presets_tests COMMAND blank_editor_presets_tests "${CATCH_EXTRA_ARGS}")
