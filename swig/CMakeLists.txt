function(setup_swig_javascript_env)
# set compiler
set(CMAKE_C_COMPILER emcc PARENT_SCOPE)
set(CMAKE_CXX_COMPILER em++ PARENT_SCOPE)
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_jsswig PARENT_SCOPE)

#options
add_definitions(-DBUILDING_NODE_EXTENSION)
add_definitions(-DFORCE_NO_EXECINFO_H)

if (NOT "$ENV{NODE_MODULES_PATH}" STREQUAL "")
  set(NODE_MODULES_PATH "$ENV{BUILD_SWIG}")
else()
  message(STATUS "search node moduels path in ${CMAKE_CURRENT_SOURCE_DIR}")
  set(NODE_MODULES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/node_modules")
  if (NOT EXISTS ${NODE_MODULES_PATH})
    set(NODE_MODULES_PATH "/usr/local/lib/node_modules")
    message(STATUS "search node moduels path in: ${NODE_MODULES_PATH}")
  endif()
endif()

if(NOT EXISTS ${NODE_MODULES_PATH})
  message(FATAL_ERROR "node_modules not found, can use NODE_MODULES_PATH=/node/node_modules to specify the path")
endif()

if(NOT EXISTS "${NODE_MODULES_PATH}/node-addon-api")
    message(FATAL_ERROR "node-addon-api not found, please run npm install: npm install -g node-addon-api")
endif()

if(NOT EXISTS "${NODE_MODULES_PATH}/emnapi")
    message(FATAL_ERROR "emnapi not found, please run npm install: npm install -g emnapi")
endif()

if(NOT EXISTS "${NODE_MODULES_PATH}/@emnapi")
    message(FATAL_ERROR "emnapi not found, please run npm install: npm install -g @emnapi")
endif()

# Include headers
include_directories(${NODE_MODULES_PATH}/node-addon-api ${NODE_MODULES_PATH}/emnapi/include .)
link_directories(${NODE_MODULES_PATH}/emnapi/lib/wasm32-emscripten)
set(NODE_MODULES_PATH ${NODE_MODULES_PATH} PARENT_SCOPE)
endfunction(setup_swig_javascript_env)

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

string(TOLOWER "$ENV{BUILD_XSPCOMM_SWIG}" swig_target)
if (NOT "${swig_target}" STREQUAL "")
if (${swig_target} STREQUAL "python")
    add_subdirectory(python)
elseif(${swig_target} STREQUAL "java")
    add_subdirectory(java)
elseif(${swig_target} STREQUAL "javascript")
    add_subdirectory(javascript)
elseif(${swig_target} STREQUAL "on")
    add_subdirectory(python)
else()
    message(FATAL_ERROR "BUILD_XSPCOMM_SWIG should be set to python/java/javascript, but it is: ${swig_target}")
endif()
endif()
