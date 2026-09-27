add_executable(export_everything_tests
    export_everything_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Export/ExportEverything.cpp
)

# The export engine is deliberately wx-free: it needs only the standard
# library and the vendored miniz, so this target compiles the engine source
# directly instead of linking the whole GUI library.
target_include_directories(export_everything_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(export_everything_tests test_common miniz)
set_property(TARGET export_everything_tests PROPERTY FOLDER "tests")

add_test(export_everything_tests export_everything_tests "${CATCH_EXTRA_ARGS}")
