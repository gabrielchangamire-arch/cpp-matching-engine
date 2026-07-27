cmake_minimum_required(VERSION 3.20)

project(
    cpp_matching_engine
    VERSION 0.1.0
    DESCRIPTION "A price-time-priority limit order book and matching engine"
    LANGUAGES CXX
)

add_executable(matching_engine_cli src/main.cpp)

target_compile_features(matching_engine_cli PRIVATE cxx_std_20)
set_target_properties(
    matching_engine_cli
    PROPERTIES
        CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(matching_engine_cli PRIVATE /W4 /WX)
else()
    target_compile_options(
        matching_engine_cli
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME cli_smoke_test COMMAND matching_engine_cli)
endif()
