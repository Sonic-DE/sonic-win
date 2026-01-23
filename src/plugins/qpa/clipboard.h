/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QMimeData>
#include <qpa/qplatformclipboard.h>

namespace KWin::QPA
{

// Stub clipboard for X11-only build
// X11 handles clipboard natively through the X server
class Clipboard : public QObject, public QPlatformClipboard
{
    Q_OBJECT

public:
    Clipboard();

    void initialize();

    QMimeData *mimeData(QClipboard::Mode mode = QClipboard::Clipboard) override;
    void setMimeData(QMimeData *data, QClipboard::Mode mode = QClipboard::Clipboard) override;
    bool supportsMode(QClipboard::Mode mode) const override;
    bool ownsMode(QClipboard::Mode mode) const override;

private:
    QMimeData m_emptyData;
};

}
