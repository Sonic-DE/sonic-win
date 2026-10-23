/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "effect/globals.h"

#include <KSharedConfig>
#include <QObject>
#include <QVariantMap>
#include <memory>

namespace KWin
{

class Output;
class ColorDevice;
class ColorManagerPrivate;

/**
 * The ColorManager class is the entry point into color management facilities.
 */
class KWIN_EXPORT ColorManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.sonicde.SoftwareBrightness")

public:
    ColorManager();
    ColorManager(const QList<Output *> &outputs, const KSharedConfigPtr &config);
    ~ColorManager() override;

    /**
     * Returns the color device for the specified @a output, or @c null if there is no
     * any device.
     */
    ColorDevice *findDevice(Output *output) const;

    /**
     * Returns the list of all available color devices.
     */
    QList<ColorDevice *> devices() const;

public Q_SLOTS:
    // Session-bus API consumed by PowerDevil. Brightness uses [100, 10000];
    // dimming is a separate, temporary multiplier and is never persisted.
    Q_SCRIPTABLE QVariantMap Displays() const;
    Q_SCRIPTABLE bool SetBrightness(const QString &outputName, int brightness, double dimming);

Q_SIGNALS:
    Q_SCRIPTABLE void DisplaysChanged();
    Q_SCRIPTABLE void BrightnessChanged(const QString &outputName, int brightness);
    /**
     * This signal is emitted when a new color device @a device has been added.
     */
    void deviceAdded(ColorDevice *device);
    /**
     * This signal is emitted when a color device has been removed. @a device indicates
     * what color device was removed.
     */
    void deviceRemoved(ColorDevice *device);

private Q_SLOTS:
    void handleOutputAdded(Output *output);
    void handleOutputRemoved(Output *output);
    void handleSessionActiveChanged(bool active);

private:
    std::unique_ptr<ColorManagerPrivate> d;
};

} // namespace KWin
