# Include jni
find_package(JNI REQUIRED)
find_package(Java REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

# Include headers
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_javaswig)

# Add swig module
set_property(SOURCE java.i PROPERTY CPLUSPLUS ON)

swig_add_library(javaxspcomm LANGUAGE java SOURCES java.i)
target_link_libraries(javaxspcomm PRIVATE xspcomm)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../java/xspcomm)
set_property(TARGET javaxspcomm PROPERTY SWIG_COMPILE_OPTIONS -package com.xspcomm)
# set rpath prefix
set(RPATH_PREFIX "")
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
 # macOS doesn't support $ORIGIN
 set(RPATH_PREFIX "@loader_path")
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
 set(RPATH_PREFIX "$ORIGIN")
endif ()
# set rpath
set(JAVAXSPCOMM_RPATH
        "$ENV{HOME}/.local/lib"
        "/usr/local/lib"
        "${RPATH_PREFIX}/../../lib"
        "${RPATH_PREFIX}/../picker/share/picker/lib"
        "${RPATH_PREFIX}/../picker/lib"
        "${RPATH_PREFIX}/"
)
set_target_properties(javaxspcomm
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR}
      INSTALL_RPATH "${JAVAXSPCOMM_RPATH}"
      BUILD_WITH_INSTALL_RPATH true
)

# copy file
add_custom_command(
    OUTPUT ${XCOMM_LIB_DIR}/javaxspcomm.java
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:javaxspcomm>
            ${XCOMM_LIB_DIR}/$<TARGET_FILE_NAME:javaxspcomm>.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:xspcomm>
            ${XCOMM_LIB_DIR}/$<TARGET_FILE_NAME:xspcomm>
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:xspcomm>
            ${XCOMM_LIB_DIR}/$<TARGET_LINKER_FILE_NAME:xspcomm>
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/*.java
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/*.java
            ${XCOMM_LIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/MANIFEST.MF
            ${XCOMM_LIB_DIR}/../
    COMMAND ${CMAKE_COMMAND} -E echo 'Version: ${PROJECT_VERSION}' >> ${XCOMM_LIB_DIR}/../MANIFEST.MF
    COMMAND ${CMAKE_COMMAND} -E echo '' >> ${XCOMM_LIB_DIR}/../MANIFEST.MF
    COMMAND ${Java_JAVAC_EXECUTABLE} -d ${XCOMM_LIB_DIR} ${XCOMM_LIB_DIR}/*.java
    DEPENDS javaxspcomm
)

add_custom_command(
    OUTPUT javaxspcomm_so_lnk
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE_NAME:javaxspcomm>.${PROJECT_VERSION}"
            "$<TARGET_FILE_NAME:javaxspcomm>"
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${XCOMM_LIB_DIR}/javaxspcomm.java
)

add_custom_command(
    OUTPUT javaxspcomm_jar_lnk
    COMMAND ${Java_JAR_EXECUTABLE} cfm xspcomm-java.jar MANIFEST.MF -C xspcomm .
    COMMAND ${CMAKE_COMMAND} -E copy
            xspcomm-java.jar
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/xspcomm-java.${PROJECT_VERSION}.jar
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}/..
    DEPENDS javaxspcomm_so_lnk
)

add_custom_target(
    _DummyTarget_create_javaxspcomm_so_link ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink xspcomm-java.${PROJECT_VERSION}.jar xspcomm-java.jar
    WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    DEPENDS javaxspcomm_jar_lnk
)
# Install
install(FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/xspcomm-java.${PROJECT_VERSION}.jar ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/xspcomm-java.jar
        DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/java)
