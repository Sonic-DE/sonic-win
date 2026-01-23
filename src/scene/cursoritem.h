/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWin
{

class ImageItem;

class KWIN_EXPORT CursorItem : public Item
{
    Q_OBJECT

public:
    explicit CursorItem(Item *parent = nullptr);
    ~CursorItem() override;

private:
    void refresh();
    void setImage(const QImage &image, const QPointF &hotspot);

    std::unique_ptr<ImageItem> m_imageItem;
};

} // namespace KWin
