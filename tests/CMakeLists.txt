include_directories(../include)

add_executable(test_xdata test_xdata.cpp)
target_link_libraries(test_xdata ${PROJECT_NAME})

