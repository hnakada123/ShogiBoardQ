/// @file tst_applicationlogging.cpp
/// @brief ログの出力形式・並列書き込み・終了処理を検証する

#include "applicationlogging.h"

#include <QtTest>
#include <QFile>
#include <QSemaphore>
#include <QSet>
#include <QTemporaryDir>

#include <atomic>
#include <thread>
#include <vector>

namespace {
std::atomic<int> previousHandlerCalls{0};

void previousHandler(QtMsgType, const QMessageLogContext&, const QString&)
{
    ++previousHandlerCalls;
}

QtMessageHandler currentHandler()
{
    const auto handler = qInstallMessageHandler(nullptr);
    qInstallMessageHandler(handler);
    return handler;
}
} // namespace

class TestApplicationLogging : public QObject
{
    Q_OBJECT

private:
    QtMessageHandler m_testHandler = nullptr;

private slots:
    void init()
    {
        previousHandlerCalls = 0;
        m_testHandler = qInstallMessageHandler(previousHandler);
    }

    void cleanup()
    {
        qInstallMessageHandler(m_testHandler);
    }

    void outputPolicyAndHandlerRestoration()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("debug.log"));
        {
            const ApplicationLogging logging(path);
            QMessageLogger("/source/main.cpp", 42, "run", "app").warning("日本語のログ");
            QMessageLogger("C:\\source\\worker.cpp", 7, "work").critical("failure");
            QMessageLogger(nullptr, 0, nullptr).info("plain message");
        }

        QCOMPARE(previousHandlerCalls.load(), 0);
        QVERIFY(currentHandler() == &previousHandler);
        QMessageLogger(nullptr, 0, nullptr).warning("after shutdown");
        QCOMPARE(previousHandlerCalls.load(), 1);

#ifdef QT_DEBUG
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);
        const QRegularExpression timestamp(QStringLiteral(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} )"));
        for (const auto& line : lines) QVERIFY(timestamp.match(line).hasMatch());
        QVERIFY(lines[0].endsWith(QStringLiteral(" [WARN] [app] main.cpp:42 (run) 日本語のログ")));
        QVERIFY(lines[1].endsWith(QStringLiteral(" [ERROR] worker.cpp:7 (work) failure")));
        QVERIFY(lines[2].endsWith(QStringLiteral(" [INFO] plain message")));
#else
        QVERIFY(!QFile::exists(path));
#endif
    }

#ifdef QT_DEBUG
    void appendsToExistingFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("debug.log"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("existing log\n"), 13);
        file.close();
        {
            const ApplicationLogging logging(path);
            QMessageLogger(nullptr, 0, nullptr).debug("new message");
        }
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray contents = file.readAll();
        QVERIFY(contents.startsWith("existing log\n"));
        QVERIFY(contents.endsWith(" [DEBUG] new message\n"));
    }

    void failedOpenKeepsPreviousHandler()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            const ApplicationLogging logging(dir.filePath(QStringLiteral("missing/debug.log")));
            QVERIFY(currentHandler() == &previousHandler);
            QMessageLogger(nullptr, 0, nullptr).warning("fallback");
        }
        QCOMPARE(previousHandlerCalls.load(), 1);
        QVERIFY(currentHandler() == &previousHandler);
    }

    void concurrentMessagesRemainComplete()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("debug.log"));
        constexpr int threadCount = 6;
        constexpr int messagesPerThread = 200;
        {
            const ApplicationLogging logging(path);
            QSemaphore ready;
            QSemaphore start;
            std::vector<std::thread> workers;
            for (int thread = 0; thread < threadCount; ++thread) {
                workers.emplace_back([thread, &ready, &start]() {
                    ready.release();
                    start.acquire();
                    for (int message = 0; message < messagesPerThread; ++message) {
                        QMessageLogger(nullptr, 0, nullptr).warning("thread=%d message=%d", thread, message);
                    }
                });
            }
            ready.acquire(threadCount);
            start.release(threadCount);
            for (auto& worker : workers) worker.join();
        }
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList lines = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), threadCount * messagesPerThread);
        const QRegularExpression record(QStringLiteral(
            R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} \[WARN\] (thread=\d+ message=\d+)$)"));
        QSet<QString> messages;
        for (const auto& line : lines) {
            const auto match = record.match(line);
            QVERIFY2(match.hasMatch(), qPrintable(line));
            messages.insert(match.captured(1));
        }
        QCOMPARE(messages.size(), threadCount * messagesPerThread);
        for (int thread = 0; thread < threadCount; ++thread) {
            for (int message = 0; message < messagesPerThread; ++message) {
                QVERIFY(messages.contains(QStringLiteral("thread=%1 message=%2").arg(thread).arg(message)));
            }
        }
    }
#endif

    void shutdownWithPendingCallbacks()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("debug.log"));
        QtMessageHandler handler = nullptr;
        QSemaphore started;
        std::thread worker;
        {
            const ApplicationLogging logging(path);
            handler = currentHandler();
            QVERIFY(handler != nullptr);
            worker = std::thread([handler, &started]() {
                // Qt が復元前のハンドラを取得済みだった場合を再現する。
                handler(QtWarningMsg, QMessageLogContext(), QStringLiteral("before shutdown"));
                started.release();
                for (int i = 0; i < 1000; ++i) {
                    handler(QtWarningMsg, QMessageLogContext(), QStringLiteral("during shutdown"));
                }
            });
            started.acquire();
        }
        worker.join();
        handler(QtWarningMsg, QMessageLogContext(), QStringLiteral("after shutdown"));
        QVERIFY(currentHandler() == &previousHandler);
        QCOMPARE(previousHandlerCalls.load(), 0);
#ifdef QT_DEBUG
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray contents = file.readAll();
        QVERIFY(contents.contains(" [WARN] before shutdown\n"));
        QVERIFY(!contents.contains("after shutdown"));
#else
        QVERIFY(!QFile::exists(path));
#endif
    }
};

QTEST_GUILESS_MAIN(TestApplicationLogging)
#include "tst_applicationlogging.moc"
