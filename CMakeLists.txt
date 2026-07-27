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

    add_executable(domain_model_test tests/domain_model_test.cpp)
    target_link_libraries(
        domain_model_test
        PRIVATE
            matching_engine
            GTest::gtest_main
    )
    set_target_properties(domain_model_test PROPERTIES CXX_EXTENSIONS OFF)
    enable_strict_warnings(domain_model_test)

    include(GoogleTest)
    gtest_discover_tests(domain_model_test)
endif()
