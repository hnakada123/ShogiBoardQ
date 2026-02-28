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
    void testPostError_emitsErrorLevel();
    void testBackwardCompatibility();
};

// ----------------------------------------------------------------------------
// postMessage() が messagePosted / errorOccurred を正しく発火するか
// ----------------------------------------------------------------------------

void TestErrorBus::testPostMessage_data()
{
    QTest::addColumn<ErrorBus::ErrorLevel>("level");
    QTest::addColumn<QString>("message");
    QTest::addColumn<bool>("emitsErrorOccurred");

    QTest::newRow("info")
        << ErrorBus::ErrorLevel::Info << QStringLiteral("info msg") << false;
    QTest::newRow("warning")
        << ErrorBus::ErrorLevel::Warning << QStringLiteral("warn msg") << false;
    QTest::newRow("error")
        << ErrorBus::ErrorLevel::Error << QStringLiteral("error msg") << true;
    QTest::newRow("critical")
        << ErrorBus::ErrorLevel::Critical << QStringLiteral("critical msg") << true;
}

void TestErrorBus::testPostMessage()
{
    QFETCH(ErrorBus::ErrorLevel, level);
    QFETCH(QString, message);
    QFETCH(bool, emitsErrorOccurred);

    auto& bus = ErrorBus::instance();

    QSignalSpy spyPosted(&bus, &ErrorBus::messagePosted);
    QSignalSpy spyError(&bus, &ErrorBus::errorOccurred);

    bus.postMessage(level, message);

    // messagePosted は常に発火する
    QCOMPARE(spyPosted.count(), 1);
    const auto args = spyPosted.takeFirst();
    QCOMPARE(args.at(0).value<ErrorBus::ErrorLevel>(), level);
    QCOMPARE(args.at(1).toString(), message);

    // errorOccurred は Error/Critical のみ
    QCOMPARE(spyError.count(), emitsErrorOccurred ? 1 : 0);
    if (emitsErrorOccurred) {
        QCOMPARE(spyError.takeFirst().at(0).toString(), message);
    }
}

// ----------------------------------------------------------------------------
// postError() は ErrorLevel::Error で postMessage() を呼ぶ
// ----------------------------------------------------------------------------

void TestErrorBus::testPostError_emitsErrorLevel()
{
    auto& bus = ErrorBus::instance();

    QSignalSpy spyPosted(&bus, &ErrorBus::messagePosted);
    QSignalSpy spyError(&bus, &ErrorBus::errorOccurred);

    bus.postError(QStringLiteral("legacy error"));

    QCOMPARE(spyPosted.count(), 1);
    const auto args = spyPosted.takeFirst();
    QCOMPARE(args.at(0).value<ErrorBus::ErrorLevel>(), ErrorBus::ErrorLevel::Error);
    QCOMPARE(args.at(1).toString(), QStringLiteral("legacy error"));

    QCOMPARE(spyError.count(), 1);
    QCOMPARE(spyError.takeFirst().at(0).toString(), QStringLiteral("legacy error"));
}

// ----------------------------------------------------------------------------
// 後方互換: errorOccurred のみ接続しても Error/Critical を受信できる
// ----------------------------------------------------------------------------

void TestErrorBus::testBackwardCompatibility()
{
    auto& bus = ErrorBus::instance();

    QSignalSpy spyError(&bus, &ErrorBus::errorOccurred);

    bus.postMessage(ErrorBus::ErrorLevel::Info, QStringLiteral("info"));
    bus.postMessage(ErrorBus::ErrorLevel::Warning, QStringLiteral("warning"));
    bus.postMessage(ErrorBus::ErrorLevel::Error, QStringLiteral("error"));
    bus.postMessage(ErrorBus::ErrorLevel::Critical, QStringLiteral("critical"));

    // Info/Warning は errorOccurred を発火しない → Error + Critical の2件のみ
    QCOMPARE(spyError.count(), 2);
    QCOMPARE(spyError.at(0).at(0).toString(), QStringLiteral("error"));
    QCOMPARE(spyError.at(1).at(0).toString(), QStringLiteral("critical"));
}

QTEST_MAIN(TestErrorBus)
#include "tst_errorbus.moc"
