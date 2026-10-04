#include <QtTest>
#include <QTemporaryDir>
#include <QTimer>
#include <QThread>
#include <QPointer>

#include "kifuloadcoordinator.h"
#include "kifuloadparser.h"
#include "sfenutils.h"
#include "sfenpositiontracer.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "usi.h"
#include "engineprocessmanager.h"
#include "usiprotocolhandler.h"
#include "enginepondersettings.h"
#include "enginegameovernotifier.h"
#include "gameendhandler.h"
#include "tsumethreadbudget.h"
#include "tsumecollection.h"
#include "tsume.h"

namespace {
struct KifuHarness {
    QList<ShogiMove> moves;
    QStringList commands;
    int active = 0, selected = 0, current = 0;
    QStringList history;
    KifuLoadCoordinator loader{moves, commands, active, selected, current, &history,
                                nullptr, nullptr, nullptr, nullptr};
};
const QString engineName = QStringLiteral("BackgroundTest");
QString fixture(const QString& name)
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
}
}

class TestBackgroundTasks : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
    QTimer m_heartbeat;
    int m_ticks = 0;
private slots:
    void tick() { ++m_ticks; }
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
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
        EnginePonderSettings::save(engineName, false, true);
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
            QSignalSpy finished(&h.loader, &KifuLoadCoordinator::loadFinished);
            QSignalSpy errors(&h.loader, &KifuLoadCoordinator::errorOccurred);
            h.loader.loadFileAsync(fixture(QStringLiteral("test_basic.") + QString::fromLatin1(extension)));
            QCOMPARE(finished.size(), 0);
            QTRY_COMPARE(finished.size(), 1);
            QVERIFY2(finished.first().first().toBool(), extension);
            QCOMPARE(errors.size(), 0);
            QCOMPARE(h.moves.size(), 7);
            QCOMPARE(h.history.first(), SfenUtils::hirateSfen());
            QCOMPARE(h.commands.size(), 8);
            QVERIFY(h.commands.last().endsWith(QStringLiteral("7g7f 3c3d 2g2f 8c8d 2f2e 8d8e 6i7h")));
        }
    }
    void replacementAndCancelDoNotApplyOldResults()
    {
        KifuHarness h;
        QSignalSpy finished(&h.loader, &KifuLoadCoordinator::loadFinished);
        h.loader.loadFileAsync(fixture(QStringLiteral("test_branch.kif")));
        QString position = SfenUtils::hirateSfen();
        position.replace(QStringLiteral(" b "), QStringLiteral(" w "));
        h.loader.loadTextAsync(position);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(h.history, QStringList{position});
        h.loader.loadFileAsync(fixture(QStringLiteral("test_basic.kif")));
        h.loader.cancelLoad();
        QCOMPARE(finished.size(), 2);
        QVERIFY(!finished.last().first().toBool());
        QTest::qWait(100);
        QCOMPARE(finished.size(), 2);
        QCOMPARE(h.history, QStringList{position});
    }
    void failedLoadKeepsExistingRecord()
    {
        KifuHarness h;
        QVERIFY(h.loader.loadKifuFromString(QStringLiteral("position startpos moves 7g7f")));
        const auto before = h.history;
        QSignalSpy finished(&h.loader, &KifuLoadCoordinator::loadFinished);
        QSignalSpy errors(&h.loader, &KifuLoadCoordinator::errorOccurred);
        h.loader.loadFileAsync(fixture(QStringLiteral("missing.kif")));
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(!finished.first().first().toBool());
        QCOMPARE(errors.size(), 1);
        QCOMPARE(h.history, before);
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
        game.setCurrentPlayer(ShogiGameController::Player2);
        Usi engine(nullptr, nullptr, &game);
        QSignalSpy ready(&engine, &Usi::engineInitialized);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        QSignalSpy errors(&engine, &Usi::errorOccurred);
        const int ticks = m_ticks;
        QElapsedTimer elapsed;
        elapsed.start();
        QVERIFY(engine.startAndInitializeEngineAsync(QStringLiteral(MOCK_USI_EXECUTABLE), engineName));
        engine.sendRaw(QStringLiteral("setoption name ReplyDelay value 250"));
        engine.requestMatchMove(QStringLiteral("position startpos moves 7g7f"), {},
                                {5000, QStringLiteral("300000"), QStringLiteral("0"), 0, 0, true});
        QVERIFY(elapsed.elapsed() < 200);
        QCOMPARE(ready.size(), 0);
        QCOMPARE(moves.size(), 0);
        QTRY_COMPARE(moves.size(), 1);
        QCOMPARE(ready.size(), 1);
        QCOMPARE(errors.size(), 0);
        QVERIFY(m_ticks > ticks + 5);
        QCOMPARE(moves.first().at(0).toPoint(), QPoint(8, 3));
        QCOMPARE(moves.first().at(1).toPoint(), QPoint(8, 4));
        QVERIFY(moves.first().at(2).toString().endsWith(QStringLiteral("7g7f 8c8d")));
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
        EnginePonderSettings::save(engineName, true, true);
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(ShogiGameController::Player2);
        Usi engine(nullptr, nullptr, &game);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        QSignalSpy resigns(&engine, &Usi::bestMoveResignReceived);
        QSignalSpy errors(&engine, &Usi::errorOccurred);
        QVERIFY(engine.startAndInitializeEngineAsync(QStringLiteral(MOCK_USI_EXECUTABLE), engineName));
        const UsiTimingParams timing{5000, QStringLiteral("300000"), QStringLiteral("0"), 0, 0, true};
        engine.requestMatchMove(QStringLiteral("position startpos moves 7g7f"), {}, timing);
        QTRY_COMPARE(moves.size(), 1);
        const QString ponder = moves.first().at(3).toString();
        QVERIFY(ponder.endsWith(QStringLiteral("2g2f")));
        const QString position = hit ? ponder : moves.first().at(2).toString() + QStringLiteral(" 6g6f");
        const int ticks = m_ticks;
        engine.requestMatchMove(position, ponder, timing);
        QCOMPARE(moves.size(), 1);
        QTRY_COMPARE(moves.size(), 2);
        QCOMPARE(moves.last().at(1).toPoint(), QPoint(3, 4));
        QCOMPARE(resigns.size(), 0); // 先読み停止時のresignは対局結果に使わない。
        QCOMPARE(errors.size(), 0);
        if (!hit) QVERIFY(m_ticks > ticks + 3);
    }
    void canceledMoveAndShutdownDoNotBlock()
    {
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(ShogiGameController::Player2);
        Usi engine(nullptr, nullptr, &game);
        QSignalSpy ready(&engine, &Usi::engineInitialized);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        QVERIFY(engine.startAndInitializeEngineAsync(QStringLiteral(MOCK_USI_EXECUTABLE), engineName));
        QTRY_COMPARE(ready.size(), 1);
        engine.sendRaw(QStringLiteral("setoption name ReplyDelay value 300"));
        engine.requestMatchMove(QStringLiteral("position startpos moves 7g7f"), {},
                                {5000, QStringLiteral("300000"), QStringLiteral("0"), 0, 0, true});
        engine.cancelCurrentOperation();
        QTest::qWait(400);
        QCOMPARE(moves.size(), 0);
        QElapsedTimer elapsed;
        elapsed.start();
        engine.cleanupEngineProcessAndThread();
        QVERIFY(elapsed.elapsed() < 200);
    }
    void gameOverCancelsLateMoveAndReapsBothEngines_data()
    {
        QTest::addColumn<bool>("loserIsP1");
        QTest::addColumn<bool>("nyugyoku");
        for (bool loserIsP1 : {false, true}) {
            for (bool nyugyoku : {false, true}) {
                QTest::newRow(qPrintable(QStringLiteral("%1-loser-%2")
                    .arg(nyugyoku ? "nyugyoku" : "resign").arg(loserIsP1 ? "p1" : "p2")))
                    << loserIsP1 << nyugyoku;
            }
        }
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
        // stopで応答が詰まり、terminateも無視するエンジンを再現する。
        qputenv("SBQ_MATCH_STOP_DELAY_MS", "10000");
        qputenv("SBQ_MATCH_IGNORE_TERMINATE", "1");
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(ShogiGameController::Player2);
        Usi engine1(nullptr, nullptr, &game), engine2(nullptr, nullptr, &game);
        QSignalSpy ready1(&engine1, &Usi::engineInitialized);
        QSignalSpy ready2(&engine2, &Usi::engineInitialized);
        QSignalSpy moves1(&engine1, &Usi::matchMoveReady);
        QSignalSpy moves2(&engine2, &Usi::matchMoveReady);
        QVERIFY(engine1.startAndInitializeEngineAsync(QStringLiteral(MOCK_USI_EXECUTABLE), engineName));
        if (bothEngines)
            QVERIFY(engine2.startAndInitializeEngineAsync(QStringLiteral(MOCK_USI_EXECUTABLE), engineName));
        QTRY_COMPARE(ready1.size(), 1);
        if (bothEngines) QTRY_COMPARE(ready2.size(), 1);
        const UsiTimingParams timing{5000, QStringLiteral("300000"), QStringLiteral("300000"), 0, 0, true};
        for (Usi* engine : {&engine1, bothEngines ? &engine2 : nullptr}) {
            if (!engine) continue;
            engine->sendRaw(QStringLiteral("setoption name ReplyDelay value 300"));
            engine->requestMatchMove(QStringLiteral("position startpos moves 7g7f"), {}, timing);
        }
        QTest::qWait(30);

        GameEndHandler handler;
        auto mode = bothEngines ? PlayMode::EvenEngineVsEngine : PlayMode::EvenHumanVsEngine;
        MatchCoordinator::GameOverState gameOver;
        GameEndHandler::Refs refs;
        refs.gc = &game;
        refs.playMode = &mode;
        refs.gameOver = &gameOver;
        refs.usi1Provider = [&engine1] { return &engine1; };
        refs.usi2Provider = [&engine2, bothEngines]() -> Usi* { return bothEngines ? &engine2 : nullptr; };
        handler.setRefs(refs);
        GameEndHandler::Hooks hooks;
        hooks.primaryEngine = [&engine1] { return &engine1; };
        handler.setHooks(hooks);
        QSignalSpy ended(&handler, &GameEndHandler::gameEnded);
        const auto existing = qApp->findChildren<QProcess*>();
        const int ticks = m_ticks;
        QElapsedTimer elapsed;
        elapsed.start();
        handler.handleBreakOff();
        handler.handleBreakOff();
        QVERIFY(elapsed.elapsed() < 200);
        QCOMPARE(ended.size(), 1);
        QCOMPARE(gameOver.lastInfo.cause, MatchCoordinator::Cause::BreakOff);
        QList<QPointer<QProcess>> retiring;
        for (auto* process : qApp->findChildren<QProcess*>())
            if (!existing.contains(process)) retiring.append(process);

        // アプリは開いたまま。quitを送るだけではこの期限で終了できない。
        QTest::qWait(4500);
        QVERIFY(!engine1.isEngineRunning());
        QVERIFY(!engine2.isEngineRunning());
        QCOMPARE(retiring.size(), bothEngines ? 2 : 1);
        for (const auto& process : retiring) QTRY_VERIFY_WITH_TIMEOUT(process.isNull(), 1500);
        QCOMPARE(moves1.size(), 0);
        QCOMPARE(moves2.size(), 0);
        QVERIFY(m_ticks > ticks + 10);
    }
    void gameOverCancelsLateMoveAndReapsBothEngines()
    {
        QFETCH(bool, loserIsP1);
        QFETCH(bool, nyugyoku);
        EnginePonderSettings::save(engineName, true, true);
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(ShogiGameController::Player2);
        Usi thinking(nullptr, nullptr, &game), pondering(nullptr, nullptr, &game);
        QSignalSpy thinkingReady(&thinking, &Usi::engineInitialized);
        QSignalSpy ponderingReady(&pondering, &Usi::engineInitialized);
        QSignalSpy thinkingMoves(&thinking, &Usi::matchMoveReady);
        QSignalSpy ponderingMoves(&pondering, &Usi::matchMoveReady);
        QSignalSpy thinkingErrors(&thinking, &Usi::errorOccurred);
        QSignalSpy ponderingErrors(&pondering, &Usi::errorOccurred);
        QVERIFY(thinking.startAndInitializeEngineAsync(QStringLiteral(MOCK_USI_EXECUTABLE), engineName));
        QVERIFY(pondering.startAndInitializeEngineAsync(QStringLiteral(MOCK_USI_EXECUTABLE), engineName));
        QTRY_COMPARE(thinkingReady.size(), 1);
        QTRY_COMPARE(ponderingReady.size(), 1);
        const UsiTimingParams timing{5000, QStringLiteral("300000"), QStringLiteral("300000"), 0, 0, true};
        const QString position = QStringLiteral("position startpos moves 7g7f");
        pondering.requestMatchMove(position, {}, timing);
        QTRY_COMPARE(ponderingMoves.size(), 1);
        QVERIFY(!ponderingMoves.first().at(3).toString().isEmpty());
        thinking.sendRaw(QStringLiteral("setoption name ReplyDelay value 300"));
        thinking.requestMatchMove(position, {}, timing);
        QTest::qWait(30);
        const auto existing = qApp->findChildren<QProcess*>();
        Usi* p1 = loserIsP1 ? &thinking : &pondering;
        Usi* p2 = loserIsP1 ? &pondering : &thinking;
        const auto sendRaw = [](Usi* engine, const QString& command) { engine->sendRaw(command); };
        if (nyugyoku)
            EngineGameOverNotifier::notifyNyugyoku(PlayMode::EvenEngineVsEngine, false, loserIsP1, p1, p2, sendRaw);
        else
            EngineGameOverNotifier::notifyResignation(PlayMode::EvenEngineVsEngine, loserIsP1, p1, p2, sendRaw);
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
        QVERIFY(!thinking.isEngineRunning());
        QVERIFY(!pondering.isEngineRunning());
    }
    void exitWhileThinkingReportsError()
    {
        qputenv("SBQ_MATCH_EXIT_ON_GO", "1");
        ShogiGameController game;
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(ShogiGameController::Player2);
        Usi engine(nullptr, nullptr, &game);
        QSignalSpy errors(&engine, &Usi::errorOccurred);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        QVERIFY(engine.startAndInitializeEngineAsync(QStringLiteral(MOCK_USI_EXECUTABLE), engineName));
        engine.requestMatchMove(QStringLiteral("position startpos moves 7g7f"), {},
                                {5000, QStringLiteral("300000"), QStringLiteral("0"), 0, 0, true});
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
