set( FIRST_POT_NAME "dhlrc" )

set( FIRST_POT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FIRST_POT_NAME}/${FIRST_POT_NAME}.pot )

# find_program(XGETTEXT_EXECUTABLE xgettext)

# # You could just do it by yourself
# #find_program(MSGINIT_EXECUTABLE msginit)

# set( _abs_DHLRC_MAIN_POT_FILE_DEPENDS )
# foreach(_currentDepends ${DHLRC_MAIN_POT_FILE_DEPENDS})
#     set(_abs_DHLRC_MAIN_POT_FILE_DEPENDS ${_abs_DHLRC_MAIN_POT_FILE_DEPENDS} ${dhlrc_SOURCE_DIR}/${_currentDepends})
# endforeach()

# if(XGETTEXT_EXECUTABLE)
# 	set( XGETTEXT_FIRST_OPTIONS -k_ -kN_ --default-domain=${FIRST_POT_NAME} --package-name=${FIRST_POT_NAME} )
#         add_custom_target( dhlrc_mkpot ALL
#             COMMAND "${XGETTEXT_EXECUTABLE}" ${XGETTEXT_FIRST_OPTIONS} --output=${FIRST_POT_FILE} ${DHLRC_MAIN_POT_FILE_DEPENDS}
#             WORKING_DIRECTORY ${dhlrc_SOURCE_DIR}
#             DEPENDS ${_abs_DHLRC_MAIN_POT_FILE_DEPENDS}
#             COMMENT "Extract first translatable messages to ${FIRST_POT_FILE}"
#         )
# endif()

# message(STATUS ${CMAKE_CURRENT_SOURCE_DIR}/${FIRST_POT_NAME})

add_custom_target(dhlrc_mkpot ALL
COMMAND cd ${CMAKE_SOURCE_DIR} && ${CMAKE_SOURCE_DIR}/make_po.sh
)

set(FIRST_PO_POS ${CMAKE_CURRENT_SOURCE_DIR}/${FIRST_POT_NAME})

file(GLOB FIRST_PO_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "dhlrc/*.po")
set(OUTPUT_LANG_POS ${CMAKE_BINARY_DIR}/locale)

message(STATUS ${FIRST_PO_FILES})

if(NOT EXISTS "${OUTPUT_LANG_POS}")
    file(MAKE_DIRECTORY ${OUTPUT_LANG_POS})
endif()

foreach( _currentPoFile_F ${FIRST_PO_FILES} )
    get_filename_component(_lang ${_currentPoFile_F} NAME_WE)
    message(STATUS ${_lang})
    set( _moFile ${OUTPUT_LANG_POS}/${_lang}/LC_MESSAGES/${FIRST_POT_NAME}.mo )
    if(NOT EXISTS ${OUTPUT_LANG_POS}/${_lang}/LC_MESSAGES)
        file(MAKE_DIRECTORY ${OUTPUT_LANG_POS}/${_lang}/LC_MESSAGES)
    endif()

    add_custom_target( dhlrc_update_po ALL
    COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE} --no-fuzzy-matching -U
        ${CMAKE_CURRENT_SOURCE_DIR}/${_currentPoFile_F} ${FIRST_POT_FILE}
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${_moFile} ${CMAKE_CURRENT_SOURCE_DIR}/${_currentPoFile_F}
        DEPENDS ${FIRST_POT_FILE}
        COMMENT "Updating translation ${_currentPoFile_F} of ${FIRST_POT_NAME}"
    )
endforeach()

