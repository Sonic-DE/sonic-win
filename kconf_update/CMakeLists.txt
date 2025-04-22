# SPDX-FileCopyrightText: 2023 Niccolò Venerandi <niccolo.venerandi@kde.org>
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

add_executable(kwin-6.0-delete-desktop-switching-shortcuts-x11)
target_sources(kwin-6.0-delete-desktop-switching-shortcuts-x11 PRIVATE kwin-6.0-delete-desktop-switching-shortcuts-x11.cpp)
target_link_libraries(kwin-6.0-delete-desktop-switching-shortcuts-x11 PRIVATE KF6::GlobalAccel)
install(TARGETS kwin-6.0-delete-desktop-switching-shortcuts-x11 DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

add_executable(kwin-6.0-reset-active-mouse-screen-x11)
target_sources(kwin-6.0-reset-active-mouse-screen-x11 PRIVATE kwin-6.0-reset-active-mouse-screen-x11.cpp)
target_link_libraries(kwin-6.0-reset-active-mouse-screen-x11 PRIVATE KF6::ConfigCore)
install(TARGETS kwin-6.0-reset-active-mouse-screen-x11 DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

add_executable(kwin-6.0-remove-breeze-tabbox-default-x11)
target_sources(kwin-6.0-remove-breeze-tabbox-default-x11 PRIVATE kwin-6.0-remove-breeze-tabbox-default-x11.cpp)
target_link_libraries(kwin-6.0-remove-breeze-tabbox-default-x11 PRIVATE KF6::ConfigCore)
install(TARGETS kwin-6.0-remove-breeze-tabbox-default-x11 DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

add_executable(kwin-6.1-remove-gridview-expose-shortcuts-x11)
target_sources(kwin-6.1-remove-gridview-expose-shortcuts-x11 PRIVATE kwin-6.1-remove-gridview-expose-shortcuts-x11.cpp)
target_link_libraries(kwin-6.1-remove-gridview-expose-shortcuts-x11 PRIVATE KF6::GlobalAccel)
install(TARGETS kwin-6.1-remove-gridview-expose-shortcuts-x11 DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

add_executable(kwin-6.5-showpaint-changes-x11)
target_sources(kwin-6.5-showpaint-changes-x11 PRIVATE kwin-6.5-showpaint-changes-x11.cpp)
target_link_libraries(kwin-6.5-showpaint-changes-x11 PRIVATE KF6::ConfigCore KF6::GlobalAccel)
install(TARGETS kwin-6.5-showpaint-changes-x11 DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

install(FILES kwin-x11.upd
        DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
