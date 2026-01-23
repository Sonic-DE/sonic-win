/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursorsource.h"
#include "cursor.h"

namespace KWin
{

CursorSource::CursorSource(QObject *parent)
    : QObject(parent)
{
}

bool CursorSource::isBlank() const
{
    return m_size.isEmpty();
}

QSizeF CursorSource::size() const
{
    return m_size;
}

QPointF CursorSource::hotspot() const
{
    return m_hotspot;
}

void CursorSource::frame(std::chrono::milliseconds timestamp)
{
}

ShapeCursorSource::ShapeCursorSource(QObject *parent)
    : CursorSource(parent)
{
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &ShapeCursorSource::selectNextSprite);
}

QImage ShapeCursorSource::image() const
{
    return m_image;
}

QByteArray ShapeCursorSource::shape() const
{
    return m_shape;
}

void ShapeCursorSource::setShape(const QByteArray &shape)
{
    if (m_shape != shape) {
        m_shape = shape;
        refresh();
    }
}

void ShapeCursorSource::setShape(Qt::CursorShape shape)
{
    setShape(CursorShape(shape).name());
}

CursorTheme ShapeCursorSource::theme() const
{
    return m_theme;
}

void ShapeCursorSource::setTheme(const CursorTheme &theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        refresh();
    }
}

void ShapeCursorSource::refresh()
{
    m_currentSprite = -1;
    m_delayTimer.stop();

    m_sprites = m_theme.shape(m_shape);
    if (m_sprites.isEmpty()) {
        const auto alternativeNames = CursorShape::alternatives(m_shape);
        for (const QByteArray &alternativeName : alternativeNames) {
            m_sprites = m_theme.shape(alternativeName);
            if (!m_sprites.isEmpty()) {
                break;
            }
        }
    }

    if (!m_sprites.isEmpty()) {
        selectSprite(0);
    }
}

void ShapeCursorSource::selectNextSprite()
{
    selectSprite((m_currentSprite + 1) % m_sprites.size());
}

void ShapeCursorSource::selectSprite(int index)
{
    if (m_currentSprite == index) {
        return;
    }
    const CursorSprite &sprite = m_sprites[index];
    m_currentSprite = index;
    m_image = sprite.data();
    m_size = QSizeF(m_image.size()) / m_image.devicePixelRatio();
    m_hotspot = sprite.hotspot();
    if (sprite.delay().count() && m_sprites.size() > 1) {
        m_delayTimer.start(sprite.delay());
    }
    Q_EMIT changed();
}

} // namespace KWin

#include "moc_cursorsource.cpp"
