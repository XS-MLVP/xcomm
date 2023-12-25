setup_swig_javascript_env()

# Add swig module
set_property(SOURCE javascript.i PROPERTY CPLUSPLUS ON)
add_compile_options(-fexceptions -sMODULARIZE=1 -sEXPORT_NAME=xspcomm)

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

set(LK_OPS " -fexceptions -sMODULARIZE=1 -sEXPORT_NAME=xspcomm -sEXPORTED_FUNCTIONS=\"['_malloc','_free','_napi_register_wasm_v1','_node_api_module_get_api_version_v1']\"")
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
            ${CMAKE_CURRENT_SOURCE_DIR}/xspcomm.js
            ${XCOMM_LIB_DIR}/xspcomm.js
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
