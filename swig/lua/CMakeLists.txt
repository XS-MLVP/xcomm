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
        "$ENV{HOME}/.local/lib"
        "/usr/local/lib"
        "${RPATH_PREFIX}/../../lib"
        "${RPATH_PREFIX}/../lib"
        "${RPATH_PREFIX}/../picker/share/picker/lib"
        "${RPATH_PREFIX}/../picker/lib"
        "${RPATH_PREFIX}/"
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
)

# Install
install(FILES 
    ${XCOMM_LIB_DIR}/$<TARGET_FILE_NAME:luaxspcomm> 
    ${XCOMM_LIB_DIR}/xspcomm.lua
    DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/lua)
