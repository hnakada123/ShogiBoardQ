/// @file tst_csagamecoordinator.cpp
/// @brief CsaGameCoordinator の指し手進行と棋譜行の組み立てを、ローカルの偽 CSA サーバーで確かめる
///
/// 実際のソケット（QTcpServer）でログイン・対局条件・指し手・終局を流し、
/// 残り時間・手数・手番・局面列・棋譜の受け手（MatchRecordSink）に書いた行・動作状態と、
/// 本体と同じ受け手（GameSession）を通した棋譜への追記を検証する。

#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <algorithm>

#include "csagamecoordinator.h"
#include "engineslots.h"
#include "engineprocessmanager.h"
#include "usi.h"
#include "humanturnpolicy.h"
#include "csagameprogress.h"
#include "appactivitystate.h"
#include "kifudocument.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphpresenter.h"
#include "gamesession.h"
#include "kifuloader.h"
#include "matchcoordinator.h"
#include "matchrecordsink.h"
#include "playerinfopresenter.h"
#include "sessionresetcoordinator.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "gameboard.h"

namespace {

/// 1接続だけ受け付ける偽 CSA サーバー
struct FakeCsaServer {
    QTcpServer server;
    QTcpSocket* peer = nullptr;
    QStringList lines;
    QByteArray buffer;

    bool listen() { return server.listen(QHostAddress::LocalHost); }
    quint16 port() const { return server.serverPort(); }
    bool accept()
    {
        if (!QTest::qWaitFor([this] { return server.hasPendingConnections(); }, 3000)) return false;
        peer = server.nextPendingConnection();
        return peer != nullptr;
    }
    void pump()
    {
        if (!peer) return;
        buffer += peer->readAll();
        qsizetype idx;
        while ((idx = buffer.indexOf('\n')) != -1) {
            lines.append(QString::fromUtf8(buffer.left(idx)));
            buffer = buffer.mid(idx + 1);
        }
    }
    bool waitForLine(const QString& text, int timeoutMs = 3000)
    {
        return QTest::qWaitFor([this, &text] {
            pump();
            return lines.contains(text);
        }, timeoutMs);
    }
    void send(const QByteArray& text)
    {
        peer->write(text + '\n');
        peer->flush();
    }
};

/// 司令塔が書いた棋譜（新しい棋譜・行・確定）を覚える受け手
struct RecordedSink final : MatchRecordSink {
    void beginRecord(const MatchRecordStart& start) override { begins.append(start); }
    bool canAppendRecord() const override { return true; }
    void appendRecord(const RecordEntry& entry) override { records.append(entry); }
    bool canUndoRecords(int) const override { return false; }
    void undoRecords(int) override {}
    void finishRecord(bool autoSave) override { finishes.append(autoSave); }

    QList<MatchRecordStart> begins;
    QList<RecordEntry> records;
    QList<bool> finishes;   ///< finishRecord の autoSave
};

/// 盤・時計・局面列・動作状態・棋譜の受け手と司令塔をそろえ、偽サーバーと対局を始める
struct Harness {
    GameBoard gc;
    ShogiClock clock;
    EngineSlots engines;
    AppActivityState activity;
    RecordedSink sink;
    CsaGameCoordinator coordinator{CsaGameCoordinator::Deps{&gc, &clock, &engines, &activity}};
    FakeCsaServer server;

    Harness()
    {
        gc.newGame(SfenUtils::hirateSfen());
        coordinator.setRecordSink(&sink);
    }

    /// 対局の手順（開始局面と、確認された手）
    const RecordLine& line() const { return coordinator.record(); }
    int moveCount() const { return coordinator.record().moveCount(); }

