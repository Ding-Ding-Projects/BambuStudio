if(NOT DEFINED BAMBU_SOURCE_DIR)
    message(FATAL_ERROR "BAMBU_SOURCE_DIR is required")
endif()

set(home_assistant_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/GUI/HomeAssistant.cpp")
set(http_cpp "${BAMBU_SOURCE_DIR}/src/slic3r/Utils/Http.cpp")
set(http_hpp "${BAMBU_SOURCE_DIR}/src/slic3r/Utils/Http.hpp")
file(READ "${home_assistant_cpp}" home_assistant_source)
file(READ "${http_cpp}" http_source)
file(READ "${http_hpp}" http_header)

# Every request created by this client carries either the Home Assistant bearer
# or printer credentials, so every call site must opt out of redirects and
# verbose protocol traces.
string(REGEX MATCHALL "Http::(get|post)\\(" credential_requests "${home_assistant_source}")
string(REGEX MATCHALL "\\.follow_redirects\\(false\\)" redirect_guards "${home_assistant_source}")
string(REGEX MATCHALL "\\.verbose\\(false\\)" verbose_guards "${home_assistant_source}")
string(REGEX MATCHALL "\\.size_limit\\(kMax[A-Za-z]+ResponseBytes\\)" response_limits "${home_assistant_source}")
list(LENGTH credential_requests credential_request_count)
list(LENGTH redirect_guards redirect_guard_count)
list(LENGTH verbose_guards verbose_guard_count)
list(LENGTH response_limits response_limit_count)
if(NOT credential_request_count EQUAL 2)
    message(FATAL_ERROR
        "Expected exactly 2 centralized Home Assistant credential request sites; audit new/removed sites explicitly")
endif()
if(NOT redirect_guard_count EQUAL credential_request_count)
    message(FATAL_ERROR "Every Home Assistant credential request must disable redirects")
endif()
if(NOT verbose_guard_count EQUAL credential_request_count)
    message(FATAL_ERROR "Every Home Assistant credential request must disable verbose protocol traces")
endif()
if(NOT response_limit_count EQUAL credential_request_count)
    message(FATAL_ERROR "Every Home Assistant credential request must bound its response body")
endif()

foreach(required_declaration
        "Http& follow_redirects\\(bool set\\)"
        "Http& verbose\\(bool set\\)")
    if(NOT http_header MATCHES "${required_declaration}")
        message(FATAL_ERROR "Missing per-request HTTP security option: ${required_declaration}")
    endif()
endforeach()

foreach(required_implementation
        "follow_redirects\\(true\\)"
        "verbose\\(true\\)"
        "CURLOPT_FOLLOWLOCATION, follow_redirects \\? 1L : 0L"
        "CURLOPT_VERBOSE, verbose \\? 1L : 0L"
        "!follow_redirects && http_status >= 300 && http_status < 400")
    if(NOT http_source MATCHES "${required_implementation}")
        message(FATAL_ERROR "Missing HTTP security behavior: ${required_implementation}")
    endif()
endforeach()

message(STATUS "Home Assistant HTTP credential security contract passed")
