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

# Windows environment is complicated, we need many commands.
if(NOT WIN32)
  add_custom_target(dhlrc_mkpot ALL
  COMMAND cd ${CMAKE_SOURCE_DIR} && ${CMAKE_SOURCE_DIR}/make_po.sh
  )
endif()

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
  # The catalogue is merged into after make_po.sh has written the pot, so the
  # two targets must not run in parallel.
  if(TARGET dhlrc_mkpot)
    add_dependencies(dhlrc_update_po dhlrc_mkpot)
  endif()
endforeach()

# The playful catalogue: the same messages with cute endings, installed as its
# own domain so the program can switch to it (the "Cute Language Pack"
# setting). It is generated from the plain catalogue by `make_cute.py` while
# building, so it does not have to be kept in git: a file list globbed here
# would miss it on a fresh checkout, hence the language list.
set(CUTE_LANGS zh_CN)
find_program(PYTHON3_EXECUTABLE NAMES python3 python)
foreach(_lang ${CUTE_LANGS})
  set(_cutePo ${CMAKE_CURRENT_SOURCE_DIR}/dhlrc-cute/${_lang}.po)
  set(_moFile ${OUTPUT_LANG_POS}/${_lang}/LC_MESSAGES/${FIRST_POT_NAME}-cute.mo)
  if(NOT EXISTS ${OUTPUT_LANG_POS}/${_lang}/LC_MESSAGES)
    file(MAKE_DIRECTORY ${OUTPUT_LANG_POS}/${_lang}/LC_MESSAGES)
  endif()

  if(PYTHON3_EXECUTABLE)
    add_custom_target( dhlrc_update_cute_po ALL
      COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/make_cute.py
      COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${_moFile} ${_cutePo}
      COMMENT "Generating and compiling the playful translation ${_lang}"
      VERBATIM
      )
    # make_cute.py reads the plain catalogue, which dhlrc_update_po has just
    # merged the pot into; running both at once could read it half-written.
    if(TARGET dhlrc_update_po)
      add_dependencies(dhlrc_update_cute_po dhlrc_update_po)
    endif()
  elseif(EXISTS ${_cutePo})
    add_custom_target( dhlrc_update_cute_po ALL
      COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${_moFile} ${_cutePo}
      COMMENT "Compiling the playful translation ${_lang}"
      VERBATIM
      )
  else()
    message(WARNING
      "python3 was not found and ${_cutePo} does not exist: the playful "
      "translation ${_lang} will not be built")
  endif()
endforeach()
