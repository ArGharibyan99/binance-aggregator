cmake_minimum_required(VERSION 3.23)

project(binance_aggregator
    VERSION 0.1.0
    LANGUAGES CXX
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build tests" ON)

include(cmake/conan.cmake)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

add_library(agg_core
    app/src/config/ConfigLoader.cpp
)

target_include_directories(agg_core
    PUBLIC
        app/include
)

target_link_libraries(agg_core
    PUBLIC
        nlohmann_json::nlohmann_json
)

target_compile_options(agg_core
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(binance_aggregator
    app/main.cpp
)

target_link_libraries(binance_aggregator
    PRIVATE
        agg_core
        Boost::headers
        OpenSSL::SSL
        OpenSSL::Crypto
        fmt::fmt
        spdlog::spdlog
)

target_compile_options(binance_aggregator
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

if(BUILD_TESTING)
    enable_testing()

    find_package(GTest REQUIRED)

    add_executable(unit_tests
        tests/unit/ConfigLoaderTest.cpp
    )

    target_link_libraries(unit_tests
        PRIVATE
            agg_core
            GTest::gtest_main
    )

    target_compile_options(unit_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    include(GoogleTest)
    gtest_discover_tests(unit_tests)
endif()

install(TARGETS binance_aggregator
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/binance-aggregator
    FILES_MATCHING PATTERN "*.json"
)

include(cmake/docker.cmake)
binance_aggregator_add_docker_targets()
