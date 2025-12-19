# set output directory
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_golangswig)

# Add swig module
set_property(SOURCE golang.i PROPERTY CPLUSPLUS ON)

swig_add_library(golangxspcomm TYPE SHARED LANGUAGE go SOURCES golang.i)
target_link_libraries(golangxspcomm PRIVATE xspcomm)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../golang/src/xspcomm)

# set rpath prefix
set(RPATH_PREFIX "")
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
 # macOS doesn't support $ORIGIN
 set(RPATH_PREFIX "@loader_path")
 target_link_options(golangxspcomm PRIVATE -Wl,-undefined,dynamic_lookup)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
 set(RPATH_PREFIX "$ORIGIN")
endif ()

# set rpath
set(GOXSPCOMM_RPATH
        "${RPATH_PREFIX}/"
        "${RPATH_PREFIX}/../../lib"
        "${RPATH_PREFIX}/../picker/share_/picker/lib"
        "${RPATH_PREFIX}/../picker/lib"
        "$ENV{HOME}/.local/lib"
        "/usr/local/lib"
)

set_target_properties(golangxspcomm
    PROPERTIES
      SUFFIX ".so"
      LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR}
      INSTALL_RPATH "${GOXSPCOMM_RPATH}"
      BUILD_WITH_INSTALL_RPATH true
)

# copy file
add_custom_command(
    OUTPUT ${XCOMM_LIB_DIR}/xspcomm.go
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:golangxspcomm>
            ${XCOMM_LIB_DIR}/$<TARGET_LINKER_FILE_NAME:golangxspcomm>.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/../xspcomm.go
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:xspcomm>
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/go.mod
            ${XCOMM_LIB_DIR}/
    DEPENDS golangxspcomm
)

add_custom_command(
    OUTPUT golangspcomm_js_lnk
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XCOMM_LIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_LINKER_FILE_NAME:golangxspcomm>.${PROJECT_VERSION}"
            "$<TARGET_LINKER_FILE_NAME:golangxspcomm>"
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${XCOMM_LIB_DIR}/xspcomm.go
)

add_custom_target(
    _DummyTarget_create_golangxspcomm_js_link ALL
  DEPENDS golangspcomm_js_lnk
)

# Install
install(DIRECTORY ${XCOMM_LIB_DIR} DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/golang/src USE_SOURCE_PERMISSIONS)
