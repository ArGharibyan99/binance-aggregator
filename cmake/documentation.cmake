option(BUILD_DOCS "Enable the 'docs' target for generating Doxygen/Graphviz documentation" OFF)

function(binance_aggregator_add_documentation_targets)
    if(NOT BUILD_DOCS)
        return()
    endif()

    find_package(Doxygen QUIET)
    find_program(DOT_EXECUTABLE dot)

    if(NOT DOXYGEN_FOUND OR NOT DOT_EXECUTABLE)
        message(STATUS
            "Doxygen and/or Graphviz (dot) not found; 'docs' target will not be added. "
            "Application and test builds are unaffected.")
        return()
    endif()

    set(DOCS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/documentation")
    set(DOXYGEN_OUTPUT_DIR "${DOCS_OUTPUT_DIR}/doxygen")
    set(GRAPHVIZ_OUTPUT_DIR "${DOCS_OUTPUT_DIR}/graphviz")

    set(DOXYGEN_IN "${CMAKE_SOURCE_DIR}/docs/Doxyfile.in")
    set(DOXYGEN_OUT "${CMAKE_BINARY_DIR}/Doxyfile")
    configure_file("${DOXYGEN_IN}" "${DOXYGEN_OUT}" @ONLY)

    add_custom_target(doxygen_docs
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DOXYGEN_OUTPUT_DIR}"
        COMMAND "${DOXYGEN_EXECUTABLE}" "${DOXYGEN_OUT}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating Doxygen HTML documentation"
        VERBATIM
    )

    file(GLOB GRAPHVIZ_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/docs/*.gv")

    set(GRAPHVIZ_SVGS "")
    foreach(GV_FILE IN LISTS GRAPHVIZ_SOURCES)
        get_filename_component(GV_NAME "${GV_FILE}" NAME_WE)
        set(SVG_FILE "${GRAPHVIZ_OUTPUT_DIR}/${GV_NAME}.svg")

        add_custom_command(
            OUTPUT "${SVG_FILE}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${GRAPHVIZ_OUTPUT_DIR}"
            COMMAND "${DOT_EXECUTABLE}" -Tsvg "${GV_FILE}" -o "${SVG_FILE}"
            DEPENDS "${GV_FILE}"
            COMMENT "Rendering ${GV_NAME}.gv -> ${GV_NAME}.svg"
            VERBATIM
        )
        list(APPEND GRAPHVIZ_SVGS "${SVG_FILE}")
    endforeach()

    add_custom_target(graphviz_diagrams DEPENDS ${GRAPHVIZ_SVGS})

    add_custom_target(docs)
    add_dependencies(docs doxygen_docs graphviz_diagrams)
endfunction()
