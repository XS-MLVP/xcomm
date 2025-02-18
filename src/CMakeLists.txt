include_directories(../include)

set(SRC_FILES xclock.cpp xdata.cpp xport.cpp xutil.cpp xcoroutine.cpp xsignal_cfg.cpp xinstance.cpp)

if(${XSPCOMM_WASM_LIB})
set(CMAKE_C_COMPILER emcc)
set(CMAKE_CXX_COMPILER em++)
add_definitions(-DBUILDING_NODE_EXTENSION)
add_definitions(-DFORCE_NO_EXECINFO_H)
endif()

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}" SOVERSION ${PROJECT_VERSION})

string(TOLOWER "$ENV{BUILD_XSPCOMM_VCS_UVMPS}" swig_vcs_pubsub)
if (NOT "${swig_vcs_pubsub}" STREQUAL "")

execute_process(COMMAND bash -c "syscan -h|grep \"SystemC version\"|awk '{print $NF}'|xargs"
                OUTPUT_VARIABLE sysc_version
                OUTPUT_STRIP_TRAILING_WHITESPACE)
set(SYSC_VERSION "${sysc_version}")
message("The systemc version is: ${SYSC_VERSION}")

set(VCS_HOME "$ENV{VCS_HOME}")
set(UVMC_HOME "$ENV{UVMC_HOME}")

if (NOT EXISTS "${VCS_HOME}")
message(FATAL_ERROR "VCS_HOME not set")
endif()

if (NOT EXISTS "${UVMC_HOME}")
message(FATAL_ERROR "UVMC_HOME not set")
endif()


set(SYSCAN syscan -cpp g++ -cc gcc -full64 -cflags -g -cflags -DVCS -tlm2 -cflags -I${VCS_HOME}/include/systemc${SYSC_VERSION}/tlm_utils -cflags -I${UVMC_HOME}/src/connect/sc)
set(VCS vcs -q -e VcsMain -sysc=deltasync -lca  -sysc -cpp g++ -cc gcc -timescale=1ns/1ps -CFLAGS -DVCS ${VCS_HOME}/etc/uvm/src/dpi/uvm_dpi.cc -slave -full64
${VCS_HOME}/linux64/lib/vcs_tls.o
)

add_custom_target(
    uvmps_vcs ALL
    COMMAND ${SYSCAN} -cflags -I${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/uvm_pbsb.cpp  ${UVMC_HOME}/src/connect/sc/uvmc.cpp
    COMMAND ${VCS} sc_main
    COMMAND cp simv ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libuvmps_vcs.so.${PROJECT_VERSION}
    COMMAND cd ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} && ln -s libuvmps_vcs.so.${PROJECT_VERSION} libuvmps_vcs.so
)

endif()

add_custom_target(copy_xspcomm_include ALL
VERBATIM
COMMAND ${CMAKE_COMMAND} -E copy_directory
							"${CMAKE_CURRENT_SOURCE_DIR}/../include"
							"${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
DEPENDS ${PROJECT_NAME}
)

# install C++ headers and libraries
install( DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/ DESTINATION ${XSPCOMM_INSTALL_PREFIX}include )
install( TARGETS ${PROJECT_NAME} DESTINATION ${XSPCOMM_INSTALL_PREFIX}lib )
install( DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/ DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/include )
install( TARGETS ${PROJECT_NAME} DESTINATION ${XSPCOMM_INSTALL_PREFIX}share/picker/lib )
