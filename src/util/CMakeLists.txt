add_library(dhutil SHARED dh_string_util.c file_util.c)

target_include_directories(dhutil
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)

set(DHUTIL_POT_FILE_DEPENDS src/util/dh_string_util.c PARENT_SCOPE)
