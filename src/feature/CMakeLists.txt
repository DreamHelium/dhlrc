project(dhlrc_feature LANGUAGES C)

set(SOURCES conv_feature.c mcdata_feature.c recipe_feature.c unzip_feature.c dh_module.c)
set(HEADERS conv_feature.h mcdata_feature.h recipe_feature.h unzip_feature.h dh_module.h)

add_library(dhlrc_feature SHARED ${SOURCES} ${HEADERS})
target_link_libraries(dhlrc_feature PUBLIC dhutil_string dhutil_file ${gm_LIBRARIES})
target_include_directories(dhlrc_feature PUBLIC ${gm_INCLUDE_DIRS} ../dhutil)
