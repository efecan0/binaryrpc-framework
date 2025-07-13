cmake_minimum_required(VERSION 3.16)
project(binaryrpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ----------------------
# Platform-specific dependencies and settings
# ----------------------
if(WIN32)
    # MSVC/vcpkg settings
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        add_compile_options(/EHsc /W4 /permissive- /analyze /utf-8)
        add_definitions(-D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    endif()
    # Check VCPKG_ROOT environment variable
    if(NOT DEFINED ENV{VCPKG_ROOT})
        message(FATAL_ERROR "VCPKG_ROOT environment variable is not set! Please set it to your vcpkg root directory.\nExample (Windows): set VCPKG_ROOT=C:/path/to/vcpkg\nExample (Linux): export VCPKG_ROOT=/path/to/vcpkg")
    endif()
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE
            "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()
    include_directories("${VCPKG_ROOT}/installed/x64-windows/include")
    find_package(unofficial-uwebsockets CONFIG REQUIRED)
    find_package(Threads         REQUIRED)
    find_package(jwt-cpp CONFIG  REQUIRED)
    find_package(folly CONFIG REQUIRED)
    find_package(msgpack-cxx CONFIG REQUIRED)
else() # UNIX/Linux
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    
    # If not using vcpkg on Linux, find packages manually
    find_package(unofficial-uwebsockets CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    set(JWT_CPP_INCLUDE_DIR /usr/include/jwt-cpp)
    find_package(folly CONFIG REQUIRED)
    find_package(msgpack-cxx CONFIG REQUIRED)
endif()

# ----------------------
# Source files
# ----------------------
file(GLOB_RECURSE BINARYRPC_CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp src/**/*.cpp)
set(_EXTRA_CANDIDATES
    src/core/session/generic_index.cpp
    src/plugins/room_plugin.cpp
    src/core/protocol/msgpack_protocol.cpp
    src/plugins/reliable_plugin.cpp)
foreach(_f IN LISTS _EXTRA_CANDIDATES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
        list(APPEND BINARYRPC_CORE_SOURCES ${_f})
    endif()
endforeach()
if(BINARYRPC_CORE_SOURCES STREQUAL "")
    message(FATAL_ERROR "No .cpp files found for binaryrpc_core — check source paths.")
endif()

add_library(binaryrpc_core STATIC ${BINARYRPC_CORE_SOURCES})

if(WIN32)
    target_include_directories(binaryrpc_core PUBLIC include include/binaryrpc)
else()
    target_include_directories(binaryrpc_core PUBLIC include include/binaryrpc ${UWEBSOCKETS_INCLUDE_DIR} ${JWT_CPP_INCLUDE_DIR})
endif()

# Optional dependencies
option(ENABLE_JWT "Enable JWT authentication middleware" OFF)
option(ENABLE_JSON_EXAMPLES "Enable JSON-based examples" OFF)

if(ENABLE_JWT)
    find_package(jwt-cpp CONFIG QUIET)
    if(NOT jwt-cpp_FOUND)
        message(FATAL_ERROR "ENABLE_JWT is ON but jwt-cpp was not found. Please install jwt-cpp or disable ENABLE_JWT.")
    endif()
endif()

if(ENABLE_JSON_EXAMPLES)
    find_package(nlohmann_json CONFIG QUIET)
    if(NOT nlohmann_json_FOUND)
        message(FATAL_ERROR "ENABLE_JSON_EXAMPLES is ON but nlohmann_json was not found. Please install nlohmann_json or disable ENABLE_JSON_EXAMPLES.")
    endif()
endif()

if(WIN32)
    target_link_libraries(binaryrpc_core PUBLIC
        unofficial::uwebsockets::uwebsockets
        Threads::Threads
        # jwt-cpp::jwt-cpp is only needed if ENABLE_JWT is ON
        $<$<BOOL:${ENABLE_JWT}>:jwt-cpp::jwt-cpp>
        Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util

        msgpack-cxx
    )
else()
    target_link_libraries(binaryrpc_core PUBLIC
        unofficial::uwebsockets::uwebsockets
        pthread
        Threads::Threads
        OpenSSL::Crypto
        # nlohmann_json::nlohmann_json is only needed if ENABLE_JSON_EXAMPLES is ON
        $<$<BOOL:${ENABLE_JSON_EXAMPLES}>:nlohmann_json::nlohmann_json>
        Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util
        msgpack-cxx
    )
endif()

# ----------------------
# Tests and example applications
# ----------------------
option(BINARYRPC_BUILD_TESTS "Build unit tests with Catch2" ON)
if (BINARYRPC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    target_compile_definitions(binaryrpc_core PRIVATE BINARYRPC_TEST)
endif()

option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(example_server)
endif()

# ----------------------
# Integration/regression test executables
# ----------------------
foreach(entry
    binaryrpc_middleware_integration_test:test_server/middleware_test.cpp
    binaryrpc_session_integration_test:test_server/session_test.cpp
    binaryrpc_advanced_general_integration_test:test_server/advanced_general_server.cpp
    binaryrpc_error_propagation_integration_test:test_server/error_propagation_test.cpp
    binaryrpc_qos1_integration_test:test_server/qos1_test.cpp
    binaryrpc_qos2_integration_test:test_server/qos2_test.cpp
    binaryrpc_main_test:test_server/main.cpp)
  string(REPLACE ":" ";" pair ${entry})
  list(GET pair 0 tgt)
  list(GET pair 1 src)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}" AND NOT TARGET ${tgt})
      add_executable(${tgt} ${src})
      target_link_libraries(${tgt} PRIVATE binaryrpc_core)
  endif()
endforeach()
