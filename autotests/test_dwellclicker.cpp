/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dwellclicker.h"

#include <QSignalSpy>
#include <QTest>

using namespace KWin;

namespace
{

class RecordingOutput : public DwellClickOutput
{
public:
    explicit RecordingOutput(QStringList *events)
        : m_events(events)
    {
    }

    bool isAvailable() const override
    {
        return true;
    }

    void button(quint8 button, bool pressed) override
    {
        m_events->append(QStringLiteral("%1%2").arg(pressed ? QLatin1Char('+') : QLatin1Char('-')).arg(button));
    }

    void escape() override
    {
        m_events->append(QStringLiteral("esc"));
    }

private:
    QStringList *m_events;
};

}

class DwellClickerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void testFirstPositionOnlySetsOrigin();
    void testBelowThresholdDoesNotStart();
    void testThresholdStarts_data();
    void testThresholdStarts();
    void testClickTypes_data();
    void testClickTypes();
    void testProgressBeforeClick();
    void testDrag();
    void testButtonChangeCancelsDrag();
    void testDisableCancelsDrag();
    void testReturnsToDefaultButton();
    void testButtonLocked();
    void testNoneIgnoresMotion();
    void testDisabledIgnoresMotion();
    void testZeroDwellTime();

private:
    void dwell(const QPointF &from);

    QStringList m_events;
    std::unique_ptr<DwellClicker> m_clicker;
};

void DwellClickerTest::init()
{
    m_events.clear();
    m_clicker = std::make_unique<DwellClicker>(std::make_unique<RecordingOutput>(&m_events));
    DwellClicker::Settings settings;
    settings.enabled = true;
    settings.delayTime = 10;
    settings.dwellTime = 30;
    settings.motionThreshold = 5;
    m_clicker->applySettings(settings);
}

void DwellClickerTest::cleanup()
{
    m_clicker.reset();
}

// Sets an origin at from, moves past the threshold and waits for the click cycle to end.
void DwellClickerTest::dwell(const QPointF &from)
{
    QSignalSpy stopped(m_clicker.get(), &DwellClicker::stopped);
    m_clicker->handleMotion(from);
    m_clicker->handleMotion(from + QPointF(10, 0));
    QVERIFY(m_clicker->isRunning());
    QVERIFY(stopped.wait());
}

void DwellClickerTest::testFirstPositionOnlySetsOrigin()
{
    m_clicker->handleMotion(QPointF(100, 100));
    QVERIFY(!m_clicker->isRunning());
}

void DwellClickerTest::testBelowThresholdDoesNotStart()
{
    m_clicker->handleMotion(QPointF(100, 100));
    m_clicker->handleMotion(QPointF(104, 96));
    QVERIFY(!m_clicker->isRunning());
}

void DwellClickerTest::testThresholdStarts_data()
{
    QTest::addColumn<QPointF>("position");
    QTest::newRow("right") << QPointF(105, 100);
    QTest::newRow("left") << QPointF(95, 100);
    QTest::newRow("down") << QPointF(100, 105);
    QTest::newRow("up") << QPointF(100, 95);
}

void DwellClickerTest::testThresholdStarts()
{
    QFETCH(QPointF, position);
    m_clicker->handleMotion(QPointF(100, 100));
    m_clicker->handleMotion(position);
    QVERIFY(m_clicker->isRunning());
    QTRY_COMPARE(m_events, QStringList({QStringLiteral("+1"), QStringLiteral("-1")}));
}

void DwellClickerTest::testClickTypes_data()
{
    QTest::addColumn<int>("button");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("left") << int(DwellClicker::LeftClick) << QStringList{QStringLiteral("+1"), QStringLiteral("-1")};
    QTest::newRow("double") << int(DwellClicker::DoubleClick) << QStringList{QStringLiteral("+1"), QStringLiteral("-1"), QStringLiteral("+1"), QStringLiteral("-1")};
    QTest::newRow("middle") << int(DwellClicker::MiddleClick) << QStringList{QStringLiteral("+2"), QStringLiteral("-2")};
    QTest::newRow("right") << int(DwellClicker::RightClick) << QStringList{QStringLiteral("+3"), QStringLiteral("-3")};
}

void DwellClickerTest::testClickTypes()
{
    QFETCH(int, button);
    QFETCH(QStringList, expected);
    m_clicker->setButton(button);
    dwell(QPointF(100, 100));
    QTRY_COMPARE(m_events, expected);
}

