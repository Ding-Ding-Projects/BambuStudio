add_test(
    NAME native_gui_accessibility_contract
    COMMAND "${CMAKE_COMMAND}"
            "-DBAMBU_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/native_gui_accessibility_contract.cmake"
)

add_executable(fan_motion_math_test fan_motion_math.cpp)
target_compile_features(fan_motion_math_test PRIVATE cxx_std_17)
add_test(NAME fan_motion_math COMMAND fan_motion_math_test)

if(WIN32)
    find_package(wxWidgets 3.1 REQUIRED COMPONENTS core base)
    include(${wxWidgets_USE_FILE})
    set(motion_source "${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Widgets/MD3Motion.cpp")
    add_executable(motion_runtime_test motion_runtime.cpp "${motion_source}")

    # A separately compiled broken production implementation must be rejected at
    # the explicit-stop case. This mutation never changes the checkout.
    file(READ "${motion_source}" motion_original)
    string(REPLACE "\r\n" "\n" motion_original "${motion_original}")
    set(stop_original "void Anim::Stop()\n{\n    wxTimer::Stop();")
    set(stop_mutation "void Anim::Stop()\n{\n    // Deliberately omit timer cancellation for the negative regression.")
    string(FIND "${motion_original}" "${stop_original}" stop_position)
    if(stop_position EQUAL -1)
        message(FATAL_ERROR "Motion cancellation mutation no longer matches production")
    endif()
    string(REPLACE "${stop_original}" "${stop_mutation}" motion_broken "${motion_original}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/MD3MotionStopMutation.cpp" "${motion_broken}")
    add_executable(motion_runtime_stop_mutation motion_runtime.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/MD3MotionStopMutation.cpp")
    foreach(target motion_runtime_test motion_runtime_stop_mutation)
        target_compile_features(${target} PRIVATE cxx_std_17)
        # Match the shipped static wx ABI. Assertions below are explicit runtime
        # conditions and remain active in Release, independently of NDEBUG.
        target_compile_definitions(${target} PRIVATE wxDEBUG_LEVEL=0)
        target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/src/slic3r/GUI/Widgets")
        target_link_libraries(${target} PRIVATE ${wxWidgets_LIBRARIES} PNG::PNG)
    endforeach()
endif()
