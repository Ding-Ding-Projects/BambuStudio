cmake_minimum_required(VERSION 3.13)
project(BambuStudio-native)

add_subdirectory(build-utils)
add_subdirectory(admesh)
# add_subdirectory(avrdude)
# boost/nowide
add_subdirectory(boost)
add_subdirectory(clipper)
add_subdirectory(clipper2)
add_subdirectory(miniz)
add_subdirectory(minilzo)
add_subdirectory(glu-libtess)
add_subdirectory(qhull)
add_subdirectory(Shiny)
add_subdirectory(semver)
add_subdirectory(libigl)
add_subdirectory(hints)
add_subdirectory(mcut)

# Adding libnest2d project for bin packing...
add_subdirectory(libnest2d)

# libnoise for structured fuzzy skin (Perlin, Billow, etc.); use dep via CMAKE_PREFIX_PATH
find_package(libnoise REQUIRED)

add_subdirectory(libslic3r)

if (SLIC3R_GUI)
    add_subdirectory(imgui)
    add_subdirectory(imguizmo)
    add_subdirectory(hidapi)
    include_directories(hidapi/include)

    message(STATUS "WXWIN environment set to: $ENV{WXWIN}")

    find_package(wxWidgets 3.1 REQUIRED COMPONENTS html adv gl core base webview aui net media richtext xml)

    include(${wxWidgets_USE_FILE})

    find_package(JPEG QUIET)
    find_package(TIFF QUIET)

    string(REGEX MATCH "wxpng" WX_PNG_BUILTIN ${wxWidgets_LIBRARIES})
    if (PNG_FOUND AND NOT WX_PNG_BUILTIN)
        list(FILTER wxWidgets_LIBRARIES EXCLUDE REGEX png)
        list(APPEND wxWidgets_LIBRARIES ${PNG_LIBRARIES})
    endif ()

    string(REGEX MATCH "wxtiff" WX_TIFF_BUILTIN ${wxWidgets_LIBRARIES})
    if (TIFF_FOUND AND NOT WX_TIFF_BUILTIN)
        list(FILTER wxWidgets_LIBRARIES EXCLUDE REGEX tiff)
        list(APPEND wxWidgets_LIBRARIES ${TIFF_LIBRARIES})
    endif ()

    string(REGEX MATCH "wxjpeg" WX_JPEG_BUILTIN ${wxWidgets_LIBRARIES})
    if (JPEG_FOUND AND NOT WX_JPEG_BUILTIN)
        list(FILTER wxWidgets_LIBRARIES EXCLUDE REGEX jpeg)
        list(APPEND wxWidgets_LIBRARIES ${JPEG_LIBRARIES})
    endif ()

    string(REGEX MATCH "wxexpat" WX_EXPAT_BUILTIN ${wxWidgets_LIBRARIES})
    if (EXPAT_FOUND AND NOT WX_EXPAT_BUILTIN)
        list(FILTER wxWidgets_LIBRARIES EXCLUDE REGEX expat)
        list(APPEND wxWidgets_LIBRARIES ${EXPAT_LIBRARIES})
    endif ()

    # libslic3r_gui links to OpenGL itself; keep wx from adding its own entry.
    list(FILTER wxWidgets_LIBRARIES EXCLUDE REGEX OpenGL)

#    list(REMOVE_ITEM wxWidgets_LIBRARIES oleacc)
    message(STATUS "wx libs: ${wxWidgets_LIBRARIES}")

    add_subdirectory(slic3r)
endif()


