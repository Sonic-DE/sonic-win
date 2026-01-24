/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/output.h"
#include "effect/globals.h"
#include "scene/item.h"

namespace KWin
{

class DragAndDropIcon;

// Stub class - DnD icons are Wayland-specific
class DragAndDropIconItem : public Item
{
    Q_OBJECT

public:
    explicit DragAndDropIconItem(DragAndDropIcon *icon, Item *parent = nullptr);
    ~DragAndDropIconItem() override;

    void setOutput(Output *output);

private:
    Output *m_output = nullptr;
};

} // namespace KWin
