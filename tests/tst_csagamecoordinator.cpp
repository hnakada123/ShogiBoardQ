/// @file tst_csagamecoordinator.cpp
/// @brief CsaGameCoordinator の指し手進行と棋譜行の組み立てを、ローカルの偽 CSA サーバーで確かめる
///
/// 実際のソケット（QTcpServer）でログイン・対局条件・指し手・終局を流し、
/// 残り時間・手数・手番・局面列・棋譜の受け手（MatchRecordSink）に書いた行・動作状態と、
/// 本体と同じ受け手（GameSession）を通した棋譜への追記を検証する。

#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <algorithm>
#include <functional>

#include "csagamecoordinator.h"
#include "engineslots.h"
#include "engineprocessmanager.h"
#include "engineprofilestore.h"
#include "usi.h"
#include "enginestatusmodel.h"
#include "usilogpanel.h"
#include "operationpolicy.h"
#include "uistatepolicymanager.h"
#include "csagamechannel.h"
#include "gamechannels.h"
#include "csagameprogress.h"
#include "csagameseries.h"
#include "csaverdictwait.h"
#include "appactivitystate.h"
#include "kifudocument.h"
#include "gameinfo_test_util.h"
#include "timecontroltext.h"
#include "evaluationchartwidget.h"
#include "csalogpanel.h"
#include "enginedisplaypresenter.h"
#include "evaluationgraphpresenter.h"
#include "evaluationseries.h"
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
    void beginRecord(const MatchRecordStart& start) override
    {
        begins.append(start);
        if (onBegin) onBegin();
    }
    bool canAppendRecord() const override { return true; }
    void appendRecord(const RecordEntry& entry) override { records.append(entry); }
    bool canUndoRecords(int) const override { return false; }
    void undoRecords(int) override {}
    void finishRecord(bool autoSave) override
    {
        finishes.append(autoSave);
        if (onFinish) onFinish();
    }

    QList<MatchRecordStart> begins;
    QList<RecordEntry> records;
    QList<bool> finishes;   ///< finishRecord の autoSave
    std::function<void()> onBegin;   ///< 新しい棋譜を作るときに呼ぶ（アプリは棋譜の作り直しで表示を消す）
    std::function<void()> onFinish;  ///< 棋譜を確定するときに呼ぶ
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
    int gameLimit = 0;   ///< 対局数（0 は制限なし）
    bool reconnect = false;   ///< 終局の後にサーバーが切ったら接続し直す
    std::function<void()> afterLogin;   ///< ログインの成功を返してから対局条件を送るまでの間に呼ぶ
    QList<QByteArray> positionLines;    ///< BEGIN Position の中身（空なら平手）

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
        options.gameLimit = gameLimit;  // 既定は 0: 終局後に LOGOUT せず、結果表示もモーダルにしない
        options.reconnectAfterGame = reconnect;
        coordinator.startGame(options);
        if (!server.accept()) return false;
        if (!server.waitForLine(QStringLiteral("LOGIN user pass"))) return false;
        server.send("LOGIN:user OK");
        if (afterLogin) afterLogin();
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
        if (positionLines.isEmpty()) {
            server.send("PI");
            server.send("+");
            for (const QByteArray& move : positionMoves) server.send(move);
        } else {
            for (const QByteArray& line : positionLines) server.send(line);
        }
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
        coordinator.sendHumanMove(*move);
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
        // 対局数の制限なし（次の対局を待つ）なので、対局の間の状態を終えて通信対局の接続中に戻っている
        m_endedAfterFinish = m_harness && m_harness->sink.finishes.size() == 1
            && m_harness->activity.is(AppActivityState::Activity::DuringCsaConnection);
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

    /// 読めない開始局面には同意しない。START の後にサーバーと違う盤で対局しない（監査 2026-11-07 F12）
    void unreadablePositionIsRejectedBeforeAgreeing_data()
    {
        QTest::addColumn<QList<QByteArray>>("position");
        QTest::newRow("unknown-piece") << QList<QByteArray>{"P1-KY-KE-GI-KI-OU-KI-GI-KE-XX", "+"};
        QTest::newRow("move-that-cannot-be-played") << QList<QByteArray>{"PI", "+", "+5554FU,T1"};
    }

    void unreadablePositionIsRejectedBeforeAgreeing()
    {
        QFETCH(QList<QByteArray>, position);
        Harness h;
        h.positionLines = position;
        QVERIFY(!h.start("+"));
        QVERIFY(h.server.lines.contains(QStringLiteral("REJECT g1")));
        QVERIFY(!h.server.lines.contains(QStringLiteral("AGREE g1")));
    }

    /// 対局条件が初期化の途中に届いても、エンジンを起動し直さず readyok を待って同意する（監査 2026-11-07 F11）
    void gameSummaryDuringInitializationKeepsTheEngine()
    {
        qputenv("SBQ_MATCH_INIT_DELAY_MS", "300");
        Harness h;
        Usi* engine = nullptr;
        std::unique_ptr<QSignalSpy> sent;
        h.afterLogin = [&] {
            QTRY_VERIFY(h.engines.engineInSlot(1));
            engine = h.engines.engineInSlot(1);
            // 最初の usi を送り終えてから、その後の起動を数える
            QTest::qWait(100);
            sent = std::make_unique<QSignalSpy>(engine->process(), &EngineProcessManager::commandSent);
        };
        QVERIFY(h.start("-", {}, true));
        qunsetenv("SBQ_MATCH_INIT_DELAY_MS");
        QCOMPARE(h.engines.engineInSlot(1), engine);
        for (const QList<QVariant>& command : std::as_const(*sent)) QVERIFY(command.first().toString() != QStringLiteral("usi"));
    }

    /// 自分のエンジンはサーバーが示した残り時間から持ち時間を読み、終局でエンジンの枠を空ける（次局は起動し直す）
    void engineReadsServerTimeAndIsReleasedAtGameEnd()
    {
        Harness h;
        QVERIFY(h.start("-", {}, true));
        QVERIFY(h.engines.engineInSlot(1));
        // 時間の出どころは対局の残り時間（加算を含まない）・決まり
        const MatchSearchTiming timing = h.coordinator.snapshot(Turn::White, false);
        const UsiGoTimeInput expected = h.coordinator.progress().engineTiming(
            false, h.coordinator.elapsedThisTurn(Turn::Black), 1000);
        QCOMPARE(timing.input.wtimeMs, expected.wtimeMs);
        // 手番の先手は今の手番の経過時間を引くので、読んだ時刻の差だけずれうる
        QVERIFY(qAbs(timing.input.btimeMs - expected.btimeMs) <= 50);
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

    /// 通信対局は、ログインとエンジンの起動の後に START で棋譜を作り直す。作り直しで消すのは棋譜に結び付く表示だけで、
    /// その対局の通信ログ（LOGIN〜START）・USI ログ（usi〜readyok）・E1 のエンジン名は残す。評価値グラフの自分の
    /// 系列名はエンジン名（棋譜の対局者はサーバーの名前）。通信ログは次に対局を始めるときに消す（監査 2026-11-01 F12）
    void startKeepsLogsAndEngineName()
    {
        Harness h;
        EngineStatusModel commLog1;
        EngineStatusModel commLog2;
        UsiLogPanel usiLog;
        usiLog.setModels(&commLog1, &commLog2);
        CsaLogPanel csaLog;
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        KifuNavigationState nav(&tree);
        LiveGameSession live(&tree, &nav);
        EvaluationChartWidget chart;
        EvaluationGraphPresenter graph({&chart, &nav, &live});
        EngineDisplayPresenter::Deps deps;
        deps.engines = &h.engines;
        deps.commLog1 = &commLog1;
        deps.commLog2 = &commLog2;
        deps.usiLog = &usiLog;
        deps.csaLog = &csaLog;
        deps.evalGraph = &graph;
        EngineDisplayPresenter display(deps);
        // 合成ルートと同じ結び（棋譜の作り直しで clearAll）
        connect(&h.coordinator, &CsaGameCoordinator::csaCommLogAppended, &csaLog, &CsaLogPanel::append);
        connect(&h.coordinator, &CsaGameCoordinator::matchEngineStarted, &display,
                &EngineDisplayPresenter::showMatchEngine);
        h.sink.onBegin = std::bind(&EngineDisplayPresenter::clearAll, &display);
        QVERIFY(h.start("-", {}, true));
        QCOMPARE(h.sink.begins.size(), 1);

        const QString csaText = csaLog.findChild<QPlainTextEdit*>(QStringLiteral("csaLogView"))->toPlainText();
        QVERIFY2(csaText.contains(QStringLiteral("LOGIN")) && csaText.contains(QStringLiteral("START:g1")),
                 qPrintable(csaText));
        const QString usiText = usiLog.findChild<QPlainTextEdit*>(QStringLiteral("usiLogView"))->toPlainText();
        QVERIFY2(usiText.contains(QStringLiteral("usiok")) && usiText.contains(QStringLiteral("readyok")),
                 qPrintable(usiText));
        QCOMPARE(commLog1.engineName(), QStringLiteral("CSA test engine"));
        // 自分は後手なので2つ目の系列
        QCOMPARE(graph.series()->name(1), QStringLiteral("CSA test engine"));

        // 次の対局を始めるときに通信ログを消す
        display.clearEngineLogs();
        QVERIFY(csaLog.findChild<QPlainTextEdit*>(QStringLiteral("csaLogView"))->toPlainText().isEmpty());
        QVERIFY(usiLog.findChild<QPlainTextEdit*>(QStringLiteral("usiLogView"))->toPlainText().isEmpty());
    }

    /// 盤で選んだ手は対局相手の窓口（GameChannel::playMove）からサーバーへ送る。盤は確認を受けるまで変えず、
    /// 合法手かは司令塔が1回だけ確かめる（ローカル対局と同じ入口。監査 2026-11-01 T3）
    void humanMoveThroughChannelIsSentNotPlayed()
    {
        Harness h;
        QVERIFY(h.start("+"));
        CsaGameChannel channel(&h.coordinator);
        // 合法手でない手は送らない
        QCOMPARE(channel.playMove(*UsiMove::parse(u"7g7e")), GameChannel::MoveResult::Rejected);
        QCOMPARE(channel.playMove(*UsiMove::parse(u"7g7f")), GameChannel::MoveResult::Sent);
        QVERIFY(h.server.waitForLine(QStringLiteral("+7776FU")));
        QCOMPARE(h.moveCount(), 0);
        // 確認を待つ間は自分の手番ではない
        QCOMPARE(channel.playMove(*UsiMove::parse(u"2g2f")), GameChannel::MoveResult::Rejected);
        h.server.send("+7776FU,T1");
        QTRY_COMPARE(h.moveCount(), 1);
    }

    /// エンジンが指した直後の go ponder の自分の持ち時間は、今の手の消費を引いた残り（加算は引かない）。
    /// 相手の残りは次の手番の加算を含まない値のまま（監査 2026-11-01 F14）
    void ponderAfterEngineMoveUsesTimeLeftAfterThisMove()
    {
        constexpr int kThinkMs = 1500;
        qputenv("SBQ_MATCH_REPLY_DELAY_MS", QByteArray::number(kThinkMs));
        EngineProfileStore::savePonder(QStringLiteral("CSA test engine"), true, true);
        Harness h;
        QVERIFY(h.start("-", {}, true));
        qunsetenv("SBQ_MATCH_REPLY_DELAY_MS");
        Usi* engine = h.engines.engineInSlot(1);
        QVERIFY(engine);
        QSignalSpy sent(engine->process(), &EngineProcessManager::commandSent);
        h.server.send("+7776FU,T1");
        QVERIFY(h.server.waitForLine(QStringLiteral("-8384FU"), 5000));
        QString ponder;
        QTRY_VERIFY_WITH_TIMEOUT(([&sent, &ponder] {
            for (const QList<QVariant>& args : std::as_const(sent)) {
                const QString command = args.at(0).toString();
                if (command.startsWith(QStringLiteral("go ponder"))) ponder = command;
            }
            return !ponder.isEmpty();
        }()), 3000);
        const QStringList words = ponder.split(QLatin1Char(' '));
        const auto field = [&words](const QString& name) {
            const qsizetype i = words.indexOf(name);
            return i >= 0 && i + 1 < words.size() ? words.at(i + 1).toLongLong() : -1;
        };
        // 後手（自分）は手番の初めに 600 秒＋加算 10 秒。kThinkMs 考えて指した残りから、通信の余裕（1秒）と
        // エンジンの余裕（250ms）を引く。加算は winc で送る（以前は今の手の消費を引かず、加算を引いていた）
        const qint64 expectedMax = 610000 - kThinkMs - 1000 - 250;
        QVERIFY2(field(QStringLiteral("wtime")) <= expectedMax && field(QStringLiteral("wtime")) > expectedMax - 1000,
                 qPrintable(ponder));
        QCOMPARE(field(QStringLiteral("winc")), 10000LL);
        // 先手は手番の初めの 610 秒から1秒使った残り（次の加算はまだ足していない）
        QCOMPARE(field(QStringLiteral("btime")), 609000LL);
        EngineProfileStore::savePonder(QStringLiteral("CSA test engine"), false, false);
    }

    /// 最大手数（打ち切り）・中断・知らない原因でも、ローカル対局と同じく gameover draw を送る（監査 2026-11-01 F15）
    void unscoredEndingsSendGameOverDrawToEngine_data()
    {
        QTest::addColumn<QList<QByteArray>>("lines");
        QTest::addColumn<QString>("expected");
        QTest::newRow("max-moves") << QList<QByteArray>{"#MAX_MOVES", "#CENSORED"} << QStringLiteral("gameover draw");
        QTest::newRow("chudan") << QList<QByteArray>{"#CHUDAN"} << QStringLiteral("gameover draw");
        QTest::newRow("unknown-cause") << QList<QByteArray>{"#SOMETHING_NEW", "#WIN"} << QStringLiteral("gameover draw");
        QTest::newRow("resign-win") << QList<QByteArray>{"#RESIGN", "#WIN"} << QStringLiteral("gameover win");
    }

    void unscoredEndingsSendGameOverDrawToEngine()
    {
        QFETCH(QList<QByteArray>, lines);
        QFETCH(QString, expected);
        Harness h;
        QVERIFY(h.start("-", {}, true));
        Usi* engine = h.engines.engineInSlot(1);
        QVERIFY(engine);
        QSignalSpy sent(engine->process(), &EngineProcessManager::commandSent);
        for (const QByteArray& line : std::as_const(lines)) h.server.send(line);
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        QStringList gameOvers;
        for (const QList<QVariant>& args : std::as_const(sent)) {
            const QString command = args.at(0).toString();
            if (command.startsWith(QStringLiteral("gameover"))) gameOvers << command;
        }
        QCOMPARE(gameOvers, QStringList{expected});
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
        GameChannels channels(&h.activity, nullptr, &h.coordinator);
        OperationPolicy::Deps deps;
        deps.activity = &h.activity;  // 対局が始まると司令塔が通信対局中にしている
        deps.channels = &channels;
        OperationPolicy policy(deps);
        // 通信対局中に盤で指した手は、BoardMoveController がそのまま司令塔へ送る
        QVERIFY(policy.isHumanTurnNow());
        h.coordinator.sendHumanMove(*UsiMove::parse(u"7g7f"));
        QVERIFY(h.server.waitForLine(QStringLiteral("+7776FU")));
        QCOMPARE(h.coordinator.turnState(), CsaGameCoordinator::TurnState::AwaitingConfirmation);
        QVERIFY(!policy.isHumanTurnNow());
        QCOMPARE(h.moveCount(), 0);
        QCOMPARE(h.gc.board()->currentPlayer(), Turn::Black);
        QCOMPARE(h.gc.board()->pieceCharacter(7, 7), Piece::BlackPawn);
        // UI の許可判定を通らない追加要求も、履歴や盤面を変更しない。
        h.coordinator.sendHumanMove(*UsiMove::parse(u"2g2f"));
        h.coordinator.sendHumanMove(*UsiMove::parse(u"3c3d"));
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
        // 反則は反則した側の反則負け。手番でない側（先手）の反則は、勝敗を変えずに手番側（後手）の反則勝ちの行にする
        // （監査 2026-11-01 F3。棋譜の形式は終局の行を手番側の行としてしか書けない）
        QTest::newRow("illegal-by-side-to-move") << QByteArray("#ILLEGAL_MOVE") << QByteArray("#LOSE")
                                                 << int(TerminalType::IllegalLoss) << QStringLiteral("△反則負け");
        QTest::newRow("illegal-by-other-side") << QByteArray("#ILLEGAL_MOVE") << QByteArray("#WIN")
                                               << int(TerminalType::IllegalWin) << QStringLiteral("△反則勝ち");
        // 連続王手の千日手は終局した局面の手番側（後手番）の行。王手をかけていた先手の反則
        QTest::newRow("oute-sennichite") << QByteArray("#OUTE_SENNICHITE") << QByteArray("#WIN")
                                         << int(TerminalType::IllegalWin) << QStringLiteral("△反則勝ち");
        QTest::newRow("jishogi-win") << QByteArray("#JISHOGI") << QByteArray("#WIN")
                                     << int(TerminalType::DeclarationWin) << QStringLiteral("△入玉勝ち");
        QTest::newRow("jishogi-draw") << QByteArray("#JISHOGI") << QByteArray("#DRAW")
                                      << int(TerminalType::Impasse) << QStringLiteral("△持将棋");
        // 最大手数はローカル対局と同じく持将棋と書く（監査 2026-11-01 F15）
        QTest::newRow("max-moves") << QByteArray("#MAX_MOVES") << QByteArray("#CENSORED")
                                   << int(TerminalType::Impasse) << QStringLiteral("△持将棋");
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
    /// 対局の間の状態を終える（終局の知らせはその後）。ローカル対局の司令塔と同じ順
    void activityFollowsRecordAtStartAndEnd()
    {
        Harness h;
        m_harness = &h;
        m_startedAfterRecord = false;
        m_endedAfterFinish = false;
        connect(&h.coordinator, &CsaGameCoordinator::gameStarted, this, &TestCsaGameCoordinator::observeStarted);
        connect(&h.coordinator, &CsaGameCoordinator::gameEnded, this, &TestCsaGameCoordinator::observeEnded);
        // 棋譜を確定するまでは通信対局中のまま（MatchCoordinator と同じ順。監査 2026-11-07 T4）
        h.sink.onFinish = [&] { QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringCsaGame); };
        QVERIFY(h.start("-"));
        QVERIFY(m_startedAfterRecord);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringCsaGame);
        QVERIFY(h.sink.finishes.isEmpty());

        h.server.send("%TORYO,T1");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_VERIFY(m_endedAfterFinish);
        QCOMPARE(h.sink.finishes, QList<bool>({true}));
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringCsaConnection);
        m_harness = nullptr;
    }

    /// 連続の通信対局の局と局の間は「通信対局の接続中」で、棋譜を見る・保存するだけ（開く・対局開始・局面編集・
    /// 棋譜解析・検討・棋譜の編集はできない）。「中断」でログアウトして待機に戻る（監査 2026-11-01 F13）
    void betweenGamesIsConnectedAndBreakOffLogsOut()
    {
        Harness h;
        QVERIFY(h.start("-"));
        h.server.send("%TORYO,T1");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringCsaConnection);

        using P = UiStatePolicyManager;
        const auto policy = [](P::UiElement element) {
            return P::tablePolicy(AppActivityState::Activity::DuringCsaConnection, element);
        };
        for (const P::UiElement element : {P::UiElement::FileSave, P::UiElement::FileSaveAs, P::UiElement::EditCopyKifu,
                                           P::UiElement::WidgetNavigation, P::UiElement::FileNew,
                                           P::UiElement::GameBreakOff})
            QCOMPARE(policy(element), P::Policy::Enabled);
        for (const P::UiElement element : {P::UiElement::FileOpen, P::UiElement::EditPasteKifu, P::UiElement::GameStart,
                                           P::UiElement::GameCsa, P::UiElement::GameAnalyzeKifu,
                                           P::UiElement::GameTsumeSearch, P::UiElement::WidgetConsiderationStart,
                                           P::UiElement::SettingsEngine, P::UiElement::GameResign})
            QCOMPARE(policy(element), P::Policy::Disabled);
        QCOMPARE(policy(P::UiElement::EditStartPosition), P::Policy::Hidden);
        GameChannels channels(&h.activity, nullptr, &h.coordinator);
        OperationPolicy::Deps turnDeps;
        turnDeps.activity = &h.activity;
        turnDeps.channels = &channels;
        const OperationPolicy turn(turnDeps);
        QVERIFY(turn.canBrowseRecord());
        QVERIFY(!turn.canEditRecord());
        QCOMPARE(turn.boardInput(), OperationPolicy::BoardInput::None);

        // 「中断」は局と局の間ならログアウトする
        CsaGameChannel channel(&h.coordinator);
        channel.breakOff();
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
        QCOMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::Idle);
    }

    /// 終局の後にサーバーが切ったとき（shogi-server）は、接続し直すまで通信対局の接続中のまま
    void serverDisconnectBetweenGamesKeepsConnectedUntilReconnect()
    {
        Harness h;
        h.reconnect = true;
        QVERIFY(h.start("-"));
        h.server.send("%TORYO,T1");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_COMPARE(h.sink.finishes.size(), 1);
        h.server.peer->disconnectFromHost();
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        QTest::qWait(50);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringCsaConnection);
        // 再接続の待ちでやめても待機に戻る
        h.coordinator.stopGame();
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
    }

    /// 通信ログの LOGIN はパスワードを伏せる（伏せるのはクライアントの1か所。監査 2026-11-01 M6）
    void loginPasswordIsMaskedInCommLog()
    {
        Harness h;
        QStringList lines;
        QSignalSpy comm(&h.coordinator, &CsaGameCoordinator::csaCommLogAppended);
        QVERIFY(h.start("-"));
        for (const QList<QVariant>& args : std::as_const(comm)) lines << args.at(0).toString();
        QVERIFY2(lines.contains(QStringLiteral("◀ LOGIN user *****")), qPrintable(lines.join(QLatin1Char('\n'))));
        for (const QString& line : std::as_const(lines)) QVERIFY2(!line.contains(QStringLiteral("pass")), qPrintable(line));
    }

    /// 裁定の待ち: 待ち始めた理由を take で受け取り、時間が過ぎたら timedOut で知らせる
    void verdictWaitHoldsTheFailureText()
    {
        CsaVerdictWait wait(30);
        QVERIFY(!wait.isPending());
        wait.begin(QStringLiteral("cannot read"));
        QVERIFY(wait.isPending());
        QCOMPARE(wait.take(), QStringLiteral("cannot read"));
        QVERIFY(!wait.isPending());
        QSignalSpy timedOut(&wait, &CsaVerdictWait::timedOut);
        wait.begin(QStringLiteral("again"));
        QTRY_COMPARE(timedOut.size(), 1);
        QCOMPARE(timedOut.first().first().toString(), QStringLiteral("again"));
        QVERIFY(!wait.isPending());
        wait.begin(QStringLiteral("cleared"));
        wait.clear();
        QTest::qWait(60);
        QCOMPARE(timedOut.size(), 1);
    }

    /// 対局数と再接続: 対局数の上限まで続け、終局がサーバーの裁定で再接続の設定があるときだけ接続し直す
    void gameSeriesCountsGamesAndSchedulesReconnect()
    {
        CsaGameSeries series;
        series.begin(2, true);
        QVERIFY(series.moreGamesWanted());
        series.setLastEndedByServer(false);
        QVERIFY(!series.scheduleReconnect());
        series.finishGame();
        series.setLastEndedByServer(true);
        QVERIFY(series.scheduleReconnect());
        QVERIFY(series.isReconnectPending());
        series.stop();
        QVERIFY(!series.isReconnectPending());
        series.finishGame();
        QCOMPARE(series.completedGames(), 2);
        QVERIFY(!series.moreGamesWanted());
        QVERIFY(!series.scheduleReconnect());
        // 制限なし（0）は数えても続ける。再接続の設定がなければ接続し直さない
        series.begin(0, false);
        series.finishGame();
        series.setLastEndedByServer(true);
        QVERIFY(series.moreGamesWanted());
        QVERIFY(!series.scheduleReconnect());
    }

    /// 受け入れの判断とログの文は対局条件の値が決める（監査 2026-11-01 M6）
    void summaryDecidesAcceptanceAndConditionLines()
    {
        CsaGameSummary summary;
        summary.format = QStringLiteral("Shogi 1.0");
        QVERIFY(summary.isTimeUnlimited());
        QVERIFY(summary.unsupportedReason(false).isEmpty());
        QVERIFY(!summary.unsupportedReason(true).isEmpty());   // エンジンは時間無制限では指せない
        summary.format = QStringLiteral("Chess 1.0");
        QVERIFY(!summary.unsupportedReason(false).isEmpty());

        summary.format = QStringLiteral("Shogi 1.0");
        summary.timeBlack.specified = true;
        summary.timeBlack.totalTime = 600;
        summary.timeBlack.increment = 10;
        summary.timeWhite = summary.timeBlack;
        summary.maxMoves = 256;
        summary.protocolMode = QStringLiteral("Direct");
        QVERIFY(summary.unsupportedReason(true).isEmpty());
        const QList<CsaGameSummary::ConditionLine> lines = summary.conditionLines();
        QStringList texts;
        for (const CsaGameSummary::ConditionLine& line : lines) texts << line.text;
        QVERIFY2(texts.size() == 4, qPrintable(texts.join(QLatin1Char('|'))));
        QVERIFY(texts.at(0).contains(QStringLiteral("600")) && texts.at(0).contains(QStringLiteral("10")));
        QVERIFY(texts.at(1).contains(QStringLiteral("256")));
        QVERIFY(!lines.at(2).warning);
        QVERIFY(lines.at(3).warning && texts.at(3).contains(QStringLiteral("Direct")));
    }

    /// 予定の対局数を終えたら、終局で待機に戻る
    void lastGameEndReturnsToIdle()
    {
        Harness h;
        h.gameLimit = 1;
        QVERIFY(h.start("-"));
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::DuringCsaGame);
        h.server.send("%TORYO,T1");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_COMPARE(h.sink.finishes.size(), 1);
        QCOMPARE(h.activity.activity(), AppActivityState::Activity::Idle);
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
        const RecordEntry blackMove = progress.moveRecord(true, QStringLiteral("▲７六歩"), 70000);
        QCOMPARE(blackMove.time.totalMs, 70000);
        progress.record.entries.append(blackMove);
        QCOMPARE(progress.sideToMove(), Turn::White);
        // 手番の自分（後手）の残りは今の手番の加算を含むので引く
        const UsiGoTimeInput timing = progress.engineTiming(false, 0, -100);
        QCOMPARE(timing.wtimeMs, 60000);
        QCOMPARE(timing.btimeMs, 0);
        QCOMPARE(timing.marginMs, 0);
    }

    /// 先読みの自分の持ち時間は、今の手の消費を引いた残り（監査 2026-11-01 F14 の例。秒読み10秒、手番の初めの
    /// 残り100秒、60秒考えて指した）。サーバーが自分の手を確認するまで、残り時間は手番の初めのまま
    void progressPonderTimingSubtractsThisMove()
    {
        CsaGameSummary summary;
        summary.myTurn = Turn::White;
        summary.toMove = Turn::White;
        summary.timeBlack.specified = true;
        summary.timeBlack.totalTime = 100;
        summary.timeBlack.byoyomi = 10;
        summary.timeWhite = summary.timeBlack;
        CsaGameProgress progress = CsaGameProgress::forSummary(summary);
        progress.startClockFromSummary();
        QCOMPARE(progress.sideToMove(), Turn::White);
        QCOMPARE(progress.engineTiming(false, 0, 0).wtimeMs, 100000);
        const UsiGoTimeInput ponder = progress.engineTiming(true, 60000, 0);
        QCOMPARE(ponder.wtimeMs, 40000);
        QCOMPARE(ponder.btimeMs, 100000);
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
        const std::optional<TimeControlText::Parsed> time =
            TimeControlText::parse(headerValue(document, GameInfoKeys::kTimeControl));
        QVERIFY(time);
        QCOMPARE(time->timeControl.black.mainMs, 600000);
        QVERIFY(!headerValue(document, GameInfoKeys::kEndDateTime).isEmpty());
        // 受け手はこの関数の中だけで生きる
        h.coordinator.setRecordSink(&h.sink);
    }
};

QTEST_MAIN(TestCsaGameCoordinator)
#include "tst_csagamecoordinator.moc"
