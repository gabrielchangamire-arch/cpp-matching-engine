cmake_minimum_required(VERSION 3.20)

project(
    cpp_matching_engine
    VERSION 0.2.0
    DESCRIPTION "A price-time-priority limit order book and matching engine"
    LANGUAGES CXX
)

function(enable_strict_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX)
    else()
        target_compile_options(
            ${target_name}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
        )
    endif()
endfunction()

add_library(
    matching_engine
    src/csv_commands.cpp
    src/matching_engine.cpp
    src/order_book.cpp
    src/order.cpp
    src/trade.cpp
    src/types.cpp
)
target_compile_features(matching_engine PUBLIC cxx_std_20)
target_include_directories(
    matching_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(matching_engine PROPERTIES CXX_EXTENSIONS OFF)
enable_strict_warnings(matching_engine)

add_executable(matching_engine_cli src/main.cpp)
target_link_libraries(matching_engine_cli PRIVATE matching_engine)
set_target_properties(matching_engine_cli PROPERTIES CXX_EXTENSIONS OFF)
enable_strict_warnings(matching_engine_cli)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME cli_sample_test
        COMMAND matching_engine_cli
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/sample_orders.csv
    )

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    include(GoogleTest)

    function(add_matching_engine_test target_name source_file)
        add_executable(${target_name} ${source_file})
        target_link_libraries(
            ${target_name}
            PRIVATE
                matching_engine
                GTest::gtest_main
        )
        set_target_properties(${target_name} PROPERTIES CXX_EXTENSIONS OFF)
        enable_strict_warnings(${target_name})
        gtest_discover_tests(${target_name})
    endfunction()

    add_matching_engine_test(domain_model_test tests/domain_model_test.cpp)
    add_matching_engine_test(order_book_test tests/order_book_test.cpp)
    add_matching_engine_test(matching_engine_test tests/matching_engine_test.cpp)
    add_matching_engine_test(integration_test tests/integration_test.cpp)
    add_matching_engine_test(csv_commands_test tests/csv_commands_test.cpp)
endif()
