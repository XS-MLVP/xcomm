include_directories(../include)

set(SRC_FILES xclock.cpp xdata.cpp xport.cpp xutil.cpp xcoroutine.cpp xinstance.cpp)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}" SOVERSION ${PROJECT_VERSION})