void DwellClickerTest::testProgressBeforeClick()
{
    QSignalSpy progress(m_clicker.get(), &DwellClicker::progressChanged);
    dwell(QPointF(100, 100));
    QTRY_COMPARE(m_events.size(), 2);
    QVERIFY(!progress.isEmpty());
    // The clicked phase follows a full dwell ring.
    const QList<QVariant> last = progress.last();
    QCOMPARE(last.at(0).toDouble(), 1.0);
    QCOMPARE(last.at(1).toDouble(), 1.0);
}

void DwellClickerTest::testDrag()
{
    m_clicker->setButton(DwellClicker::DragClick);
    dwell(QPointF(100, 100));
    QTRY_COMPARE(m_events, QStringList{QStringLiteral("+1")});
    QVERIFY(m_clicker->isDragging());
    QCOMPARE(m_clicker->button(), int(DwellClicker::DragClick));

    dwell(QPointF(200, 200));
    QTRY_COMPARE(m_events, (QStringList{QStringLiteral("+1"), QStringLiteral("-1")}));
    QVERIFY(!m_clicker->isDragging());
}

void DwellClickerTest::testButtonChangeCancelsDrag()
{
    m_clicker->setButton(DwellClicker::DragClick);
    dwell(QPointF(100, 100));
    QTRY_VERIFY(m_clicker->isDragging());

    m_clicker->setButton(DwellClicker::RightClick);
    QCOMPARE(m_events, (QStringList{QStringLiteral("+1"), QStringLiteral("esc"), QStringLiteral("-1")}));
    QVERIFY(!m_clicker->isDragging());
}

void DwellClickerTest::testDisableCancelsDrag()
{
    m_clicker->setButton(DwellClicker::DragClick);
    dwell(QPointF(100, 100));
    QTRY_VERIFY(m_clicker->isDragging());

    m_clicker->setEnabled(false);
    QCOMPARE(m_events, (QStringList{QStringLiteral("+1"), QStringLiteral("esc"), QStringLiteral("-1")}));
}

void DwellClickerTest::testReturnsToDefaultButton()
{
    QSignalSpy buttonChanged(m_clicker.get(), &DwellClicker::buttonChanged);
    m_clicker->setButton(DwellClicker::RightClick);
    dwell(QPointF(100, 100));
    QTRY_COMPARE(m_events, (QStringList{QStringLiteral("+3"), QStringLiteral("-3")}));
    QCOMPARE(m_clicker->button(), int(DwellClicker::LeftClick));
    QCOMPARE(buttonChanged.size(), 2);
}

void DwellClickerTest::testButtonLocked()
{
    DwellClicker::Settings settings = m_clicker->settings();
    settings.buttonLocked = true;
    m_clicker->applySettings(settings);
    m_clicker->setButton(DwellClicker::RightClick);
    dwell(QPointF(100, 100));
    QTRY_COMPARE(m_events, (QStringList{QStringLiteral("+3"), QStringLiteral("-3")}));
    QCOMPARE(m_clicker->button(), int(DwellClicker::RightClick));
}

void DwellClickerTest::testNoneIgnoresMotion()
{
    m_clicker->setButton(DwellClicker::None);
    m_clicker->handleMotion(QPointF(100, 100));
    m_clicker->handleMotion(QPointF(200, 200));
    QVERIFY(!m_clicker->isRunning());
}

void DwellClickerTest::testDisabledIgnoresMotion()
{
    m_clicker->setEnabled(false);
    m_clicker->handleMotion(QPointF(100, 100));
    m_clicker->handleMotion(QPointF(200, 200));
    QVERIFY(!m_clicker->isRunning());
}

void DwellClickerTest::testZeroDwellTime()
{
    DwellClicker::Settings settings = m_clicker->settings();
    settings.dwellTime = 0;
    m_clicker->applySettings(settings);
    QSignalSpy progress(m_clicker.get(), &DwellClicker::progressChanged);
    dwell(QPointF(100, 100));
    QTRY_COMPARE(m_events, (QStringList{QStringLiteral("+1"), QStringLiteral("-1")}));
    QVERIFY(progress.isEmpty());
}

QTEST_GUILESS_MAIN(DwellClickerTest)
#include "test_dwellclicker.moc"
