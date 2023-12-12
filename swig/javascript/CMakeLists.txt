# set compiler
set(CMAKE_C_COMPILER emcc)
set(CMAKE_CXX_COMPILER em++)
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_jsswig)

#options
add_definitions(-DBUILDING_NODE_EXTENSION)
add_definitions(-DFORCE_NO_EXECINFO_H)

set(NODE_MODULES_PATH /usr/local/lib/node_modules)

# Include headers
include_directories(${NODE_MODULES_PATH}/node-addon-api ${NODE_MODULES_PATH}/emnapi/include)
link_directories(${NODE_MODULES_PATH}/emnapi/lib/wasm32-emscripten)

# Add swig module
set_property(SOURCE javascript.i PROPERTY CPLUSPLUS ON)

swig_add_library(jsxspcomm LANGUAGE javascript SOURCES 
javascript.i
../../src/xclock.cpp
../../src/xport.cpp
../../src/xdata.cpp
../../src/xcoroutine.cpp
../../src/xutil.cpp)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../javascript/xspcomm)
set_target_properties(jsxspcomm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR})
set_target_properties(jsxspcomm PROPERTIES SUFFIX ".js")
set_property(TARGET jsxspcomm PROPERTY SWIG_COMPILE_OPTIONS -napi)
target_link_libraries(jsxspcomm emnapi)

set(LK_OPS, "-sNO_DISABLE_EXCEPTION_CATCHING")
string(APPEND LK_OPS " -sEXPORTED_FUNCTIONS=\"['_malloc','_free','_napi_register_wasm_v1','_node_api_module_get_api_version_v1']\"")
string(APPEND LK_OPS " --js-library=${NODE_MODULES_PATH}/emnapi/dist/library_napi.js")

target_link_options(jsxspcomm PRIVATE "SHELL: ${LK_OPS}")
