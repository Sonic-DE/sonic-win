/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/renderloop.h"
#include "core/renderloop_p.h"
#include "main.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace KWin;
using namespace std::chrono_literals;

// Supply the application singleton without starting a compositor or an X server.
class RenderLoopTestApplication : public Application
{
public:
    RenderLoopTestApplication(int &argc, char **argv)
        : Application(OperationModeX11, argc, argv)
    {
    }

protected:
    void performStartup() override
    {
    }
};

class TestRenderLoop : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testDeadlinePrecision();
    void testOverdueDeadline();
    void testCompletionReschedules();
    void testInhibition();
    void testPendingFrame_data();
    void testPendingFrame();
    void testCoalescing();
    void testSharedLoopLogging();
};

static std::chrono::nanoseconds now()
{
    return std::chrono::steady_clock::now().time_since_epoch();
}

void TestRenderLoop::testDeadlinePrecision()
{
    RenderLoop loop(nullptr);
    auto d = RenderLoopPrivate::get(&loop);
    d->lastPresentationTimestamp = now();
    const auto before = now();
    d->scheduleRepaint(d->lastPresentationTimestamp);
    const auto after = now();

    const auto deadline = d->nextPresentationTimestamp - d->renderJournal.result() - d->safetyMargin - 1ms;
    const auto interval = d->compositeTimer.interval();
    // Bound the clock sampled inside scheduleRepaint(), not actual OS wakeup time.
    QVERIFY(interval >= std::max(0ns, deadline - after));
    QVERIFY(interval <= std::max(0ns, deadline - before));
    QVERIFY(d->compositeTimer.isSingleShot());
    QCOMPARE(d->compositeTimer.timerType(), Qt::PreciseTimer);
    QVERIFY(d->compositeTimer.isActive());
}

void TestRenderLoop::testOverdueDeadline()
{
    RenderLoop loop(nullptr);
    auto d = RenderLoopPrivate::get(&loop);
    // Async targets now, so its render deadline is already in the past.
    loop.setPresentationMode(PresentationMode::Async);
    d->scheduleRepaint(0ns);
    QCOMPARE(d->compositeTimer.interval(), 0ns);
    QSignalSpy requested(&loop, &RenderLoop::frameRequested);
    QTRY_COMPARE(requested.count(), 1);
    QVERIFY(!d->compositeTimer.isActive());
    QVERIFY(!d->pendingRepaint);
}

void TestRenderLoop::testCompletionReschedules()
{
    RenderLoop loop(nullptr);
    auto d = RenderLoopPrivate::get(&loop);
    const auto refresh = std::chrono::nanoseconds(1'000'000'000'000ull / loop.refreshRate());
    d->lastPresentationTimestamp = now();
    d->scheduleRepaint(d->lastPresentationTimestamp);
    const auto target = d->nextPresentationTimestamp;
    const auto timestamp = d->lastPresentationTimestamp + 100us;
    loop.prepareNewFrame();
    OutputFrame frame(&loop, refresh);
    frame.presented(timestamp, PresentationMode::VSync);
    QCOMPARE(d->pendingFrameCount, 0);
    QCOMPARE(d->nextPresentationTimestamp, target + 100us);
    QCOMPARE(d->nextPresentationTimestamp, timestamp + refresh);
    QVERIFY(d->compositeTimer.isActive());
}

void TestRenderLoop::testInhibition()
{
    RenderLoop loop(nullptr);
    auto d = RenderLoopPrivate::get(&loop);
    loop.scheduleRepaint();
    QVERIFY(d->compositeTimer.isActive());
    loop.inhibit();
    loop.inhibit();
    QVERIFY(!d->compositeTimer.isActive());
    loop.scheduleRepaint();
    QVERIFY(d->pendingReschedule);
    loop.uninhibit();
    QVERIFY(!d->compositeTimer.isActive());
    loop.uninhibit();
    QVERIFY(d->compositeTimer.isActive());
    QVERIFY(!d->pendingReschedule);
}

void TestRenderLoop::testPendingFrame_data()
{
    QTest::addColumn<bool>("presented");
    QTest::addColumn<bool>("inhibited");
    QTest::newRow("presented") << true << false;
    QTest::newRow("dropped") << false << false;
    QTest::newRow("presented-inhibited") << true << true;
    QTest::newRow("dropped-inhibited") << false << true;
}

void TestRenderLoop::testPendingFrame()
{
    QFETCH(bool, presented);
    QFETCH(bool, inhibited);
    RenderLoop loop(nullptr);
    auto d = RenderLoopPrivate::get(&loop);
    loop.prepareNewFrame();
    if (inhibited) {
        loop.inhibit();
    }
    {
        OutputFrame frame(&loop, 16ms);
        loop.scheduleRepaint();
        QVERIFY(d->pendingReschedule);
        QVERIFY(!d->compositeTimer.isActive());
        QCOMPARE(d->pendingFrameCount, 1);
        if (presented) {
            frame.presented(now(), PresentationMode::VSync);
        }
    }
    QCOMPARE(d->pendingFrameCount, 0);
    QCOMPARE(d->compositeTimer.isActive(), !inhibited);
    if (inhibited) {
        QVERIFY(d->pendingReschedule);
        loop.uninhibit();
    }
    QVERIFY(d->compositeTimer.isActive());
    QVERIFY(!d->pendingReschedule);
}

void TestRenderLoop::testCoalescing()
{
    RenderLoop loop(nullptr);
    auto d = RenderLoopPrivate::get(&loop);
    loop.scheduleRepaint();
    const auto timerId = d->compositeTimer.id();
    const auto target = d->nextPresentationTimestamp;
    for (int i = 0; i < 100; ++i) {
        loop.scheduleRepaint();
    }
    QCOMPARE(d->compositeTimer.id(), timerId);
    QCOMPARE(d->nextPresentationTimestamp, target);
    QCOMPARE(d->pendingFrameCount, 0);
    QCOMPARE(d->maxPendingFrameCount, 1);
}

void TestRenderLoop::testSharedLoopLogging()
{
    const bool logging = qEnvironmentVariableIntValue("KWIN_LOG_PERFORMANCE_DATA") != 0;
    const QString filename = QStringLiteral("kwin perf statistics x11.csv");
    QFile::remove(filename);
    {
        RenderLoop loop(nullptr);
        loop.prepareNewFrame();
        OutputFrame frame(&loop, 16ms);
        frame.presented(now(), PresentationMode::VSync);
        QCOMPARE(RenderLoopPrivate::get(&loop)->m_debugOutput.has_value(), logging);
    } // Close and flush the CSV before reading it.
    if (!logging) {
        QVERIFY(!QFile::exists(filename));
        return;
    }
    QFile csv(filename);
    QVERIFY(csv.open(QIODevice::ReadOnly));
    QVERIFY(csv.readLine().startsWith("target pageflip timestamp,pageflip timestamp,"));
    QCOMPARE(csv.readLine().trimmed().split(',').size(), 9);
    QVERIFY(csv.atEnd());
}

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    RenderLoopTestApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid() || !QDir::setCurrent(directory.path())) {
        return 1;
    }
    TestRenderLoop test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_renderloop.moc"
