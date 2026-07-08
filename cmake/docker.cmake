function(binance_aggregator_add_docker_targets)
    find_program(DOCKER_EXECUTABLE docker)

    if(NOT DOCKER_EXECUTABLE)
        message(STATUS "Docker executable not found. Docker wrapper targets will not be added.")
        return()
    endif()

    if(CMAKE_BUILD_TYPE)
        set(DOCKER_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    else()
        set(DOCKER_BUILD_TYPE "Release")
    endif()

    set(DOCKER_TEST_IMAGE_NAME "binance_aggregator_test")
    set(DOCKER_EXPORT_DIR "${CMAKE_SOURCE_DIR}/dist/docker")

    add_custom_target(docker_test
        COMMAND ${DOCKER_EXECUTABLE} build
            --progress=plain
            --target test
            --build-arg BUILD_TYPE=${DOCKER_BUILD_TYPE}
            -t ${DOCKER_TEST_IMAGE_NAME}:${DOCKER_BUILD_TYPE}
            ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
        COMMENT "Building Binance Aggregator and running tests inside Docker"
    )

    add_custom_target(docker_export_binary
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${DOCKER_EXPORT_DIR}
        COMMAND ${DOCKER_EXECUTABLE} build
            --progress=plain
            --target artifact
            --build-arg BUILD_TYPE=${DOCKER_BUILD_TYPE}
            --output type=local,dest=${DOCKER_EXPORT_DIR}
            ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
        COMMENT "Building, testing, installing, and exporting Binance Aggregator binary from Docker"
    )
endfunction()