# Create a slic3r executable
# Process manifests for various platforms.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/platform/msw/BambuStudio.rc.in ${CMAKE_CURRENT_BINARY_DIR}/BambuStudio.rc @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/platform/msw/BambuStudio.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/BambuStudio.manifest @ONLY)
# BaseException/StackWalker back SET_DEFULTER_HANDLER() in bambustu_main(): the
# unhandled-exception filter that writes <data_dir>/log/crash_*.log. They were
# present in the tree but compiled by nothing, so the handler could not be
# enabled without a link error -- and while it was disabled a crash left no
# dump, no stack and no marker in the studio log at all. Windows-only (Dbghelp,
# and this fork is Windows-only anyway).
add_library(BambuStudio SHARED BambuStudio.cpp BambuStudio.hpp
    BaseException.cpp BaseException.h
    StackWalker.cpp StackWalker.h)

target_link_libraries(BambuStudio libslic3r cereal)
# Manifest is provided through BambuStudio.rc, don't generate your own.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")

# Keep the bounded-regex worker available to headless test configurations as
# well as GUI builds so evaluator safety can be tested without compiling or
# launching Studio's UI. Boost.Regex supplies portable ECMAScript multiline
# anchors; MSVC's std::regex omits the C++17 multiline flag.
if (SLIC3R_GUI OR SLIC3R_BUILD_TESTS)
    add_executable(BambuStudio_regex_worker
        slic3r/GUI/Widgets/BoundedRegexWorkerMain.cpp
        slic3r/GUI/Widgets/BoundedRegex.hpp
        slic3r/GUI/Widgets/BoundedRegexProtocol.hpp
    )
    target_include_directories(BambuStudio_regex_worker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(BambuStudio_regex_worker PRIVATE cxx_std_17)
    target_compile_definitions(BambuStudio_regex_worker PRIVATE BOOST_REGEX_MAX_STATE_COUNT=1000000)
    target_link_libraries(BambuStudio_regex_worker PRIVATE Boost::regex)
    set_target_properties(BambuStudio_regex_worker PROPERTIES OUTPUT_NAME "bambu-regex-worker")
endif ()

# Add the Slic3r GUI library, libcurl, OpenGL and GLU libraries.
if (SLIC3R_GUI)
#    target_link_libraries(BambuStudio ws2_32 uxtheme setupapi libslic3r_gui ${wxWidgets_LIBRARIES})
target_link_libraries(BambuStudio libslic3r_gui)

    # User-authored regular expressions execute in the helper above. The GUI
    # client can terminate/restart it without leaving an unbounded regex thread
    # in the application.
    add_dependencies(BambuStudio BambuStudio_regex_worker)

    # Generate debug symbols even in release mode.
    target_link_options(BambuStudio PUBLIC "$<$<CONFIG:RELEASE>:/DEBUG>")
    target_link_libraries(BambuStudio user32.lib Setupapi.lib)
    #if (WIN32)
    #    find_library(PSAPI_LIB NAMES Psapi)
    #    target_link_libraries(BambuStudio ${PSAPI_LIB})
    #endif ()
endif ()

# On Windows, a shim application is required to produce a console / non console version of the Slic3r application.
# Also the shim may load the Mesa software OpenGL renderer if the default renderer does not support OpenGL 2.0 and higher.
if (WIN32)
    if (MINGW)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -municode")
    endif()

    add_executable(BambuStudio_app_gui WIN32 BambuStudio_app_msvc.cpp ${CMAKE_CURRENT_BINARY_DIR}/BambuStudio.rc)
    # Generate debug symbols even in release mode.
    if(MSVC)
        target_link_options(BambuStudio_app_gui PUBLIC "$<$<CONFIG:RELEASE>:/DEBUG>")
    endif()
    target_compile_definitions(BambuStudio_app_gui PRIVATE -DSLIC3R_WRAPPER_NOCONSOLE)
    add_dependencies(BambuStudio_app_gui BambuStudio)
    set_target_properties(BambuStudio_app_gui PROPERTIES OUTPUT_NAME "bambu-studio")
    target_link_libraries(BambuStudio_app_gui PRIVATE boost_headeronly)
endif ()

if (SLIC3R_GUI AND TARGET device_page_build)
    add_dependencies(BambuStudio device_page_build)
    if (TARGET BambuStudio_app_gui)
        add_dependencies(BambuStudio_app_gui device_page_build)
    endif()
endif()

# Link the resources dir to where Slic3r GUI expects it
set(output_dlls_Release "")
set(output_dlls_Debug "")
set(output_dlls_RelWithDebInfo "")
if (WIN32)
    # This has to be a separate target due to the windows command line length limits
    add_custom_target(BambuStudioDllsCopy ALL DEPENDS BambuStudio)

    if (CMAKE_CONFIGURATION_TYPES)
        foreach (CONF ${CMAKE_CONFIGURATION_TYPES})
            file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${CONF}" WIN_CONF_OUTPUT_DIR)
            file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${CONF}/resources" WIN_RESOURCES_SYMLINK)
            add_custom_command(TARGET BambuStudio POST_BUILD
                COMMAND if exist "${WIN_CONF_OUTPUT_DIR}" "("
                        if not exist "${WIN_RESOURCES_SYMLINK}" "("
                            mklink /J "${WIN_RESOURCES_SYMLINK}" "${SLIC3R_RESOURCES_DIR_WIN}"
                        ")"
                    ")"
                COMMENT "Symlinking the resources directory into the build tree"
                VERBATIM
            )
        endforeach ()

        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            bambustudio_copy_dlls(BambuStudioDllsCopy "Debug" "d" output_dlls_Debug)
        elseif("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
            bambustudio_copy_dlls(BambuStudioDllsCopy "RelWithDebInfo" "" output_dlls_RelWithDebInfo)
        else()
            bambustudio_copy_dlls(BambuStudioDllsCopy "Release" "" output_dlls_Release)
        endif()
    else ()
        file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/resources" WIN_RESOURCES_SYMLINK)
        add_custom_command(TARGET BambuStudio POST_BUILD
            COMMAND if not exist "${WIN_RESOURCES_SYMLINK}" "(" mklink /J "${WIN_RESOURCES_SYMLINK}" "${SLIC3R_RESOURCES_DIR_WIN}" ")"
            COMMENT "Symlinking the resources directory into the build tree"
            VERBATIM
        )
        # Single-config generators (Ninja on CI) skipped the runtime DLL copy
        # entirely: output_dlls_<config> stayed empty, install() shipped a
        # payload whose BambuStudio.dll could never load (OCCT/ffmpeg/gmp
        # missing, LoadLibrary error 126). Mirror the multi-config branch.
        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            bambustudio_copy_dlls(BambuStudioDllsCopy "Debug" "d" output_dlls_Debug)
        elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
            bambustudio_copy_dlls(BambuStudioDllsCopy "RelWithDebInfo" "" output_dlls_RelWithDebInfo)
        else ()
            bambustudio_copy_dlls(BambuStudioDllsCopy "Release" "" output_dlls_Release)
        endif ()
    endif ()


endif ()

# Slic3r binary install target. Default build type is release in case no CMAKE_BUILD_TYPE is provided.
if( ("${CMAKE_BUILD_TYPE}" STREQUAL "Release") OR ("${CMAKE_BUILD_TYPE}" STREQUAL "") )
    set (build_type "Release")
    set(CMAKE_BUILD_POSTFIX "")
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set (build_type "RelWithDebInfo")
    set(CMAKE_BUILD_POSTFIX "")
else()
    set (build_type "Debug")
    set(CMAKE_BUILD_POSTFIX "d")
endif()
message(STATUS "libslic3r-CMAKE_BUILD_TYPE: ${build_type}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")
if (WIN32)
    install(TARGETS BambuStudio RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
    if (MSVC)
        install(TARGETS BambuStudio_app_gui RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
    endif ()
    if (TARGET BambuStudio_regex_worker)
        install(TARGETS BambuStudio_regex_worker RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")
    endif ()
    install(FILES ${output_dlls_${build_type}} DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif ()
