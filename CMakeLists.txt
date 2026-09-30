cmake_minimum_required(VERSION 3.15)
set(XSPCOMM_CMAKE_VERSION "0.1.0")
if(DEFINED SKBUILD_PROJECT_VERSION)
    set(XSPCOMM_CMAKE_VERSION "${SKBUILD_PROJECT_VERSION}")
endif()
project(xspcomm VERSION ${XSPCOMM_CMAKE_VERSION})
if(DEFINED SKBUILD_PROJECT_VERSION_FULL)
    set(XSPCOMM_PACKAGE_VERSION "${SKBUILD_PROJECT_VERSION_FULL}")
else()
    set(XSPCOMM_PACKAGE_VERSION "${PROJECT_VERSION}")
endif()

# Increment only when the native C++ ABI becomes incompatible.
set(XSPCOMM_ABI_VERSION 1)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(xcomm)
init_xspcomm()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(swig)
