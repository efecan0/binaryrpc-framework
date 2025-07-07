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
        add_definitions(-DGLOG_NO_ABBREVIATED_SEVERITIES)
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
    find_package(ZLIB            REQUIRED)
    find_package(Threads         REQUIRED)
    find_package(jwt-cpp CONFIG  REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(Boost REQUIRED COMPONENTS thread)
    find_package(glog CONFIG REQUIRED)
    find_package(gflags CONFIG REQUIRED)
    find_package(double-conversion CONFIG REQUIRED)
    find_library(FOLLY_LIBRARY NAMES folly PATHS "${VCPKG_ROOT}/installed/x64-windows/lib")
    find_package(fmt CONFIG REQUIRED)
else() # UNIX/Linux
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
    find_path(UWEBSOCKETS_INCLUDE_DIR uWebSockets/App.h PATHS /usr/include /usr/local/include)
    find_library(USOCKETS_LIBRARY usockets PATHS /usr/lib /usr/local/lib)
    if(NOT UWEBSOCKETS_INCLUDE_DIR)
        message(FATAL_ERROR "uWebSockets include directory not found!")
    endif()
    if(NOT USOCKETS_LIBRARY)
        message(FATAL_ERROR "uSockets library not found!")
    endif()
    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(Threads REQUIRED)
    find_package(nlohmann_json REQUIRED)
    set(JWT_CPP_INCLUDE_DIR /usr/include/jwt-cpp)
    
    # Find folly
    find_library(FOLLY_LIBRARY NAMES folly PATHS /usr/lib /usr/local/lib)
    if(NOT FOLLY_LIBRARY)
        message(FATAL_ERROR "folly library not found! Please install folly: sudo pacman -S folly")
    endif()
    
    # Required definitions for glog
    add_definitions(-DGLOG_NO_ABBREVIATED_SEVERITIES)
    add_definitions(-DGOOGLE_GLOG_DLL_DECL=)
    add_definitions(-DGLOG_EXPORT=)
    add_definitions(-DGLOG_NO_EXPORT=)
    add_definitions(-DGLOG_DEPRECATED=)
    add_definitions(-DGLOG_NO_EXPORT=)
    
    # Disable glog (if incompatible with folly)
    add_definitions(-DGOOGLE_GLOG_DISABLE_LOGGING)
    
    # Find glog
    find_library(GLOG_LIBRARY NAMES glog PATHS /usr/lib /usr/local/lib)
    if(NOT GLOG_LIBRARY)
        message(FATAL_ERROR "glog library not found!")
    endif()
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
        ZLIB::ZLIB
        Threads::Threads
        Boost::thread
        # jwt-cpp::jwt-cpp is only needed if ENABLE_JWT is ON
        $<$<BOOL:${ENABLE_JWT}>:jwt-cpp::jwt-cpp>
        ${FOLLY_LIBRARY}
        glog::glog
        gflags::gflags
        fmt::fmt
        double-conversion::double-conversion
    )
else()
    target_link_libraries(binaryrpc_core PUBLIC
        ${USOCKETS_LIBRARY}
        pthread
        ZLIB::ZLIB
        Threads::Threads
        OpenSSL::Crypto
        # nlohmann_json::nlohmann_json is only needed if ENABLE_JSON_EXAMPLES is ON
        $<$<BOOL:${ENABLE_JSON_EXAMPLES}>:nlohmann_json::nlohmann_json>
        ${GLOG_LIBRARY}
        ${FOLLY_LIBRARY}
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
#if(BUILD_EXAMPLES)
#    add_subdirectory(examples)
#endif()

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
      if(WIN32)
          target_link_libraries(${tgt} PRIVATE binaryrpc_core glog::glog)
      endif()
  endif()
endforeach()
