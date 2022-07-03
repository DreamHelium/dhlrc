add_library(dhutil dh_string_util.c file_util.c)

target_include_directories(dhutil
	INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
	)
