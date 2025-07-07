find_package(Catch2 3 REQUIRED CONFIG)

add_executable(binaryrpc_unit_tests
    session_manager.test.cpp
    generic_index.test.cpp
    middleware_chain.test.cpp
    rpc_manager.test.cpp
    protocol.test.cpp
    rate_limiter.test.cpp
)

target_link_libraries(binaryrpc_unit_tests
    PRIVATE
        binaryrpc_core
        Catch2::Catch2WithMain
        glog::glog
)

target_include_directories(binaryrpc_unit_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_test(NAME SessionManagerTests COMMAND binaryrpc_unit_tests)


add_executable(binaryrpc_qos_tests
    mock_client.cpp
    qos.test.cpp
)

target_link_libraries(binaryrpc_qos_tests
    PRIVATE
        binaryrpc_core
        Catch2::Catch2WithMain
        glog::glog
)

target_include_directories(binaryrpc_qos_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(binaryrpc_qos_tests PUBLIC BINARYRPC_TEST)

if(MSVC)
    target_compile_options(binaryrpc_qos_tests PRIVATE
        $<$<CONFIG:Debug>:/MDd /D_ITERATOR_DEBUG_LEVEL=2>
        $<$<CONFIG:Release>:/MD /D_ITERATOR_DEBUG_LEVEL=0>
    )

    target_compile_options(binaryrpc_unit_tests PRIVATE
            $<$<CONFIG:Debug>:/MDd /D_ITERATOR_DEBUG_LEVEL=2>
            $<$<CONFIG:Release>:/MD /D_ITERATOR_DEBUG_LEVEL=0>
    )
endif()

add_test(NAME QoSTests COMMAND binaryrpc_qos_tests)