/*
    SPDX-FileCopyrightText: 2026 Sebastian Sauer <dipesh@gmx.de>
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "kwin_export.h"

#include <KSharedConfig>

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QVariantAnimation>

#include <memory>
#include <optional>

namespace Sonic::Ipc
{
class Connection;
class Server;
}

namespace KWin
{

// Sends the synthetic input; X buttons 1 left, 2 middle, 3 right.
class KWIN_EXPORT DwellClickOutput
{
public:
    virtual ~DwellClickOutput() = default;
    virtual bool isAvailable() const = 0;
    virtual void button(quint8 button, bool pressed) = 0;
    virtual void escape() = 0;
};

// Clicks when the pointer rests. Works without compositing; the dwellclicker effect only draws the ring.
class KWIN_EXPORT DwellClicker : public QObject
{
    Q_OBJECT

public:
    enum Button {
        None = 0,
        LeftClick = 10,
        DoubleClick = 11,
        DragClick = 12,
        MiddleClick = 13,
        RightClick = 14,
    };
    Q_ENUM(Button)

    struct Settings
    {
        bool enabled = false;
        int delayTime = 150;
        int dwellTime = 500;
        int motionThreshold = 5;
        int defaultButton = LeftClick;
        bool buttonLocked = false;
        QColor color = QColor(0xff, 0x00, 0x00, 0xa0);
    };

    // Without an output, clicks go through XTEST on sonic-win's X connection.
    explicit DwellClicker(std::unique_ptr<DwellClickOutput> output = nullptr, QObject *parent = nullptr);
    ~DwellClicker() override;

    static Settings readSettings(const KSharedConfigPtr &config);
    static bool isValidButton(int button);

    // Connects to the cursor, the shell's OSD and the applet socket; only sonic-win's Workspace calls it.
    void attach();

    void applySettings(const Settings &settings);
    const Settings &settings() const;

    bool isEnabled() const;
    void setEnabled(bool enabled);
    // Shortcut and applet path: also stores the choice and shows the on-screen message.
    void requestEnabled(bool enabled);
    int button() const;
    void setButton(int button);
    bool isDragging() const;
    bool isRunning() const;

    void handleMotion(const QPointF &position);

public Q_SLOTS:
    void toggle();
    void reconfigure();

Q_SIGNALS:
    void progressChanged(qreal dwellProgress, qreal clickedProgress);
    void stopped();
    void enabledChanged(bool enabled);
    void buttonChanged(int button);

private:
    void start();
    void stop();
    void onClicked();
    void click(quint8 button);
    void cancelDrag();
    void updateEffect();
    void showOsd();
    void sendState(Sonic::Ipc::Connection *connection);
    void broadcastState();
    void handleFrame(Sonic::Ipc::Connection *connection, quint32 type, const QByteArray &payload);

    std::unique_ptr<DwellClickOutput> m_output;
    Settings m_settings;
    QTimer m_delayTimer;
    QVariantAnimation m_dwellAnimation;
    QVariantAnimation m_clickedAnimation;
    std::optional<QPointF> m_origin;
    int m_button = LeftClick;
    bool m_enabled = false;
    bool m_running = false;
    bool m_dragging = false;
    bool m_attached = false;
    Sonic::Ipc::Server *m_server = nullptr;
};

}
