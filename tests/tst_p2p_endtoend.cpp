/// @file tst_p2p_endtoend.cpp
/// @brief P2P対局の端から端までの試験（本物の通信プロセス2つを sbq-local-1 で起動し、招待から終局と証跡の保存まで）
///
/// 実装仕様 13章の P2 の完了条件（招待から終局・棋譜と証跡の保存まで）を、同じ PC の2つの部品の組で確かめる。
/// 通信プロセスはビルドした shogiboardq-peer を使う（デバッグビルドの SHOGIBOARDQ_PEER_EXECUTABLE）。

#include <QtTest>

#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>

#include <memory>

#include "appactivitystate.h"
#include "gameboard.h"
#include "matchrecordsink.h"
#include "p2pevidenceverifier.h"
#include "p2pgamecoordinator.h"
#include "p2pidentitystore.h"
#include "p2plobbyservice.h"
#include "p2pplayerprofile.h"
#include "p2precordstore.h"
#include "shogiboard.h"
#include "shogiclock.h"

using namespace P2p;

namespace {

class RecordingSink final : public MatchRecordSink
{
public:
    void beginRecord(const MatchRecordStart& start) override
    {
        started = start;
        entries.clear();
        finished = false;
    }
    bool canAppendRecord() const override { return !finished; }
    void appendRecord(const RecordEntry& entry) override { entries.append(entry); }
    bool canUndoRecords(int) const override { return false; }
    void undoRecords(int) override {}
    void finishRecord(bool) override { finished = true; }

    MatchRecordStart started;
    QList<RecordEntry> entries;
    bool finished = false;
};

/// アプリ1つ分の部品
struct App {
    explicit App(const QString& root)
        : game(P2pGameCoordinator::Deps{&board, &clock, &activity})
        , profile(root)
        , lobby(P2pLobbyService::Deps{&activity, &game}, &profile)
    {
        game.setRecordSink(&sink);
    }

    GameBoard board;
    ShogiClock clock;
    AppActivityState activity;
    RecordingSink sink;
    P2pGameCoordinator game;
    P2pPlayerProfile profile;
    P2pLobbyService lobby;
};

} // namespace

