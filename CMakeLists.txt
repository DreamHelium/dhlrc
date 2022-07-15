CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT( dhlrc )

find_package(Gettext REQUIRED)

if(NOT GETTEXT_FOUND)
    add_definitions(-DDH_DISABLE_TRANSLATION)
endif()

add_subdirectory( src )
if(NOT DEFINED DH_DISAABLE_TRANSLATION)
    add_subdirectory( po )
endif()

file(COPY ${dhlrc_SOURCE_DIR}/lang DESTINATION ${dhlrc_BINARY_DIR}/src)
file(COPY ${dhlrc_SOURCE_DIR}/config DESTINATION ${dhlrc_BINARY_DIR}/src)
