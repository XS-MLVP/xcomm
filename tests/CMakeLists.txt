include_directories(../include)
enable_testing()
if(NOT "${XSPCOMM_WASM_LIB}" STREQUAL "TRUE")
add_executable(test_xdata test_xdata.cpp)
target_link_libraries(test_xdata ${PROJECT_NAME})
add_test(NAME test_xdata COMMAND test_xdata)

add_executable(test_xcomuse_base test_xcomuse_base.cpp)
target_link_libraries(test_xcomuse_base ${PROJECT_NAME})
add_test(NAME test_xcomuse_base COMMAND test_xcomuse_base)

add_executable(test_xexpr test_xexpr.cpp)
target_link_libraries(test_xexpr ${PROJECT_NAME})
add_test(NAME test_xexpr COMMAND test_xexpr)

add_executable(test_xfsm test_xfsm.cpp)
target_link_libraries(test_xfsm ${PROJECT_NAME})
add_test(NAME test_xfsm COMMAND test_xfsm)
endif()
