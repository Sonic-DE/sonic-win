/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "options.h"
#include "placement.h"

using namespace KWin;

/**
 * @brief Tests for PlacementPolicy enum and Placement class utilities.
 *
 * These tests verify the window placement policy enumeration and related
 * functionality work correctly.
 */
class PlacementPolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testPlacementPolicyValues();
    void testPlacementPolicyToString();
    void testClientAreaOptionValues();
    void testHiddenPreviewsValues();
};

void PlacementPolicyTest::testPlacementPolicyValues()
{
    // Test that all placement policies have expected values
    // and are distinct from each other
    QCOMPARE(static_cast<int>(PlacementNone), 0);
    QCOMPARE(static_cast<int>(PlacementDefault), 1);
    QCOMPARE(static_cast<int>(PlacementUnknown), 2);
    QCOMPARE(static_cast<int>(PlacementRandom), 3);
    QCOMPARE(static_cast<int>(PlacementSmart), 4);
    QCOMPARE(static_cast<int>(PlacementCentered), 5);
    QCOMPARE(static_cast<int>(PlacementZeroCornered), 6);
    QCOMPARE(static_cast<int>(PlacementUnderMouse), 7);
    QCOMPARE(static_cast<int>(PlacementOnMainWindow), 8);
    QCOMPARE(static_cast<int>(PlacementMaximizing), 9);
}

void PlacementPolicyTest::testPlacementPolicyToString()
{
    // Test the Placement::policyToString function returns valid strings
    QVERIFY(Placement::policyToString(PlacementNone) != nullptr);
    QVERIFY(Placement::policyToString(PlacementDefault) != nullptr);
    QVERIFY(Placement::policyToString(PlacementRandom) != nullptr);
    QVERIFY(Placement::policyToString(PlacementSmart) != nullptr);
    QVERIFY(Placement::policyToString(PlacementCentered) != nullptr);
    QVERIFY(Placement::policyToString(PlacementZeroCornered) != nullptr);
    QVERIFY(Placement::policyToString(PlacementUnderMouse) != nullptr);
    QVERIFY(Placement::policyToString(PlacementOnMainWindow) != nullptr);
    QVERIFY(Placement::policyToString(PlacementMaximizing) != nullptr);

    // Verify strings are non-empty
    QVERIFY(strlen(Placement::policyToString(PlacementNone)) > 0);
    QVERIFY(strlen(Placement::policyToString(PlacementSmart)) > 0);
    QVERIFY(strlen(Placement::policyToString(PlacementCentered)) > 0);
}

void PlacementPolicyTest::testClientAreaOptionValues()
{
    // Test that client area options have distinct values
    QVERIFY(PlacementArea != MovementArea);
    QVERIFY(MovementArea != MaximizeArea);
    QVERIFY(MaximizeArea != MaximizeFullArea);
    QVERIFY(MaximizeFullArea != FullScreenArea);
    QVERIFY(FullScreenArea != WorkArea);
    QVERIFY(WorkArea != FullArea);
    QVERIFY(FullArea != ScreenArea);

    // Test expected values (they should be sequential)
    QCOMPARE(static_cast<int>(PlacementArea), 0);
    QCOMPARE(static_cast<int>(MovementArea), 1);
    QCOMPARE(static_cast<int>(MaximizeArea), 2);
    QCOMPARE(static_cast<int>(MaximizeFullArea), 3);
    QCOMPARE(static_cast<int>(FullScreenArea), 4);
    QCOMPARE(static_cast<int>(WorkArea), 5);
    QCOMPARE(static_cast<int>(FullArea), 6);
    QCOMPARE(static_cast<int>(ScreenArea), 7);
}

void PlacementPolicyTest::testHiddenPreviewsValues()
{
    // Test hidden previews enum values
    QCOMPARE(static_cast<int>(HiddenPreviewsNever), 0);
    QCOMPARE(static_cast<int>(HiddenPreviewsShown), 1);
    QCOMPARE(static_cast<int>(HiddenPreviewsAlways), 2);

    // Verify they are distinct
    QVERIFY(HiddenPreviewsNever != HiddenPreviewsShown);
    QVERIFY(HiddenPreviewsShown != HiddenPreviewsAlways);
    QVERIFY(HiddenPreviewsNever != HiddenPreviewsAlways);
}

QTEST_GUILESS_MAIN(PlacementPolicyTest)
#include "test_placement_policy.moc"
