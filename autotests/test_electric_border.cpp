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
 * @brief Tests for ElectricBorder enum and MaximizeMode.
 *
 * These tests verify the screen edge and maximize mode enums work correctly,
 * which is essential for screen edge triggers and window maximization.
 */
class ElectricBorderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testElectricBorderValues();
    void testElectricBorderCount();
    void testElectricBorderActionValues();
    void testMaximizeModeValues();
    void testMaximizeModeOperations();
    void testSwipeDirectionValues();
    void testPinchDirectionValues();
};

void ElectricBorderTest::testElectricBorderValues()
{
    // Test that all borders have distinct values
    QVERIFY(ElectricTop != ElectricTopRight);
    QVERIFY(ElectricTopRight != ElectricRight);
    QVERIFY(ElectricRight != ElectricBottomRight);
    QVERIFY(ElectricBottomRight != ElectricBottom);
    QVERIFY(ElectricBottom != ElectricBottomLeft);
    QVERIFY(ElectricBottomLeft != ElectricLeft);
    QVERIFY(ElectricLeft != ElectricTopLeft);
    QVERIFY(ElectricTopLeft != ElectricTop);

    // ElectricNone should be distinct from all borders
    QVERIFY(ElectricNone != ElectricTop);
    QVERIFY(ElectricNone != ElectricBottom);
    QVERIFY(ElectricNone != ElectricLeft);
    QVERIFY(ElectricNone != ElectricRight);
}

void ElectricBorderTest::testElectricBorderCount()
{
    // Test that ELECTRIC_COUNT is correct (8 borders)
    QCOMPARE(static_cast<int>(ELECTRIC_COUNT), 8);

    // Verify all borders are less than count
    QVERIFY(static_cast<int>(ElectricTop) < ELECTRIC_COUNT);
    QVERIFY(static_cast<int>(ElectricTopRight) < ELECTRIC_COUNT);
    QVERIFY(static_cast<int>(ElectricRight) < ELECTRIC_COUNT);
    QVERIFY(static_cast<int>(ElectricBottomRight) < ELECTRIC_COUNT);
    QVERIFY(static_cast<int>(ElectricBottom) < ELECTRIC_COUNT);
    QVERIFY(static_cast<int>(ElectricBottomLeft) < ELECTRIC_COUNT);
    QVERIFY(static_cast<int>(ElectricLeft) < ELECTRIC_COUNT);
    QVERIFY(static_cast<int>(ElectricTopLeft) < ELECTRIC_COUNT);
}

void ElectricBorderTest::testElectricBorderActionValues()
{
    // Test action values are distinct
    QVERIFY(ElectricActionNone != ElectricActionShowDesktop);
    QVERIFY(ElectricActionShowDesktop != ElectricActionLockScreen);
    QVERIFY(ElectricActionLockScreen != ElectricActionKRunner);
    QVERIFY(ElectricActionKRunner != ElectricActionActivityManager);
    QVERIFY(ElectricActionActivityManager != ElectricActionApplicationLauncher);

    // ElectricActionNone should be 0
    QCOMPARE(static_cast<int>(ElectricActionNone), 0);

    // Count should be correct
    QCOMPARE(static_cast<int>(ELECTRIC_ACTION_COUNT), 6);
}

void ElectricBorderTest::testMaximizeModeValues()
{
    // Test maximize mode values
    QCOMPARE(static_cast<int>(MaximizeRestore), 0);
    QCOMPARE(static_cast<int>(MaximizeVertical), 1);
    QCOMPARE(static_cast<int>(MaximizeHorizontal), 2);
    QCOMPARE(static_cast<int>(MaximizeFull), 3);

    // MaximizeFull should be Vertical | Horizontal
    QCOMPARE(MaximizeFull, MaximizeVertical | MaximizeHorizontal);
}

void ElectricBorderTest::testMaximizeModeOperations()
{
    // Test XOR operation for toggling maximize
    MaximizeMode mode = MaximizeFull;

    // Toggle vertical off
    mode = mode ^ MaximizeVertical;
    QCOMPARE(mode, MaximizeHorizontal);

    // Toggle vertical back on
    mode = mode ^ MaximizeVertical;
    QCOMPARE(mode, MaximizeFull);

    // Toggle horizontal off
    mode = mode ^ MaximizeHorizontal;
    QCOMPARE(mode, MaximizeVertical);

    // Toggle both off
    mode = mode ^ MaximizeVertical;
    QCOMPARE(mode, MaximizeRestore);

    // Test OR operation
    MaximizeMode combined = MaximizeRestore;
    combined = MaximizeMode(combined | MaximizeVertical);
    QCOMPARE(combined, MaximizeVertical);

    combined = MaximizeMode(combined | MaximizeHorizontal);
    QCOMPARE(combined, MaximizeFull);
}

void ElectricBorderTest::testSwipeDirectionValues()
{
    // Test SwipeDirection enum values are distinct
    QVERIFY(SwipeDirection::Invalid != SwipeDirection::Down);
    QVERIFY(SwipeDirection::Down != SwipeDirection::Left);
    QVERIFY(SwipeDirection::Left != SwipeDirection::Up);
    QVERIFY(SwipeDirection::Up != SwipeDirection::Right);
    QVERIFY(SwipeDirection::Right != SwipeDirection::Invalid);
}

void ElectricBorderTest::testPinchDirectionValues()
{
    // Test PinchDirection enum values are distinct
    QVERIFY(PinchDirection::Expanding != PinchDirection::Contracting);
}

QTEST_GUILESS_MAIN(ElectricBorderTest)
#include "test_electric_border.moc"
