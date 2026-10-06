if(WIN32)
    set(library_build_type "Shared")
else()
    set(library_build_type "Static")
endif()

bambustudio_add_cmake_project(OCCT
    URL https://github.com/Open-Cascade-SAS/OCCT/archive/refs/tags/V7_6_0.zip
    URL_HASH SHA256=28334f0e98f1b1629799783e9b4d21e05349d89e695809d7e6dfa45ea43e1dbc
    PATCH_COMMAND ${CMAKE_COMMAND}
        "-DPATCH_ROOT=${CMAKE_SOURCE_DIR}/.."
        "-DPATCH_SOURCE=<SOURCE_DIR>"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        -DPATCH_COUNT=2
        "-DPATCH_1=${CMAKE_CURRENT_LIST_DIR}/0001-OCCT-fix.patch"
        "-DPATCH_2=${CMAKE_CURRENT_LIST_DIR}/0002-OCCT-config-flag-quoting.patch"
        -P "${CMAKE_SOURCE_DIR}/../cmake/modules/ApplyPatchesIdempotently.cmake"
    #DEPENDS dep_Boost
    #DEPENDS dep_FREETYPE
    CMAKE_ARGS
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        -DBUILD_LIBRARY_TYPE=${library_build_type}
        -DUSE_TK=OFF
        -DUSE_TBB=OFF
	#-DUSE_FREETYPE=OFF
        -DUSE_FFMPEG=OFF
        -DUSE_VTK=OFF
        -DBUILD_DOC_Overview=OFF
        -DBUILD_MODULE_ApplicationFramework=OFF
        #-DBUILD_MODULE_DataExchange=OFF
        -DBUILD_MODULE_Draw=OFF
        -DBUILD_MODULE_FoundationClasses=OFF
        -DBUILD_MODULE_ModelingAlgorithms=OFF
        -DBUILD_MODULE_ModelingData=OFF
        -DBUILD_MODULE_Visualization=OFF
)

if (DEP_BUILD_FREETYPE)
    add_dependencies(dep_OCCT ${FREETYPE_PKG})
endif ()