    /// ログインから START まで進める（持時間 600 秒・加算 10 秒）
    bool start(const QByteArray& yourTurn, const QList<QByteArray>& positionMoves = {}, bool engine = false)
    {
        if (!server.listen()) return false;
        CsaGameCoordinator::StartOptions options;
        options.host = QStringLiteral("127.0.0.1");
        options.port = server.port();
        options.username = QStringLiteral("user");
        options.password = QStringLiteral("pass");
        if (engine) {
            options.playerType = CsaGameCoordinator::PlayerType::Engine;
            options.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
            options.engineName = QStringLiteral("CSA test engine");
        }
        options.keepAliveEnabled = false;
        options.gameLimit = 0;  // 終局後に LOGOUT せず、結果表示もモーダルにしない
        coordinator.startGame(options);
        if (!server.accept()) return false;
        if (!server.waitForLine(QStringLiteral("LOGIN user pass"))) return false;
        server.send("LOGIN:user OK");
        server.send("BEGIN Game_Summary");
        server.send("Protocol_Version:1.2");
        server.send("Protocol_Mode:Server");
        server.send("Format:Shogi 1.0");
        server.send("Game_ID:g1");
        server.send("Name+:Sente");
        server.send("Name-:Gote");
        server.send("Your_Turn:" + yourTurn);
        server.send(positionMoves.size() % 2 == 0 ? "To_Move:+" : "To_Move:-");
        server.send("BEGIN Time");
        server.send("Time_Unit:1sec");
        server.send("Total_Time:600");
        server.send("Increment:10");
        server.send("END Time");
        server.send("BEGIN Position");
        server.send("PI");
        server.send("+");
        for (const QByteArray& move : positionMoves) server.send(move);
        server.send("END Position");
        server.send("END Game_Summary");
        if (!server.waitForLine(QStringLiteral("AGREE g1"))) return false;
        server.send("START:g1");
        return QTest::qWaitFor([this] {
            return coordinator.gameState() == CsaGameCoordinator::GameState::InGame;
        }, 3000);
    }

    /// 確認前の盤面は変えず、指し手の要求だけを渡す。
    void playHumanMove(const QString& usi)
    {
        const auto move = UsiMove::parse(usi);
        QVERIFY(move.has_value());
        coordinator.onHumanMove(*move);
    }

};

} // namespace

class TestCsaGameCoordinator : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
    Harness* m_harness = nullptr;       ///< 知らせを受けた時点の状態を見る対局
    bool m_startedAfterRecord = false;  ///< 開始の知らせの時点で、棋譜を作って通信対局中にしていた
    bool m_endedAfterFinish = false;    ///< 終局の知らせの時点で、棋譜を確定して待機に戻していた

public:
    void observeStarted()
    {
        m_startedAfterRecord = m_harness && m_harness->sink.begins.size() == 1
            && m_harness->activity.is(AppActivityState::Activity::DuringCsaGame);
    }
    void observeEnded()
    {
        m_endedAfterFinish = m_harness && m_harness->sink.finishes.size() == 1
            && m_harness->activity.is(AppActivityState::Activity::Idle);
    }

