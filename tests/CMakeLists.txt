# X11-only tests
add_executable(normalhintsbasesizetest normalhintsbasesizetest.cpp)
target_link_libraries(normalhintsbasesizetest XCB::XCB XCB::ICCCM KF6::WindowSystem)

add_executable(x11shadowreader x11shadowreader.cpp)
target_link_libraries(x11shadowreader XCB::XCB Qt::GuiPrivate Qt::Widgets KF6::ConfigCore KF6::WindowSystem)

# Platform-agnostic Qt-based tests
add_executable(cursorhotspottest cursorhotspottest.cpp)
target_link_libraries(cursorhotspottest Qt::Widgets)

add_executable(xdgactivationtest-qt6 xdgactivationtest-qt6.cpp)
target_link_libraries(xdgactivationtest-qt6 Qt6::Widgets)
