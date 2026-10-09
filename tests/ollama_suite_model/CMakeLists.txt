# Pure-model tests for the local Ollama suite: hardware-fit evidence, catalog
# snapshots and Model Store queries. OllamaCore.cpp needs only the C++17
# standard library and the bundled nlohmann json header from src/, so this
# target needs no GUI, network, runtime or model download.
add_executable(ollama_suite_model_tests
    ollama_suite_model_tests.cpp
    catalog_snapshot_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/libslic3r/OllamaSuite/OllamaCore.cpp
)
target_include_directories(ollama_suite_model_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(ollama_suite_model_tests PRIVATE cxx_std_17)
target_link_libraries(ollama_suite_model_tests PRIVATE test_common)
set_property(TARGET ollama_suite_model_tests PROPERTY FOLDER "tests")

add_test(NAME ollama_suite_model_tests COMMAND ollama_suite_model_tests "${CATCH_EXTRA_ARGS}")
