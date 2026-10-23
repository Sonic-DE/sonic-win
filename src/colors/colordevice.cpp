/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "colordevice.h"
#include "core/colorpipelinestage.h"
#include "core/colortransformation.h"
#include "core/output.h"
#include "utils/common.h"

#include "3rdparty/colortemperature.h"

#include <QTimer>

#include <algorithm>
#include <cmath>

#include <lcms2.h>

namespace KWin
{

class ColorDevicePrivate
{
public:
    void recalculateFactors();

    Output *output;
    QTimer *updateTimer;
    uint temperature = 6500;
    double brightness = 1.0;
    double dimming = 1.0;

    QVector3D temperatureFactors = QVector3D(1, 1, 1);
};

static qreal interpolate(qreal a, qreal b, qreal blendFactor)
{
    return (1 - blendFactor) * a + blendFactor * b;
}

void ColorDevicePrivate::recalculateFactors()
{
    if (temperature == 6500) {
        temperatureFactors = QVector3D(1, 1, 1);
    } else {
        // Note that cmsWhitePointFromTemp() returns a slightly green-ish white point.
        const int blackBodyColorIndex = ((temperature - 1000) / 100) * 3;
        const qreal blendFactor = (temperature % 100) / 100.0;

        const qreal xWhitePoint = interpolate(blackbodyColor[blackBodyColorIndex + 0],
                                              blackbodyColor[blackBodyColorIndex + 3],
                                              blendFactor);
        const qreal yWhitePoint = interpolate(blackbodyColor[blackBodyColorIndex + 1],
                                              blackbodyColor[blackBodyColorIndex + 4],
                                              blendFactor);
        const qreal zWhitePoint = interpolate(blackbodyColor[blackBodyColorIndex + 2],
                                              blackbodyColor[blackBodyColorIndex + 5],
                                              blendFactor);
        // the values in the blackbodyColor array are "gamma corrected", but we need a linear value
        temperatureFactors = TransferFunction(TransferFunction::gamma22, 0, 1).encodedToNits(QVector3D(xWhitePoint, yWhitePoint, zWhitePoint));
    }
}

ColorDevice::ColorDevice(Output *output, QObject *parent)
    : QObject(parent)
    , d(new ColorDevicePrivate)
{
    d->updateTimer = new QTimer(this);
    d->updateTimer->setSingleShot(true);
    connect(d->updateTimer, &QTimer::timeout, this, &ColorDevice::update);

    d->output = output;
    scheduleUpdate();
}

ColorDevice::~ColorDevice()
{
}

Output *ColorDevice::output() const
{
    return d->output;
}

uint ColorDevice::temperature() const
{
    return d->temperature;
}

void ColorDevice::setTemperature(uint temperature)
{
    if (temperature > 6500) {
        qCWarning(KWIN_CORE) << "Got invalid temperature value:" << temperature;
        temperature = 6500;
    }
    if (d->temperature == temperature) {
        return;
    }
    d->temperature = temperature;
    scheduleUpdate();
    Q_EMIT temperatureChanged();
}

double ColorDevice::brightness() const
{
    return d->brightness;
}

void ColorDevice::setBrightness(double brightness)
{
    if (!std::isfinite(brightness)) {
        return;
    }
    // Do not let an ordinary brightness slider make the screen completely black.
    brightness = std::clamp(brightness, 0.01, 1.0);
    if (d->brightness == brightness) {
        return;
    }
    d->brightness = brightness;
    scheduleUpdate();
    Q_EMIT brightnessChanged();
}

double ColorDevice::dimming() const
{
    return d->dimming;
}

void ColorDevice::setDimming(double dimming)
{
    if (!std::isfinite(dimming)) {
        return;
    }
    dimming = std::clamp(dimming, 0.0, 1.0);
    if (d->dimming == dimming) {
        return;
    }
    d->dimming = dimming;
    scheduleUpdate();
}

void ColorDevice::update()
{
    d->recalculateFactors();
    // X11Output applies these factors in linear light, then encodes gamma 2.2.
    // Convert the requested encoded-output multiplier so the visible result is
    // equivalent to xrandr --brightness, composed with (not replacing) Night Light.
    const float multiplier = std::pow(d->brightness * d->dimming, 2.2);
    d->output->setChannelFactors(d->temperatureFactors * multiplier);
}

void ColorDevice::scheduleUpdate()
{
    d->updateTimer->start();
}

} // namespace KWin

#include "moc_colordevice.cpp"
