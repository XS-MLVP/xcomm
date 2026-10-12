# Build-tree staging must not retain headers removed or renamed in the source.
file(REMOVE_RECURSE "${STAGE_INCLUDE}")
file(MAKE_DIRECTORY "${STAGE_INCLUDE}")
file(COPY "${SOURCE_INCLUDE}/" DESTINATION "${STAGE_INCLUDE}"
     FILES_MATCHING PATTERN "*.h")
file(COPY "${GENERATED_INCLUDE}/xconfig.h" DESTINATION "${STAGE_INCLUDE}")
