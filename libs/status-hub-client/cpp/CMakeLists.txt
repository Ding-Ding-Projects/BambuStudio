cmake_minimum_required(VERSION 3.16)

project(status_hub_client_cpp VERSION 0.1.0 LANGUAGES CXX)

option(STATUS_HUB_CLIENT_ENABLE_CURL "Build the optional libcurl transport" OFF)
option(STATUS_HUB_CLIENT_BUILD_EXAMPLE "Build the concise C++ client example" OFF)
option(STATUS_HUB_CLIENT_BUILD_TESTS "Build deterministic native client contract tests" OFF)
option(STATUS_HUB_CLIENT_BUILD_CONTRACT_PROBE "Build the native C++ contract probe" OFF)

find_package(Threads REQUIRED)

add_library(status_hub_client STATIC
  src/status_hub_client.cpp
  src/status_hub_transport_unavailable.cpp
)

target_compile_features(status_hub_client PUBLIC cxx_std_17)
target_include_directories(status_hub_client
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(status_hub_client PUBLIC Threads::Threads)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/status_hub_client.cpp" STATUS_HUB_CLIENT_CORE_SOURCE)
if (NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "ProtocolLimits::clientRequestsPerMinute")
  message(FATAL_ERROR "Client rate source check requires the 50-request-per-minute ceiling.")
endif()
if (NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "effectiveRequestsPerMinute")
  message(FATAL_ERROR "Client rate source check requires deterministic high-request clamping.")
endif()
if (NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "remainingMs" OR NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "Deadline")
  message(FATAL_ERROR "Client deadline source check requires one absolute deadline path.")
endif()
if (NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "kWorktreeWalkEntriesMax"
    OR NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "ProtocolLimits::maxWorktrees")
  message(FATAL_ERROR "Worktree source check requires root and filesystem bounds.")
endif()
if (NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "validBaseUrl")
  message(FATAL_ERROR "Base URL source check requires absolute URL validation.")
endif()
if (NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "generateSessionKey"
    OR NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "validSessionKey")
  message(FATAL_ERROR "Session-key source check requires the platform CSPRNG and exact-key validation paths.")
endif()
if (NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "JsonDocumentParser"
    OR NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "parseRoot"
    OR NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "maxJsonNodes"
    OR NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "duplicateKey")
  message(FATAL_ERROR "Response source check requires complete bounded JSON validation, duplicate-key rejection, and a non-empty object root.")
endif()
message(STATUS "Client rate/deadline/worktree/URL/session-key/response source checks present.")

if (WIN32)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/status_hub_transport_winhttp.cpp" STATUS_HUB_CLIENT_WINHTTP_SOURCE)
  if (NOT STATUS_HUB_CLIENT_WINHTTP_SOURCE MATCHES "struct[ \\t]+WinHttpHandle"
      OR STATUS_HUB_CLIENT_WINHTTP_SOURCE MATCHES "HINTERNET[ \\t]+(session|connection|handle)[ \\t]*="
      OR STATUS_HUB_CLIENT_WINHTTP_SOURCE MATCHES "while[ \\t]*\\([ \\t]*WinHttpQueryDataAvailable")
    message(FATAL_ERROR "WinHTTP lifetime source check rejected raw handles or unchecked data iteration.")
  endif()
  if (NOT STATUS_HUB_CLIENT_WINHTTP_SOURCE MATCHES "if[ \\t]*\\(!WinHttpQueryDataAvailable"
      OR NOT STATUS_HUB_CLIENT_WINHTTP_SOURCE MATCHES "if[ \\t]*\\(!WinHttpReadData")
    message(FATAL_ERROR "WinHTTP read source check requires typed failure handling for query and read calls.")
  endif()
  message(STATUS "WinHTTP lifetime/read source check: RAII and fail-closed reads present.")
  target_sources(status_hub_client PRIVATE src/status_hub_transport_winhttp.cpp)
  target_link_libraries(status_hub_client PUBLIC winhttp bcrypt)
endif()

if (NOT WIN32)
  if (NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "pthread_sigmask"
      OR NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "SIGPIPE"
      OR NOT STATUS_HUB_CLIENT_CORE_SOURCE MATCHES "EPIPE")
    message(FATAL_ERROR "POSIX child-input source check requires SIGPIPE and EPIPE handling.")
  endif()
  message(STATUS "POSIX child-input source check: SIGPIPE blocked and EPIPE typed.")
endif()

if (STATUS_HUB_CLIENT_ENABLE_CURL)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/status_hub_transport_curl.cpp" STATUS_HUB_CLIENT_CURL_SOURCE)
  function(status_hub_curl_lifetime_violation source output)
    string(REPLACE "\r\n" "\n" normalized "${source}")
    string(REPLACE "\n" ";" lines "${normalized}")
    set(cleanup_count 0)
    set(init_count 0)
    set(destructor_cleanup FALSE)
    set(constructor_init FALSE)
    foreach(line IN LISTS lines)
      string(STRIP "${line}" trimmed)
      string(REGEX MATCHALL "curl_global_cleanup\\(" cleanup_calls "${trimmed}")
      list(LENGTH cleanup_calls cleanup_on_line)
      math(EXPR cleanup_count "${cleanup_count}+${cleanup_on_line}")
      string(REGEX MATCHALL "curl_global_init\\(" init_calls "${trimmed}")
      list(LENGTH init_calls init_on_line)
      math(EXPR init_count "${init_count}+${init_on_line}")
      if (trimmed MATCHES "^~CurlTransport[ \\t]*\\(")
        if (trimmed MATCHES "curl_global_cleanup\\(")
          set(destructor_cleanup TRUE)
        endif()
      endif()
      if (trimmed MATCHES "^CurlTransport[ \\t]*\\(")
        if (trimmed MATCHES "curl_global_(init|cleanup)\\(")
          set(constructor_init TRUE)
        endif()
      endif()
    endforeach()
    if (destructor_cleanup OR constructor_init OR cleanup_count GREATER 1 OR init_count GREATER 1)
      set(violation TRUE)
    else()
      set(violation FALSE)
    endif()
    set(${output} ${violation} PARENT_SCOPE)
  endfunction()

  status_hub_curl_lifetime_violation("${STATUS_HUB_CLIENT_CURL_SOURCE}" STATUS_HUB_CLIENT_CURL_VIOLATION)
  if (STATUS_HUB_CLIENT_CURL_VIOLATION)
    message(FATAL_ERROR "Curl lifetime source check rejected per-instance global cleanup or initialization.")
  endif()
  string(FIND "${STATUS_HUB_CLIENT_CURL_SOURCE}" "class CurlGlobal final" STATUS_HUB_CLIENT_CURL_CLASS)
  string(FIND "${STATUS_HUB_CLIENT_CURL_SOURCE}" "std::atexit(&CurlGlobal::cleanup)" STATUS_HUB_CLIENT_CURL_ATEXIT)
  if (STATUS_HUB_CLIENT_CURL_CLASS EQUAL -1 OR STATUS_HUB_CLIENT_CURL_ATEXIT EQUAL -1)
    message(FATAL_ERROR "Curl lifetime source check requires one process-scope helper and teardown registration.")
  endif()
  set(STATUS_HUB_CLIENT_CURL_MUTATION "${STATUS_HUB_CLIENT_CURL_SOURCE}\n~CurlTransport() noexcept { curl_global_cleanup(); }\n")
  status_hub_curl_lifetime_violation("${STATUS_HUB_CLIENT_CURL_MUTATION}" STATUS_HUB_CLIENT_CURL_MUTATION_VIOLATION)
  if (NOT STATUS_HUB_CLIENT_CURL_MUTATION_VIOLATION)
    message(FATAL_ERROR "Curl lifetime source check mutation did not reject destructor cleanup.")
  endif()
  message(STATUS "Curl lifetime source check: process-scope helper and mutation rejection present.")
  find_package(CURL REQUIRED)
  target_compile_definitions(status_hub_client PRIVATE STATUS_HUB_CLIENT_HAS_CURL=1)
  target_sources(status_hub_client PRIVATE src/status_hub_transport_curl.cpp)
  target_link_libraries(status_hub_client PUBLIC CURL::libcurl)
endif()

add_library(StatusHub::Client ALIAS status_hub_client)

if (STATUS_HUB_CLIENT_BUILD_EXAMPLE)
  add_executable(status_hub_client_example ../examples/main.cpp)
  target_compile_features(status_hub_client_example PRIVATE cxx_std_17)
  target_link_libraries(status_hub_client_example PRIVATE status_hub_client)
endif()

if (STATUS_HUB_CLIENT_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_executable(status_hub_client_native_test test/native_runtime.cpp)
  target_compile_features(status_hub_client_native_test PRIVATE cxx_std_17)
  target_link_libraries(status_hub_client_native_test PRIVATE status_hub_client)
  add_test(NAME status_hub_client_native_runtime COMMAND status_hub_client_native_test)
  add_executable(status_hub_client_cpp_contract ../test/cpp-contract.cpp)
  target_compile_features(status_hub_client_cpp_contract PRIVATE cxx_std_17)
  target_link_libraries(status_hub_client_cpp_contract PRIVATE status_hub_client)
  add_test(NAME status_hub_client_cpp_contract COMMAND status_hub_client_cpp_contract)
  set_tests_properties(status_hub_client_cpp_contract PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
endif()

if (STATUS_HUB_CLIENT_BUILD_CONTRACT_PROBE AND NOT STATUS_HUB_CLIENT_BUILD_TESTS)
  add_executable(status_hub_client_cpp_contract ../test/cpp-contract.cpp)
  target_compile_features(status_hub_client_cpp_contract PRIVATE cxx_std_17)
  target_link_libraries(status_hub_client_cpp_contract PRIVATE status_hub_client)
endif()

include(GNUInstallDirs)
install(TARGETS status_hub_client
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
