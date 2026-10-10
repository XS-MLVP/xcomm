include(${PROJECT_SOURCE_DIR}/cmake/Sources.cmake)

if(XSPCOMM_WASM_LIB)
    set(CMAKE_C_COMPILER emcc)
    set(CMAKE_CXX_COMPILER em++)
    add_definitions(-DBUILDING_NODE_EXTENSION -DFORCE_NO_EXECINFO_H)
endif()

add_library(${PROJECT_NAME} SHARED ${XSPCOMM_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/third_party)
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    VERSION ${PROJECT_VERSION}
    SOVERSION ${XSPCOMM_ABI_VERSION})

add_custom_target(copy_xspcomm_include ALL
    COMMAND ${CMAKE_COMMAND}
        "-DSOURCE_INCLUDE=${PROJECT_SOURCE_DIR}/include/xspcomm"
        "-DGENERATED_INCLUDE=${PROJECT_BINARY_DIR}/generated/xspcomm"
        "-DSTAGE_INCLUDE=${XSPCOMM_HEADER_STAGE}/xspcomm"
        -P ${PROJECT_SOURCE_DIR}/cmake/StageHeaders.cmake
    DEPENDS ${PROJECT_NAME}
    VERBATIM)

foreach(destination IN ITEMS include share/picker/include)
    install(DIRECTORY ${XSPCOMM_HEADER_STAGE}/
            DESTINATION "${XSPCOMM_INSTALL_PREFIX}${destination}")
endforeach()
foreach(destination IN ITEMS lib share/picker/lib)
    install(TARGETS ${PROJECT_NAME}
            DESTINATION "${XSPCOMM_INSTALL_PREFIX}${destination}")
endforeach()
