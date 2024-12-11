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
target_link_options(luaxspcomm PRIVATE
 -Wl,-rpath,~/.local/lib
 -Wl,-rpath,/usr/local/lib)
 target_link_libraries(luaxspcomm PRIVATE xspcomm)

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../lua/)
set_target_properties(luaxspcomm
    PROPERTIES
      LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR}
      INSTALL_RPATH "$ORIGIN/../../lib:$ORIGIN/../lib:$ORIGIN/../picker/share/picker/lib:$ORIGIN/../picker/lib:$ORIGIN/"
      BUILD_WITH_INSTALL_RPATH true
)

# copy file
add_custom_command(
    OUTPUT ${XCOMM_LIB_DIR}/luaxspcomm.so.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/luaxspcomm.so
            ${XCOMM_LIB_DIR}/luaxspcomm.so.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/xspcomm.lua
            ${XCOMM_LIB_DIR}/
    DEPENDS luaxspcomm
)

add_custom_target(
    _DummyTarget_create_luaspcomm_so_link ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink luaxspcomm.so.${PROJECT_VERSION} luaxspcomm.so
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${XCOMM_LIB_DIR}/luaxspcomm.so.${PROJECT_VERSION}
)

# Install
install(FILES ${XCOMM_LIB_DIR}/luaxspcomm.so.${PROJECT_VERSION} ${XCOMM_LIB_DIR}/luaxspcomm.so ${XCOMM_LIB_DIR}/xspcomm.lua
        DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/lua)
