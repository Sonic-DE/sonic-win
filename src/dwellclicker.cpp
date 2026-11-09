/*
    SPDX-FileCopyrightText: 2026 Sebastian Sauer <dipesh@gmx.de>
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dwellclicker.h"
#include "DwellClickerIpc.h"
#include "cursor.h"
#include "effect/effecthandler.h"
#include "main.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusMessage>

#include <Sonic/Ipc/Connection>
#include <Sonic/Ipc/Server>
#include <Sonic/Logging/Log>

#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xtest.h>

#include <cstdlib>

// libkwin's component: Sonic Logging components are per binary, and sonic-win's own lives in main_x11.cpp.
SONIC_LOGGING_COMPONENT("org.sonicde.sonicwin.core")

namespace KWin
{

static const QString s_effectName = QStringLiteral("dwellclicker");

namespace
{

class XTestClickOutput : public DwellClickOutput
{
public:
    XTestClickOutput()
        : m_connection(kwinApp() ? kwinApp()->x11Connection() : nullptr)
    {
        if (!m_connection) {
            sonicWarning() << "Dwell clicker: no X connection; clicking is unavailable";
            return;
        }
        const xcb_query_extension_reply_t *extension = xcb_get_extension_data(m_connection, &xcb_test_id);
        if (!extension || !extension->present) {
            sonicWarning() << "Dwell clicker: the X server lacks the XTEST extension; clicking is unavailable";
            m_connection = nullptr;
            return;
        }
        if (xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(m_connection)) {
            if (xcb_keycode_t *codes = xcb_key_symbols_get_keycode(symbols, XK_Escape)) {
                m_escape = codes[0];
                std::free(codes);
            }
            xcb_key_symbols_free(symbols);
        }
        if (m_escape == XCB_NO_SYMBOL) {
            sonicWarning() << "Dwell clicker: no key maps to Escape; a cancelled drag may drop";
        }
    }

    bool isAvailable() const override
    {
        return m_connection;
    }

    void button(quint8 button, bool pressed) override
    {
        send(pressed ? XCB_BUTTON_PRESS : XCB_BUTTON_RELEASE, button);
    }

    void escape() override
    {
        if (m_escape == XCB_NO_SYMBOL) {
            return;
        }
        send(XCB_KEY_PRESS, m_escape);
        send(XCB_KEY_RELEASE, m_escape);
    }

private:
    void send(uint8_t type, uint8_t detail)
    {
        if (!m_connection) {
            return;
        }
        xcb_test_fake_input(m_connection, type, detail, XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
        xcb_flush(m_connection);
    }

    xcb_connection_t *m_connection;
    xcb_keycode_t m_escape = XCB_NO_SYMBOL;
};

}

DwellClicker::DwellClicker(std::unique_ptr<DwellClickOutput> output, QObject *parent)
    : QObject(parent)
    , m_output(output ? std::move(output) : std::make_unique<XTestClickOutput>())
{
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, [this] {
        if (m_settings.dwellTime == 0) {
            stop();
            onClicked();
            return;
        }
        m_dwellAnimation.start();
    });

    m_dwellAnimation.setStartValue(0.0);
    m_dwellAnimation.setEndValue(1.0);
    connect(&m_dwellAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        Q_EMIT progressChanged(value.toDouble(), 0.0);
    });
    connect(&m_dwellAnimation, &QVariantAnimation::finished, this, [this] {
        m_clickedAnimation.start();
    });

    m_clickedAnimation.setStartValue(0.0);
    m_clickedAnimation.setEndValue(1.0);
    m_clickedAnimation.setDuration(100);
    connect(&m_clickedAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        Q_EMIT progressChanged(1.0, value.toDouble());
    });
    connect(&m_clickedAnimation, &QVariantAnimation::finished, this, [this] {
        stop();
        onClicked();
    });

    applySettings(Settings());
}

DwellClicker::~DwellClicker()
{
    cancelDrag();
}

DwellClicker::Settings DwellClicker::readSettings(const KSharedConfigPtr &config)
{
    Settings settings;
    settings.enabled = config->group(QStringLiteral("Plugins")).readEntry("dwellclickerEnabled", false);

    const KConfigGroup group = config->group(QStringLiteral("Effect-dwellclicker"));
    settings.delayTime = group.readEntry("DelayTime", settings.delayTime);
    settings.dwellTime = group.readEntry("DwellTime", settings.dwellTime);
    settings.motionThreshold = group.readEntry("MotionThreshold", settings.motionThreshold);
    settings.defaultButton = group.readEntry("DefaultButton", settings.defaultButton);
    settings.buttonLocked = group.readEntry("ButtonLocked", settings.buttonLocked);
    settings.color = group.readEntry("DwellColor", settings.color);

    if (settings.delayTime < 0 || settings.dwellTime < 0 || settings.motionThreshold < 0) {
        sonicWarning() << "Dwell clicker: negative times or threshold in kwinrc; using the defaults";
        const Settings defaults;
        settings.delayTime = settings.delayTime < 0 ? defaults.delayTime : settings.delayTime;
        settings.dwellTime = settings.dwellTime < 0 ? defaults.dwellTime : settings.dwellTime;
        settings.motionThreshold = settings.motionThreshold < 0 ? defaults.motionThreshold : settings.motionThreshold;
    }
    if (!isValidButton(settings.defaultButton)) {
        sonicWarning() << "Dwell clicker: unknown DefaultButton" << settings.defaultButton << "; using left click";
        settings.defaultButton = LeftClick;
    }
    return settings;
}

bool DwellClicker::isValidButton(int button)
{
    return button == None || (button >= LeftClick && button <= RightClick);
}

void DwellClicker::attach()
{
    m_attached = true;
    reconfigure();

    if (!m_output->isAvailable()) {
        sonicWarning() << "Dwell clicker: no way to send clicks; dwell clicking stays inactive";
    }

    connect(Cursors::self()->mouse(), &Cursor::posChanged, this, &DwellClicker::handleMotion);
    connect(this, &DwellClicker::enabledChanged, this, [this] {
        updateEffect();
        broadcastState();
    });
    connect(this, &DwellClicker::buttonChanged, this, &DwellClicker::broadcastState);

    const QString path = Sonic::DwellClickerIpc::socketPath();
    if (path.isEmpty()) {
        sonicWarning() << "Dwell clicker: no safe location for the applet socket; the applet can't connect";
        return;
    }
    m_server = new Sonic::Ipc::Server(this);
    if (!m_server->listen(path, Sonic::Ipc::Server::Access::User, Sonic::DwellClickerIpc::ProtocolVersion, Sonic::DwellClickerIpc::MaxPayload)) {
        sonicWarning() << "Dwell clicker: can't listen on" << path << ":" << m_server->errorString();
        return;
    }
    connect(m_server, &Sonic::Ipc::Server::newConnection, this, [this](Sonic::Ipc::Connection *connection) {
        connect(connection, &Sonic::Ipc::Connection::frameReceived, this, [this, connection](quint32 type, const QByteArray &payload) {
            handleFrame(connection, type, payload);
        });
        sendState(connection);
    });
}

void DwellClicker::reconfigure()
{
    applySettings(readSettings(kwinApp()->config()));
}

void DwellClicker::applySettings(const Settings &settings)
{
    m_settings = settings;
    m_delayTimer.setInterval(m_settings.delayTime);
    m_dwellAnimation.setDuration(m_settings.dwellTime);
    // Upstream sets the defaults on every reconfigure, ending a drag.
    setButton(m_settings.defaultButton);
    setEnabled(m_settings.enabled);
}

const DwellClicker::Settings &DwellClicker::settings() const
{
    return m_settings;
}

bool DwellClicker::isEnabled() const
{
    return m_enabled;
}

void DwellClicker::setEnabled(bool enabled)
{
    m_settings.enabled = enabled;
    if (m_enabled == enabled) {
        return;
    }
    if (!enabled) {
        cancelDrag();
        stop();
    }
    m_enabled = enabled;
    m_origin.reset();
    sonicDebug() << "Dwell clicker" << (enabled ? "enabled" : "disabled");
    Q_EMIT enabledChanged(enabled);
}

void DwellClicker::requestEnabled(bool enabled)
{
    if (m_attached) {
        KConfigGroup plugins = kwinApp()->config()->group(QStringLiteral("Plugins"));
        plugins.writeEntry("dwellclickerEnabled", enabled);
        plugins.sync();
    }
    const bool changed = m_enabled != enabled;
    setEnabled(enabled);
    if (changed && m_attached) {
        showOsd();
    }
}

void DwellClicker::toggle()
{
    requestEnabled(!m_enabled);
}

int DwellClicker::button() const
{
    return m_button;
}

void DwellClicker::setButton(int button)
{
    if (!isValidButton(button)) {
        sonicWarning() << "Dwell clicker: ignoring unknown button" << button;
        return;
    }
    if (m_button == button && !m_dragging) {
        return;
    }
    cancelDrag();
    stop();
    m_origin.reset();
    if (m_button == button) {
        return;
    }
    m_button = button;
    sonicDebug() << "Dwell clicker: next click is" << static_cast<Button>(button);
    Q_EMIT buttonChanged(button);
}

bool DwellClicker::isDragging() const
{
    return m_dragging;
}

bool DwellClicker::isRunning() const
{
    return m_running;
}

void DwellClicker::handleMotion(const QPointF &position)
{
    if (!m_enabled || m_button == None || !m_output->isAvailable()) {
        return;
    }
    if (!m_origin) {
        // Motion past the threshold is needed after enabling, a click or a button change too.
        m_origin = position;
        return;
    }
    const int threshold = m_settings.motionThreshold;
    const bool aboveThreshold = position.x() >= m_origin->x() + threshold
        || position.x() <= m_origin->x() - threshold
        || position.y() >= m_origin->y() + threshold
        || position.y() <= m_origin->y() - threshold;
    if (!aboveThreshold) {
        return;
    }
    m_origin = position;
    start();
}

void DwellClicker::start()
{
    stop();
    m_running = true;
    m_delayTimer.start();
}

void DwellClicker::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_delayTimer.stop();
    m_dwellAnimation.stop();
    m_clickedAnimation.stop();
    Q_EMIT stopped();
}

void DwellClicker::click(quint8 button)
{
    m_output->button(button, true);
    m_output->button(button, false);
}

void DwellClicker::onClicked()
{
    switch (m_button) {
    case LeftClick:
        click(1);
        break;
    case DoubleClick:
        click(1);
        click(1);
        break;
    case DragClick:
        m_output->button(1, !m_dragging);
        m_dragging = !m_dragging;
        break;
    case MiddleClick:
        click(2);
        break;
    case RightClick:
        click(3);
        break;
    default:
        break;
    }
    if (m_button == DragClick) {
        sonicDebug() << "Dwell clicker: sent drag" << (m_dragging ? "press" : "release");
    } else {
        sonicDebug() << "Dwell clicker: sent" << static_cast<Button>(m_button);
    }

    m_origin.reset();
    if (!m_settings.buttonLocked && !m_dragging) {
        setButton(m_settings.defaultButton);
    }
}

void DwellClicker::cancelDrag()
{
    if (!m_dragging) {
        return;
    }
    m_dragging = false;
    // Escape first so the drag is cancelled rather than dropped by the release.
    m_output->escape();
    m_output->button(1, false);
    sonicDebug() << "Dwell clicker: drag cancelled";
}

void DwellClicker::updateEffect()
{
    if (!effects) {
        return;
    }
    const bool loaded = effects->isEffectLoaded(s_effectName);
    if (m_enabled && !loaded) {
        effects->loadEffect(s_effectName);
    } else if (!m_enabled && loaded) {
        effects->unloadEffect(s_effectName);
    }
}

void DwellClicker::showOsd()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.sonicde.sonicshell"),
                                                          QStringLiteral("/org/sonicde/osdService"),
                                                          QStringLiteral("org.sonicde.osdService"),
                                                          QStringLiteral("showText"));
    message << QStringLiteral("input-mouse-click-left")
            << (m_enabled ? i18n("Dwell clicking on") : i18n("Dwell clicking off"));
    if (!QDBusConnection::sessionBus().send(message)) {
        sonicWarning() << "Dwell clicker: can't send the on-screen message:" << QDBusConnection::sessionBus().lastError().message();
    }
}

void DwellClicker::sendState(Sonic::Ipc::Connection *connection)
{
    connection->send(Sonic::DwellClickerIpc::State, Sonic::DwellClickerIpc::encodeState(m_enabled, m_button));
}

void DwellClicker::broadcastState()
{
    if (m_server) {
        m_server->broadcast(Sonic::DwellClickerIpc::State, Sonic::DwellClickerIpc::encodeState(m_enabled, m_button));
    }
}

void DwellClicker::handleFrame(Sonic::Ipc::Connection *connection, quint32 type, const QByteArray &payload)
{
    switch (type) {
    case Sonic::DwellClickerIpc::GetState:
        sendState(connection);
        break;
    case Sonic::DwellClickerIpc::SetButton: {
        qint32 button = None;
        if (!Sonic::DwellClickerIpc::decodeButton(payload, &button) || !isValidButton(button)) {
            sonicWarning() << "Dwell clicker: bad SetButton message from the applet";
            return;
        }
        setButton(button);
        break;
    }
    case Sonic::DwellClickerIpc::SetEnabled: {
        bool enabled = false;
        if (!Sonic::DwellClickerIpc::decodeEnabled(payload, &enabled)) {
            sonicWarning() << "Dwell clicker: bad SetEnabled message from the applet";
            return;
        }
        requestEnabled(enabled);
        break;
    }
    default:
        sonicWarning() << "Dwell clicker: unknown message" << type << "from the applet";
        break;
    }
}

}

#include "moc_dwellclicker.cpp"
