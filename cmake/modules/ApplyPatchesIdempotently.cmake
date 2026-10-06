cmake_minimum_required(VERSION 3.21)

foreach(required PATCH_ROOT PATCH_SOURCE PATCH_COUNT GIT_EXECUTABLE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing patch parameter ${required}")
    endif()
endforeach()
if(NOT PATCH_COUNT MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "PATCH_COUNT must be a positive integer")
endif()
get_filename_component(root "${PATCH_ROOT}" ABSOLUTE)
get_filename_component(source "${PATCH_SOURCE}" ABSOLUTE)
file(RELATIVE_PATH relative_source "${root}" "${source}")
if(IS_ABSOLUTE "${relative_source}" OR relative_source MATCHES "^\.\.(/|$)" OR NOT IS_DIRECTORY "${source}")
    message(FATAL_ERROR "Patch source must be an existing directory within the declared root")
endif()

foreach(index RANGE 1 ${PATCH_COUNT})
    if(NOT DEFINED PATCH_${index} OR NOT EXISTS "${PATCH_${index}}")
        message(FATAL_ERROR "Missing patch input PATCH_${index}")
    endif()
endforeach()

foreach(index RANGE 1 ${PATCH_COUNT})
    set(patch "${PATCH_${index}}")
    set(common_args apply "--directory=${relative_source}" --ignore-space-change --whitespace=fix)
    execute_process(COMMAND "${GIT_EXECUTABLE}" ${common_args} --check "${patch}"
        WORKING_DIRECTORY "${root}" RESULT_VARIABLE forward_result
        OUTPUT_VARIABLE forward_output ERROR_VARIABLE forward_error)
    if(forward_result EQUAL 0)
        execute_process(COMMAND "${GIT_EXECUTABLE}" ${common_args} --verbose "${patch}"
            WORKING_DIRECTORY "${root}" RESULT_VARIABLE apply_result
            OUTPUT_VARIABLE apply_output ERROR_VARIABLE apply_error)
        if(NOT apply_result EQUAL 0)
            message(FATAL_ERROR "Applying patch '${patch}' failed (${apply_result}).\n${apply_output}${apply_error}")
        endif()
        message(STATUS "Applied patch '${patch}'.\n${apply_output}${apply_error}")
    else()
        execute_process(COMMAND "${GIT_EXECUTABLE}" ${common_args} --reverse --check "${patch}"
            WORKING_DIRECTORY "${root}" RESULT_VARIABLE reverse_result
            OUTPUT_VARIABLE reverse_output ERROR_VARIABLE reverse_error)
        if(NOT reverse_result EQUAL 0)
            message(FATAL_ERROR "Patch '${patch}' is neither applicable nor already applied.\nForward check (${forward_result}):\n${forward_output}${forward_error}\nReverse check (${reverse_result}):\n${reverse_output}${reverse_error}")
        endif()
        message(STATUS "Patch '${patch}' is already applied (reverse check passed).")
    endif()
endforeach()