private slots:
    void engineMoveIsCommittedOnlyOnConfirmation()
    {
        Harness h;
        QSignalSpy committed(&h.gc, &GameBoard::moveCommitted);
        QVERIFY(h.start("-", {}, true));
        h.server.send("+7776FU,T1");
        QVERIFY(h.server.waitForLine(QStringLiteral("-8384FU")));
        QCOMPARE(h.sink.records.size(), 1);
        QCOMPARE(committed.size(), 1);
        QCOMPARE(h.moveCount(), 1);
        QCOMPARE(h.moveCount(), 1);
        QCOMPARE(h.gc.board()->pieceCharacter(8, 3), Piece::WhitePawn);
        QCOMPARE(h.coordinator.turnState(), CsaGameCoordinator::TurnState::AwaitingConfirmation);
        h.server.send("-8384FU,T2");
        QTRY_COMPARE(h.sink.records.size(), 2);
        QCOMPARE(committed.size(), 2);
        QCOMPARE(h.moveCount(), 2);
        QCOMPARE(h.moveCount(), 2);
        QCOMPARE(h.gc.board()->pieceCharacter(8, 4), Piece::WhitePawn);
        QCOMPARE(h.sink.records.at(1).move.move.toString(), QStringLiteral("8c8d"));
    }

    /// 自分のエンジンはサーバーが示した残り時間から持ち時間を読み、終局でエンジンの枠を空ける（次局は起動し直す）
    void engineReadsServerTimeAndIsReleasedAtGameEnd()
    {
        Harness h;
        QVERIFY(h.start("-", {}, true));
        QVERIFY(h.engines.engineInSlot(1));
        // 時間の出どころは対局の残り時間（加算を含まない）・決まり
        const MatchSearchTiming timing = h.coordinator.snapshot(Turn::White, false);
        const UsiGoTimeInput expected = h.coordinator.progress().engineTiming(1000);
        QCOMPARE(timing.input.wtimeMs, expected.wtimeMs);
        QCOMPARE(timing.input.btimeMs, expected.btimeMs);
        QCOMPARE(timing.input.timeControl.white.incrementMs, qint64(10000));
        // 手番の考慮時間は手番の側だけが数える
        h.server.send("+7776FU,T1");
        QVERIFY(h.server.waitForLine(QStringLiteral("-8384FU")));
        QCOMPARE(h.coordinator.elapsedThisTurn(Turn::Black), 0);
        h.server.send("-8384FU,T2");
        QTRY_COMPARE(h.sink.records.size(), 2);
        QTest::qWait(50);
        QVERIFY(h.coordinator.elapsedThisTurn(Turn::Black) > 0);
        QCOMPARE(h.coordinator.elapsedThisTurn(Turn::White), 0);

        h.server.send("%TORYO,T3");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        QVERIFY(!h.engines.engineInSlot(1));
    }

    /// 対局の進み具合は、接続の段階をクライアントの状態から、対局中と終局後を司令塔の段階から求める
    void gameStateFollowsConnectionAndGame()
    {
        Harness h;
        QSignalSpy states(&h.coordinator, &CsaGameCoordinator::gameStateChanged);
        QVERIFY(h.start("+"));
        QList<CsaGameCoordinator::GameState> seen;
        for (const QList<QVariant>& args : std::as_const(states))
            seen << args.at(0).value<CsaGameCoordinator::GameState>();
        const QList<CsaGameCoordinator::GameState> expected{
            CsaGameCoordinator::GameState::Connecting, CsaGameCoordinator::GameState::LoggingIn,
            CsaGameCoordinator::GameState::WaitingForMatch, CsaGameCoordinator::GameState::WaitingForAgree,
            CsaGameCoordinator::GameState::InGame};
        QCOMPARE(seen, expected);

        // 終局の後にサーバーが切っても、再接続の判断のために GameOver のまま
        h.server.send("%TORYO,T1");
        h.server.send("#RESIGN");
        h.server.send("#LOSE");
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        h.server.peer->disconnectFromHost();
        QTRY_VERIFY(!h.coordinator.isConnected());
        QCOMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);

        // 止めると待機中に戻る
        h.coordinator.stopGame();
        QCOMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::Idle);
    }

    /// 接続を始められなかったときは、接続中のまま残らず Error になり、もう一度始められる
    void connectionThatCannotStartEndsInError()
    {
        CsaGameCoordinator coordinator;
        QSignalSpy errors(&coordinator, &CsaGameCoordinator::errorOccurred);
        CsaGameCoordinator::StartOptions options;
        options.host = QStringLiteral("127.0.0.1");
        options.port = 0;
        coordinator.startGame(options);
        QCOMPARE(coordinator.gameState(), CsaGameCoordinator::GameState::Error);
        QCOMPARE(errors.size(), 1);

        FakeCsaServer server;
        QVERIFY(server.listen());
        options.port = server.port();
        coordinator.startGame(options);
        QCOMPARE(errors.size(), 1);
        QVERIFY(server.accept());
        QTRY_COMPARE(coordinator.gameState(), CsaGameCoordinator::GameState::LoggingIn);
    }

    /// 引き分けの終局もローカル対局と同じく、探索を止めてから gameover draw を送る
    void drawSendsGameOverDrawToEngine()
    {
        Harness h;
        QVERIFY(h.start("-", {}, true));
        Usi* engine = h.engines.engineInSlot(1);
        QVERIFY(engine);
        QSignalSpy sent(engine->process(), &EngineProcessManager::commandSent);
        h.server.send("#SENNICHITE");
        h.server.send("#DRAW");
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        QStringList commands;
        for (const QList<QVariant>& args : std::as_const(sent)) commands << args.at(0).toString();
        QVERIFY2(commands.contains(QStringLiteral("gameover draw")), qPrintable(commands.join(QLatin1Char(','))));
    }

    void rejectedUnconfirmedMoveLeavesOriginalPosition()
    {
        Harness h;
        QVERIFY(h.start("+"));
        const QString initial = h.line().startSfen;
        h.playHumanMove(QStringLiteral("7g7f"));
        QVERIFY(h.server.waitForLine(QStringLiteral("+7776FU")));
        h.server.send("#ILLEGAL_MOVE");
        h.server.send("#LOSE");
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        QCOMPARE(h.sink.records.size(), 1);
        QVERIFY(h.sink.records.at(0).isTerminal());
        QCOMPARE(h.line().lastSfen(), initial);
        QCOMPARE(h.moveCount(), 0);
        QCOMPARE(h.gc.board()->pieceCharacter(7, 7), Piece::BlackPawn);
    }

    void pendingConfirmationKeepsBoardAndHistoryUnchanged()
    {
        Harness h;
        QVERIFY(h.start("+"));
        HumanTurnPolicy::Deps deps;
        deps.activity = &h.activity;  // 対局が始まると司令塔が通信対局中にしている
        deps.board = h.gc.board();
        deps.csaGame = &h.coordinator;
        HumanTurnPolicy policy(deps);
        // 通信対局中に盤で指した手は、BoardMoveController がそのまま司令塔へ送る
        QVERIFY(policy.isHumanTurnNow());
        h.coordinator.onHumanMove(*UsiMove::parse(u"7g7f"));
        QVERIFY(h.server.waitForLine(QStringLiteral("+7776FU")));
        QCOMPARE(h.coordinator.turnState(), CsaGameCoordinator::TurnState::AwaitingConfirmation);
        QVERIFY(!policy.isHumanTurnNow());
        QCOMPARE(h.moveCount(), 0);
        QCOMPARE(h.gc.board()->currentPlayer(), Turn::Black);
        QCOMPARE(h.gc.board()->pieceCharacter(7, 7), Piece::BlackPawn);
        // UI の許可判定を通らない追加要求も、履歴や盤面を変更しない。
        h.coordinator.onHumanMove(*UsiMove::parse(u"2g2f"));
        h.coordinator.onHumanMove(*UsiMove::parse(u"3c3d"));
        QCOMPARE(h.moveCount(), 0);
        h.server.send("+7776FU,T5");
        QTRY_COMPARE(h.coordinator.progress().moveCount(), 1);
        QCOMPARE(h.moveCount(), 1);
        QCOMPARE(h.gc.board()->pieceCharacter(7, 6), Piece::BlackPawn);
        QCOMPARE(h.coordinator.turnState(), CsaGameCoordinator::TurnState::OpponentTurn);
        h.server.send("-3334FU,T2");
        QTRY_VERIFY(policy.isHumanTurnNow());
        QCOMPARE(h.moveCount(), 2);
    }

    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        qRegisterMetaType<RecordEntry>();
    }

    /// 自分の手（確認）と相手の手で、残り時間・累計・手数・手番・局面列・棋譜行がそろって進む
    void myMoveAndOpponentMoveUpdateTimesAndCount()
    {
        Harness h;
        QSignalSpy turns(&h.coordinator, &CsaGameCoordinator::turnChanged);
        QSignalSpy applied(&h.gc, &GameBoard::moveCommitted);
        QVERIFY(h.start("+"));
        QVERIFY(h.coordinator.isMyTurn());
        QVERIFY(h.coordinator.isBlackSide());
        // 手番の始めに加算する
        QCOMPARE(h.coordinator.blackRemainingMs(), 610000);
        QCOMPARE(h.coordinator.whiteRemainingMs(), 600000);
        QCOMPARE(h.moveCount(), 0);

        h.playHumanMove(QStringLiteral("7g7f"));
        QVERIFY(h.server.waitForLine(QStringLiteral("+7776FU")));
        h.server.send("+7776FU,T5");
        QTRY_COMPARE(h.sink.records.size(), 1);
        const RecordEntry mine = h.sink.records.at(0);
        QCOMPARE(mine.mover, Turn::Black);
        QVERIFY(mine.displayText.contains(QStringLiteral("７六歩")));
        QCOMPARE(mine.time.elapsedMs, 5000);
        QCOMPARE(mine.time.totalMs, 5000);
        QCOMPARE(mine.sfen, h.line().lastSfen());
        QCOMPARE(h.coordinator.blackTotalTimeMs(), 5000);
        QCOMPARE(h.coordinator.blackRemainingMs(), 605000);
        QCOMPARE(h.coordinator.whiteRemainingMs(), 610000);
        QCOMPARE(h.coordinator.progress().moveCount(), 1);
        QVERIFY(!h.coordinator.isMyTurn());
        QCOMPARE(turns.last().at(0).toBool(), false);
        QCOMPARE(applied.size(), 1);  // 確認時に人間の手も一度だけ通知する
        QCOMPARE(h.moveCount(), 1);

        h.server.send("-3334FU,T7");
        QTRY_COMPARE(h.sink.records.size(), 2);
        const RecordEntry theirs = h.sink.records.at(1);
        QCOMPARE(theirs.mover, Turn::White);
        QVERIFY(theirs.displayText.contains(QStringLiteral("３四歩")));
        QCOMPARE(theirs.time.elapsedMs, 7000);
        QCOMPARE(theirs.time.totalMs, 7000);
        QCOMPARE(h.coordinator.whiteTotalTimeMs(), 7000);
        QCOMPARE(h.coordinator.whiteRemainingMs(), 603000);
        QCOMPARE(h.coordinator.blackRemainingMs(), 615000);
        QCOMPARE(h.coordinator.progress().moveCount(), 2);
        QVERIFY(h.coordinator.isMyTurn());
        QCOMPARE(turns.last().at(0).toBool(), true);
        QCOMPARE(applied.size(), 2);
        QCOMPARE(h.coordinator.record().usiMoves(), QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
        QCOMPARE(h.moveCount(), 2);
        QCOMPARE(theirs.sfen, h.line().lastSfen());
        QCOMPARE(h.moveCount(), 2);
        QCOMPARE(theirs.move.move.toString(), QStringLiteral("3c3d"));
    }

    /// 対局が始まると、サーバーの開始局面・対局者名・持ち時間で新しい棋譜を作らせ、再開局面（Game_Summary の
    /// 指し手）を棋譜行として書いてから知らせる。再開局面の手も手数と時間に数える
    void resumedPositionIsWrittenToNewRecord()
    {
        Harness h;
        QSignalSpy started(&h.coordinator, &CsaGameCoordinator::gameStarted);
        QVERIFY(h.start("+", {"+7776FU,T3", "-3334FU,T4"}));
        QCOMPARE(started.size(), 1);
        QCOMPARE(h.sink.begins.size(), 1);
        const MatchRecordStart& begun = h.sink.begins.first();
        QCOMPARE(begun.startSfen, h.line().startSfen);
        QCOMPARE(begun.players.name(Turn::Black), QStringLiteral("Sente"));
        QCOMPARE(begun.players.name(Turn::White), QStringLiteral("Gote"));
        QCOMPARE(begun.timeControl.black.mainMs, qint64(600000));
        const QList<RecordEntry>& initial = h.sink.records;
        QCOMPARE(initial.size(), 2);
        QCOMPARE(initial.at(1).mover, Turn::White);
        QCOMPARE(initial.at(1).time.elapsedMs, 4000);
        QCOMPARE(initial.at(1).sfen, h.line().sfenAt(2));
        QCOMPARE(h.coordinator.progress().moveCount(), 2);
        QCOMPARE(h.coordinator.blackTotalTimeMs(), 3000);
        QCOMPARE(h.coordinator.whiteTotalTimeMs(), 4000);
        // 先手: 600 + 10 - 3 + 10（今の手番の加算）、後手: 600 + 10 - 4
        QCOMPARE(h.coordinator.blackRemainingMs(), 617000);
        QCOMPARE(h.coordinator.whiteRemainingMs(), 606000);
        QCOMPARE(h.moveCount(), 2);
    }

    /// サーバーの終局通知を終局行（TerminalType）にする。対局の手順は指した手だけで、終局行は棋譜の受け手へ渡す
    void endCauseMapsToTerminalType_data()
    {
        QTest::addColumn<QByteArray>("causeLine");
        QTest::addColumn<QByteArray>("resultLine");
        QTest::addColumn<int>("terminal");
        QTest::addColumn<QString>("display");
        // 自分は後手。相手（先手）の手の後で、自分の手番（後手番）に終局する
        QTest::newRow("resign-lose") << QByteArray("#RESIGN") << QByteArray("#LOSE")
                                     << int(TerminalType::Resign) << QStringLiteral("△投了");
        QTest::newRow("resign-win") << QByteArray("#RESIGN") << QByteArray("#WIN")
                                    << int(TerminalType::Resign) << QStringLiteral("▲投了");
        QTest::newRow("timeout") << QByteArray("#TIME_UP") << QByteArray("#LOSE")
                                 << int(TerminalType::Timeout) << QStringLiteral("△時間切れ");
        QTest::newRow("sennichite") << QByteArray("#SENNICHITE") << QByteArray("#DRAW")
                                    << int(TerminalType::Repetition) << QStringLiteral("△千日手");
        QTest::newRow("illegal") << QByteArray("#ILLEGAL_MOVE") << QByteArray("#WIN")
                                 << int(TerminalType::IllegalLoss) << QStringLiteral("▲反則負け");
        // 連続王手の千日手は終局した局面の手番側（後手番）の行。王手をかけていた先手の反則
        QTest::newRow("oute-sennichite") << QByteArray("#OUTE_SENNICHITE") << QByteArray("#WIN")
                                         << int(TerminalType::IllegalWin) << QStringLiteral("△反則勝ち");
        QTest::newRow("jishogi-win") << QByteArray("#JISHOGI") << QByteArray("#WIN")
                                     << int(TerminalType::DeclarationWin) << QStringLiteral("△入玉勝ち");
        QTest::newRow("jishogi-draw") << QByteArray("#JISHOGI") << QByteArray("#DRAW")
                                      << int(TerminalType::Impasse) << QStringLiteral("△持将棋");
        QTest::newRow("max-moves") << QByteArray("#MAX_MOVES") << QByteArray("#CENSORED")
                                   << int(TerminalType::MaxMoves) << QStringLiteral("△最大手数到達");
    }

    void endCauseMapsToTerminalType()
    {
        QFETCH(QByteArray, causeLine);
        QFETCH(QByteArray, resultLine);
        QFETCH(int, terminal);
        QFETCH(QString, display);

        Harness h;
        QSignalSpy ended(&h.coordinator, &CsaGameCoordinator::gameEnded);
        QVERIFY(h.start("-"));
        QVERIFY(!h.coordinator.isMyTurn());
        h.server.send("+7776FU,T2");
        QTRY_COMPARE(h.sink.records.size(), 1);
        h.server.send("%TORYO,T6");  // 消費時間の付いた意思表示（勝敗は次の2行で決まる）
        h.server.send(causeLine);
        h.server.send(resultLine);
        QTRY_COMPARE(ended.size(), 1);
        QCOMPARE(h.sink.records.size(), 2);
        const RecordEntry ending = h.sink.records.at(1);
        QCOMPARE(int(ending.terminal), terminal);
        QCOMPARE(ending.displayText, display);
        QCOMPARE(ending.time.elapsedMs, 6000);
        QCOMPARE(h.moveCount(), 1);
        QCOMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
    }

    /// 相手の投了に消費時間が付いていなければ、相手の手番が始まってからの時間を数える（U4）。
    /// 加算のある対局でも、残り時間から逆算して 0 にしない
    void opponentResignWithoutTimeUsesElapsedTurnTime()
    {
        Harness h;
        QSignalSpy ended(&h.coordinator, &CsaGameCoordinator::gameEnded);
        QVERIFY(h.start("-"));
        h.server.send("+7776FU,T2");
        QTRY_COMPARE(h.sink.records.size(), 1);
        h.playHumanMove(QStringLiteral("3c3d"));
        QVERIFY(h.server.waitForLine(QStringLiteral("-3334FU")));
        h.server.send("-3334FU,T1");
        QTRY_COMPARE(h.sink.records.size(), 2);
        QTest::qWait(1200);
        h.server.send("%TORYO");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_COMPARE(ended.size(), 1);
        const RecordEntry ending = h.sink.records.at(2);
        QCOMPARE(ending.terminal, TerminalType::Resign);
        QVERIFY2(ending.time.elapsedMs >= 1000 && ending.time.elapsedMs < 5000,
                 qPrintable(QString::number(ending.time.elapsedMs)));
    }

    /// 棋譜を作った後で通信対局中にし（開始の知らせはその後）、終局では棋譜を確定して自動保存を頼んでから
    /// 待機に戻す（終局の知らせはその後）。ローカル対局の司令塔と同じ順
    void activityFollowsRecordAtStartAndEnd()
    {
        Harness h;
        m_harness = &h;
        m_startedAfterRecord = false;
        m_endedAfterFinish = false;
        connect(&h.coordinator, &CsaGameCoordinator::gameStarted, this, &TestCsaGameCoordinator::observeStarted);
        connect(&h.coordinator, &CsaGameCoordinator::gameEnded, this, &TestCsaGameCoordinator::observeEnded);
        QVERIFY(h.start("-"));
        QVERIFY(m_startedAfterRecord);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringCsaGame);
        QVERIFY(h.sink.finishes.isEmpty());

        h.server.send("%TORYO,T1");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_VERIFY(m_endedAfterFinish);
        QCOMPARE(h.sink.finishes, QList<bool>({true}));
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
        m_harness = nullptr;
    }

    /// 対局の途中で止めても待機に戻る
    void stopGameReleasesActivity()
    {
        Harness h;
        QVERIFY(h.start("-"));
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringCsaGame);
        h.coordinator.stopGame();
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    /// 終局行の組み立ては値型だけで決まる（消費時間は終局行を行った側の累計に足す）
    void progressEndRecordUsesMoverTotals()
    {
        CsaGameProgress progress;
        progress.isBlackSide = false;
        progress.blackTotalMs = 30000;
        progress.whiteTotalMs = 50000;
        const RecordEntry resign = progress.endRecord(CsaClient::GameResult::Lose,
                                                      CsaClient::GameEndCause::Resign, false, 4000);
        QCOMPARE(resign.displayText, QStringLiteral("△投了"));
        QCOMPARE(resign.time.totalMs, 54000);
        const RecordEntry declared = progress.endRecord(CsaClient::GameResult::Lose,
                                                        CsaClient::GameEndCause::Jishogi, true, 2000);
        QCOMPARE(declared.displayText, QStringLiteral("▲入玉勝ち"));
        QCOMPARE(declared.time.totalMs, 32000);
        const RecordEntry interrupted = progress.endRecord(CsaClient::GameResult::Chudan,
                                                           CsaClient::GameEndCause::Chudan, true, 0);
        QCOMPARE(interrupted.terminal, TerminalType::Interrupt);
        // 勝敗のない終局は手番側の行（ローカルの対局・棋譜の読み込みと同じ）
        QCOMPARE(interrupted.displayText, QStringLiteral("▲中断"));
    }

    /// 加算の扱いとエンジンへ渡す時間（手番側は今の手番の加算を除く）
    void progressTimeTracking()
    {
        CsaGameSummary summary;
        summary.myTurn = Turn::White;
        summary.toMove = Turn::Black;
        summary.timeBlack.specified = true;
        summary.timeBlack.totalTime = 60;
        summary.timeBlack.increment = 5;
        summary.timeWhite = summary.timeBlack;
        CsaGameProgress progress = CsaGameProgress::forSummary(summary);
        QVERIFY(!progress.isBlackSide);
        progress.startClockFromSummary();
        QCOMPARE(progress.blackRemainingMs, 65000);
        QCOMPARE(progress.whiteRemainingMs, 60000);
        progress.recordMoveTime(true, 70000);  // 残りを超えた分は 0 に丸める
        QCOMPARE(progress.blackRemainingMs, 0);
        QCOMPARE(progress.blackTotalMs, 70000);
        QCOMPARE(progress.whiteRemainingMs, 65000);
        const UsiGoTimeInput timing = progress.engineTiming(-100);
        QCOMPARE(timing.wtimeMs, 60000);
        QCOMPARE(timing.btimeMs, 0);
        QCOMPARE(timing.marginMs, 0);
        QCOMPARE(progress.moveRecord(true, QStringLiteral("▲７六歩"), 70000).time.totalMs, 70000);
    }

    /// 通信対局のセッションを通すと、棋譜（ライブ記録）には着手ごと・終局で1行ずつだけ入る
    void sessionAppendsEachRecordOnce()
    {
        Harness h;
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        KifuNavigationState nav(&tree);
        LiveGameSession live(&tree, &nav);
        // 棋譜の受け手（本体と同じ GameSession。通信対局の棋譜を作り直して書くのに使う部品を渡す）
        KifuDocument document(&tree);
        MatchCoordinator match({&h.gc, &h.clock, &h.activity, &h.engines, &document});
        KifuLoader loader(&document);
        EvaluationChartWidget chart;
        EvaluationGraphPresenter graph({&chart, &nav, &live});
        SessionResetCoordinator::Deps rd;
        rd.document = &document;
        rd.liveSession = &live;
        rd.kifuLoader = &loader;
        rd.evalGraph = &graph;
        SessionResetCoordinator reset(rd);
        GameSession::Deps sd;
        sd.activity = &h.activity;
        sd.match = &match;
        sd.reset = &reset;
        sd.navState = &nav;
        sd.liveSession = &live;
        sd.document = &document;
        sd.csaGame = &h.coordinator;
        GameSession recordService(sd);
        h.coordinator.setRecordSink(&recordService);
        QSignalSpy sounds(&h.gc, &GameBoard::moveCommitted);

        QVERIFY(h.start("-"));
        h.server.send("+7776FU,T1");
        QTRY_COMPARE(live.moveCount(), 1);
        QCOMPARE(sounds.size(), 1);
        QCOMPARE(nav.currentSfen(), h.line().lastSfen());
        const KifuBranchNode* first = live.liveNode();
        QCOMPARE(first->move().move.toString(), QStringLiteral("7g7f"));
        QCOMPARE(first->parent()->sfen(), h.line().sfenAt(0));

        // 自分（後手）の手：人間の操作で盤を動かしてから送る
        h.playHumanMove(QStringLiteral("3c3d"));
        QVERIFY(h.server.waitForLine(QStringLiteral("-3334FU")));
        h.server.send("-3334FU,T2");
        QTRY_COMPARE(live.moveCount(), 2);
        QCOMPARE(live.liveNode()->move().move.toString(), QStringLiteral("3c3d"));

        h.server.send("%TORYO,T3");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        // 終局でローカル対局と同じくライブ記録を確定し、終了日時を記録する
        QVERIFY(!live.isActive());
        const QList<KifuBranchNode*> line = tree.mainLine();
        QCOMPARE(line.size(), 4);
        QVERIFY(line.last()->isTerminal());
        QCOMPARE(line.last()->displayText(), QStringLiteral("▲投了"));
        QCOMPARE(sounds.size(), 2);
        // 対局者はサーバーが知らせた名前、持ち時間はサーバーの対局条件
        QCOMPARE(document.headerPlayerName(Turn::Black), QStringLiteral("Sente"));
        QCOMPARE(document.headerPlayerName(Turn::White), QStringLiteral("Gote"));
        QCOMPARE(document.timeRecord().timeControl().black.mainMs, 600000);
        QVERIFY(document.timeRecord().endDateTime().isValid());
        // 受け手はこの関数の中だけで生きる
        h.coordinator.setRecordSink(&h.sink);
    }
};

QTEST_MAIN(TestCsaGameCoordinator)
#include "tst_csagamecoordinator.moc"
