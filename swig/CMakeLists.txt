# Include SWIG
set(CMAKE_VERBOSE_MAKEFILE ON)

if(POLICY CMP0078)
    message("Find CMP0078, set it be NEW")
    cmake_policy(SET CMP0078 NEW)
endif()
if(POLICY CMP0086)
    message("Find CMP0086, set it be NEW")
    cmake_policy(SET CMP0086 NEW)
endif()
if(POLICY CMP0148)
    message("Find CMP0148, set it be NEW")
    cmake_policy(SET CMP0148 NEW)
endif()

find_package(SWIG 4.2.0 REQUIRED)
include(${SWIG_USE_FILE})

# Include headers
include_directories(../include)

string(TOLOWER "$ENV{BUILD_SWIG}" swig_target)
if (${swig_target} STREQUAL "python")
    add_subdirectory(python)
elseif(${swig_target} STREQUAL "java")
    add_subdirectory(java)
elseif(${swig_target} STREQUAL "javascript")
    add_subdirectory(javascript)
else()
    message(FATAL_ERROR "BUILD_SWIG should be set to python/java/javascript, but it is: ${swig_target}")
endif()

