cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(basic_chat basic_chat.cpp)

find_package(OpenSSL REQUIRED)


target_link_libraries(basic_chat PRIVATE binaryrpc_core
        OpenSSL::Crypto
)