add_library(dhutil SHARED dh_string_util.c file_util.c)

message(${GIO_INCLUDE_DIRS})

target_include_directories(dhutil
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        "${GLIB_INCLUDE_DIRS}"
        "${GIO_INCLUDE_DIRS}"
)

target_link_libraries(dhutil PUBLIC cjson ${GLIB_LIBRARIES} ${GIO_LIBRARIES})

set(DHUTIL_POT_FILE_DEPENDS src/util/dh_string_util.c PARENT_SCOPE)
