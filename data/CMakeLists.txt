add_subdirectory(icons)

########### next target ###############
add_executable(kwin5_update_default_rules_x11 update_default_rules.cpp)
target_link_libraries(kwin5_update_default_rules_x11
    KF6::ConfigCore
    Qt::Core
    Qt::DBus
)
install(TARGETS kwin5_update_default_rules_x11 DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin/)

########### install files ###############

install(FILES org_kde_kwin.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR} RENAME org_kde_kwin_x11.categories)

install(
    FILES 10-sonicde-disable-vrr.rc
    DESTINATION /etc/nvidia/nvidia-application-profiles-rc.d
)

install(
    FILES 10-sonicde-disable-adaptive-sync.conf
    DESTINATION ${KDE_INSTALL_DATADIR}/drirc.d
)
