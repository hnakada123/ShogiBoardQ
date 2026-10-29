#include <QtTest>

#include "timecontrol_test_helper.h"
#include <QTemporaryDir>
#include <QTimer>
#include <QThread>
#include <QPointer>
#include <memory>
#include <optional>

#include "kifuloader.h"
#include "kifuloadparser.h"
#include "sfenutils.h"
#include "sfenreplay.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "usi.h"
#include "usienginesession.h"
#include "engineprocessmanager.h"
#include "usiprotocolhandler.h"
#include "engineprofilestore.h"
#include "appactivitystate.h"
#include "engineslots.h"
#include "matchcoordinator.h"
#include "matchengineseat.h"
#include "shogiclock.h"
#include "tsumethreadbudget.h"
#include "tsumecollection.h"
#include "tsume.h"
#include "kifunavigationstate.h"
#include "matchtimesource.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"

namespace {
/// 読み込んだ棋譜は分岐ツリー（棋譜の正本）に入る
struct KifuHarness {
    KifuBranchTree tree;
    KifuNavigationState nav{&tree};
    KifuDocument document{&tree};
    KifuLoader loader{&document};

    QStringList positions() const
    {
        QStringList out;
        const QList<KifuBranchNode*> nodes = tree.mainLine();
        for (const KifuBranchNode* node : nodes) out << node->sfen();
        return out;
    }
    QString usiMoves() const { return tree.mainRecordLine().usiMoves().join(QLatin1Char(' ')); }
};
const QString engineName = QStringLiteral("BackgroundTest");
/// 先手が７六歩と指した後手番の局面
const QString afterFirstMove = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
QString fixture(const QString& name)
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
}
}

/// Usi の通信ログに書かれた行を順に残す（送ったコマンドを確かめる）
class CommLogRecorder : public QObject
{
    Q_OBJECT
public:
    /// エンジンが送ったコマンドを記録する
    explicit CommLogRecorder(const Usi* engine)
    {
        connect(engine->process(), &EngineProcessManager::commandSent, this, &CommLogRecorder::record);
    }
    QStringList sentCommands() const { return m_sent; }
public slots:
    void record(const QString& command) { m_sent << command; }
private:
    QStringList m_sent;
};

class TestBackgroundTasks : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
    QTimer m_heartbeat;
    int m_ticks = 0;
