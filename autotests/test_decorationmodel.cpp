/*
    SPDX-FileCopyrightText: 2026 Joseph Crowell <joseph.w.crowell@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// The decoration KCM's theme model reports titlebar menu support from each plugin's metadata.
#include "decorationmodel.h"

#include <KPluginMetaData>

#include <QTest>

using KDecoration3::Configuration::DecorationsModel;

class TestDecorationModel : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testTitleBarMenuRole();
};

void TestDecorationModel::testTitleBarMenuRole()
{
    DecorationsModel model;
    model.init();
    QVERIFY2(model.rowCount() > 0, "no decoration plugins found; set QT_PLUGIN_PATH");
    QCOMPARE(model.roleNames().value(DecorationsModel::TitleBarMenuRole), QByteArrayLiteral("titleBarMenu"));

    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        const QString pluginId = index.data(DecorationsModel::PluginNameRole).toString();
        const KPluginMetaData metaData = KPluginMetaData::findPluginById(QStringLiteral("org.kde.kdecoration3"), pluginId);
        QVERIFY2(metaData.isValid(), qPrintable(pluginId));
        QCOMPARE(index.data(DecorationsModel::TitleBarMenuRole).toBool(), metaData.value(QStringLiteral("X-SonicDE-TitleBarMenu"), false));
    }
}

QTEST_MAIN(TestDecorationModel)
#include "test_decorationmodel.moc"
