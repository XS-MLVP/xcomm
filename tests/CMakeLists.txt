if(NOT XSPCOMM_WASM_LIB)
    foreach(component IN ITEMS xdata xclock xtrigger xcoverage xcomuse xexpr xvpi xfsm)
        add_executable(test_${component} unit/test_${component}.cpp)
        target_include_directories(test_${component} PRIVATE
            ${PROJECT_SOURCE_DIR}/third_party/catch2
            ${PROJECT_SOURCE_DIR}/src)
        target_link_libraries(test_${component} PRIVATE ${PROJECT_NAME})
        add_test(NAME test_${component} COMMAND test_${component})
    endforeach()
    add_executable(test_coverage_core unit/test_coverage_core.cpp)
    target_include_directories(test_coverage_core PRIVATE
        ${PROJECT_SOURCE_DIR}/third_party/catch2 ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(test_coverage_core PRIVATE ${PROJECT_NAME})
    add_test(NAME test_coverage_core COMMAND test_coverage_core)
    target_sources(test_xdata PRIVATE unit/xdata_cases.cpp)
endif()
