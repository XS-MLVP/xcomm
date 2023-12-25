include_directories(../include)

if(NOT ${XSPCOMM_WASM_LIB})
add_executable(test_xdata test_xdata.cpp)
target_link_libraries(test_xdata ${PROJECT_NAME})
endif()
