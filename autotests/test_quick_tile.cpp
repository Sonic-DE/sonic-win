/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "effect/globals.h"

using namespace KWin;

/**
 * @brief Tests for QuickTileMode and QuickTileFlag enums.
 *
 * These tests verify the quick tiling flag combinations work correctly,
 * which is essential for window snapping to screen edges functionality.
 */
class QuickTileTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testQuickTileFlagValues();
    void testQuickTileModeOperations();
    void testQuickTileModeCompositions();
    void testQuickTileModeNegation();
    void testQuickTileModeCorners();
    void testQuickTileModeHorizontalVertical();
};

void QuickTileTest::testQuickTileFlagValues()
{
    // Test individual flag values
    QCOMPARE(static_cast<int>(QuickTileFlag::None), 0);
    QCOMPARE(static_cast<int>(QuickTileFlag::Left), 1);
    QCOMPARE(static_cast<int>(QuickTileFlag::Right), 2);
    QCOMPARE(static_cast<int>(QuickTileFlag::Top), 4);
    QCOMPARE(static_cast<int>(QuickTileFlag::Bottom), 8);
    QCOMPARE(static_cast<int>(QuickTileFlag::Custom), 16);
}

void QuickTileTest::testQuickTileModeOperations()
{
    // Test flag operations
    QuickTileMode mode = QuickTileFlag::Left;
    QVERIFY(mode.testFlag(QuickTileFlag::Left));
    QVERIFY(!mode.testFlag(QuickTileFlag::Right));

    mode |= QuickTileFlag::Top;
    QVERIFY(mode.testFlag(QuickTileFlag::Left));
    QVERIFY(mode.testFlag(QuickTileFlag::Top));
    QVERIFY(!mode.testFlag(QuickTileFlag::Right));
    QVERIFY(!mode.testFlag(QuickTileFlag::Bottom));
}

void QuickTileTest::testQuickTileModeCompositions()
{
    // Test Horizontal and Vertical compositions
    QuickTileMode horizontal = QuickTileFlag::Horizontal;
    QVERIFY(horizontal.testFlag(QuickTileFlag::Left));
    QVERIFY(horizontal.testFlag(QuickTileFlag::Right));
    QVERIFY(!horizontal.testFlag(QuickTileFlag::Top));
    QVERIFY(!horizontal.testFlag(QuickTileFlag::Bottom));

    QuickTileMode vertical = QuickTileFlag::Vertical;
    QVERIFY(!vertical.testFlag(QuickTileFlag::Left));
    QVERIFY(!vertical.testFlag(QuickTileFlag::Right));
    QVERIFY(vertical.testFlag(QuickTileFlag::Top));
    QVERIFY(vertical.testFlag(QuickTileFlag::Bottom));
}

void QuickTileTest::testQuickTileModeNegation()
{
    // Test negation operator
    QuickTileMode notLeft = ~QuickTileFlag::Left;
    QVERIFY(!notLeft.testFlag(QuickTileFlag::Left));

    // Using the negated mode to clear a flag
    QuickTileMode mode = QuickTileMode(QuickTileFlag::Left) | QuickTileFlag::Top;
    mode &= ~QuickTileFlag::Left;
    QVERIFY(!mode.testFlag(QuickTileFlag::Left));
    QVERIFY(mode.testFlag(QuickTileFlag::Top));
}

void QuickTileTest::testQuickTileModeCorners()
{
    // Test corner combinations (common use case for window snapping)
    QuickTileMode topLeft = QuickTileMode(QuickTileFlag::Top) | QuickTileFlag::Left;
    QVERIFY(topLeft.testFlag(QuickTileFlag::Top));
    QVERIFY(topLeft.testFlag(QuickTileFlag::Left));
    QVERIFY(!topLeft.testFlag(QuickTileFlag::Bottom));
    QVERIFY(!topLeft.testFlag(QuickTileFlag::Right));

    QuickTileMode topRight = QuickTileMode(QuickTileFlag::Top) | QuickTileFlag::Right;
    QVERIFY(topRight.testFlag(QuickTileFlag::Top));
    QVERIFY(topRight.testFlag(QuickTileFlag::Right));
    QVERIFY(!topRight.testFlag(QuickTileFlag::Bottom));
    QVERIFY(!topRight.testFlag(QuickTileFlag::Left));

    QuickTileMode bottomLeft = QuickTileMode(QuickTileFlag::Bottom) | QuickTileFlag::Left;
    QVERIFY(bottomLeft.testFlag(QuickTileFlag::Bottom));
    QVERIFY(bottomLeft.testFlag(QuickTileFlag::Left));
    QVERIFY(!bottomLeft.testFlag(QuickTileFlag::Top));
    QVERIFY(!bottomLeft.testFlag(QuickTileFlag::Right));

    QuickTileMode bottomRight = QuickTileMode(QuickTileFlag::Bottom) | QuickTileFlag::Right;
    QVERIFY(bottomRight.testFlag(QuickTileFlag::Bottom));
    QVERIFY(bottomRight.testFlag(QuickTileFlag::Right));
    QVERIFY(!bottomRight.testFlag(QuickTileFlag::Top));
    QVERIFY(!bottomRight.testFlag(QuickTileFlag::Left));
}

void QuickTileTest::testQuickTileModeHorizontalVertical()
{
    // Test that Horizontal == Left | Right
    QuickTileMode horizontal = QuickTileFlag::Horizontal;
    QuickTileMode leftRight = QuickTileMode(QuickTileFlag::Left) | QuickTileFlag::Right;
    QCOMPARE(horizontal, leftRight);

    // Test that Vertical == Top | Bottom
    QuickTileMode vertical = QuickTileFlag::Vertical;
    QuickTileMode topBottom = QuickTileMode(QuickTileFlag::Top) | QuickTileFlag::Bottom;
    QCOMPARE(vertical, topBottom);

    // Test combined maximize-like mode
    QuickTileMode fullScreen = QuickTileMode(QuickTileFlag::Horizontal) | QuickTileFlag::Vertical;
    QVERIFY(fullScreen.testFlag(QuickTileFlag::Left));
    QVERIFY(fullScreen.testFlag(QuickTileFlag::Right));
    QVERIFY(fullScreen.testFlag(QuickTileFlag::Top));
    QVERIFY(fullScreen.testFlag(QuickTileFlag::Bottom));
}

QTEST_GUILESS_MAIN(QuickTileTest)
#include "test_quick_tile.moc"
