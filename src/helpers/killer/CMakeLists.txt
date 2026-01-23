########### next target ###############

set(kwin_killer_helper_x11_SRCS killer.cpp)

add_executable(kwin_killer_helper_x11 ${kwin_killer_helper_x11_SRCS})

target_link_libraries(kwin_killer_helper_x11
    KF6::AuthCore
    KF6::GuiAddons
    KF6::I18n
    KF6::Service
    KF6::WidgetsAddons
    Qt::GuiPrivate
    Qt::Widgets
)

ecm_qt_declare_logging_category(kwin_killer_helper_x11
    HEADER debug.h
    IDENTIFIER KWIN_KILLER
    CATEGORY_NAME org.kde.kwin_x11.killer
)

install(TARGETS kwin_killer_helper_x11 DESTINATION ${KDE_INSTALL_LIBEXECDIR})

configure_file(org.kde.kwin_x11.killer.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin_x11.killer.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.kwin_x11.killer.desktop DESTINATION ${KDE_INSTALL_APPDIR})