class TestP2pEndToEnd : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    static bool createIdentity(App& app)
    {
        if (!app.profile.open()) return false;
        app.profile.identity()->create(P2pIdentityStore::Method::Plain);
        return app.profile.identity()->isUnlocked();
    }

    /// 2人が sbq-local-1 に参加し、招待から対局の開始まで進める（招待した側が先手）
    static void startGame(App& host, App& guest)
    {
        QCOMPARE(host.lobby.begin(QStringLiteral("sbq-local-1"), {}, false), P2pLobbyService::StartFailure::None);
        QCOMPARE(guest.lobby.begin(QStringLiteral("sbq-local-1"), {}, false), P2pLobbyService::StartFailure::None);
        QVERIFY(host.activity.is(AppActivityState::Activity::DuringP2pConnection));
        QTRY_COMPARE_WITH_TIMEOUT(host.lobby.status(), P2pLobbyService::Status::Online, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(guest.lobby.status(), P2pLobbyService::Status::Online, 15000);
        // 通信プロセスは権限の制限を掛けて動いている
        QVERIFY(host.lobby.sandbox().value(QStringLiteral("applied")).toBool());

        // 招待を作り、相手が読んで接続する（招待文字列は対局・UI側では読まない）
        QSignalSpy created(&host.lobby, &P2pLobbyService::inviteCreated);
        host.lobby.createInvite(QStringLiteral("招待した側"), TimeControlTerms{60'000, 10'000}, SidePreference::Black);
        QTRY_COMPARE_WITH_TIMEOUT(created.count(), 1, 10000);
        const QString invite = created.first().at(0).toString();
        QVERIFY(invite.startsWith(QStringLiteral("sbqinv1.")));
        QSignalSpy inspected(&guest.lobby, &P2pLobbyService::inviteInspected);
        guest.lobby.inspectInvite(invite);
        QTRY_COMPARE_WITH_TIMEOUT(inspected.count(), 1, 10000);
        guest.lobby.connectInvite(invite, false, QStringLiteral("招待された側"));

        // 双方が条件確認画面の段階まで進む
        QTRY_VERIFY_WITH_TIMEOUT(guest.game.negotiation()
                                     && guest.game.negotiation()->stage() == Negotiation::Stage::Confirming, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(host.game.negotiation()
                                     && host.game.negotiation()->stage() == Negotiation::Stage::Confirming, 20000);
        QCOMPARE(guest.game.opponentDisplayName(), QStringLiteral("招待した側"));
        // 招待した側の先手希望が通る
        QVERIFY(host.game.negotiation()->iAmBlack());
        QVERIFY(!guest.game.negotiation()->opponentVerified());

        QSignalSpy hostStarted(&host.game, &P2pGameCoordinator::gameStarted);
        QSignalSpy guestStarted(&guest.game, &P2pGameCoordinator::gameStarted);
        host.game.confirmStart();
        guest.game.confirmStart();
        QTRY_COMPARE_WITH_TIMEOUT(hostStarted.count(), 1, 20000);
        QTRY_COMPARE_WITH_TIMEOUT(guestStarted.count(), 1, 20000);
        QVERIFY(host.activity.is(AppActivityState::Activity::DuringP2pGame));
        QTRY_VERIFY_WITH_TIMEOUT(host.game.isHumanToMove(), 20000);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(Crypto::initialize());
        QVERIFY(m_dir.isValid());
#ifndef Q_OS_LINUX
        QSKIP("the communication process is sandboxed only on Linux");
#endif
        QVERIFY2(QFileInfo(QStringLiteral(PEER_EXECUTABLE)).isExecutable(), PEER_EXECUTABLE);
        // 通信プロセスは試験の実行ファイルの隣に置いてある（CMake）。デバッグビルドでは差し替えでも同じものを指す
        qputenv("SHOGIBOARDQ_PEER_EXECUTABLE", QByteArrayLiteral(PEER_EXECUTABLE));
    }

    /// ほかの動作（ローカル対局など）の最中は接続を始めず、その動作状態を取り上げない。以前は、開いたままの P2P の
    /// ウィンドウで「接続」を押すと、進行中の対局の動作状態を取り上げ、対局が裏で続いていた（監査 2026-11-13 P15）
    void beginRefusesWhileAnotherActivity()
    {
        App app(m_dir.filePath(QStringLiteral("busy")));
        QVERIFY(createIdentity(app));
        QObject localGame;
        ActivityLease gameLease = app.activity.begin(AppActivityState::Activity::DuringGame, &localGame);
        QVERIFY(!app.lobby.canBegin());
        QCOMPARE(app.lobby.begin(QStringLiteral("sbq-local-1"), {}, false), P2pLobbyService::StartFailure::Busy);
        QVERIFY(gameLease.isCurrent());
        QVERIFY(app.activity.is(AppActivityState::Activity::DuringGame));
        QCOMPARE(app.lobby.status(), P2pLobbyService::Status::Off);
        // 対局を終えれば接続できる（変わったことを statusChanged で知らせる）
        QSignalSpy changed(&app.lobby, &P2pLobbyService::statusChanged);
        gameLease.reset();
        QVERIFY(app.lobby.canBegin());
        QVERIFY(changed.count() > 0);
    }

    void inviteGameOverRealPeers()
    {
        App host(m_dir.filePath(QStringLiteral("host")));
        App guest(m_dir.filePath(QStringLiteral("guest")));
        QVERIFY(createIdentity(host));
        QVERIFY(createIdentity(guest));

        startGame(host, guest);
        if (QTest::currentTestFailed()) return;

        // 着手: 承認を受けてから両方の盤に指す
        const char* moves[] = {"7g7f", "3c3d", "2g2f", "8c8d"};
        int ply = 0;
        for (const char* move : moves) {
            App& mover = ply % 2 == 0 ? host : guest;
            App& other = ply % 2 == 0 ? guest : host;
            QTRY_VERIFY_WITH_TIMEOUT(mover.game.isHumanToMove(), 20000);
            QCOMPARE(mover.game.playMove(UsiMove::fromString(QString::fromLatin1(move))), GameChannel::MoveResult::Sent);
            ++ply;
            QTRY_COMPARE_WITH_TIMEOUT(mover.sink.entries.size(), ply, 20000);
            QTRY_COMPARE_WITH_TIMEOUT(other.sink.entries.size(), ply, 20000);
        }
        QCOMPARE(host.board.board()->position().toSfen(), guest.board.board()->position().toSfen());

        // 回線が切れた: 招待した側が接続を閉じる。招待された側（申込側）が接続し直し、照合して対局を続ける
        const quint64 before = host.game.currentConnection();
        QVERIFY(before != 0);
        host.lobby.closeConnection(before);
        QTRY_VERIFY_WITH_TIMEOUT(guest.game.match()->phase() == Match::Phase::Reconnecting, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(host.game.currentConnection() != 0 && host.game.currentConnection() != before, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(host.game.match()->phase() == Match::Phase::Playing
                                     && guest.game.match()->phase() == Match::Phase::Playing, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(host.game.isHumanToMove(), 20000);
        QCOMPARE(host.game.playMove(UsiMove::fromString(u"2f2e")), GameChannel::MoveResult::Sent);
        QTRY_COMPARE_WITH_TIMEOUT(guest.sink.entries.size(), 5, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(guest.game.isHumanToMove(), 20000);
        QCOMPARE(guest.game.playMove(UsiMove::fromString(u"8d8e")), GameChannel::MoveResult::Sent);
        QTRY_COMPARE_WITH_TIMEOUT(host.sink.entries.size(), 6, 20000);

        // 先手（招待した側）の投了
        QSignalSpy hostFinished(&host.game, &P2pGameCoordinator::gameFinished);
        QSignalSpy guestFinished(&guest.game, &P2pGameCoordinator::gameFinished);
        QTRY_VERIFY_WITH_TIMEOUT(host.game.canResign(), 20000);
        host.game.resign();
        QTRY_COMPARE_WITH_TIMEOUT(hostFinished.count(), 1, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(guestFinished.count(), 1, 30000);
        for (App* app : {&host, &guest}) {
            QCOMPARE(app->game.match()->settlement(), Settlement::MutuallySigned);
            QVERIFY(app->sink.finished);
            QCOMPARE(app->sink.entries.last().terminal, TerminalType::Resign);
            QFile file(app->game.lastEvidencePath());
            QVERIFY(file.open(QIODevice::ReadOnly));
            const EvidenceReport report = EvidenceVerifier::verify(file.readAll());
            QVERIFY2(report.isValid(), qPrintable(report.error));
            QCOMPARE(report.category, EvidenceReport::Category::MutuallySigned);
            QCOMPARE(report.result->outcome, Outcome::WhiteWins);
            // 対局が終わったら P2P接続中に戻る
            QTRY_VERIFY_WITH_TIMEOUT(app->activity.is(AppActivityState::Activity::DuringP2pConnection), 5000);
        }

        host.lobby.end();
        guest.lobby.end();
        QTRY_VERIFY_WITH_TIMEOUT(host.lobby.status() == P2pLobbyService::Status::Off, 10000);
        QVERIFY(host.activity.is(AppActivityState::Activity::Idle));
    }

    /// アプリを終えるとき（end の後はイベントループが回らない）も、対局の相手に中断が届く。以前は通信プロセスに要求を
    /// 書き終える前に終わり、通信プロセスも親の終了で止まるので、相手はつなぎ直しを待ち続けた（監査 2026-11-13）
    void quittingDuringGameTellsTheOpponent()
    {
        auto host = std::make_unique<App>(m_dir.filePath(QStringLiteral("quit-host")));
        App guest(m_dir.filePath(QStringLiteral("quit-guest")));
        QVERIFY(createIdentity(*host));
        QVERIFY(createIdentity(guest));
        startGame(*host, guest);
        if (QTest::currentTestFailed()) return;

        QSignalSpy guestFinished(&guest.game, &P2pGameCoordinator::gameFinished);
        // アプリの終了と同じ順（P2pGameUiController::shutdown）。この後、招待した側のイベントループは回さずに部品を消す
        // （アプリの終了では、通信プロセスも親の終了で止まる）
        host->lobby.end();
        host->lobby.waitForExit(1500);
        host.reset();
        // 招待された側は、つなぎ直しを待たずに中断で終わる
        QTRY_COMPARE_WITH_TIMEOUT(guestFinished.count(), 1, 10000);
        QCOMPARE(guest.game.match()->settlement(), Settlement::Aborted);
        guest.lobby.end();
    }

    /// 開始のジャーナルを書けず、対局が始まった呼び出しの中で終わっても、P2P の接続は続く（監査 2026-11-12 B1）。
    /// 「P2P対局中」を始めたときの、ロビーへの取り上げの知らせは次のイベントループで届く。そのときロビーは
    /// 「P2P接続中」を取り直しているので、古い知らせで通信プロセスを止めない
    void journalFailureAtStartKeepsConnection()
    {
        App host(m_dir.filePath(QStringLiteral("journal-host")));
        App guest(m_dir.filePath(QStringLiteral("journal-guest")));
        QVERIFY(createIdentity(host));
        QVERIFY(createIdentity(guest));
        QCOMPARE(host.lobby.begin(QStringLiteral("sbq-local-1"), {}, false), P2pLobbyService::StartFailure::None);
        QCOMPARE(guest.lobby.begin(QStringLiteral("sbq-local-1"), {}, false), P2pLobbyService::StartFailure::None);
        QTRY_COMPARE_WITH_TIMEOUT(host.lobby.status(), P2pLobbyService::Status::Online, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(guest.lobby.status(), P2pLobbyService::Status::Online, 15000);

        QSignalSpy created(&host.lobby, &P2pLobbyService::inviteCreated);
        host.lobby.createInvite(QStringLiteral("招待した側"), TimeControlTerms{60'000, 10'000}, SidePreference::None);
        QTRY_COMPARE_WITH_TIMEOUT(created.count(), 1, 10000);
        guest.lobby.connectInvite(created.first().at(0).toString(), false, QStringLiteral("招待された側"));
        QTRY_VERIFY_WITH_TIMEOUT(host.game.negotiation()
                                     && host.game.negotiation()->stage() == Negotiation::Stage::Confirming, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(guest.game.negotiation()
                                     && guest.game.negotiation()->stage() == Negotiation::Stage::Confirming, 20000);

        // 招待した側のジャーナルの場所にフォルダを置き、開始の件を書けなくする
        const QString journal = QDir(host.profile.records()->root())
                                    .filePath(QStringLiteral("journal/%1.journal")
                                                  .arg(QString::fromLatin1(host.game.negotiation()->gameId().toHex())));
        QVERIFY(QDir().mkpath(journal));

        QSignalSpy hostFinished(&host.game, &P2pGameCoordinator::gameFinished);
        host.game.confirmStart();
        guest.game.confirmStart();
        QTRY_COMPARE_WITH_TIMEOUT(hostFinished.count(), 1, 20000);
        QCOMPARE(host.game.match()->settlement(), Settlement::Aborted);
        QCOMPARE(host.game.match()->endingError(), std::optional<ErrorCode>(ErrorCode::StorageError));

        // 遅れて届く取り上げの知らせを処理させても、接続したまま「P2P接続中」に戻っている
        QTest::qWait(200);
        QCOMPARE(host.lobby.status(), P2pLobbyService::Status::Online);
        QVERIFY(host.activity.is(AppActivityState::Activity::DuringP2pConnection));

        host.lobby.end();
        guest.lobby.end();
        QTRY_VERIFY_WITH_TIMEOUT(host.lobby.status() == P2pLobbyService::Status::Off
                                     && guest.lobby.status() == P2pLobbyService::Status::Off, 10000);
    }

    /// 「直接接続を使わない」: 協力ノード（--service --relay）の中継だけを通って、招待から対局し、合意の引き分けで終える。
    /// 招待された側は相手の直接のアドレスへつながない（通信プロセスの試験 relay_only_* が宛先の規則を確かめる）
    void relayOnlyGameThroughServiceNode()
    {
        // 協力ノードを固定のポートで起動し、標準エラーの最初の行から PeerId を読む
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost));
        const quint16 port = probe.serverPort();
        probe.close();
        const QString listen = QStringLiteral("/ip4/127.0.0.1/tcp/%1").arg(port);
        QProcess service;
        service.setProcessEnvironment(QProcessEnvironment());
        service.start(QStringLiteral(PEER_EXECUTABLE),
                      {QStringLiteral("--service"), QStringLiteral("--network"), QStringLiteral("sbq-local-1"),
                       QStringLiteral("--identity"), m_dir.filePath(QStringLiteral("service.key")), QStringLiteral("--listen"),
                       listen, QStringLiteral("--relay")});
        service.setReadChannel(QProcess::StandardError);
        QVERIFY(service.waitForStarted(5000));
        QTRY_VERIFY_WITH_TIMEOUT((service.waitForReadyRead(100), service.canReadLine()), 10000);
        const QString line = QString::fromUtf8(service.readLine()).trimmed();
        QVERIFY2(line.contains(QStringLiteral("service started")), qPrintable(line));
        const QString bootstrap = listen + QStringLiteral("/p2p/") + line.section(QLatin1Char(' '), -1);

        App host(m_dir.filePath(QStringLiteral("relay-host")));
        App guest(m_dir.filePath(QStringLiteral("relay-guest")));
        QVERIFY(createIdentity(host));
        QVERIFY(createIdentity(guest));
        // 招待する側は直接接続も使う設定、招待される側は「直接接続を使わない」
        QCOMPARE(host.lobby.begin(QStringLiteral("sbq-local-1"), {bootstrap}, false), P2pLobbyService::StartFailure::None);
        QCOMPARE(guest.lobby.begin(QStringLiteral("sbq-local-1"), {bootstrap}, true), P2pLobbyService::StartFailure::None);
        QTRY_COMPARE_WITH_TIMEOUT(host.lobby.status(), P2pLobbyService::Status::Online, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(guest.lobby.status(), P2pLobbyService::Status::Online, 15000);
        QVERIFY(guest.lobby.relayOnly());
        // 中継の予約は、直接つなげる側も取る（直接接続を使わない相手が、中継経由でつなげるように）
        QTRY_VERIFY_WITH_TIMEOUT(host.lobby.relayReservations() >= 1 && guest.lobby.relayReservations() >= 1, 20000);

        QSignalSpy created(&host.lobby, &P2pLobbyService::inviteCreated);
        host.lobby.createInvite(QStringLiteral("招待した側"), TimeControlTerms{300'000, 30'000}, SidePreference::None);
        QTRY_COMPARE_WITH_TIMEOUT(created.count(), 1, 10000);
        const int hostAddresses = created.first().at(2).toInt();
        QVERIFY(hostAddresses >= 2);   // 直接のアドレスと、中継経由のアドレス
        QSignalSpy inspected(&guest.lobby, &P2pLobbyService::inviteInspected);
        guest.lobby.inspectInvite(created.first().at(0).toString());
        QTRY_COMPARE_WITH_TIMEOUT(inspected.count(), 1, 10000);
        guest.lobby.connectInvite(created.first().at(0).toString(), false, QStringLiteral("招待された側"));

        QTRY_VERIFY_WITH_TIMEOUT(host.game.negotiation()
                                     && host.game.negotiation()->stage() == Negotiation::Stage::Confirming, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(guest.game.negotiation()
                                     && guest.game.negotiation()->stage() == Negotiation::Stage::Confirming, 20000);
        QVERIFY(guest.game.negotiation()->sidesByCoin());
        host.game.confirmStart();
        guest.game.confirmStart();
        QTRY_VERIFY_WITH_TIMEOUT(host.game.isInProgress() && guest.game.isInProgress(), 20000);
        // どちらから見ても、経路は中継
        QVERIFY(host.game.viaRelay());
        QVERIFY(guest.game.viaRelay());

        App& black = host.game.match()->iAmBlack() ? host : guest;
        App& white = host.game.match()->iAmBlack() ? guest : host;
        QTRY_VERIFY_WITH_TIMEOUT(black.game.isHumanToMove(), 20000);
        QCOMPARE(black.game.playMove(UsiMove::fromString(u"2g2f")), GameChannel::MoveResult::Sent);
        QTRY_VERIFY_WITH_TIMEOUT(white.game.isHumanToMove(), 20000);
        white.game.offerDraw();
        QSignalSpy offered(&black.game, &P2pGameCoordinator::offerReceived);
        QTRY_COMPARE_WITH_TIMEOUT(offered.count(), 1, 20000);
        black.game.answerOffer(true);
        QTRY_VERIFY_WITH_TIMEOUT(black.game.match()->settlement() == Settlement::MutuallySigned
                                     && white.game.match()->settlement() == Settlement::MutuallySigned, 30000);
        QCOMPARE(black.game.match()->agreedResult()->cause, Cause::AgreedDraw);
        QCOMPARE(black.sink.entries.last().terminal, TerminalType::Draw);
        // 対局が終わっても、直接の経路には切り替わっていない
        QVERIFY(host.game.viaRelay());
        QVERIFY(guest.game.viaRelay());

        host.lobby.end();
        guest.lobby.end();
        QTRY_VERIFY_WITH_TIMEOUT(host.lobby.status() == P2pLobbyService::Status::Off
                                     && guest.lobby.status() == P2pLobbyService::Status::Off, 10000);
        service.kill();
        service.waitForFinished(3000);
    }
};

QTEST_MAIN(TestP2pEndToEnd)
#include "tst_p2p_endtoend.moc"
