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

#ifdef Q_OS_WIN
#include <qt_windows.h>

#include <tlhelp32.h>
#endif

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
        // sysctl は許可した名前だけ読める（インターフェースの一覧と CPU の数は読め、ほかのプロセスの引数・環境変数、
        // プロセスの一覧、コンピュータ名は読めない。監査 2026-11-21 S12）。プロセスの情報は自分のものだけで、libproc でも
        // プロセスの一覧と起動した側の実行ファイルの場所は読めない（kern.procargs2 と同じくサンドボックスの process-info）
        QCOMPARE(results.value(QStringLiteral("list_interfaces")).toString(), QStringLiteral("allowed"));
        QCOMPARE(results.value(QStringLiteral("sysctl_hw_ncpu")).toString(), QStringLiteral("allowed"));
        for (const char* name : {"sysctl_procargs_parent", "sysctl_proc_all", "sysctl_hostname", "proc_list_pids",
                                 "proc_pidpath_parent"}) {
            QVERIFY2(results.value(QLatin1String(name)).toString() == QStringLiteral("denied"), name);
        }
        // CoreFoundation の主バンドルは掛ける前に作ってあり、掛けた後に実行ファイルのあるディレクトリを読まない（読むと
        // 拒否され、起動のたびにログに残る。アプリのバンドルの中の通信プロセスでも同じ）
        QCOMPARE(results.value(QStringLiteral("main_bundle")).toString(), QStringLiteral("allowed"));
        // loopback を止める規則が、ほかの宛先（0.0.0.0・IPv4 射影・127.0.0.2・自分のアドレス）にも効くかは、結果を
        // 残すだけにする（OS が止めると確かめているのは 127.0.0.1 と ::1 だけ。protocol/ipc.md 6.3節。S14）
        for (const char* name : {"connect_unspecified_v4", "connect_unspecified_v6", "connect_mapped_loopback",
                                 "connect_loopback_other", "connect_own_address"}) {
            qInfo().noquote() << name << results.value(QLatin1String(name)).toString();
        }
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

    /// 同じ PC のどのプレイヤーの通信プロセスも同じ AppContainer に入る（別々だと同じ PC の中でつながらない）が、
    /// ほかの通信プロセスのプロセスとスレッドは開けない（監査 2026-11-21 S13）
    void peersCannotOpenEachOther()
    {
        QProcess first;
        first.setProgram(QStringLiteral(PEER_EXECUTABLE));
        QString error;
        const std::unique_ptr<PeerConfinement> firstConfinement = PeerConfinement::apply(
            &first, QStringLiteral(PEER_EXECUTABLE), PeerConfinement::Reach::LocalNetwork, &error);
        QVERIFY2(firstConfinement, qPrintable(error));
        first.start();
        QVERIFY(first.waitForStarted(10000));
        // 起動の直後に作られるスレッドも含めて調べる
        QVERIFY(!first.waitForFinished(1000));
        const DWORD pid = static_cast<DWORD>(first.processId());

        // 1つ目の通信プロセスのスレッドの番号
        QStringList threads;
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        QVERIFY(snapshot != INVALID_HANDLE_VALUE);
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID == pid) threads << QString::number(entry.th32ThreadID);
        }
        CloseHandle(snapshot);
        QVERIFY(!threads.isEmpty());

        int exitCode = -1;
        const QJsonObject report = runSelfTest(
            PeerConfinement::Reach::LocalNetwork,
            {QStringLiteral("pid=%1").arg(pid), QStringLiteral("tid=") + threads.join(QLatin1Char(','))}, &exitCode);
        QCOMPARE(exitCode, 0);
        const QJsonObject results = report.value(QStringLiteral("results")).toObject();
        QCOMPARE(results.value(QStringLiteral("open_process")).toString(), QStringLiteral("denied"));
        QCOMPARE(results.value(QStringLiteral("open_thread")).toString(), QStringLiteral("denied"));
        // 1つ目は動き続けている（2つ目の起動で、AppContainer のプロファイルを登録し直しても）
        QCOMPARE(first.state(), QProcess::Running);
    }

    /// ほかの通信プロセスが終わっても、起動の途中の通信プロセスの AppContainer のプロファイルは消えない（同じ PC の
    /// 2人の一方が対局を終えたのと同時に、もう一方が「接続」しても起動できる）
    void profileSurvivesWhileAnotherPeerStarts()
    {
        QProcess first;
        first.setProgram(QStringLiteral(PEER_EXECUTABLE));
        QString error;
        std::unique_ptr<PeerConfinement> firstConfinement = PeerConfinement::apply(
            &first, QStringLiteral(PEER_EXECUTABLE), PeerConfinement::Reach::Internet, &error);
        QVERIFY2(firstConfinement, qPrintable(error));
        first.start();
        QVERIFY(first.waitForStarted(10000));

        // 2つ目はプロファイルを登録したところで、まだプロセスを作っていない
        QProcess second;
        second.setProgram(QStringLiteral(PEER_EXECUTABLE));
        const std::unique_ptr<PeerConfinement> secondConfinement = PeerConfinement::apply(
            &second, QStringLiteral(PEER_EXECUTABLE), PeerConfinement::Reach::Internet, &error);
        QVERIFY2(secondConfinement, qPrintable(error));

        // 1つ目が終わる（プロファイルを使っている最後の通信プロセスなら、ここで消す）
        firstConfinement.reset();
        QVERIFY(first.waitForFinished(10000));

        second.start();
        QVERIFY2(second.waitForStarted(10000), qPrintable(second.errorString()));
        // 標準入力を開けたままなので、自分からは終わらない（制限を確かめて動き続けている）
        QVERIFY(!second.waitForFinished(1000));
    }
#endif
};

QTEST_MAIN(TestP2pSandbox)
#include "tst_p2p_sandbox.moc"
