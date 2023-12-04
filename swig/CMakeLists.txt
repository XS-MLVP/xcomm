# Include SWIG
set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_policy(SET CMP0078 NEW)
cmake_policy(SET CMP0086 NEW)

find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# Include python
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH})

# Include headers
include_directories(../include)
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_pyswig)

# Add swig module
set_property(SOURCE python.i PROPERTY CPLUSPLUS ON)

swig_add_library(pyxcomm LANGUAGE python SOURCES 
python.i
../src/xclock.cpp
../src/xport.cpp
../src/xdata.cpp
../src/xcoroutine.cpp
../src/xutil.cpp)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/python/xcomm)
set_target_properties(pyxcomm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${XCOMM_LIB_DIR})

# copy file
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/pyxcomm.py PROPERTIES GENERATED TRUE)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pyxcomm.py
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/pyxcomm.py
            ${XCOMM_LIB_DIR}/pyxcomm.py
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/xcomm.py
            ${XCOMM_LIB_DIR}/__init__.py
    DEPENDS pyxcomm
)

add_custom_target(
  _DummyTarget ALL
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pyxcomm.py
)
