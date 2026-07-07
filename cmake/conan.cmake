include_guard(GLOBAL)

option(ENABLE_CONAN "Use Conan to install dependencies during CMake configure" ON)

if(NOT ENABLE_CONAN)
    message(STATUS "Conan auto-install is disabled")
    return()
endif()

find_program(CONAN_COMMAND conan)

if(NOT CONAN_COMMAND)
    message(FATAL_ERROR
        "Conan executable was not found. "
        "Please install Conan 2.x or configure with -DENABLE_CONAN=OFF "
        "and provide dependencies manually."
    )
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CONAN_OUTPUT_FOLDER "${CMAKE_BINARY_DIR}/conan" CACHE PATH
    "Folder where Conan generated files will be placed"
)

message(STATUS "Checking Conan default profile")

execute_process(
    COMMAND "${CONAN_COMMAND}" profile path default
    RESULT_VARIABLE CONAN_PROFILE_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT CONAN_PROFILE_RESULT EQUAL 0)
    message(STATUS "Conan default profile was not found; detecting profile")

    execute_process(
        COMMAND "${CONAN_COMMAND}" profile detect --force
        RESULT_VARIABLE CONAN_PROFILE_DETECT_RESULT
    )

    if(NOT CONAN_PROFILE_DETECT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to detect Conan profile")
    endif()
endif()

message(STATUS "Running Conan install")
message(STATUS "Conan output folder: ${CONAN_OUTPUT_FOLDER}")
message(STATUS "Conan build type: ${CMAKE_BUILD_TYPE}")

execute_process(
    COMMAND
        "${CONAN_COMMAND}" install "${CMAKE_SOURCE_DIR}"
        "--output-folder=${CONAN_OUTPUT_FOLDER}"
        "--build=missing"
        "-s" "build_type=${CMAKE_BUILD_TYPE}"
    RESULT_VARIABLE CONAN_INSTALL_RESULT
)

if(NOT CONAN_INSTALL_RESULT EQUAL 0)
    message(FATAL_ERROR "Conan install failed")
endif()

file(GLOB_RECURSE CONAN_GENERATED_CONFIGS
    "${CONAN_OUTPUT_FOLDER}/*Config.cmake"
    "${CONAN_OUTPUT_FOLDER}/*-config.cmake"
    "${CONAN_OUTPUT_FOLDER}/Find*.cmake"
)

if(NOT CONAN_GENERATED_CONFIGS)
    message(FATAL_ERROR
        "Conan install finished, but no generated CMake package files were found."
    )
endif()

set(CONAN_GENERATOR_DIRS "")

foreach(CONAN_CONFIG_FILE ${CONAN_GENERATED_CONFIGS})
    get_filename_component(CONAN_CONFIG_DIR "${CONAN_CONFIG_FILE}" DIRECTORY)
    list(APPEND CONAN_GENERATOR_DIRS "${CONAN_CONFIG_DIR}")
endforeach()

list(REMOVE_DUPLICATES CONAN_GENERATOR_DIRS)

foreach(CONAN_GENERATOR_DIR ${CONAN_GENERATOR_DIRS})
    list(PREPEND CMAKE_PREFIX_PATH "${CONAN_GENERATOR_DIR}")
    list(PREPEND CMAKE_MODULE_PATH "${CONAN_GENERATOR_DIR}")
endforeach()

message(STATUS "Conan generated CMake package directories:")
foreach(CONAN_GENERATOR_DIR ${CONAN_GENERATOR_DIRS})
    message(STATUS "  ${CONAN_GENERATOR_DIR}")
endforeach()