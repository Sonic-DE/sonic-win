/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "cursor.h"

using namespace KWin;

/**
 * @brief Tests for CursorShape and ExtendedCursor::Shape enums.
 *
 * These tests verify cursor shape handling works correctly for both
 * Qt standard cursors and KWin extended cursors.
 */
class CursorShapeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testExtendedCursorShapeValues();
    void testCursorShapeFromQt();
    void testCursorShapeFromExtended();
    void testCursorShapeComparison();
    void testCursorShapeName();
    void testDefaultTheme();
};

void CursorShapeTest::testExtendedCursorShapeValues()
{
    // Test that extended cursor shapes start at offset 0x100
    QCOMPARE(static_cast<int>(ExtendedCursor::SizeNorthWest), 0x100 + 0);
    QCOMPARE(static_cast<int>(ExtendedCursor::SizeNorth), 0x100 + 1);
    QCOMPARE(static_cast<int>(ExtendedCursor::SizeNorthEast), 0x100 + 2);
    QCOMPARE(static_cast<int>(ExtendedCursor::SizeEast), 0x100 + 3);
    QCOMPARE(static_cast<int>(ExtendedCursor::SizeWest), 0x100 + 4);
    QCOMPARE(static_cast<int>(ExtendedCursor::SizeSouthEast), 0x100 + 5);
    QCOMPARE(static_cast<int>(ExtendedCursor::SizeSouth), 0x100 + 6);
    QCOMPARE(static_cast<int>(ExtendedCursor::SizeSouthWest), 0x100 + 7);

    // Verify they don't conflict with Qt::CursorShape (which has values < 0x100)
    QVERIFY(static_cast<int>(ExtendedCursor::SizeNorthWest) > static_cast<int>(Qt::LastCursor));
}

void CursorShapeTest::testCursorShapeFromQt()
{
    // Test constructing CursorShape from Qt::CursorShape
    CursorShape arrow(Qt::ArrowCursor);
    QCOMPARE(static_cast<int>(arrow), static_cast<int>(Qt::ArrowCursor));

    CursorShape wait(Qt::WaitCursor);
    QCOMPARE(static_cast<int>(wait), static_cast<int>(Qt::WaitCursor));

    CursorShape crosshair(Qt::CrossCursor);
    QCOMPARE(static_cast<int>(crosshair), static_cast<int>(Qt::CrossCursor));

    CursorShape sizeAll(Qt::SizeAllCursor);
    QCOMPARE(static_cast<int>(sizeAll), static_cast<int>(Qt::SizeAllCursor));
}

void CursorShapeTest::testCursorShapeFromExtended()
{
    // Test constructing CursorShape from ExtendedCursor::Shape
    CursorShape sizeNW(ExtendedCursor::SizeNorthWest);
    QCOMPARE(static_cast<int>(sizeNW), static_cast<int>(ExtendedCursor::SizeNorthWest));

    CursorShape sizeN(ExtendedCursor::SizeNorth);
    QCOMPARE(static_cast<int>(sizeN), static_cast<int>(ExtendedCursor::SizeNorth));

    CursorShape sizeSE(ExtendedCursor::SizeSouthEast);
    QCOMPARE(static_cast<int>(sizeSE), static_cast<int>(ExtendedCursor::SizeSouthEast));
}

void CursorShapeTest::testCursorShapeComparison()
{
    // Test equality operator
    CursorShape arrow1(Qt::ArrowCursor);
    CursorShape arrow2(Qt::ArrowCursor);
    QVERIFY(arrow1 == arrow2);

    CursorShape wait(Qt::WaitCursor);
    QVERIFY(!(arrow1 == wait));

    // Test extended cursors
    CursorShape sizeNW1(ExtendedCursor::SizeNorthWest);
    CursorShape sizeNW2(ExtendedCursor::SizeNorthWest);
    QVERIFY(sizeNW1 == sizeNW2);

    CursorShape sizeSE(ExtendedCursor::SizeSouthEast);
    QVERIFY(!(sizeNW1 == sizeSE));

    // Test Qt vs Extended
    CursorShape qtArrow(Qt::ArrowCursor);
    CursorShape extNW(ExtendedCursor::SizeNorthWest);
    QVERIFY(!(qtArrow == extNW));
}

void CursorShapeTest::testCursorShapeName()
{
    // Test that cursor shapes return valid names
    CursorShape arrow(Qt::ArrowCursor);
    QByteArray arrowName = arrow.name();
    QVERIFY(!arrowName.isEmpty());

    CursorShape wait(Qt::WaitCursor);
    QByteArray waitName = wait.name();
    QVERIFY(!waitName.isEmpty());

    CursorShape pointer(Qt::PointingHandCursor);
    QByteArray pointerName = pointer.name();
    QVERIFY(!pointerName.isEmpty());

    // Extended cursors should also have names
    CursorShape sizeNW(ExtendedCursor::SizeNorthWest);
    QByteArray sizeNWName = sizeNW.name();
    QVERIFY(!sizeNWName.isEmpty());

    CursorShape sizeSE(ExtendedCursor::SizeSouthEast);
    QByteArray sizeSEName = sizeSE.name();
    QVERIFY(!sizeSEName.isEmpty());
}

void CursorShapeTest::testDefaultTheme()
{
    // Test that default theme name and size are valid
    QString themeName = Cursor::defaultThemeName();
    // Default theme name should either be empty (use system) or a valid name
    // Just verify the function doesn't crash

    int themeSize = Cursor::defaultThemeSize();
    QVERIFY(themeSize > 0);

    // Fallback theme should always have a name
    QString fallbackName = Cursor::fallbackThemeName();
    QVERIFY(!fallbackName.isEmpty());
}

QTEST_GUILESS_MAIN(CursorShapeTest)
#include "test_cursor_shape.moc"
