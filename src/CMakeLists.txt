include_directories(../include)

set(SRC_FILES xclock.cpp xdata.cpp xport.cpp xutil.cpp xcoroutine.cpp xinstance.cpp)

if(${XSPCOMM_WASM_LIB})
set(CMAKE_C_COMPILER emcc)
set(CMAKE_CXX_COMPILER em++)
add_definitions(-DBUILDING_NODE_EXTENSION)
add_definitions(-DFORCE_NO_EXECINFO_H)
endif()

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}" SOVERSION ${PROJECT_VERSION})
