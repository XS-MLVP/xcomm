# Include jni
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

# Include headers
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_javaswig)

# Add swig module
set_property(SOURCE java.i PROPERTY CPLUSPLUS ON)

swig_add_library(javaxspcomm LANGUAGE java SOURCES java.i)
target_link_options(javaxspcomm PRIVATE
 -Wl,-rpath,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
 -Wl,-rpath,~/.local/lib
 -Wl,-rpath,/usr/local/lib)
 target_link_libraries(javaxspcomm PRIVATE xspcomm)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../java/xspcomm)
set_target_properties(javaxspcomm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR})

# copy file
add_custom_command(
    OUTPUT ${XCOMM_LIB_DIR}/javaxspcomm.java
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/libjavaxspcomm.so
            ${XCOMM_LIB_DIR}/libjavaxspcomm.so.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/test/*.java
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/*.java
            ${XCOMM_LIB_DIR}/
    DEPENDS javaxspcomm
)

add_custom_command(
    OUTPUT javaxspcomm_so_lnk
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "libjavaxspcomm.so.${PROJECT_VERSION}"
            "libjavaxspcomm.so"
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${XCOMM_LIB_DIR}/javaxspcomm.java
)
add_custom_target(
    _DummyTarget_create_javaxspcomm_so_link ALL
  DEPENDS javaxspcomm_so_lnk
)
# Install
install(DIRECTORY ${XCOMM_LIB_DIR} DESTINATION share/mcv/java USE_SOURCE_PERMISSIONS)
