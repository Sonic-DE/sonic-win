/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "inputmethod.h"

namespace KWin
{

InputMethod::InputMethod()
    : QObject()
{
}

InputMethod::~InputMethod() = default;

void InputMethod::init()
{
    // No-op for X11 only build
}

void InputMethod::setEnabled(bool enable)
{
    // No-op for X11 only build
}
}

#include "moc_inputmethod.cpp"
