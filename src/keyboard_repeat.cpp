/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_repeat.h"
#include "input_event.h"
#include "keyboard_input.h"
#include "main.h"
#include "xkb.h"

#include <KConfigGroup>
#include <QTimer>

namespace KWin
{

KeyboardRepeat::KeyboardRepeat(Xkb *xkb)
    : QObject()
    , m_timer(new QTimer(this))
    , m_xkb(xkb)
{
    connect(m_timer, &QTimer::timeout, this, &KeyboardRepeat::handleKeyRepeat);
    loadConfig();
}

void KeyboardRepeat::loadConfig()
{
    const auto config = kwinApp()->inputConfig()->group(QStringLiteral("Keyboard"));
    m_delay = config.readEntry("RepeatDelay", 600);
    m_rate = std::ceil(config.readEntry("RepeatRate", 25.0));
    const QString repeatMode = config.readEntry("KeyRepeat", "repeat");
    m_enabled = repeatMode == QLatin1String("accent") || repeatMode == QLatin1String("repeat");
}

KeyboardRepeat::~KeyboardRepeat() = default;

void KeyboardRepeat::handleKeyRepeat()
{
    if (m_rate != 0) {
        m_timer->setInterval(1000 / m_rate);
    }
    // TODO: better time
    Q_EMIT keyRepeat(m_key, m_time);
}

void KeyboardRepeat::keyboardKey(KeyboardKeyEvent *event)
{
    if (event->state == KeyboardKeyState::Repeated) {
        return;
    }
    const quint32 key = event->nativeScanCode;
    if (event->state == KeyboardKeyState::Pressed) {
        if (m_enabled && m_xkb->shouldKeyRepeat(key) && m_delay != 0) {
            m_timer->setInterval(m_delay);
            m_key = key;
            m_time = event->timestamp;
            m_timer->start();
        }
    } else if (event->state == KeyboardKeyState::Released) {
        if (key == m_key) {
            m_timer->stop();
        }
    }
}

}

#include "moc_keyboard_repeat.cpp"
