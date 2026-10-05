cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(LocalConverterTests LANGUAGES C CXX)
    enable_testing()
endif()
get_filename_component(CONVERTER_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_library(local_converter_core STATIC
    "${CONVERTER_ROOT}/src/libslic3r/LocalConverter/Converter.cpp"
    "${CONVERTER_ROOT}/src/libslic3r/LocalConverter/ArchiveAdapter.cpp"
    "${CONVERTER_ROOT}/src/miniz/miniz.c"
    "${CONVERTER_ROOT}/src/libslic3r/LocalConverter/Worker.cpp")
target_compile_features(local_converter_core PUBLIC cxx_std_17)
target_include_directories(local_converter_core PUBLIC "${CONVERTER_ROOT}/src")
set(LOCAL_CONVERTER_QPDF_SDK "" CACHE PATH "Verified qpdf SDK directory")
if(LOCAL_CONVERTER_QPDF_SDK)
    target_sources(local_converter_core PRIVATE
        "${CONVERTER_ROOT}/src/libslic3r/LocalConverter/PdfAdapter.cpp"
        "${CONVERTER_ROOT}/src/libslic3r/LocalConverter/PdfRequest.cpp"
        "${CONVERTER_ROOT}/src/libslic3r/LocalConverter/PdfPackage.cpp")
    target_include_directories(local_converter_core PRIVATE "${LOCAL_CONVERTER_QPDF_SDK}/include")
    target_compile_definitions(local_converter_core PUBLIC LOCAL_CONVERTER_WITH_PDF=1)
    add_executable(local_converter_pdf_tests pdf_tests.cpp)
    target_link_libraries(local_converter_pdf_tests PRIVATE local_converter_core)
    add_executable(local_converter_pdf_request_tests pdf_request_tests.cpp)
    target_link_libraries(local_converter_pdf_request_tests PRIVATE local_converter_core)
endif()
if(MSVC)
    target_compile_options(local_converter_core PRIVATE /utf-8 /W4)
    target_link_libraries(local_converter_core PUBLIC bcrypt userenv advapi32)
endif()
add_executable(BambuStudio_converter_worker "${CONVERTER_ROOT}/src/libslic3r/LocalConverter/WorkerMain.cpp")
target_link_libraries(BambuStudio_converter_worker PRIVATE local_converter_core)
add_executable(local_converter_tests converter_tests.cpp)
target_link_libraries(local_converter_tests PRIVATE local_converter_core)
add_test(NAME local_converter_contracts COMMAND local_converter_tests)
