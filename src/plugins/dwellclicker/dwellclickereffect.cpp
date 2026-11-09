/*
    SPDX-FileCopyrightText: 2026 Sebastian Sauer <dipesh@gmx.de>
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "plugins/dwellclicker/dwellclickereffect.h"
#include "cursor.h"
#include "dwellclicker.h"
#include "effect/effecthandler.h"
#include "scene/imageitem.h"
#include "scene/itemrenderer.h"
#include "scene/workspacescene.h"
#include "workspace.h"

#include <QPainter>
#include <QPen>

namespace KWin
{

DwellClickerRingItem::DwellClickerRingItem(Item *parentItem)
    : Item(parentItem)
    , m_imageItem(scene()->renderer()->createImageItem(this))
{
}

DwellClickerRingItem::~DwellClickerRingItem() = default;

void DwellClickerRingItem::setProgress(qreal dwellProgress, qreal clickedProgress, const QColor &color)
{
    const int side = Cursors::self()->mouse()->themeSize() + 4;
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QRect rect = image.rect().adjusted(2, 2, -2, -2);
    if (clickedProgress > 0.0) {
        const int shrink = static_cast<int>((rect.width() / 2) * clickedProgress);
        rect.adjust(shrink, shrink, -shrink, -shrink);
    }
    painter.setPen(QPen(color, 4));
    painter.drawArc(rect, 90 * 16, static_cast<int>(-5760 * dwellProgress));
    painter.end();

    m_imageItem->setImage(image);
    m_imageItem->setSize(image.size());
    m_imageItem->setPosition(QPointF(-side / 2.0, -side / 2.0));
    // setImage() doesn't repaint, and the ring often only changes its image.
    m_imageItem->scheduleRepaint(m_imageItem->rect());
}

DwellClickerEffect::DwellClickerEffect()
{
    DwellClicker *clicker = workspace()->dwellClicker();
    connect(clicker, &DwellClicker::progressChanged, this, &DwellClickerEffect::updateProgress);
    connect(clicker, &DwellClicker::stopped, this, &DwellClickerEffect::hide);
}

DwellClickerEffect::~DwellClickerEffect() = default;

bool DwellClickerEffect::isActive() const
{
    return m_ring != nullptr;
}

void DwellClickerEffect::updateProgress(qreal dwellProgress, qreal clickedProgress)
{
    // Reading the position can report a move, which may restart the dwell and hide the ring.
    const quint64 hides = m_hides;
    const QPointF position = effects->cursorPos();
    if (m_hides != hides) {
        return;
    }
    if (!m_ring) {
        m_ring = std::make_unique<DwellClickerRingItem>(effects->scene()->overlayItem());
    }
    m_ring->setPosition(position);
    m_ring->setProgress(dwellProgress, clickedProgress, workspace()->dwellClicker()->settings().color);
}

void DwellClickerEffect::hide()
{
    ++m_hides;
    m_ring.reset();
}

}

#include "moc_dwellclickereffect.cpp"
