/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace KWin
{

constexpr uint32_t x11RefreshRateMillihertz(uint32_t dotClock,
                                            uint16_t horizontalTotal,
                                            uint16_t verticalTotal,
                                            bool interlaced,
                                            bool doubleScan)
{
    if (horizontalTotal == 0 || verticalTotal == 0) {
        return 0;
    }

    // Keep the complete calculation in integer millihertz. This avoids both
    // overflowing high pixel clocks and truncating a stable mode just below
    // the rate that RandR clients request, which can cause configuration loops.
    const uint64_t adjustedDotClock = uint64_t(dotClock) * (interlaced ? 2 : 1);
    const uint64_t adjustedVerticalTotal = uint64_t(verticalTotal) * (doubleScan ? 2 : 1);
    const uint64_t totalPixels = uint64_t(horizontalTotal) * adjustedVerticalTotal;
    const uint64_t millihertz = (adjustedDotClock * 1000 + totalPixels / 2) / totalPixels;

    return std::min<uint64_t>(millihertz, std::numeric_limits<uint32_t>::max());
}

}
