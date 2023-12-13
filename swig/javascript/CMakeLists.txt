# set compiler
set(CMAKE_C_COMPILER emcc)
set(CMAKE_CXX_COMPILER em++)
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_jsswig)

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
include_directories(${NODE_MODULES_PATH}/node-addon-api ${NODE_MODULES_PATH}/emnapi/include)
link_directories(${NODE_MODULES_PATH}/emnapi/lib/wasm32-emscripten)

# Add swig module
set_property(SOURCE javascript.i PROPERTY CPLUSPLUS ON)
add_compile_options("-sNO_DISABLE_EXCEPTION_CATCHING")

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

set(LK_OPS " -sEXPORTED_FUNCTIONS=\"['_malloc','_free','_napi_register_wasm_v1','_node_api_module_get_api_version_v1']\"")
string(APPEND LK_OPS " --js-library=${NODE_MODULES_PATH}/emnapi/dist/library_napi.js")

target_link_options(jsxspcomm PRIVATE "SHELL: ${LK_OPS}")

# copy file
set_source_files_properties(${SWIG_OUTFILE_DIR}/jsxspcomm.js PROPERTIES GENERATED TRUE)
add_custom_command(
    OUTPUT ${XCOMM_LIB_DIR}/jsxspcomm.js
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/jsxspcomm.js
            ${XCOMM_LIB_DIR}/jsxspcomm.js.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/jsxspcomm.wasm
            ${XCOMM_LIB_DIR}/jsxspcomm.wasm.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/test_index.html
            ${XCOMM_LIB_DIR}/test_index.html
    COMMAND ${CMAKE_COMMAND} -E copy
            ${NODE_MODULES_PATH}/@emnapi/runtime/dist/emnapi.min.js
            ${XCOMM_LIB_DIR}/emnapi.min.js
    DEPENDS jsxspcomm
)

add_custom_command(
    OUTPUT jsxspcomm_js_lnk
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "jsxspcomm.js.${PROJECT_VERSION}"
            "jsxspcomm.js"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "jsxspcomm.wasm.${PROJECT_VERSION}"
            "jsxspcomm.wasm"
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${XCOMM_LIB_DIR}/jsxspcomm.js
)

add_custom_target(
    _DummyTarget_create_jsxspcomm_js_link ALL
  DEPENDS jsxspcomm_js_lnk
)

install(DIRECTORY ${XCOMM_LIB_DIR} DESTINATION . USE_SOURCE_PERMISSIONS)
