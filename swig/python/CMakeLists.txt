# Include python
find_package(Python COMPONENTS Interpreter Development)
include_directories(${Python_INCLUDE_DIRS})

# Include headers
set(SWIG_OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_pyswig)

# Add swig module
string(TOLOWER "$ENV{BUILD_XSPCOMM_VCS_UVMPS}" swig_vcs_pubsub)
if (NOT "${swig_vcs_pubsub}" STREQUAL "")
set(VCS_HOME "$ENV{VCS_HOME}")
set(UVMC_HOME "$ENV{UVMC_HOME}")
set_property(SOURCE python_uvmps.i PROPERTY CPLUSPLUS ON)
swig_add_library(pyxspcomm LANGUAGE python SOURCES 
python_uvmps.i
thirdcall.cpp
)
add_dependencies(pyxspcomm uvmps_vcs)
target_link_options(pyxspcomm PRIVATE
        -Wl,-rpath,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        -Wl,-rpath,~/.local/lib 
        -Wl,-rpath,/usr/local/lib
        -L./
        -L/lib64
	-L${VCS_HOME}/linux64/lib
        -no-pie
        -Wl,--no-as-needed
        -rdynamic
        -Wl,-whole-archive
        -lvcsucli
        -Wl,-no-whole-archive
        -luclinative
        -lvirsim
        -lerrorinf
        -lsnpsmalloc
        -lvfs
        -lvcsnew
        -lrt
        -lmdb
        -ldl
        -lc
        -lm
        -lpthread
        -lvcs_tls
        -lvcsucli
)
target_link_libraries(pyxspcomm PRIVATE xspcomm 
    ${VCS_HOME}/linux64/lib/libvcs_tls.so
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libuvmps_vcs.so
    /lib64/libnuma.so.1
    #${VCS_HOME}/linux64/lib/vcs_tls.o
    #${VCS_HOME}/linux64/lib/vcs_save_restore.o
)
else()
set_property(SOURCE python.i PROPERTY CPLUSPLUS ON)
swig_add_library(pyxspcomm LANGUAGE python SOURCES 
python.i
thirdcall.cpp
)
target_link_options(pyxspcomm PRIVATE
        -Wl,-rpath,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        -Wl,-rpath,~/.local/lib 
        -Wl,-rpath,/usr/local/lib )
target_link_libraries(pyxspcomm PRIVATE xspcomm)
endif()

# set buld output directory
set(XCOMM_LIB_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../python/xspcomm)
set_target_properties(pyxspcomm PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${SWIG_OUTFILE_DIR})

# copy file
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/pyxspcomm.py PROPERTIES GENERATED TRUE)
add_custom_command(
    OUTPUT ${XCOMM_LIB_DIR}/info.py
    COMMAND ${CMAKE_COMMAND} -E copy
            ${SWIG_OUTFILE_DIR}/_pyxspcomm.so
            ${XCOMM_LIB_DIR}/_pyxspcomm.so.${PROJECT_VERSION}
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/pyxspcomm.py
            ${XCOMM_LIB_DIR}/pyxspcomm.py
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/xcomm.py
            ${XCOMM_LIB_DIR}/__init__.py
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/info.py
            ${XCOMM_LIB_DIR}/info.py
    DEPENDS pyxspcomm
)

add_custom_command(
    OUTPUT pyxspcomm_so_lnk
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "_pyxspcomm.so.${PROJECT_VERSION}"
            "_pyxspcomm.so"
    WORKING_DIRECTORY ${XCOMM_LIB_DIR}
    DEPENDS ${XCOMM_LIB_DIR}/info.py
)

add_custom_target(
    _DummyTarget_create_pyxspcomm_so_link ALL
  DEPENDS pyxspcomm_so_lnk
)

install(DIRECTORY ${XCOMM_LIB_DIR} DESTINATION share/mcv/python USE_SOURCE_PERMISSIONS)
