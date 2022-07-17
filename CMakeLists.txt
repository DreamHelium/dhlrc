CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT( dhlrc )

set(CMAKE_BUILD_TYPE Release)

find_package(Gettext)

set(DH_ENABLE_TRANSLATION true)

if(NOT GETTEXT_FOUND)
    set(DH_ENABLE_TRANSLATION false)
endif()

if(NOT DH_ENABLE_TRANSLATION)
    add_definitions(-DDH_DISABLE_TRANSLATION)
endif()

#add_definitions(-DDH_DEBUG_IN_IDE)

add_subdirectory( src )
if(DH_ENABLE_TRANSLATION)
    add_subdirectory( po )
endif()

file(COPY ${dhlrc_SOURCE_DIR}/lang DESTINATION ${dhlrc_BINARY_DIR}/src)
file(COPY ${dhlrc_SOURCE_DIR}/config DESTINATION ${dhlrc_BINARY_DIR}/src)
