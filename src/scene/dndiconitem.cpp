/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/dndiconitem.h"

namespace KWin
{

DragAndDropIconItem::DragAndDropIconItem(DragAndDropIcon *icon, Item *parent)
    : Item(parent)
{
}

DragAndDropIconItem::~DragAndDropIconItem()
{
}

void DragAndDropIconItem::setOutput(Output *output)
{
    m_output = output;
}

} // namespace KWin
