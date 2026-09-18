/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "backends/x11/standalone/x11_standalone_refresh_rate.h"

#include <QTest>

using KWin::x11RefreshRateMillihertz;

class X11RefreshRateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void calculatesCommonMode();
    void handlesHighPixelClock();
    void roundsToNearestMillihertz();
    void handlesScanFlags();
    void rejectsInvalidTotals();
};

void X11RefreshRateTest::calculatesCommonMode()
{
    QCOMPARE(x11RefreshRateMillihertz(148500000, 2200, 1125, false, false), 60000U);
}

void X11RefreshRateTest::handlesHighPixelClock()
{
    QCOMPARE(x11RefreshRateMillihertz(2332000000U, 4400, 2208, false, false), 240036U);
}

void X11RefreshRateTest::roundsToNearestMillihertz()
{
    QCOMPARE(x11RefreshRateMillihertz(148350510, 2200, 1125, false, false), 59940U);
}

void X11RefreshRateTest::handlesScanFlags()
{
    QCOMPARE(x11RefreshRateMillihertz(74250000, 2200, 1125, true, false), 60000U);
    QCOMPARE(x11RefreshRateMillihertz(148500000, 2200, 1125, false, true), 30000U);
}

void X11RefreshRateTest::rejectsInvalidTotals()
{
    QCOMPARE(x11RefreshRateMillihertz(148500000, 0, 1125, false, false), 0U);
    QCOMPARE(x11RefreshRateMillihertz(148500000, 2200, 0, false, false), 0U);
}

QTEST_GUILESS_MAIN(X11RefreshRateTest)

#include "test_x11_refresh_rate.moc"
