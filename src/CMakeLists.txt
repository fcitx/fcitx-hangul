include(FindPkgConfig)

PKG_CHECK_MODULES(LIBHANGUL REQUIRED "libhangul" )

include_directories (${LIBHANGUL_INCLUDE_DIRS})
include_directories (${FCITX_CONFIG_INCLUDE_DIRS})
include_directories (${FCITX_INCLUDE_DIRS})

set( fcitx_hangul_sources
    eim.cpp
   
    config.cpp
    )

add_definitions( -DLOCALEDIR=\"${CMAKE_INSTALL_PREFIX}/share/locale\" )

fcitx_add_addon(fcitx-hangul ${fcitx_hangul_sources} )
target_link_libraries (fcitx-hangul ${LIBHANGUL_LIBRARIES})
target_link_libraries (fcitx-hangul ${FCITX4_FCITX_CONFIG_LIBRARIES})
target_link_libraries (fcitx-hangul ${FCITX4_FCITX_LIBRARIES})
