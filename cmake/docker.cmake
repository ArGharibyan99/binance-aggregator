function(binance_aggregator_add_docker_targets)
    find_program(DOCKER_EXECUTABLE docker)

    if(NOT DOCKER_EXECUTABLE)
        message(WARNING "Docker was not found. Docker targets will not be available.")
        return()
    endif()

    if(NOT CMAKE_BUILD_TYPE)
        set(DOCKER_BUILD_TYPE Release)
    else()
        set(DOCKER_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    endif()

    add_custom_target(docker_build_test
        COMMAND "${DOCKER_EXECUTABLE}" build
                --progress=plain
                --target build
                --build-arg BUILD_TYPE=${DOCKER_BUILD_TYPE}
                -t binance_aggregator_build:${DOCKER_BUILD_TYPE}
                -f "${CMAKE_SOURCE_DIR}/Dockerfile"
                "${CMAKE_SOURCE_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Building and testing Binance Aggregator inside Docker"
    )

    add_custom_target(docker_image
        COMMAND "${DOCKER_EXECUTABLE}" build
                --progress=plain
                --build-arg BUILD_TYPE=${DOCKER_BUILD_TYPE}
                -t binance_aggregator:${DOCKER_BUILD_TYPE}
                -f "${CMAKE_SOURCE_DIR}/Dockerfile"
                "${CMAKE_SOURCE_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Building final Binance Aggregator runtime Docker image"
    )

    add_custom_target(docker_run
        COMMAND "${DOCKER_EXECUTABLE}" run
                --rm
                binance_aggregator:${DOCKER_BUILD_TYPE}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Running Binance Aggregator Docker image"
    )
endfunction()