// SPDX-License-Identifier: GPL-2.0-or-later

#include "colors/colordevice.h"
#include "colors/colormanager.h"
#include "core/output.h"

#include <KConfigGroup>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <limits>

using namespace KWin;

class TestOutput : public Output
{
public:
    TestOutput(const QString &name, bool supported = true)
    {
        Information info;
        info.name = name;
        info.capabilities = supported ? Capability::BrightnessControl : Capabilities{};
        setInformation(info);
        setEnabled(true);
    }
    RenderLoop *renderLoop() const override
    {
        return nullptr;
    }
    bool setChannelFactors(const QVector3D &factors) override
    {
        lastFactors = factors;
        return true;
    }
    void setEnabled(bool enabled)
    {
        auto state = m_state;
        state.enabled = enabled;
        setState(state);
    }
    QVector3D lastFactors;
};

class TestSoftwareBrightness : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void composesNightLight()
    {
        TestOutput output(QStringLiteral("HDMI-1"));
        ColorDevice device(&output);
        device.setTemperature(3500);
        device.update();
        const auto warm = output.lastFactors;
        QVERIFY(warm.x() > warm.z());
        device.setBrightness(0.5);
        device.setDimming(0.4);
        device.update();
        const auto expected = warm * float(std::pow(0.2, 2.2));
        QVERIFY((output.lastFactors - expected).length() < 0.00001f);
        QCOMPARE(device.brightness(), 0.5);
        device.setTemperature(6500);
        device.update();
        QVERIFY((output.lastFactors - QVector3D(1, 1, 1) * float(std::pow(0.2, 2.2))).length() < 0.00001f);
        device.setDimming(1.0);
        device.setBrightness(1.0);
        device.update();
        QCOMPARE(output.lastFactors, QVector3D(1, 1, 1));
    }

    void clampsUnsafeValues()
    {
        TestOutput output(QStringLiteral("HDMI-1"));
        ColorDevice device(&output);
        device.setBrightness(-1.0);
        QCOMPARE(device.brightness(), 0.01);
        device.setBrightness(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(device.brightness(), 0.01);
        device.setBrightness(2.0);
        QCOMPARE(device.brightness(), 1.0);
        device.setDimming(-1.0);
        QCOMPARE(device.dimming(), 0.0);
        device.setDimming(std::numeric_limits<double>::infinity());
        QCOMPARE(device.dimming(), 0.0);
    }

    void protocolPersistenceAndHotplug()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto config = KSharedConfig::openConfig(dir.filePath(QStringLiteral("kwinrc")), KConfig::SimpleConfig);
        TestOutput first(QStringLiteral("HDMI-1"));
        TestOutput second(QStringLiteral("DP-1"));
        TestOutput unsupported(QStringLiteral("dummy"), false);
        {
            ColorManager manager({&first, &second, &unsupported}, config);
            QCOMPARE(manager.Displays().size(), 2);
            QSignalSpy brightness(&manager, &ColorManager::BrightnessChanged);
            QVERIFY(manager.SetBrightness(first.name(), 4500, 0.5));
            QCOMPARE(brightness.count(), 1);
            QCOMPARE(manager.findDevice(&first)->brightness(), 0.45);
            QCOMPARE(manager.findDevice(&first)->dimming(), 0.5);
            QCOMPARE(manager.findDevice(&second)->brightness(), 1.0);
            QVERIFY(!manager.SetBrightness(first.name(), 0, 1.0));
            QVERIFY(!manager.SetBrightness(first.name(), 10001, 1.0));
            QVERIFY(!manager.SetBrightness(first.name(), 1000, std::numeric_limits<double>::quiet_NaN()));
            QVERIFY(!manager.SetBrightness(unsupported.name(), 1000, 1.0));
            QVERIFY(!manager.SetBrightness(QStringLiteral("missing"), 1000, 1.0));

            auto message = QDBusMessage::createMethodCall(QDBusConnection::sessionBus().baseService(),
                                                          QStringLiteral("/org/sonicde/SoftwareBrightness"), QStringLiteral("org.sonicde.SoftwareBrightness"), QStringLiteral("Displays"));
            QDBusPendingCallWatcher watcher(QDBusConnection::sessionBus().asyncCall(message));
            QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
            if (!watcher.isFinished()) {
                QVERIFY(finished.wait());
            }
            const QDBusPendingReply<QVariantMap> reply = watcher;
            QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
            QCOMPARE(reply.value().size(), 2);

            QSignalSpy displays(&manager, &ColorManager::DisplaysChanged);
            first.setEnabled(false);
            QCOMPARE(displays.count(), 1);
            QCOMPARE(manager.Displays().size(), 1);
            QVERIFY(!manager.SetBrightness(first.name(), 2000, 1.0));
            first.setEnabled(true);
        }
        // Temporary idle dimming must not survive a WM/session restart.
        ColorManager restored({&first, &second}, config);
        QCOMPARE(restored.findDevice(&first)->brightness(), 0.45);
        QCOMPARE(restored.findDevice(&first)->dimming(), 1.0);
        QCOMPARE(restored.findDevice(&second)->brightness(), 1.0);
    }
};

QTEST_GUILESS_MAIN(TestSoftwareBrightness)
#include "test_softwarebrightness.moc"
