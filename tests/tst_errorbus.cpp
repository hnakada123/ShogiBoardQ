/// @file tst_errorbus.cpp
/// @brief ErrorBus の深刻度(severity)分類テスト

#include "errorbus.h"

#include <QSignalSpy>
#include <QTest>

class TestErrorBus : public QObject
{
    Q_OBJECT

private slots:
    void testPostMessage_data();
    void testPostMessage();
};

// ----------------------------------------------------------------------------
// postMessage() が深刻度と本文をそのまま messagePosted で知らせるか
// ----------------------------------------------------------------------------

void TestErrorBus::testPostMessage_data()
{
    QTest::addColumn<ErrorBus::ErrorLevel>("level");
    QTest::addColumn<QString>("message");

    QTest::newRow("info") << ErrorBus::ErrorLevel::Info << QStringLiteral("info msg");
    QTest::newRow("warning") << ErrorBus::ErrorLevel::Warning << QStringLiteral("warn msg");
    QTest::newRow("error") << ErrorBus::ErrorLevel::Error << QStringLiteral("error msg");
    QTest::newRow("critical") << ErrorBus::ErrorLevel::Critical << QStringLiteral("critical msg");
}

void TestErrorBus::testPostMessage()
{
    QFETCH(ErrorBus::ErrorLevel, level);
    QFETCH(QString, message);

    auto& bus = ErrorBus::instance();
    QSignalSpy spyPosted(&bus, &ErrorBus::messagePosted);

    bus.postMessage(level, message);

    QCOMPARE(spyPosted.count(), 1);
    const auto args = spyPosted.takeFirst();
    QCOMPARE(args.at(0).value<ErrorBus::ErrorLevel>(), level);
    QCOMPARE(args.at(1).toString(), message);
}

QTEST_MAIN(TestErrorBus)
#include "tst_errorbus.moc"
