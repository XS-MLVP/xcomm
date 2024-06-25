cmake_minimum_required(VERSION 3.15)
project(xspcomm VERSION 0.0.1)

string(TOLOWER "$ENV{CMAKE_CXX_COMPILER}" cmake_cxx_compiler)
if (NOT "${cmake_cxx_compiler}" STREQUAL "")
set(CMAKE_CXX_COMPILER ${cmake_cxx_compiler})
endif()

include(CheckIncludeFile)
check_include_file("execinfo.h" HAVE_EXECINFO_H)
if(HAVE_EXECINFO_H)
    add_definitions(-DHAVE_EXECINFO_H)
else()
    message(WARNING "The <execinfo.h> not find, xutil.Assert will not print the call stack.")
endif()

function(init_xspcomm)
set(XSPCOMM_VERSION "${PROJECT_VERSION}" PARENT_SCOPE)
add_definitions(-DXSPCOMM_VERSION="${PROJECT_VERSION}")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++20" COMPILER_SUPPORTS_CXX20)
if(COMPILER_SUPPORTS_CXX20)
    set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
	set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=c++20 -fcoroutines" PARENT_SCOPE)
	add_definitions(-DENABLE_XCOROUTINE=true)
	set(ENABLE_XCOROUTINE true PARENT_SCOPE)
else()
    message(WARNING "The compiler ${CMAKE_CXX_COMPILER} has no C++20 support. If you need coroutines, please use a different C++ compiler.")
	set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=c++17" PARENT_SCOPE)
	add_definitions(-DENABLE_XCOROUTINE=false)
	set(ENABLE_XCOROUTINE false PARENT_SCOPE)
endif()
endfunction()

init_xspcomm()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(swig)

add_custom_target(copy_xspcomm_include ALL
VERBATIM
COMMAND ${CMAKE_COMMAND} -E copy_directory
							"${CMAKE_CURRENT_SOURCE_DIR}/include"
							"${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
COMMAND sed -i "s/XSPCOMM_VERSION/\"${PROJECT_VERSION}\"/g" ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/xspcomm/xutil.h
COMMAND sed -i "s/ENABLE_XCOROUTINE/${ENABLE_XCOROUTINE}/g" ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/xspcomm/xclock.h
COMMAND sed -i "s/ENABLE_XCOROUTINE/${ENABLE_XCOROUTINE}/g" ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/xspcomm/xcoroutine.h
DEPENDS ${PROJECT_NAME}
)

install( DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/ DESTINATION include )
install( TARGETS ${PROJECT_NAME} DESTINATION lib )
install( DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/ DESTINATION share/picker/include )
install( TARGETS ${PROJECT_NAME} DESTINATION share/picker/lib )