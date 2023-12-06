cmake_minimum_required(VERSION 3.15)
project(xcomm VERSION 0.0.1)

add_definitions(-DXSPCOMM_VERSION="${PROJECT_VERSION}")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++20" COMPILER_SUPPORTS_CXX20)
if(COMPILER_SUPPORTS_CXX20)
    set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=c++20 -fcoroutines")
	add_definitions(-DENABLE_XCOROUTINE=true)
else()
    message(WARNING "The compiler ${CMAKE_CXX_COMPILER} has no C++20 support. If you need coroutines, please use a different C++ compiler.")
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	add_definitions(-DENABLE_XCOROUTINE=false)
endif()


set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(src)
add_subdirectory(tests)

if ("$ENV{BUILD_SWIG}" STREQUAL "ON")
	add_subdirectory(swig)
endif()

add_custom_target(copy_include ALL
VERBATIM
COMMAND ${CMAKE_COMMAND} -E copy_directory
							"${CMAKE_CURRENT_SOURCE_DIR}/include"
							"${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
)

install( DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/ DESTINATION include )
install( TARGETS ${PROJECT_NAME} DESTINATION lib )
