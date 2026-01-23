/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Contributor

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_inputbackend.h"

namespace KWin
{

X11CorePointerDevice::X11CorePointerDevice(QObject *parent)
    : InputDevice(parent)
{
}

X11InputBackend::X11InputBackend(QObject *parent)
    : InputBackend(parent)
{
}

X11InputBackend::~X11InputBackend() = default;

void X11InputBackend::initialize()
{
    // X11 always has a core pointer device (the virtual pointer managed by the X server)
    m_pointerDevice = std::make_unique<X11CorePointerDevice>(this);
    Q_EMIT deviceAdded(m_pointerDevice.get());
}

} // namespace KWin

#include "moc_x11_standalone_inputbackend.cpp"
