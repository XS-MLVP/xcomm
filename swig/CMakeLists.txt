# Include SWIG
set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_policy(SET CMP0078 NEW)
cmake_policy(SET CMP0086 NEW)

find_package(SWIG 4.2.0 REQUIRED)
include(${SWIG_USE_FILE})

# Include python
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH})

# Include headers
include_directories(../include)
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_pyswig)

# Add swig module
set_property(SOURCE python.i PROPERTY CPLUSPLUS ON)

swig_add_library(pyxspcomm LANGUAGE python SOURCES 
python.i
../src/xclock.cpp
../src/xport.cpp
../src/xdata.cpp
../src/xcoroutine.cpp
../src/xutil.cpp)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/python/xspcomm)
set_target_properties(pyxspcomm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR})

# copy file
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/pyxspcomm.py PROPERTIES GENERATED TRUE)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pyxspcomm.py
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/_pyxspcomm.so
            ${XCOMM_LIB_DIR}/_pyxspcomm.so.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/pyxspcomm.py
            ${XCOMM_LIB_DIR}/pyxspcomm.py
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/xcomm.py
            ${XCOMM_LIB_DIR}/__init__.py
    DEPENDS pyxspcomm
)

add_custom_command(
    OUTPUT pyxspcomm_so_lnk
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "_pyxspcomm.so.${PROJECT_VERSION}"
            "_pyxspcomm.so"
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pyxspcomm.py
)

add_custom_target(
  _DummyTarget ALL
  DEPENDS pyxspcomm_so_lnk
)

find_package(Python REQUIRED COMPONENTS Interpreter Development)
install(DIRECTORY ${XCOMM_LIB_DIR} DESTINATION . USE_SOURCE_PERMISSIONS)
