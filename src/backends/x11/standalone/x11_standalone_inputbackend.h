/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 Contributor

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/inputbackend.h"
#include "core/inputdevice.h"

namespace KWin
{

class X11CorePointerDevice : public InputDevice
{
    Q_OBJECT
public:
    explicit X11CorePointerDevice(QObject *parent = nullptr);

    QString name() const override
    {
        return QStringLiteral("X11 Core Pointer");
    }
    bool isEnabled() const override
    {
        return true;
    }
    void setEnabled(bool enabled) override
    {
    }
    bool isKeyboard() const override
    {
        return false;
    }
    bool isPointer() const override
    {
        return true;
    }
    bool isTouchpad() const override
    {
        return false;
    }
    bool isTouch() const override
    {
        return false;
    }
    bool isTabletTool() const override
    {
        return false;
    }
    bool isTabletPad() const override
    {
        return false;
    }
    bool isTabletModeSwitch() const override
    {
        return false;
    }
    bool isLidSwitch() const override
    {
        return false;
    }
};

class X11InputBackend : public InputBackend
{
    Q_OBJECT
public:
    explicit X11InputBackend(QObject *parent = nullptr);
    ~X11InputBackend() override;

    void initialize() override;

private:
    std::unique_ptr<X11CorePointerDevice> m_pointerDevice;
};

} // namespace KWin
