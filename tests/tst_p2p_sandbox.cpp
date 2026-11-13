/// @file tst_p2p_sandbox.cpp
/// @brief 通信プロセスの権限の制限の試験（アプリと同じ起動のしかた PeerConfinement で起動し、禁止した操作が拒否される）
///
/// 実装仕様 10.4節・protocol/ipc.md 6章。通信プロセスの --sandbox-self-test に、試す相手（このプロセスが作った
/// ファイル・名前付きパイプ（macOS では Unix ドメインのソケット）・loopback の待受と、このプロセス自身）を渡し、
/// 結果を確かめる。

#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QProcess>
#include <QTcpServer>
#include <QTemporaryDir>

#include "peerconfinement.h"

class TestP2pSandbox : public QObject
{
    Q_OBJECT

private:
    /// 通信プロセスを制限つきで起動し、自己試験の結果を返す（起動できなければ空）
    static QJsonObject runSelfTest(PeerConfinement::Reach reach, const QStringList& targets, int* exitCode)
    {
        QProcess process;
        process.setProgram(QStringLiteral(PEER_EXECUTABLE));
        process.setArguments(QStringList{QStringLiteral("--sandbox-self-test")} + targets);
        QString error;
        const std::unique_ptr<PeerConfinement> confinement =
            PeerConfinement::apply(&process, QStringLiteral(PEER_EXECUTABLE), reach, &error);
        if (!confinement) {
            qWarning() << "confinement:" << error;
            return {};
        }
        process.start();
        if (!process.waitForStarted(10000)) {
            qWarning() << "start:" << process.errorString();
            return {};
        }
        process.closeWriteChannel();
        if (!process.waitForFinished(30000)) return {};
        *exitCode = process.exitCode();
        const QByteArray output = process.readAllStandardOutput();
        qInfo().noquote() << output.trimmed();
        return QJsonDocument::fromJson(output).object();
    }

private slots:
    void initTestCase()
    {
        if (!PeerConfinement::isSupported()) QSKIP("the communication process cannot be confined on this OS");
        QVERIFY2(QFileInfo(QStringLiteral(PEER_EXECUTABLE)).isExecutable(), PEER_EXECUTABLE);
    }

    void selfTestDeniesForbiddenOperations_data()
    {
        QTest::addColumn<bool>("localNetwork");
        QTest::newRow("internet") << false;
        QTest::newRow("local network") << true;
    }

    void selfTestDeniesForbiddenOperations()
    {
        QFETCH(bool, localNetwork);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("secret.txt"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("secret");
        file.close();
        QLocalServer pipe;
        const QString pipeName = QStringLiteral("sbq-sandbox-test-%1").arg(QCoreApplication::applicationPid());
        QVERIFY(pipe.listen(pipeName));
#ifdef Q_OS_MACOS
        // Unix ドメインのソケットは、ソケットのファイルの場所で渡す
        const QString pipeTarget = pipe.fullServerName();
#else
        const QString pipeTarget = pipeName;
#endif
        QTcpServer tcp;
        QVERIFY(tcp.listen(QHostAddress::LocalHost));

        int exitCode = -1;
        const QJsonObject report = runSelfTest(
            localNetwork ? PeerConfinement::Reach::LocalNetwork : PeerConfinement::Reach::Internet,
            {QStringLiteral("file=") + QDir::toNativeSeparators(filePath), QStringLiteral("pipe=") + pipeTarget,
             QStringLiteral("pid=%1").arg(QCoreApplication::applicationPid()),
             QStringLiteral("port=%1").arg(tcp.serverPort())},
            &exitCode);
        QCOMPARE(exitCode, 0);
        QVERIFY(report.value(QStringLiteral("applied")).toBool());
        const QJsonObject results = report.value(QStringLiteral("results")).toObject();
#ifdef Q_OS_WIN
        // loopback への接続も OS が止める（ローカルのネットワークを使うときも）
        const QStringList denied{QStringLiteral("read_file"),          QStringLiteral("write_file"),
                                 QStringLiteral("exec"),               QStringLiteral("connect_named_pipe"),
                                 QStringLiteral("open_process"),       QStringLiteral("connect_loopback_tcp")};
#elif defined(Q_OS_MACOS)
        const QStringList denied{QStringLiteral("read_file"),           QStringLiteral("write_file"),
                                 QStringLiteral("exec"),                QStringLiteral("fork"),
                                 QStringLiteral("connect_unix_socket"), QStringLiteral("signal_parent")};
#else
        const QStringList denied{QStringLiteral("read_file"),   QStringLiteral("write_file"),
                                 QStringLiteral("exec"),        QStringLiteral("fork"),
                                 QStringLiteral("unix_socket"), QStringLiteral("signal_parent")};
#endif
        for (const QString& name : denied) {
            QVERIFY2(results.value(name).toString() == QStringLiteral("denied"), qPrintable(name));
        }
#ifdef Q_OS_MACOS
        // loopback への発信は、ローカルのネットワークを使わないときだけサンドボックスが止める。loopback でない待受は
        // どちらでもできる
        QCOMPARE(results.value(QStringLiteral("connect_loopback_tcp")).toString(),
                 localNetwork ? QStringLiteral("allowed") : QStringLiteral("denied"));
        QCOMPARE(results.value(QStringLiteral("listen_tcp")).toString(), QStringLiteral("allowed"));
        const QStringList mechanisms = report.value(QStringLiteral("mechanisms")).toVariant().toStringList();
        QCOMPARE(mechanisms.contains(QStringLiteral("seatbelt_no_loopback")), !localNetwork);
#endif
        // 禁止した操作は、実際に何も残していない
        QVERIFY(!QFileInfo::exists(filePath + QStringLiteral(".created")));
        QVERIFY(!pipe.hasPendingConnections());
        QVERIFY(!tcp.waitForNewConnection(100));
    }

#ifdef Q_OS_WIN
    /// 制限を掛けずに起動された通信プロセスは動かない（終了コード 3。Windows では制限は起動する側が掛ける）
    void refusesToRunUnconfined()
    {
        QProcess process;
        process.start(QStringLiteral(PEER_EXECUTABLE), {});
        QVERIFY(process.waitForStarted(10000));
        QVERIFY(process.waitForFinished(30000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 3);
        // hello の応答を返していない
        QVERIFY(process.readAllStandardOutput().isEmpty());
    }

    /// 起動した側が制限（ジョブ）を手放すと、通信プロセスは終わる
    void endsWhenTheConfinementIsReleased()
    {
        QProcess process;
        process.setProgram(QStringLiteral(PEER_EXECUTABLE));
        QString error;
        std::unique_ptr<PeerConfinement> confinement = PeerConfinement::apply(
            &process, QStringLiteral(PEER_EXECUTABLE), PeerConfinement::Reach::Internet, &error);
        QVERIFY2(confinement, qPrintable(error));
        process.start();
        QVERIFY(process.waitForStarted(10000));
        // 標準入力を開けたままなので、自分からは終わらない
        QVERIFY(!process.waitForFinished(1000));
        confinement.reset();
        QVERIFY(process.waitForFinished(10000));
    }
#endif
};

QTEST_MAIN(TestP2pSandbox)
#include "tst_p2p_sandbox.moc"
