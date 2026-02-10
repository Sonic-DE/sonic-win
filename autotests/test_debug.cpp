/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 KDE

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QByteArray>
#include <QDebug>
#include <QLoggingCategory>
#include <QString>
#include <QTest>
#include <QVector>

// Include the project's logging category declarations
#include "backends/x11/standalone/x11_standalone_logging.h"
#include "decorations/decorations_logging.h"
#include "tabbox/tabbox_logging.h"
#include "utils/common.h"

class TestDebug : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testQCDebugOutput();
    void testKwinCoreLoggingCategory();
    void testKwinOpenGLLoggingCategory();
    void testKwinVirtualKeyboardLoggingCategory();
    void testKwinX11StandaloneLoggingCategory();
    void testKwinTabboxLoggingCategory();
    void testKwinDecorationsLoggingCategory();
    void testQCDebugWithMessage_data();
    void testQCDebugWithMessage();

private:
    static void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static QVector<QString> s_messages;
    static QString s_lastCategory;
};

QVector<QString> TestDebug::s_messages;
QString TestDebug::s_lastCategory;

void TestDebug::customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    s_messages.append(msg);
    s_lastCategory = QString::fromLatin1(context.category);
}

void TestDebug::initTestCase()
{
    // Enable all kwin logging categories for testing (both debug and warning levels)
    QLoggingCategory::setFilterRules("kwin.*.debug=true");
}

void TestDebug::testQCDebugOutput()
{
    s_messages.clear();
    s_lastCategory.clear();
    qInstallMessageHandler(customMessageHandler);

    qCDebug(KWIN_CORE) << "Test" << 42;

    qInstallMessageHandler(nullptr);

    QVERIFY2(!s_messages.isEmpty(), "qCDebug should produce output for KWIN_CORE category");
    QVERIFY2(s_lastCategory == "kwin_core", QString("Category should be 'kwin_core', got '%1'").arg(s_lastCategory).toUtf8());
}

void TestDebug::testKwinCoreLoggingCategory()
{
    s_messages.clear();
    s_lastCategory.clear();
    qInstallMessageHandler(customMessageHandler);

    // Test that the actual KWIN_CORE logging category from the project works
    qCDebug(KWIN_CORE) << "Testing KWIN_CORE logging category";

    qInstallMessageHandler(nullptr);

    QVERIFY2(!s_messages.isEmpty(), "qCDebug should produce output for KWIN_CORE category");
    QVERIFY2(s_lastCategory == "kwin_core", QString("Category should be 'kwin_core', got '%1'").arg(s_lastCategory).toUtf8());
    QVERIFY2(s_messages.last().contains("Testing KWIN_CORE logging category"),
             "Message should contain expected text");
}

void TestDebug::testKwinOpenGLLoggingCategory()
{
    s_messages.clear();
    s_lastCategory.clear();
    qInstallMessageHandler(customMessageHandler);

    // Test that the KWIN_OPENGL logging category works
    qCDebug(KWIN_OPENGL) << "Testing KWIN_OPENGL logging category";

    qInstallMessageHandler(nullptr);

    QVERIFY2(!s_messages.isEmpty(), "qCDebug should produce output for KWIN_OPENGL category");
    QVERIFY2(s_lastCategory == "kwin_scene_opengl", QString("Category should be 'kwin_scene_opengl', got '%1'").arg(s_lastCategory).toUtf8());
    QVERIFY2(s_messages.last().contains("Testing KWIN_OPENGL logging category"),
             "Message should contain expected text");
}

