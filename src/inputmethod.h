/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <kwin_export.h>

class QPlatformInputContext;

namespace KWin
{

class Window;

/**
 * Stub InputMethod class.
 * The full input method functionality requires Wayland protocols.
 */
class KWIN_EXPORT InputMethod : public QObject
{
    Q_OBJECT
public:
    enum ForceModifiers {
        NoForce = 0,
        Force = 1
    };

    InputMethod();
    ~InputMethod() override;

    void init();
    void setEnabled(bool enable);
    bool isEnabled() const
    {
        return false;
    }
    bool isActive() const
    {
        return false;
    }
    void setActive(bool active)
    {
    }
    void hide()
    {
    }
    void show()
    {
    }
    bool isVisible() const
    {
        return false;
    }
    bool isAvailable() const
    {
        return false;
    }
    Window *activeWindow() const
    {
        return nullptr;
    }
    void commitPendingText()
    {
    }
    void forwardModifiers(ForceModifiers force)
    {
    }
    bool activeClientSupportsTextInput() const
    {
        return false;
    }
    bool shouldShowOnActive() const
    {
        return false;
    }
    void forceActivate()
    {
    }
    QPlatformInputContext *internalContext() const
    {
        return nullptr;
    }

Q_SIGNALS:
    void activeClientSupportsTextInputChanged();
    void panelChanged();
    void activeChanged(bool active);
    void enabledChanged(bool enabled);
    void visibleChanged();
    void availableChanged();
};

}
