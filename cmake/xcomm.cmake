# Init function:
# 1. set the project version
# 2. set the compiler
# 3. check the execinfo.h
# 4. set the xspcomm version
# 5. set the c++ standard
# 6. set the xconfig.h
# 7. set install location prefix

function(init_xspcomm)
string(TOLOWER "$ENV{CMAKE_CXX_COMPILER}" cmake_cxx_compiler)
if (NOT "${cmake_cxx_compiler}" STREQUAL "")
set(CMAKE_CXX_COMPILER ${cmake_cxx_compiler} PARENT_SCOPE)
endif()

string(TOLOWER "$ENV{CMAKE_C_COMPILER}" cmake_c_compiler)
if (NOT "${cmake_c_compiler}" STREQUAL "")
set(CMAKE_C_COMPILER ${cmake_cxx_compiler} PARENT_SCOPE)
endif()

include(CheckIncludeFile)
check_include_file("execinfo.h" HAVE_EXECINFO_H)
if(HAVE_EXECINFO_H)
    add_definitions(-DHAVE_EXECINFO_H)
else()
    message(WARNING "The <execinfo.h> not find, xutil.Assert will not print the call stack.")
endif()

set(XSPCOMM_VERSION "${PROJECT_VERSION}" PARENT_SCOPE)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++20" COMPILER_SUPPORTS_CXX20)
if(COMPILER_SUPPORTS_CXX20)
    set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=c++20 -fcoroutines" PARENT_SCOPE)
    add_definitions(-DENABLE_XCOROUTINE=true)
    set(ENABLE_XCOROUTINE "true")
else()
    message(WARNING "The compiler ${CMAKE_CXX_COMPILER} has no C++20 support. If you need coroutines, please use a different C++ compiler.")
    set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=c++17" PARENT_SCOPE)
    add_definitions(-DENABLE_XCOROUTINE=false)
    set(ENABLE_XCOROUTINE "false")
endif()

set(GIT_BRANCH "none")
set(GIT_HASH "none")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
else()
    message(WARNING "Git not found, the branch and hash in version will be empty.")
endif()

message(STATUS "GIT_BRANCH: ${GIT_BRANCH}")
message(STATUS "GIT_HASH: ${GIT_HASH}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/xspcomm/xconfig.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/xspcomm/xconfig.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

if (NOT "$ENV{XSPCOMM_INSTALL_PREFIX}" STREQUAL "")
  set(XSPCOMM_INSTALL_PREFIX "$ENV{XSPCOMM_INSTALL_PREFIX}" PARENT_SCOPE)
else()
  set(XSPCOMM_INSTALL_PREFIX "" PARENT_SCOPE)
endif()
endfunction()