private slots:
    /// 起動の失敗は同期では知らせない（呼び出しから戻った後に1回だけ）。知らせる前に quit すれば知らせない
    void engineStartFailureIsReportedAfterReturning()
    {
        UsiEngineSession session;
        QSignalSpy errors(&session, &UsiEngineSession::errorOccurred);
        session.startAsync(QStringLiteral("/nonexistent/engine"), QString());
        QCOMPARE(errors.count(), 0);
        QTRY_COMPARE(errors.count(), 1);
        QVERIFY(errors.first().at(0).toString().contains(QStringLiteral("/nonexistent/engine")));

        session.startAsync(QString(), QString());
        QCOMPARE(errors.count(), 1);
        session.quit();
        QTest::qWait(20);
        QCOMPARE(errors.count(), 1);

        Usi engine;
        QSignalSpy usiErrors(&engine, &Usi::errorOccurred);
        engine.startEngine(QString(), QString());
        QCOMPARE(usiErrors.count(), 0);
        QTRY_COMPARE(usiErrors.count(), 1);
    }

    /// 枠のエンジンは持ち主のリースで使う。ほかの持ち主が同じ枠を取ると前の持ち主に知らせ、手放しても知らせない
    void engineLeaseRevokesPreviousOwner()
    {
        EngineSlots engines;
        QObject first;
        QObject second;
        QSignalSpy revoked(&engines, &EngineSlots::engineRevoked);
        QSignalSpy changed(&engines, &EngineSlots::enginesChanged);

        EngineLease a = engines.acquire(&first, 1, QStringLiteral("A"));
        QVERIFY(a.engine());
        QCOMPARE(engines.engineInSlot(1), a.engine());
        EngineLease b = engines.acquire(&second, 1, QStringLiteral("B"));
        QCOMPARE(revoked.count(), 1);
        QCOMPARE(revoked.first().at(0).value<QObject*>(), &first);
        QVERIFY(!a.engine());
        QCOMPARE(engines.engineInSlot(1), b.engine());

        // 取り上げられたリースを手放しても、今の持ち主の枠には触れない
        a.reset();
        QCOMPARE(engines.engineInSlot(1), b.engine());

        // 手放すと枠が空く（自分には知らせない）。移したリースは元が空になる
        EngineLease moved = std::move(b);
        QVERIFY(!b.engine());
        QVERIFY(moved.engine());
        moved.reset();
        QVERIFY(!engines.engineInSlot(1));
        QCOMPARE(revoked.count(), 1);

        // 全部の枠を破棄すると、持ち主に知らせる
        EngineLease c = engines.acquire(&first, 2, QStringLiteral("C"));
        engines.destroyEngines(false);
        QCOMPARE(revoked.count(), 2);
        QVERIFY(!c.engine());
        QVERIFY(changed.count() >= 5);
    }

    void tick() { ++m_ticks; }
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        connect(&m_heartbeat, &QTimer::timeout, this, &TestBackgroundTasks::tick);
        m_heartbeat.start(10);
    }
    void cleanup()
    {
        qunsetenv("SBQ_MATCH_INIT_DELAY_MS");
        qunsetenv("SBQ_MATCH_STOP_DELAY_MS");
        qunsetenv("SBQ_MATCH_EXIT_ON_GO");
        qunsetenv("SBQ_MATCH_NO_USIOK");
        qunsetenv("SBQ_MATCH_IGNORE_TERMINATE");
        qunsetenv("SBQ_MATCH_REPLY_DELAY_MS");
        EngineProfileStore::savePonder(engineName, false, true);
    }
    void processShutdownReapsAfterOwnerIsDestroyed_data()
    {
        QTest::addColumn<bool>("explicitStop");
        QTest::addColumn<bool>("unresponsive");
        QTest::newRow("destructor") << false << false;
        QTest::newRow("repeated-stop") << true << false;
        QTest::newRow("ignores-quit-and-terminate") << true << true;
    }
    void processShutdownReapsAfterOwnerIsDestroyed()
    {
        QFETCH(bool, explicitStop);
        QFETCH(bool, unresponsive);
        if (unresponsive) qputenv("SBQ_MATCH_IGNORE_TERMINATE", "1");
        const auto existing = qApp->findChildren<QProcess*>();
        auto process = std::make_unique<EngineProcessManager>();
        QSignalSpy started(process.get(), &EngineProcessManager::processStarted);
        QVERIFY(process->startProcessAsync(QStringLiteral(MOCK_USI_EXECUTABLE)));
        QTRY_COMPARE(started.count(), 1);
        process->sendCommand(QStringLiteral("setoption name ReplyDelay value %1").arg(unresponsive ? 6000 : 300));
        process->sendCommand(QStringLiteral("go depth 1"));
        QTest::qWait(30);
        const int ticks = m_ticks;
        QElapsedTimer elapsed;
        elapsed.start();
        if (explicitStop) {
            process->stopProcessAsync();
            process->stopProcessAsync();
            QVERIFY(!process->isRunning());
            QVERIFY(process->currentEnginePath().isEmpty());
        }
        process.reset();
        QVERIFY(elapsed.elapsed() < 200);
        QList<QPointer<QProcess>> retiring;
        for (auto* child : qApp->findChildren<QProcess*>()) {
            if (!existing.contains(child)) retiring.append(child);
        }
        QCOMPARE(retiring.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(retiring.first().isNull(), 5000);
        QVERIFY(m_ticks > ticks);
    }
    void allKifuFormatsLoadInBackground()
    {
        for (const auto& extension : {"kif", "ki2", "csa", "jkf", "usi", "usen"}) {
            KifuHarness h;
            QSignalSpy finished(&h.loader, &KifuLoader::loadFinished);
            QSignalSpy errors(&h.loader, &KifuLoader::errorOccurred);
            h.loader.loadFileAsync(fixture(QStringLiteral("test_basic.") + QString::fromLatin1(extension)));
            QCOMPARE(finished.size(), 0);
            QTRY_COMPARE(finished.size(), 1);
            QVERIFY2(finished.first().first().toBool(), extension);
            QCOMPARE(errors.size(), 0);
            QCOMPARE(h.positions().first(), SfenUtils::hirateSfen());
            QCOMPARE(h.usiMoves(), QStringLiteral("7g7f 3c3d 2g2f 8c8d 2f2e 8d8e 6i7h"));
        }
    }
    void replacementAndCancelDoNotApplyOldResults()
    {
        KifuHarness h;
        QSignalSpy finished(&h.loader, &KifuLoader::loadFinished);
        h.loader.loadFileAsync(fixture(QStringLiteral("test_branch.kif")));
        QString position = SfenUtils::hirateSfen();
        position.replace(QStringLiteral(" b "), QStringLiteral(" w "));
        h.loader.loadTextAsync(position);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(h.positions(), QStringList{position});
        h.loader.loadFileAsync(fixture(QStringLiteral("test_basic.kif")));
        h.loader.cancelLoad();
        QCOMPARE(finished.size(), 2);
        QVERIFY(!finished.last().first().toBool());
        QTest::qWait(100);
        QCOMPARE(finished.size(), 2);
        QCOMPARE(h.positions(), QStringList{position});
    }
    void failedLoadKeepsExistingRecord()
    {
        KifuHarness h;
        QVERIFY(h.loader.loadText(QStringLiteral("position startpos moves 7g7f")));
        const auto before = h.positions();
        QSignalSpy finished(&h.loader, &KifuLoader::loadFinished);
        QSignalSpy errors(&h.loader, &KifuLoader::errorOccurred);
        h.loader.loadFileAsync(fixture(QStringLiteral("missing.kif")));
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(!finished.first().first().toBool());
        QCOMPARE(errors.size(), 1);
        QCOMPARE(h.positions(), before);
    }
    /// 局面の読み込みは、渡された分岐ツリー（棋譜の正本）に入り、自分だけのツリーは作らない。
    /// 表示中の手は作り直したツリーの開始局面になる
    void positionLoadUsesTheSharedTree()
    {
        const QString position = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1");
        KifuHarness h;
        QVERIFY(h.loader.loadText(QStringLiteral("position startpos moves 7g7f")));
        h.nav.setCurrentNode(h.tree.mainLine().constLast());
        QVERIFY(h.loader.loadPositionFromSfen(QStringLiteral("sfen ") + position));
        QCOMPARE(h.tree.root()->sfen(), position);
        QCOMPARE(h.nav.currentNode(), h.tree.root());
        QVERIFY(h.loader.findChildren<KifuBranchTree*>().isEmpty());
    }
    void destructionDoesNotWaitForParsing()
    {
        auto h = std::make_unique<KifuHarness>();
        h->loader.loadFileAsync(fixture(QStringLiteral("test_branch.kif")));
        h.reset();
        QTest::qWait(50); // ワーカー完了後も解放済みGUIに触れない。
    }
    void protocolInitializationTimeoutAndCancel()
    {
        UsiProtocolHandler handler;
        QSignalSpy done(&handler, &UsiProtocolHandler::initializationFinished);
        QSignalSpy errors(&handler, &UsiProtocolHandler::errorOccurred);
        handler.initializeEngineAsync(30);
        QCOMPARE(done.size(), 0);
        QTRY_COMPARE(done.size(), 1);
        QVERIFY(!done.first().first().toBool());
        QCOMPARE(errors.size(), 1);
        handler.initializeEngineAsync(30);
        handler.cancelCurrentOperation();
        handler.onDataReceived(QStringLiteral("usiok"));
        handler.onDataReceived(QStringLiteral("readyok"));
        QTest::qWait(60);
        QCOMPARE(done.size(), 1);
        handler.initializeEngineAsync();
        handler.onDataReceived(QStringLiteral("readyok")); // 順序の違う応答は完了にしない。
        QCOMPARE(done.size(), 1);
        handler.onDataReceived(QStringLiteral("usiok"));
        handler.onDataReceived(QStringLiteral("readyok"));
        QCOMPARE(done.size(), 2);
        QVERIFY(done.last().first().toBool());
    }
    void initializationAndMoveKeepEventLoopResponsive()
    {
        qputenv("SBQ_MATCH_INIT_DELAY_MS", "250");
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(Turn::White);
        Usi engine;
        QSignalSpy ready(&engine, &Usi::engineInitialized);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        QSignalSpy errors(&engine, &Usi::errorOccurred);
        const int ticks = m_ticks;
        QElapsedTimer elapsed;
        elapsed.start();
        engine.startEngine(QStringLiteral(MOCK_USI_EXECUTABLE), engineName);
        engine.sendSetOptionCommand(QStringLiteral("ReplyDelay"), QStringLiteral("250"));
        FixedTimeSource matchTime(TestTime::byoyomiTiming(5000, 300000, 0));
        engine.setMatchTimeSource(&matchTime);
        engine.requestMatchMove(*UsiPosition::parse(u"position startpos moves 7g7f"));
        QVERIFY(elapsed.elapsed() < 200);
        QCOMPARE(ready.size(), 0);
        QCOMPARE(moves.size(), 0);
        QTRY_COMPARE(moves.size(), 1);
        QCOMPARE(ready.size(), 1);
        QCOMPARE(errors.size(), 0);
        QVERIFY(m_ticks > ticks + 5);
        QCOMPARE(moves.first().at(0).value<UsiMatchMove>().move.toString(), QStringLiteral("8c8d"));
    }
    /// 準備中に「すぐ指させる」を押しても、保留中の position・go を捨てずにエンジンが指す
    void immediateMoveDuringInitializationKeepsPendingSearch()
    {
        qputenv("SBQ_MATCH_INIT_DELAY_MS", "250");
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(Turn::White);
        Usi engine;
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        engine.startEngine(QStringLiteral(MOCK_USI_EXECUTABLE), engineName);
        FixedTimeSource matchTime(TestTime::byoyomiTiming(5000, 300000, 0));
        engine.setMatchTimeSource(&matchTime);
        engine.requestMatchMove(*UsiPosition::parse(u"position startpos moves 7g7f"));
        QVERIFY(engine.isInitializing());
        engine.requestImmediateMove();
        QTRY_COMPARE(moves.size(), 1);
        QCOMPARE(moves.first().at(0).value<UsiMatchMove>().move.to, (BoardSquare{8, 4}));
    }
    void ponderSwitch_data()
    {
        QTest::addColumn<bool>("hit");
        QTest::newRow("hit") << true;
        QTest::newRow("miss") << false;
    }
    void ponderSwitch()
    {
        QFETCH(bool, hit);
        qputenv("SBQ_MATCH_STOP_DELAY_MS", "200");
        EngineProfileStore::savePonder(engineName, true, true);
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(Turn::White);
        Usi engine;
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        QSignalSpy resigns(&engine, &Usi::resigned);
        QSignalSpy errors(&engine, &Usi::errorOccurred);
        engine.startEngine(QStringLiteral(MOCK_USI_EXECUTABLE), engineName);
        FixedTimeSource matchTime(TestTime::byoyomiTiming(5000, 300000, 0));
        engine.setMatchTimeSource(&matchTime);
        const UsiPosition first = *UsiPosition::parse(u"position startpos moves 7g7f");
        engine.requestMatchMove(first);
        QTRY_COMPARE(moves.size(), 1);
        // 模擬エンジンは相手の手を 2g2f と予想して先読みする
        const UsiPosition played = *first.withMove(moves.first().at(0).value<UsiMatchMove>().move);
        const UsiPosition position = *played.withMove(*UsiMove::parse(hit ? u"2g2f" : u"6g6f"));
        const int ticks = m_ticks;
        engine.requestMatchMove(position);
        QCOMPARE(moves.size(), 1);
        QTRY_COMPARE(moves.size(), 2);
        QCOMPARE(moves.last().at(0).value<UsiMatchMove>().move.to, (BoardSquare{3, 4}));
        QCOMPARE(resigns.size(), 0); // 先読み停止時のresignは対局結果に使わない。
        QCOMPARE(errors.size(), 0);
        if (!hit) QVERIFY(m_ticks > ticks + 3);
    }
    void canceledMoveAndShutdownDoNotBlock()
    {
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(Turn::White);
        Usi engine;
        QSignalSpy ready(&engine, &Usi::engineInitialized);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        engine.startEngine(QStringLiteral(MOCK_USI_EXECUTABLE), engineName);
        QTRY_COMPARE(ready.size(), 1);
        engine.sendSetOptionCommand(QStringLiteral("ReplyDelay"), QStringLiteral("300"));
        FixedTimeSource matchTime(TestTime::byoyomiTiming(5000, 300000, 0));
        engine.setMatchTimeSource(&matchTime);
        engine.requestMatchMove(*UsiPosition::parse(u"position startpos moves 7g7f"));
        engine.cancelCurrentOperation();
        QTest::qWait(400);
        QCOMPARE(moves.size(), 0);
        QElapsedTimer elapsed;
        elapsed.start();
        engine.quit();
        QVERIFY(elapsed.elapsed() < 200);
    }
    void gameOverCancelsLateMoveAndReapsBothEngines_data()
    {
        QTest::addColumn<bool>("loserIsBlack");
        QTest::newRow("loser-black") << true;
        QTest::newRow("loser-white") << false;
    }
    void breakOffReapsUnresponsiveEngines_data()
    {
        QTest::addColumn<bool>("bothEngines");
        QTest::newRow("human-vs-engine") << false;
        QTest::newRow("engine-vs-engine") << true;
    }
    void breakOffReapsUnresponsiveEngines()
    {
        QFETCH(bool, bothEngines);
        // stopで応答が詰まり、terminateも無視するエンジンを再現する。指し手は300ミリ秒考えてから返す。
        qputenv("SBQ_MATCH_STOP_DELAY_MS", "10000");
        qputenv("SBQ_MATCH_IGNORE_TERMINATE", "1");
        qputenv("SBQ_MATCH_REPLY_DELAY_MS", "300");
        ShogiGameController game;
        ShogiClock clock;
        AppActivityState activity;
        EngineSlots engines;
        MatchCoordinator match({&game, &clock, &activity, &engines, nullptr});
        // 後手番の局面から、後手（と先手）をエンジンが指す
        MatchStartOptions opt;
        opt.recordLine.startSfen = afterFirstMove;
        opt.players.seat(Turn::White).engine = true;
        opt.players.seat(Turn::White).engineName = engineName;
        opt.engine(Turn::White).path = QStringLiteral(MOCK_USI_EXECUTABLE);
        if (bothEngines) {
            opt.players.seat(Turn::Black).engine = true;
            opt.players.seat(Turn::Black).engineName = engineName;
            opt.engine(Turn::Black).path = QStringLiteral(MOCK_USI_EXECUTABLE);
        }
        match.start(opt);
        match.beginPlay();
        // 人間対エンジンではエンジンは E1、エンジン同士では後手が E2
        Usi* thinking = engines.engineInSlot(bothEngines ? 2 : 1);
        Usi* waiting = bothEngines ? engines.engineInSlot(1) : nullptr;
        QVERIFY(thinking);
        QVERIFY(!bothEngines || waiting);
        QSignalSpy thinkingMoves(thinking, &Usi::matchMoveReady);
        std::unique_ptr<QSignalSpy> waitingMoves;
        if (waiting) waitingMoves = std::make_unique<QSignalSpy>(waiting, &Usi::matchMoveReady);
        QTRY_VERIFY(thinking->protocol()->isSearching());
        if (waiting) QTRY_VERIFY(waiting->isRunning() && !waiting->isInitializing());
        QTest::qWait(30);

        QSignalSpy ended(&match, &MatchCoordinator::gameEnded);
        const auto existing = qApp->findChildren<QProcess*>();
        const int ticks = m_ticks;
        QElapsedTimer elapsed;
        elapsed.start();
        match.handleBreakOff();
        match.handleBreakOff();
        QVERIFY(elapsed.elapsed() < 200);
        QCOMPARE(ended.size(), 1);
        QCOMPARE(match.gameEnd()->cause, MatchCoordinator::Cause::BreakOff);
        QList<QPointer<QProcess>> retiring;
        for (auto* process : qApp->findChildren<QProcess*>())
            if (!existing.contains(process)) retiring.append(process);

        // アプリは開いたまま。quitを送るだけではこの期限で終了できない。
        QTest::qWait(4500);
        QVERIFY(!thinking->isRunning());
        QVERIFY(!waiting || !waiting->isRunning());
        QCOMPARE(retiring.size(), bothEngines ? 2 : 1);
        for (const auto& process : retiring) QTRY_VERIFY_WITH_TIMEOUT(process.isNull(), 1500);
        QCOMPARE(thinkingMoves.size(), 0);
        QVERIFY(!waitingMoves || waitingMoves->isEmpty());
        QVERIFY(m_ticks > ticks + 10);
        engines.destroyEngines();
    }
    void gameOverCancelsLateMoveAndReapsBothEngines()
    {
        QFETCH(bool, loserIsBlack);
        EngineProfileStore::savePonder(engineName, true, true);
        EngineSlots engines;
        FixedTimeSource matchTime(TestTime::byoyomiTiming(5000, 300000, 0));
        // 負けた側のエンジンは思考中、勝った側のエンジンは先読み中
        const Turn loser = loserIsBlack ? Turn::Black : Turn::White;
        const Turn winner = loserIsBlack ? Turn::White : Turn::Black;
        MatchEngineSeat thinkingSeat(&engines);
        MatchEngineSeat ponderingSeat(&engines);
        QSignalSpy thinkingMoves(&thinkingSeat, &MatchEngineSeat::moveReady);
        QSignalSpy ponderingMoves(&ponderingSeat, &MatchEngineSeat::moveReady);
        QSignalSpy thinkingErrors(&thinkingSeat, &MatchEngineSeat::errorOccurred);
        QSignalSpy ponderingErrors(&ponderingSeat, &MatchEngineSeat::errorOccurred);
        MatchEngineSeat::Params params;
        params.enginePath = QStringLiteral(MOCK_USI_EXECUTABLE);
        params.engineName = engineName;
        params.timeSource = &matchTime;
        params.slot = loserIsBlack ? 1 : 2;
        params.side = loser;
        thinkingSeat.start(params);
        params.slot = loserIsBlack ? 2 : 1;
        params.side = winner;
        ponderingSeat.start(params);
        QTRY_VERIFY(thinkingSeat.isReady());
        QTRY_VERIFY(ponderingSeat.isReady());
        Usi* thinking = thinkingSeat.engine();
        Usi* pondering = ponderingSeat.engine();
        const UsiPosition position = *UsiPosition::parse(u"position startpos moves 7g7f");
        pondering->requestMatchMove(position);
        QTRY_COMPARE(ponderingMoves.size(), 1);
        QVERIFY(pondering->protocol()->isSearching());
        QCOMPARE(pondering->protocol()->searchKind(), UsiProtocolHandler::SearchKind::Ponder);
        thinking->sendSetOptionCommand(QStringLiteral("ReplyDelay"), QStringLiteral("300"));
        thinking->requestMatchMove(position);
        QTest::qWait(30);
        const auto existing = qApp->findChildren<QProcess*>();
        thinkingSeat.finish(GameOverResult::Lose);
        ponderingSeat.finish(GameOverResult::Win);
        QList<QPointer<QProcess>> retiring;
        for (auto* child : qApp->findChildren<QProcess*>())
            if (!existing.contains(child)) retiring.append(child);
        // 終局後に返るbestmoveから着手や先読みが再開されないこと。
        QTest::qWait(400);
        QCOMPARE(thinkingMoves.size(), 0);
        QCOMPARE(ponderingMoves.size(), 1);
        QCOMPARE(thinkingErrors.size(), 0);
        QCOMPARE(ponderingErrors.size(), 0);
        QCOMPARE(retiring.size(), 2);
        for (const auto& process : retiring) QTRY_VERIFY_WITH_TIMEOUT(process.isNull(), 5000);
        QVERIFY(!thinking->isRunning());
        QVERIFY(!pondering->isRunning());
        engines.destroyEngines();
    }
    void gameOverResultFollowsEngineSide_data()
    {
        QTest::addColumn<bool>("engineIsBlack");
        QTest::addColumn<bool>("loserIsBlack");
        QTest::addColumn<QString>("expected");
        // エンジンの枠ではなく、エンジンが指す手番で勝ち負けを知らせる（人間対エンジンではエンジンは常に E1）
        QTest::newRow("engine-white-wins") << false << true << "gameover win";
        QTest::newRow("engine-white-loses") << false << false << "gameover lose";
        QTest::newRow("engine-black-loses") << true << true << "gameover lose";
        QTest::newRow("engine-black-wins") << true << false << "gameover win";
    }

    void gameOverResultFollowsEngineSide()
    {
        QFETCH(bool, engineIsBlack);
        QFETCH(bool, loserIsBlack);
        QFETCH(QString, expected);
        QStringList gameOvers;
        // 終局の後はエンジンを閉じるので、対局ごとにエンジンを起動する（2回目は中断で引き分け）
        for (const bool breakOff : {false, true}) {
            ShogiGameController game;
            ShogiClock clock;
            AppActivityState activity;
            EngineSlots engines;
            MatchCoordinator match({&game, &clock, &activity, &engines, nullptr});
            MatchStartOptions opt;
            opt.recordLine.startSfen = SfenUtils::hirateSfen();
            if (engineIsBlack) {
                opt.players.seat(Turn::Black).engine = true;
                opt.players.seat(Turn::Black).engineName = engineName;
                opt.engine(Turn::Black).path = QStringLiteral(MOCK_USI_EXECUTABLE);
            } else {
                opt.players.seat(Turn::White).engine = true;
                opt.players.seat(Turn::White).engineName = engineName;
                opt.engine(Turn::White).path = QStringLiteral(MOCK_USI_EXECUTABLE);
            }
            match.start(opt);
            Usi* engine = engines.engineInSlot(1);
            QVERIFY(engine);
            CommLogRecorder recorder(engine);
            QTRY_VERIFY(engine->isRunning() && !engine->isInitializing());
            if (breakOff) match.handleBreakOff();
            else emit clock.playerTimeOut(loserIsBlack ? Turn::Black : Turn::White);
            for (const QString& command : recorder.sentCommands())
                if (command.startsWith(QStringLiteral("gameover "))) gameOvers << command;
            engines.destroyEngines(false);
        }
        QCOMPARE(gameOvers, QStringList({expected, QStringLiteral("gameover draw")}));
    }

    void exitWhileThinkingReportsError()
    {
        qputenv("SBQ_MATCH_EXIT_ON_GO", "1");
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(Turn::White);
        Usi engine;
        QSignalSpy errors(&engine, &Usi::errorOccurred);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        engine.startEngine(QStringLiteral(MOCK_USI_EXECUTABLE), engineName);
        FixedTimeSource matchTime(TestTime::byoyomiTiming(5000, 300000, 0));
        engine.setMatchTimeSource(&matchTime);
        engine.requestMatchMove(*UsiPosition::parse(u"position startpos moves 7g7f"));
        QTRY_COMPARE(errors.size(), 1);
        QCOMPARE(moves.size(), 0);
    }
    void embeddedSearchBudgetIsSharedAndReleased()
    {
        const int maximum = std::clamp(QThread::idealThreadCount() / 2, 1, 4);
        {
            const TsumeThreadBudget first;
            const TsumeThreadBudget second;
            QCOMPARE(first.threads(), maximum);
            QCOMPARE(second.threads(), 1);
        }
        const TsumeThreadBudget next;
        QCOMPARE(next.threads(), maximum);
    }
    void parallelMateSearchPreservesResult()
    {
        QFile file(fixture(QStringLiteral("tsume_positions_with_moves.sfen")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto collection = TsumeCollection::parse(QString::fromUtf8(file.readAll()));
        QVERIFY(!collection.problems.isEmpty());
        for (const auto& problem : collection.problems) {
            shogi::Position position;
            QVERIFY(position.set_sfen(problem.sfen.toStdString(), true));
            std::atomic_bool stop{false};
            shogi::TsumeSearch solver;
            const auto sequential = solver.solve(position, position.side_to_move(), 7, 10000, stop, 1);
            const auto parallel = solver.solve(position, position.side_to_move(), 7, 10000, stop, 4);
            QCOMPARE(parallel.status, sequential.status);
            QCOMPARE(parallel.plies, sequential.plies);
            stop.store(true);
            QCOMPARE(solver.solve(position, position.side_to_move(), 7, 10000, stop, 4).status,
                     shogi::TsumeStatus::Cancelled);
        }
    }
};
QTEST_MAIN(TestBackgroundTasks)
#include "tst_background_tasks.moc"
