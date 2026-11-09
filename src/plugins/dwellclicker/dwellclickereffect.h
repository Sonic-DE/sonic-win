/*
    SPDX-FileCopyrightText: 2026 Sebastian Sauer <dipesh@gmx.de>
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "effect/effect.h"
#include "scene/item.h"

#include <memory>

namespace KWin
{

class ImageItem;

class DwellClickerRingItem : public Item
{
    Q_OBJECT

public:
    explicit DwellClickerRingItem(Item *parentItem);
    ~DwellClickerRingItem() override;

    void setProgress(qreal dwellProgress, qreal clickedProgress, const QColor &color);

private:
    std::unique_ptr<ImageItem> m_imageItem;
};

// Draws the dwell clicker's progress ring; timing and clicks live in KWin::DwellClicker.
class DwellClickerEffect : public Effect
{
    Q_OBJECT

public:
    DwellClickerEffect();
    ~DwellClickerEffect() override;

    bool isActive() const override;

private:
    void updateProgress(qreal dwellProgress, qreal clickedProgress);
    void hide();

    std::unique_ptr<DwellClickerRingItem> m_ring;
    quint64 m_hides = 0;
};

}