void TestDebug::testKwinVirtualKeyboardLoggingCategory()
{
    s_messages.clear();
    s_lastCategory.clear();
    qInstallMessageHandler(customMessageHandler);

    // Test that the KWIN_VIRTUALKEYBOARD logging category works
    qCDebug(KWIN_VIRTUALKEYBOARD) << "Testing KWIN_VIRTUALKEYBOARD logging category";

    qInstallMessageHandler(nullptr);

    QVERIFY2(!s_messages.isEmpty(), "qCDebug should produce output for KWIN_VIRTUALKEYBOARD category");
    QVERIFY2(s_lastCategory == "kwin_virtualkeyboard", QString("Category should be 'kwin_virtualkeyboard', got '%1'").arg(s_lastCategory).toUtf8());
    QVERIFY2(s_messages.last().contains("Testing KWIN_VIRTUALKEYBOARD logging category"),
             "Message should contain expected text");
}

void TestDebug::testKwinX11StandaloneLoggingCategory()
{
    s_messages.clear();
    s_lastCategory.clear();
    qInstallMessageHandler(customMessageHandler);

    // Test that the KWIN_X11STANDALONE logging category works (uses warning level)
    qCWarning(KWIN_X11STANDALONE) << "Testing KWIN_X11STANDALONE logging category";

    qInstallMessageHandler(nullptr);

    QVERIFY2(!s_messages.isEmpty(), "qCWarning should produce output for KWIN_X11STANDALONE category");
    QVERIFY2(s_lastCategory == "kwin_platform_x11_standalone", QString("Category should be 'kwin_platform_x11_standalone', got '%1'").arg(s_lastCategory).toUtf8());
    QVERIFY2(s_messages.last().contains("Testing KWIN_X11STANDALONE logging category"),
             "Message should contain expected text");
}

void TestDebug::testKwinTabboxLoggingCategory()
{
    s_messages.clear();
    s_lastCategory.clear();
    qInstallMessageHandler(customMessageHandler);

    // Test that the KWIN_TABBOX logging category works (uses warning level)
    qCWarning(KWIN_TABBOX) << "Testing KWIN_TABBOX logging category";

    qInstallMessageHandler(nullptr);

    QVERIFY2(!s_messages.isEmpty(), "qCWarning should produce output for KWIN_TABBOX category");
    QVERIFY2(s_lastCategory == "kwin_tabbox", QString("Category should be 'kwin_tabbox', got '%1'").arg(s_lastCategory).toUtf8());
    QVERIFY2(s_messages.last().contains("Testing KWIN_TABBOX logging category"),
             "Message should contain expected text");
}

void TestDebug::testKwinDecorationsLoggingCategory()
{
    s_messages.clear();
    s_lastCategory.clear();
    qInstallMessageHandler(customMessageHandler);

    // Test that the KWIN_DECORATIONS logging category works (uses warning level)
    qCWarning(KWIN_DECORATIONS) << "Testing KWIN_DECORATIONS logging category";

    qInstallMessageHandler(nullptr);

    QVERIFY2(!s_messages.isEmpty(), "qCWarning should produce output for KWIN_DECORATIONS category");
    QVERIFY2(s_lastCategory == "kwin_decorations", QString("Category should be 'kwin_decorations', got '%1'").arg(s_lastCategory).toUtf8());
    QVERIFY2(s_messages.last().contains("Testing KWIN_DECORATIONS logging category"),
             "Message should contain expected text");
}

void TestDebug::testQCDebugWithMessage_data()
{
    QTest::addColumn<QString>("message");
    QTest::addColumn<int>("value");

    QTest::newRow("integer") << "Value" << 100;
    QTest::newRow("string") << "Message" << 200;
}

void TestDebug::testQCDebugWithMessage()
{
    s_messages.clear();
    s_lastCategory.clear();
    qInstallMessageHandler(customMessageHandler);

    QFETCH(QString, message);
    QFETCH(int, value);

    qCDebug(KWIN_CORE) << message << value;

    qInstallMessageHandler(nullptr);

    QVERIFY2(!s_messages.isEmpty(), "qCDebug should produce output");
    QVERIFY2(s_messages.last().contains(message), "Message should contain expected text");
    QVERIFY2(s_lastCategory == "kwin_core", QString("Category should be 'kwin_core', got '%1'").arg(s_lastCategory).toUtf8());
}

QTEST_MAIN(TestDebug)
#include "test_debug.moc"
