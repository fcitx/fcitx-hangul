fcitx_add_addon_conf_file(fcitx-hangul.conf)
fcitx_add_configdesc_file(fcitx-hangul.desc)
fcitx_add_inputmethod_conf_file(hangul.conf)

install(FILES symbol.txt DESTINATION ${FCITX4_PREFIX}/share/fcitx/hangul/)
install(FILES hangul.png DESTINATION ${FCITX4_PREFIX}/share/fcitx/hangul/)
install(FILES hangul.png RENAME fcitx-hangul.png DESTINATION share/icons/hicolor/64x64/apps/)