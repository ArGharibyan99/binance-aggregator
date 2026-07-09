if(BINANCE_AGGREGATOR_DOCKER_ACTION STREQUAL "build")
    if(NOT SOURCE_DIR)
        get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    endif()

    if(NOT BUILD_TYPE)
        set(BUILD_TYPE Release)
    endif()

    execute_process(
        COMMAND docker build
                --build-arg BUILD_TYPE=${BUILD_TYPE}
                -t binance_aggregator:${BUILD_TYPE}
                -f Dockerfile
                .
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE rv)
    if(rv)
        message(FATAL_ERROR "Docker build failed (${rv})")
    endif()

    return()
endif()

if(BINANCE_AGGREGATOR_DOCKER_ACTION STREQUAL "dist")
    if(NOT SOURCE_DIR)
        get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    endif()

    if(NOT BUILD_TYPE)
        set(BUILD_TYPE Release)
    endif()

    set(DIST_DIR "${SOURCE_DIR}/dist")
    file(REMOVE_RECURSE "${DIST_DIR}")
    file(MAKE_DIRECTORY "${DIST_DIR}/docker")

    find_program(GZIP_EXECUTABLE gzip REQUIRED)
    set(IMAGE_PATH "${DIST_DIR}/docker/binance_aggregator-${BUILD_TYPE}.image.tgz")
    execute_process(
        COMMAND docker save binance_aggregator:${BUILD_TYPE}
        COMMAND "${GZIP_EXECUTABLE}" -c
        OUTPUT_FILE "${IMAGE_PATH}"
        RESULTS_VARIABLE rvs)
    foreach(rv IN LISTS rvs)
        if(NOT rv STREQUAL "0")
            message(FATAL_ERROR "docker image archive failed (${rvs})")
        endif()
    endforeach()

    message(STATUS "Docker distributable bundle created -> ${DIST_DIR}")
    return()
endif()

if(DEFINED BINANCE_AGGREGATOR_DOCKER_ACTION)
    message(FATAL_ERROR "Unknown Docker action: ${BINANCE_AGGREGATOR_DOCKER_ACTION}")
endif()

function(binance_aggregator_add_docker_targets)
    add_custom_target(docker
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
                "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
                -DBINANCE_AGGREGATOR_DOCKER_ACTION=build
                -P "${CMAKE_SOURCE_DIR}/cmake/docker.cmake"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Building binance_aggregator:${CMAKE_BUILD_TYPE} inside Docker")

    add_custom_target(release_docker
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
                "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
                -DBINANCE_AGGREGATOR_DOCKER_ACTION=build
                -P "${CMAKE_SOURCE_DIR}/cmake/docker.cmake"
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
                "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
                -DBINANCE_AGGREGATOR_DOCKER_ACTION=dist
                -P "${CMAKE_SOURCE_DIR}/cmake/docker.cmake"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Creating Docker distributable bundle under dist/")
endfunction()
