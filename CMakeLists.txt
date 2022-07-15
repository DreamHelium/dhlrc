CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT( dhlrc )

find_package(Gettext REQUIRED)

include_directories(/usr/lib64/gcc/x86_64-suse-linux/12/include)

add_subdirectory( src )
add_subdirectory( po )

file(COPY ${dhlrc_SOURCE_DIR}/lang DESTINATION ${dhlrc_BINARY_DIR}/src)
file(COPY ${dhlrc_SOURCE_DIR}/config DESTINATION ${dhlrc_BINARY_DIR}/src)
