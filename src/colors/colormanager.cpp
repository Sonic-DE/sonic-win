/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colormanager.h"
#include "colordevice.h"
#include "core/output.h"
#include "core/session.h"
#include "main.h"
#include "utils/common.h"
#include "workspace.h"

#include <KConfigGroup>
#include <QDBusConnection>
#include <cmath>

namespace KWin
{

class ColorManagerPrivate
{
public:
    QList<ColorDevice *> devices;
    KSharedConfigPtr config;
    bool registered = false;
};

ColorManager::ColorManager()
    : ColorManager(workspace()->outputs(), kwinApp()->config())
{
    connect(workspace(), &Workspace::outputAdded, this, &ColorManager::handleOutputAdded);
    connect(workspace(), &Workspace::outputRemoved, this, &ColorManager::handleOutputRemoved);
    connect(kwinApp()->session(), &Session::activeChanged, this, &ColorManager::handleSessionActiveChanged);
}

ColorManager::ColorManager(const QList<Output *> &outputs, const KSharedConfigPtr &config)
    : d(std::make_unique<ColorManagerPrivate>())
{
    d->config = config;
    for (Output *output : outputs) {
        handleOutputAdded(output);
    }
    // org.kde.KWin is owned by DBusInterface. Do not acquire a second service:
    // PowerDevil also uses its ownership changes to detect WM restarts.
    d->registered = QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/sonicde/SoftwareBrightness"), this,
                                                                 QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
    Q_EMIT DisplaysChanged();
}

ColorManager::~ColorManager()
{
    if (d->registered) {
        QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/org/sonicde/SoftwareBrightness"));
    }
}

QVariantMap ColorManager::Displays() const
{
    QVariantMap result;
    for (ColorDevice *device : d->devices) {
        Output *output = device->output();
        if (!output->isEnabled() || !(output->capabilities() & Output::Capability::BrightnessControl)) {
            continue;
        }
        const QString label = QString(output->manufacturer() + QLatin1Char(' ') + output->model()).simplified();
        result.insert(output->name(), QVariantMap{
                                          {QStringLiteral("brightness"), qRound(device->brightness() * 10000)},
                                          {QStringLiteral("label"), label.isEmpty() ? output->name() : label},
                                          {QStringLiteral("internal"), output->isInternal()},
                                          {QStringLiteral("edid"), output->edid().raw()},
                                      });
    }
    return result;
}

bool ColorManager::SetBrightness(const QString &outputName, int brightness, double dimming)
{
    if (brightness < 100 || brightness > 10000 || !std::isfinite(dimming) || dimming < 0.0 || dimming > 1.0) {
        return false;
    }
    for (ColorDevice *device : d->devices) {
        Output *output = device->output();
        if (output->name() == outputName && output->isEnabled()
            && (output->capabilities() & Output::Capability::BrightnessControl)) {
            device->setBrightness(brightness / 10000.0);
            device->setDimming(dimming);
            return true;
        }
    }
    return false;
}

QList<ColorDevice *> ColorManager::devices() const
{
    return d->devices;
}

ColorDevice *ColorManager::findDevice(Output *output) const
{
    auto it = std::find_if(d->devices.begin(), d->devices.end(), [&output](ColorDevice *device) {
        return device->output() == output;
    });
    if (it != d->devices.end()) {
        return *it;
    }
    return nullptr;
}

void ColorManager::handleOutputAdded(Output *output)
{
    ColorDevice *device = new ColorDevice(output, this);
    d->devices.append(device);
    if (output->capabilities() & Output::Capability::BrightnessControl) {
        const KConfigGroup group(d->config, QStringLiteral("SoftwareBrightness"));
        device->setBrightness(group.readEntry(output->uuid().toString(), 10000) / 10000.0);
        connect(device, &ColorDevice::brightnessChanged, this, [this, device, output] {
            const int value = qRound(device->brightness() * 10000);
            KConfigGroup group(d->config, QStringLiteral("SoftwareBrightness"));
            group.writeEntry(output->uuid().toString(), value);
            group.sync();
            Q_EMIT BrightnessChanged(output->name(), value);
        });
    }
    connect(output, &Output::enabledChanged, this, &ColorManager::DisplaysChanged);
    connect(output, &Output::capabilitiesChanged, this, &ColorManager::DisplaysChanged);
    connect(output, &Output::enabledChanged, device, &ColorDevice::scheduleUpdate);
    connect(output, &Output::currentModeChanged, device, &ColorDevice::scheduleUpdate);
    connect(output, &Output::dpmsModeChanged, device, &ColorDevice::scheduleUpdate);
    Q_EMIT deviceAdded(device);
    Q_EMIT DisplaysChanged();
}

void ColorManager::handleOutputRemoved(Output *output)
{
    auto it = std::find_if(d->devices.begin(), d->devices.end(), [&output](ColorDevice *device) {
        return device->output() == output;
    });
    if (it == d->devices.end()) {
        qCWarning(KWIN_CORE) << "Could not find any color device for output" << output;
        return;
    }
    ColorDevice *device = *it;
    d->devices.erase(it);
    disconnect(output, nullptr, this, nullptr);
    Q_EMIT deviceRemoved(device);
    delete device;
    Q_EMIT DisplaysChanged();
}

void ColorManager::handleSessionActiveChanged(bool active)
{
    if (!active) {
        return;
    }
    for (ColorDevice *device : std::as_const(d->devices)) {
        device->scheduleUpdate();
    }
}

} // namespace KWin

#include "moc_colormanager.cpp"
