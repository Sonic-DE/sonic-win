/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QString>

#include <Sonic/Ipc/Server>

// Protocol between sonic-win's dwell clicker and its panel applet. Each message is one Sonic IPC
// frame; the payload holds only the fields noted next to the message.
namespace Sonic::DwellClickerIpc
{

// Raise on any incompatible change.
inline constexpr quint16 ProtocolVersion = 1;
inline constexpr qint64 MaxPayload = 1024;

enum Message : quint32 {
    GetState = 1, // client → server: no fields
    SetButton, // client → server: qint32 button
    SetEnabled, // client → server: bool enabled
    State, // server → client, on connect, on request and on every change: bool enabled, qint32 button
};

// Empty when Sonic IPC finds no safe location.
inline QString socketPath()
{
    return Sonic::Ipc::Server::userSocketPath(QStringLiteral("dwellclicker.sock"));
}

inline QByteArray encodeState(bool enabled, qint32 button)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out << enabled << button;
    return payload;
}

inline bool decodeState(const QByteArray &payload, bool *enabled, qint32 *button)
{
    QDataStream in(payload);
    in >> *enabled >> *button;
    return in.status() == QDataStream::Ok;
}

inline QByteArray encodeButton(qint32 button)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out << button;
    return payload;
}

inline bool decodeButton(const QByteArray &payload, qint32 *button)
{
    QDataStream in(payload);
    in >> *button;
    return in.status() == QDataStream::Ok;
}

inline QByteArray encodeEnabled(bool enabled)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out << enabled;
    return payload;
}

inline bool decodeEnabled(const QByteArray &payload, bool *enabled)
{
    QDataStream in(payload);
    in >> *enabled;
    return in.status() == QDataStream::Ok;
}

}
