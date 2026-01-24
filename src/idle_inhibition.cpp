/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "idle_inhibition.h"
#include "input.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <algorithm>
#include <functional>

namespace KWin
{

IdleInhibition::IdleInhibition(QObject *parent)
    : QObject(parent)
{
    connect(kwinApp(), &Application::workspaceCreated, this, &IdleInhibition::slotWorkspaceCreated);
}

IdleInhibition::~IdleInhibition() = default;

void IdleInhibition::registerClient(Window *client)
{
    // X11 apps use DPMS or screensaver inhibit APIs directly
}

void IdleInhibition::inhibit(Window *client)
{
    input()->addIdleInhibitor(client);
}

void IdleInhibition::uninhibit(Window *client)
{
    input()->removeIdleInhibitor(client);
}

void IdleInhibition::update(Window *client)
{
}

void IdleInhibition::slotWorkspaceCreated()
{
    connect(workspace(), &Workspace::windowAdded, this, &IdleInhibition::registerClient);
    connect(workspace(), &Workspace::currentDesktopChanged, this, &IdleInhibition::slotDesktopChanged);
}

void IdleInhibition::slotDesktopChanged()
{
}
}

#include "moc_idle_inhibition.cpp"
