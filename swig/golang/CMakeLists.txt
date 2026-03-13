# set output directory
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_golangswig)

# Add swig module
set_property(SOURCE golang.i PROPERTY CPLUSPLUS ON)

swig_add_library(golangxspcomm TYPE SHARED LANGUAGE go SOURCES golang.i)
target_link_libraries(golangxspcomm PRIVATE xspcomm)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../golang/src/xspcomm)
set(STAGE_NATIVE_LIBRARY_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../StageSharedLibrary.cmake)

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
    COMMAND ${CMAKE_COMMAND}
            "-DDEST_DIR=${XCOMM_LIB_DIR}"
            "-DTARGET_FILE=$<TARGET_FILE:golangxspcomm>"
            "-DTARGET_FILE_NAME=$<TARGET_FILE_NAME:golangxspcomm>"
            "-DTARGET_LINKER_FILE_NAME=$<TARGET_LINKER_FILE_NAME:golangxspcomm>"
            "-DPLATFORM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DPROJECT_VERSION=${PROJECT_VERSION}"
            -P ${STAGE_NATIVE_LIBRARY_SCRIPT}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/../xspcomm.go
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:xspcomm>
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/go.mod
            ${XCOMM_LIB_DIR}/
    DEPENDS golangxspcomm ${STAGE_NATIVE_LIBRARY_SCRIPT}
)

add_custom_target(
    _DummyTarget_stage_golangxspcomm ALL
  DEPENDS ${XCOMM_LIB_DIR}/xspcomm.go
)

# Install
install(DIRECTORY ${XCOMM_LIB_DIR} DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/golang/src USE_SOURCE_PERMISSIONS)
