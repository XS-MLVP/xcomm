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
set(XSPCOMM_ABI_VERSION 3)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(xcomm)
init_xspcomm()

# Source headers must win over copies left in the build-tree install staging
# directory.  Otherwise an incremental build can compile against yesterday's
# public API before copy_xspcomm_include refreshes that directory.
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/include)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(XSPCOMM_HEADER_STAGE ${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(src)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
add_subdirectory(integrations/tlm)
add_subdirectory(swig)
