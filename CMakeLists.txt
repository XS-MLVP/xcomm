cmake_minimum_required(VERSION 3.11)
project(xcomm VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -std=c++20")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

add_subdirectory(src)
add_subdirectory(tests)

if ("$ENV{BUILD_SWIG}" STREQUAL "ON")
	add_subdirectory(swig)
endif()

add_custom_target(copy_include ALL
VERBATIM
COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/include"
COMMAND ${CMAKE_COMMAND} -E copy_directory
							"${CMAKE_CURRENT_SOURCE_DIR}/include"
							"${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/include"
)
