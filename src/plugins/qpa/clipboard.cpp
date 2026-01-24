/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "plugins/qpa/clipboard.h"

namespace KWin::QPA
{

Clipboard::Clipboard()
{
}

void Clipboard::initialize()
{
    // clipboard is handled by X11 natively
}

QMimeData *Clipboard::mimeData(QClipboard::Mode mode)
{
    // return empty data
    return &m_emptyData;
}

void Clipboard::setMimeData(QMimeData *data, QClipboard::Mode mode)
{
    // no-op
}

bool Clipboard::supportsMode(QClipboard::Mode mode) const
{
    return mode == QClipboard::Clipboard || mode == QClipboard::Selection;
}

bool Clipboard::ownsMode(QClipboard::Mode mode) const
{
    return false;
}

}
