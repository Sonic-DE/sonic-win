add_subdirectory(helper)

add_library(KWinIntegrationTestFramework STATIC)

if (Qt6_VERSION VERSION_GREATER_EQUAL "6.8.0")
    set(private_code_option  "PRIVATE_CODE")
endif()
qt6_generate_wayland_protocol_client_sources(KWinIntegrationTestFramework
    ${private_code_option}
    NO_INCLUDE_CORE_ONLY
    FILES
        ${WaylandProtocols_DATADIR}/unstable/input-method/input-method-unstable-v1.xml
)
qt6_generate_wayland_protocol_client_sources(KWinIntegrationTestFramework
    ${private_code_option}
    FILES
        ${CMAKE_SOURCE_DIR}/src/wayland/protocols/wlr-layer-shell-unstable-v1.xml
        ${PROJECT_SOURCE_DIR}/src/wayland/protocols/color-management-v1.xml
        ${WaylandProtocols_DATADIR}/stable/xdg-shell/xdg-shell.xml
        ${WaylandProtocols_DATADIR}/staging/cursor-shape/cursor-shape-v1.xml
        ${WaylandProtocols_DATADIR}/staging/fractional-scale/fractional-scale-v1.xml
        ${WaylandProtocols_DATADIR}/staging/security-context/security-context-v1.xml
        ${WaylandProtocols_DATADIR}/staging/xdg-dialog/xdg-dialog-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
        ${WaylandProtocols_DATADIR}/unstable/tablet/tablet-unstable-v2.xml
        ${WaylandProtocols_DATADIR}/unstable/text-input/text-input-unstable-v3.xml
        ${WaylandProtocols_DATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-device-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-output-management-v2.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/kde-screen-edge-v1.xml
        ${PLASMA_WAYLAND_PROTOCOLS_DIR}/fake-input.xml
)
target_link_libraries(KWinIntegrationTestFramework
    PUBLIC
        Qt::Test
        Qt::Concurrent
        Libdrm::Libdrm
        kwin

    PRIVATE
        # Static plugins
        KWinQpaPlugin
        KF6WindowSystemKWinPlugin
        KF6IdleTimeKWinPlugin
)
if(KWIN_BUILD_X11)
    target_link_libraries(KWinIntegrationTestFramework PRIVATE KWinXwaylandServerModule)
endif()
if(TARGET KF6GlobalAccelKWinPlugin)
    target_link_libraries(KWinIntegrationTestFramework PUBLIC KF6GlobalAccelKWinPlugin)
endif()

if(TARGET PW::KScreenLocker)
    target_link_libraries(KWinIntegrationTestFramework PUBLIC PW::KScreenLocker)
endif()

function(integrationTest)
    set(optionArgs BUILTIN_EFFECTS)
    set(oneValueArgs NAME)
    set(multiValueArgs SRCS LIBS)
    cmake_parse_arguments(ARGS "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if (NOT KWIN_BUILD_X11 AND ARGS_LIBS MATCHES XCB::)
        return()
    endif()
    add_executable(${ARGS_NAME} ${ARGS_SRCS})
    target_link_libraries(${ARGS_NAME} KWinIntegrationTestFramework Qt::Test ${ARGS_LIBS})
    if(${ARGS_BUILTIN_EFFECTS})
        kcoreaddons_target_static_plugins(${ARGS_NAME} NAMESPACE "${KWIN_PLUGINDIR}/effects/plugins")
    endif()
    add_test(NAME kwin-${ARGS_NAME} COMMAND dbus-run-session ${CMAKE_BINARY_DIR}/bin/${ARGS_NAME})
endfunction()

add_subdirectory(fakes)
