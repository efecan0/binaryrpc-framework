cmake_minimum_required(VERSION 3.16)
project(binaryrpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC-specific settings, applied only if using MSVC
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/EHsc /W4 /permissive- /analyze /utf-8)
    add_definitions(-D_WIN32_WINNT=0x0601 -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
endif()

# Since the vcpkg toolchain file is used (via CMakePresets.json),
# vcpkg will automatically find the packages. We just need to declare them.
find_package(unofficial-uwebsockets CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(jwt-cpp REQUIRED)

find_package(msgpack-cxx REQUIRED)
find_package(nlohmann_json QUIET) # Optional dependency for examples
find_package(OpenSSL QUIET) # Optional, mainly for non-windows

# ----------------------
# Source files
# ----------------------
file(GLOB_RECURSE BINARYRPC_CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp src/**/*.cpp)
set(_EXTRA_CANDIDATES
        src/core/session/generic_index.cpp
        src/core/session/session_manager.cpp
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

# PUBLIC includes are handled by vcpkg's targets. We just need to add our own project's include path.
target_include_directories(binaryrpc_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/unordered_dense/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link all libraries using their modern imported targets provided by vcpkg.
target_link_libraries(binaryrpc_core PUBLIC
    unofficial::uwebsockets::uwebsockets
    Threads::Threads
    jwt-cpp::jwt-cpp
    msgpack-cxx
)
# Conditionally link optional libraries if found
if(nlohmann_json_FOUND)
    target_link_libraries(binaryrpc_core PUBLIC nlohmann_json::nlohmann_json)
endif()
if(OpenSSL_FOUND)
    target_link_libraries(binaryrpc_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
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
