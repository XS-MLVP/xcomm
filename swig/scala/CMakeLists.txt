# Include jni
find_package(JNI REQUIRED)
find_package(Java REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

# Scala
find_program(SCALAC_EXECUTABLE scalac)

if(SCALAC_EXECUTABLE)
    message(STATUS "Found scalac: ${SCALAC_EXECUTABLE}")
else()
    message(FATAL_ERROR "scalac not found. Please install Scala compiler.")
endif()

# Include headers
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_scalaswig)

# Add swig module
set_property(SOURCE scala.i PROPERTY CPLUSPLUS ON)

swig_add_library(scalaxspcomm LANGUAGE java SOURCES scala.i)

target_link_libraries(scalaxspcomm PRIVATE xspcomm)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../scala/xspcomm)
set_property(TARGET scalaxspcomm PROPERTY SWIG_COMPILE_OPTIONS -package com.xspcomm)

# set rpath prefix
set(RPATH_PREFIX "")
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # macOS doesn't support $ORIGIN
    set(RPATH_PREFIX "@loader_path")
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(RPATH_PREFIX "$ORIGIN")
endif ()

# set rpath
set(SCALAXSPCOMM_RPATH
        "$ENV{HOME}/.local/lib"
        "/usr/local/lib"
        "${RPATH_PREFIX}/../../lib"
        "${RPATH_PREFIX}/../picker/share/picker/lib"
        "${RPATH_PREFIX}/../picker/lib"
        "${RPATH_PREFIX}/"
)

set_target_properties(scalaxspcomm
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR}
      INSTALL_RPATH "${SCALAXSPCOMM_RPATH}"
      BUILD_WITH_INSTALL_RPATH true
)

# copy file
add_custom_command(
    OUTPUT ${XCOMM_LIB_DIR}/scalaxspcomm.java
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XCOMM_LIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:scalaxspcomm>
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/*.java
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:xspcomm>
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:xspcomm>
            ${XCOMM_LIB_DIR}/$<TARGET_LINKER_FILE_NAME:xspcomm>
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/*.scala
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/*.java
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/MANIFEST.MF
            ${XCOMM_LIB_DIR}/../
    COMMAND ${CMAKE_COMMAND} -E echo 'Version: ${PROJECT_VERSION}' >> ${XCOMM_LIB_DIR}/../MANIFEST.MF
    COMMAND ${CMAKE_COMMAND} -E echo '' >> ${XCOMM_LIB_DIR}/../MANIFEST.MF
    COMMAND ${Java_JAVAC_EXECUTABLE} -d ${XCOMM_LIB_DIR} ${XCOMM_LIB_DIR}/*.java
    COMMAND scalac -cp ${XCOMM_LIB_DIR} -d ${XCOMM_LIB_DIR} ${XCOMM_LIB_DIR}/*.scala
    DEPENDS scalaxspcomm
)

add_custom_command(
    OUTPUT scalaxspcomm_so_lnk
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE_NAME:scalaxspcomm>"
            "$<TARGET_LINKER_FILE_NAME:scalaxspcomm>"
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${XCOMM_LIB_DIR}/scalaxspcomm.java
)

add_custom_command(
    OUTPUT  scalaxspcomm_so_jar_create
    COMMAND ${Java_JAR_EXECUTABLE} cfm xspcomm-scala.jar MANIFEST.MF -C xspcomm .
    COMMAND ${CMAKE_COMMAND} -E copy
            xspcomm-scala.jar
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/xspcomm-scala.${PROJECT_VERSION}.jar
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}/..
    DEPENDS scalaxspcomm_so_lnk
)

add_custom_target(
    _DummyTarget_create_scalaxspcomm_so_link ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink xspcomm-scala.${PROJECT_VERSION}.jar xspcomm-scala.jar
    WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    DEPENDS scalaxspcomm_so_jar_create
)

# Install
install(FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/xspcomm-scala.${PROJECT_VERSION}.jar ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/xspcomm-scala.jar
        DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/scala)
