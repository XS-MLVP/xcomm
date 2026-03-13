if (NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "DEST_DIR is required")
endif ()

if (NOT DEFINED TARGET_FILE)
    message(FATAL_ERROR "TARGET_FILE is required")
endif ()

if (NOT DEFINED TARGET_FILE_NAME)
    message(FATAL_ERROR "TARGET_FILE_NAME is required")
endif ()

if (NOT DEFINED TARGET_LINKER_FILE_NAME)
    message(FATAL_ERROR "TARGET_LINKER_FILE_NAME is required")
endif ()

if (NOT DEFINED PLATFORM_NAME)
    message(FATAL_ERROR "PLATFORM_NAME is required")
endif ()

if (NOT DEFINED PROJECT_VERSION)
    message(FATAL_ERROR "PROJECT_VERSION is required")
endif ()

file(MAKE_DIRECTORY "${DEST_DIR}")

if (PLATFORM_NAME MATCHES "Darwin")
    set(stage_name "${TARGET_FILE_NAME}")
    set(legacy_stage_name "${TARGET_LINKER_FILE_NAME}.${PROJECT_VERSION}")
else ()
    set(stage_name "${TARGET_LINKER_FILE_NAME}.${PROJECT_VERSION}")
endif ()

if (DEFINED legacy_stage_name AND
    NOT "${legacy_stage_name}" STREQUAL "${stage_name}" AND
    NOT "${legacy_stage_name}" STREQUAL "${TARGET_LINKER_FILE_NAME}")
    file(REMOVE "${DEST_DIR}/${legacy_stage_name}")
endif ()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy "${TARGET_FILE}" "${DEST_DIR}/${stage_name}"
    COMMAND_ERROR_IS_FATAL ANY
)

if (NOT "${TARGET_LINKER_FILE_NAME}" STREQUAL "${stage_name}")
    file(REMOVE "${DEST_DIR}/${TARGET_LINKER_FILE_NAME}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "${stage_name}" "${TARGET_LINKER_FILE_NAME}"
        WORKING_DIRECTORY "${DEST_DIR}"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif ()
