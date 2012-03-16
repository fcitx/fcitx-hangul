
include_directories (${HANGUL_INCLUDE_DIR}
                     ${FCITX_CONFIG_INCLUDE_DIRS}
                     ${FCITX_INCLUDE_DIRS}
                     ${PROJECT_BINARY_DIR})

set( fcitx_hangul_sources
    ustring.c
    eim.c
    config.c
    keyboard.c
    )

add_definitions( -DLOCALEDIR=\"${CMAKE_INSTALL_PREFIX}/share/locale\" )

fcitx_add_addon(fcitx-hangul ${fcitx_hangul_sources} )
target_link_libraries (fcitx-hangul ${HANGUL_LIBRARIES})
target_link_libraries (fcitx-hangul ${FCITX4_FCITX_CONFIG_LIBRARIES})
target_link_libraries (fcitx-hangul ${FCITX4_FCITX_LIBRARIES})
