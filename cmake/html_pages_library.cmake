# Function to create a library which creates headers for a list of given source files
# Parameters
#   NAME    - Name of the resulting library to be included
#   SOURCES - List of source files which should be created
# Usage:
#   add_html_pages_library(my-pages SOURCES html/page1.html html/page2.html)
function(add_html_pages_library)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(HEADER_DIR ${PROJECT_BINARY_DIR}/generated)
    set(HEADER_FILE ${HEADER_DIR}/${ARG_NAME}.h)
    file(MAKE_DIRECTORY ${HEADER_DIR})


    add_custom_command(OUTPUT ${HEADER_FILE}
        DEPENDS ${ARG_SOURCES}
        COMMAND ${CMAKE_COMMAND} -DHEADER_FILE="${HEADER_FILE}" -DSOURCES="${ARG_SOURCES}" -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin2h.cmake
    )

    add_library(${ARG_NAME} STATIC ${HEADER_FILE})
    set_target_properties(${ARG_NAME} PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(${ARG_NAME} PUBLIC ${HEADER_DIR})
endfunction()

