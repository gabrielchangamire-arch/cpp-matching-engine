cmake_minimum_required(VERSION 3.20)

project(
    cpp_matching_engine
    VERSION 1.1.0
    DESCRIPTION "A price-time-priority limit order book and matching engine"
    LANGUAGES CXX
)

find_package(Threads REQUIRED)

option(MATCHING_ENGINE_BUILD_BENCHMARKS "Build Google Benchmark targets" OFF)
option(MATCHING_ENGINE_BUILD_FUZZERS "Build Clang libFuzzer targets" OFF)
option(MATCHING_ENGINE_ENABLE_ASAN_UBSAN "Enable AddressSanitizer and UBSan" OFF)
option(MATCHING_ENGINE_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(MATCHING_ENGINE_ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)

if(MATCHING_ENGINE_ENABLE_ASAN_UBSAN AND MATCHING_ENGINE_ENABLE_TSAN)
    message(FATAL_ERROR "ASan/UBSan and TSan require separate build trees")
endif()

if(MATCHING_ENGINE_BUILD_FUZZERS)
    if(MATCHING_ENGINE_ENABLE_ASAN_UBSAN OR MATCHING_ENGINE_ENABLE_TSAN)
        message(FATAL_ERROR "Fuzzers configure their own sanitizer build")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "The fuzz targets require Clang libFuzzer")
    endif()
endif()

if(MATCHING_ENGINE_ENABLE_ASAN_UBSAN OR MATCHING_ENGINE_ENABLE_TSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "Configured sanitizers require Clang or GCC")
    endif()

    if(MATCHING_ENGINE_ENABLE_ASAN_UBSAN)
        set(MATCHING_ENGINE_SANITIZERS "address,undefined")
    else()
        set(MATCHING_ENGINE_SANITIZERS "thread")
    endif()

    add_compile_options(
        -fsanitize=${MATCHING_ENGINE_SANITIZERS}
        -fno-omit-frame-pointer
    )
    add_link_options(-fsanitize=${MATCHING_ENGINE_SANITIZERS})
endif()

if(MATCHING_ENGINE_ENABLE_CLANG_TIDY)
    find_program(MATCHING_ENGINE_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
endif()

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

function(enable_clang_tidy target_name)
    if(MATCHING_ENGINE_ENABLE_CLANG_TIDY)
        set_target_properties(
            ${target_name}
            PROPERTIES
                CXX_CLANG_TIDY
                    "${MATCHING_ENGINE_CLANG_TIDY_EXECUTABLE};--warnings-as-errors=*"
        )
    endif()
endfunction()

add_library(
    matching_engine
    src/concurrent_matching_engine.cpp
    src/csv_commands.cpp
    src/matching_engine.cpp
    src/order_book.cpp
    src/order.cpp
    src/trade.cpp
    src/types.cpp
)
target_compile_features(matching_engine PUBLIC cxx_std_20)
target_link_libraries(matching_engine PUBLIC Threads::Threads)
target_include_directories(
    matching_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(matching_engine PROPERTIES CXX_EXTENSIONS OFF)
enable_strict_warnings(matching_engine)
enable_clang_tidy(matching_engine)

add_executable(matching_engine_cli src/main.cpp)
target_link_libraries(matching_engine_cli PRIVATE matching_engine)
set_target_properties(matching_engine_cli PROPERTIES CXX_EXTENSIONS OFF)
enable_strict_warnings(matching_engine_cli)
enable_clang_tidy(matching_engine_cli)

if(MATCHING_ENGINE_BUILD_FUZZERS)
    target_compile_options(
        matching_engine
        PRIVATE
            -fsanitize=fuzzer-no-link,address,undefined
            -fno-omit-frame-pointer
    )
    target_link_options(
        matching_engine_cli
        PRIVATE
            -fsanitize=fuzzer-no-link,address,undefined
    )

    add_executable(csv_command_fuzzer fuzz/csv_command_fuzzer.cpp)
    target_link_libraries(csv_command_fuzzer PRIVATE matching_engine)
    target_compile_options(
        csv_command_fuzzer
        PRIVATE
            -fsanitize=fuzzer,address,undefined
            -fno-omit-frame-pointer
    )
    target_link_options(
        csv_command_fuzzer
        PRIVATE
            -fsanitize=fuzzer,address,undefined
    )
    set_target_properties(csv_command_fuzzer PROPERTIES CXX_EXTENSIONS OFF)
    enable_strict_warnings(csv_command_fuzzer)
    enable_clang_tidy(csv_command_fuzzer)
endif()

find_program(MATCHING_ENGINE_CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(MATCHING_ENGINE_CLANG_FORMAT_EXECUTABLE)
    file(
        GLOB_RECURSE MATCHING_ENGINE_FORMAT_FILES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )
    add_custom_target(
        format
        COMMAND ${MATCHING_ENGINE_CLANG_FORMAT_EXECUTABLE}
                -i
                ${MATCHING_ENGINE_FORMAT_FILES}
        COMMENT "Formatting project C++ sources"
    )
    add_custom_target(
        format-check
        COMMAND ${MATCHING_ENGINE_CLANG_FORMAT_EXECUTABLE}
                --dry-run
                --Werror
                ${MATCHING_ENGINE_FORMAT_FILES}
        COMMENT "Checking C++ source formatting"
    )
endif()

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
        enable_clang_tidy(${target_name})
        gtest_discover_tests(${target_name})
    endfunction()

    add_matching_engine_test(domain_model_test tests/domain_model_test.cpp)
    add_matching_engine_test(order_book_test tests/order_book_test.cpp)
    add_matching_engine_test(matching_engine_test tests/matching_engine_test.cpp)
    add_matching_engine_test(
        matching_engine_property_test
        tests/matching_engine_property_test.cpp
    )
    add_matching_engine_test(integration_test tests/integration_test.cpp)
    add_matching_engine_test(csv_commands_test tests/csv_commands_test.cpp)
    add_matching_engine_test(thread_safe_queue_test tests/thread_safe_queue_test.cpp)
    add_matching_engine_test(
        concurrent_matching_engine_test
        tests/concurrent_matching_engine_test.cpp
    )
endif()

if(MATCHING_ENGINE_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googlebenchmark)

    add_executable(
        matching_engine_benchmark
        benchmarks/matching_engine_benchmark.cpp
    )
    target_link_libraries(
        matching_engine_benchmark
        PRIVATE
            matching_engine
            benchmark::benchmark
    )
    set_target_properties(
        matching_engine_benchmark
        PROPERTIES CXX_EXTENSIONS OFF
    )
    enable_strict_warnings(matching_engine_benchmark)
    enable_clang_tidy(matching_engine_benchmark)
endif()
