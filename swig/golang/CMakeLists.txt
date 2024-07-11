# set output directory
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_golangswig)

# Add swig module
set_property(SOURCE golang.i PROPERTY CPLUSPLUS ON)

swig_add_library(golangxspcomm LANGUAGE go SOURCES golang.i)
target_link_options(golangxspcomm PRIVATE
 -Wl,-rpath,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
 -Wl,-rpath,~/.local/lib
 -Wl,-rpath,/usr/local/lib)
 target_link_libraries(golangxspcomm PRIVATE xspcomm)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../golang/src/xspcomm)
set_target_properties(golangxspcomm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR})

# copy file
add_custom_command(
    OUTPUT ${XCOMM_LIB_DIR}/xspcomm.go
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/golangxspcomm.so
            ${XCOMM_LIB_DIR}/golangxspcomm.so.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/../xspcomm.go
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/go.mod
            ${XCOMM_LIB_DIR}/
    DEPENDS golangxspcomm
)

add_custom_command(
    OUTPUT golangspcomm_js_lnk
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "golangxspcomm.so.${PROJECT_VERSION}"
            "golangxspcomm.so"
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${XCOMM_LIB_DIR}/xspcomm.go
)

add_custom_target(
    _DummyTarget_create_golangxspcomm_js_link ALL
  DEPENDS golangspcomm_js_lnk
)

# Install
install(DIRECTORY ${XCOMM_LIB_DIR} DESTINATION share/picker/golang/src USE_SOURCE_PERMISSIONS)
