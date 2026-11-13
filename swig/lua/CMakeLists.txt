# Find lua
find_program(LUA_COMPILER luac)

if(NOT LUA_COMPILER)
    message(FATAL_ERROR "luac not found. Please install Lua compiler.")
endif()

find_package(Lua REQUIRED)

if(NOT LUA_FOUND)
    message(FATAL_ERROR "Lua-dev not found. Please install Lua development package.")
endif()

include_directories(${LUA_INCLUDE_DIR})

# Include headers
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_luaswig)

# Add swig module
set_property(SOURCE lua.i PROPERTY CPLUSPLUS ON)

swig_add_library(luaxspcomm LANGUAGE lua SOURCES lua.i)
set_property(TARGET luaxspcomm PROPERTY SWIG_DEPENDS
    "${PROJECT_SOURCE_DIR}/swig/xcomm.i"
    "${PROJECT_SOURCE_DIR}/swig/view_raw.i"
    "${PROJECT_SOURCE_DIR}/include/xspcomm/xview.h")
target_link_libraries(luaxspcomm PRIVATE xspcomm)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../lua)

# set rpath prefix
set(RPATH_PREFIX "")
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # macOS doesn't support $ORIGIN
    set(RPATH_PREFIX "@loader_path")
    target_link_options(luaxspcomm PRIVATE -Wl,-undefined,dynamic_lookup)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(RPATH_PREFIX "$ORIGIN")
endif ()
# set rpath
set(LUAXSPCOMM_RPATH
        "${RPATH_PREFIX}/"
        "${RPATH_PREFIX}/../../lib"
        "${RPATH_PREFIX}/../lib"
        "${RPATH_PREFIX}/../picker/share/picker/lib"
        "${RPATH_PREFIX}/../picker/lib"
        "$ENV{HOME}/.local/lib"
        "/usr/local/lib"
)

set_target_properties(luaxspcomm
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR}
      INSTALL_RPATH "${LUAXSPCOMM_RPATH}"
      BUILD_WITH_INSTALL_RPATH true
)

# copy file
add_custom_command(TARGET luaxspcomm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XCOMM_LIB_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:luaxspcomm>
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/xspcomm.lua
            ${XCOMM_LIB_DIR}/
    COMMAND ${CMAKE_COMMAND}
            "-DDEST_DIR=${XCOMM_LIB_DIR}"
            "-DTARGET_FILE=$<TARGET_FILE:xspcomm>"
            "-DTARGET_FILE_NAME=$<TARGET_FILE_NAME:xspcomm>"
            "-DTARGET_LINKER_FILE_NAME=$<TARGET_LINKER_FILE_NAME:xspcomm>"
            "-DTARGET_SONAME_FILE_NAME=$<TARGET_SONAME_FILE_NAME:xspcomm>"
            "-DPLATFORM_NAME=${CMAKE_SYSTEM_NAME}"
            "-DPROJECT_VERSION=${PROJECT_VERSION}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/../StageSharedLibrary.cmake
)

# Install
install(DIRECTORY ${XCOMM_LIB_DIR}/
    DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/lua)

if(BUILD_TESTING)
    find_program(LUA_EXECUTABLE lua REQUIRED)
    add_test(NAME test_xstruct_lua
        COMMAND ${CMAKE_COMMAND} -E env "LUA_PATH=${XCOMM_LIB_DIR}/?.lua;;"
            ${LUA_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/bindings/lua/test_xstruct.lua)
endif()
