#include <QtTest>
#include <QSettings>
#include <QFile>

#include "engineprocessmanager.h"
#include "usiprotocolhandler.h"
#include "usimatchhandler.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "thinkinginfopresenter.h"
#include "settingscommon.h"
#include "matchtimekeeper.h"

// 時計・USI・子プロセスは実装を使用。盤面表示だけスタブにする。
class MatchHarness : public QObject
{
public:
    ShogiClock clock;
    ShogiGameController game;
    EngineProcessManager process;
    UsiProtocolHandler protocol;
    ThinkingInfoPresenter presenter;
    UsiMatchHandler match{&protocol, &presenter, &game};
    QStringList commands;
    QString position = QStringLiteral("position startpos moves 7g7f");
    QString ponder;
    int errors = 0;
    int byoyomi = 5000;

    MatchHarness()
    {
        game.setCurrentPlayer(ShogiGameController::Player2);
        protocol.setProcessManager(&process);
        protocol.setGameController(&game);
        match.setClock(&clock);
        match.setHooks({[this] { ++errors; protocol.cancelCurrentOperation(); }});
        connect(&protocol, &UsiProtocolHandler::bestMoveReceived, this, &MatchHarness::acceptMove);
        connect(&process, &EngineProcessManager::commandSent, this, &MatchHarness::recordCommand);
    }
    ~MatchHarness() override
    {
        protocol.sendQuit();
        process.stopProcess();
        QFile::remove(SettingsCommon::settingsFilePath());
    }
    void acceptMove() { match.onBestMoveReceived(); }
    void recordCommand(const QString& command) { commands.append(command); }
    bool start(const QString& path, bool ponderEnabled = true)
    {
        {
            QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
            settings.beginWriteArray(QStringLiteral("MatchTest"), 1);
            settings.setArrayIndex(0);
            settings.setValue(QStringLiteral("name"), QStringLiteral("USI_Ponder"));
            settings.setValue(QStringLiteral("type"), QStringLiteral("check"));
            settings.setValue(QStringLiteral("value"), ponderEnabled ? QStringLiteral("true") : QStringLiteral("false"));
            settings.endArray();
        }
        protocol.loadEngineOptions(QStringLiteral("MatchTest"));
        if (!process.startProcess(path)) return false;
        protocol.sendUsi();
        if (!protocol.waitForUsiOk(10000)) return false;
        protocol.sendSetOption(QStringLiteral("USI_Ponder"), ponderEnabled ? QStringLiteral("true") : QStringLiteral("false"));
        protocol.sendIsReady();
        if (!protocol.waitForReadyOk(10000)) return false;
        protocol.sendUsiNewGame();
        clock.setPlayerTimes(300, 0, 0, byoyomi / 1000, 0, 0, true);
        clock.setCurrentPlayer(2);
        clock.startClock();
        return true;
    }
    QPoint reply()
    {
        QPoint from, to;
        const UsiTimingParams timing{byoyomi, QStringLiteral("300000"), QStringLiteral("0"), 0, 0, true};
        match.handleEngineVsHumanOrEngineMatchCommunication(position, ponder, from, to, timing);
        return to;
    }
    void humanMove(const QString& move)
    {
        clock.setCurrentPlayer(1);
        clock.applyByoyomiAndResetConsideration2();
        position += QLatin1Char(' ') + move;
        clock.setCurrentPlayer(2);
    }
};

class TestUsiMatchHandler : public QObject
{
    Q_OBJECT
    QString mockPath() const
    {
        QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
#ifdef Q_OS_WIN
        path += QStringLiteral(".exe");
#endif
        return path;
    }
private slots:
    void asymmetricByoyomiUsesSideToMove()
    {
        ShogiGameController game;
        MatchTimekeeper keeper;
        keeper.setRefs({&game});
        keeper.setTimeControlConfig(true, 0, 5000, 0, 0, true);
        game.setCurrentPlayer(ShogiGameController::Player2);
        QCOMPARE(keeper.computeGoTimes().byoyomi, 5000LL);
        game.setCurrentPlayer(ShogiGameController::Player1);
        QCOMPARE(keeper.computeGoTimes().byoyomi, 0LL);
    }
    void ponderTransition_data()
    {
        QTest::addColumn<bool>("hit");
        QTest::newRow("hit") << true;
        QTest::newRow("miss-discards-resign") << false;
    }
    void ponderTransition()
    {
        QFETCH(bool, hit);
        MatchHarness h;
        QVERIFY(h.start(mockPath()));
        QCOMPARE(h.reply(), QPoint(8, 4));
        QCOMPARE(h.protocol.predictedMove(), QStringLiteral("2g2f"));
        QVERIFY(h.commands.last().startsWith(QStringLiteral("go ponder btime ")));
        QVERIFY(h.commands.last().contains(QStringLiteral("byoyomi 4750")));
        h.commands.clear();
        h.humanMove(hit ? QStringLiteral("2g2f") : QStringLiteral("5g5f"));
        QCOMPARE(h.reply(), QPoint(3, 4));
        QCOMPARE(h.errors, 0);
        QVERIFY(!h.clock.isGameOver());
        if (hit) {
            QCOMPARE(h.commands.first(), QStringLiteral("ponderhit"));
            QVERIFY(!h.commands.contains(QStringLiteral("stop")));
        } else {
            QCOMPARE(h.commands.first(), QStringLiteral("stop"));
            QVERIFY(h.commands.at(1).startsWith(QStringLiteral("position ")));
            QVERIFY(h.commands.at(2).startsWith(QStringLiteral("go btime ")));
        }
    }
    void receivedBeforeDeadlineSurvivesUiDelay()
    {
        MatchHarness h;
        h.byoyomi = 1000;
        QVERIFY(h.start(mockPath(), false));
        h.protocol.sendSetOption(QStringLiteral("ReplyDelay"), QStringLiteral("850"));
        QCOMPARE(h.reply(), QPoint(8, 4));
        QTest::qWait(250);
        QVERIFY(!h.clock.isGameOver());
        QCOMPARE(h.errors, 0);
    }
    void humanMoveApiRecognizesPonderHit()
    {
        MatchHarness h;
        QVERIFY(h.start(mockPath()));
        h.position = QStringLiteral("position startpos moves");
        QStringList history;
        const UsiTimingParams timing{5000, QStringLiteral("300000"), QStringLiteral("0"), 0, 0, true};
        QPoint from(7, 7), to(7, 6);
        h.match.handleHumanVsEngineCommunication(h.position, h.ponder, from, to, timing, history);
        QCOMPARE(to, QPoint(8, 4));
        h.clock.setCurrentPlayer(1);
        h.clock.applyByoyomiAndResetConsideration2();
        h.clock.setCurrentPlayer(2);
        h.commands.clear();
        from = QPoint(2, 7);
        to = QPoint(2, 6);
        h.match.handleHumanVsEngineCommunication(h.position, h.ponder, from, to, timing, history);
        QCOMPARE(to, QPoint(3, 4));
        QCOMPARE(h.commands.first(), QStringLiteral("ponderhit"));
        QCOMPARE(history.size(), 2);
    }
    void actualDeadlineRejectsLateMove()
    {
        MatchHarness h;
        h.byoyomi = 1000;
        QVERIFY(h.start(mockPath(), false));
        h.protocol.sendSetOption(QStringLiteral("ReplyDelay"), QStringLiteral("1100"));
        QCOMPARE(h.reply(), QPoint(-1, -1));
        QVERIFY(h.clock.isGameOver());
        QCOMPARE(h.position, QStringLiteral("position startpos moves 7g7f"));
    }
    void expiredClockDoesNotStartSearch()
    {
        MatchHarness h;
        h.byoyomi = 1000;
        QVERIFY(h.start(mockPath()));
        QTest::qSleep(1050); // タイマー未配送でも期限を確認する
        h.commands.clear();
        QCOMPARE(h.reply(), QPoint(-1, -1));
        QVERIFY(h.commands.isEmpty());
    }
    void aperyRealProcess()
    {
        const QString engine = qEnvironmentVariable("SHOGIBOARDQ_TEST_ENGINE");
        if (engine.isEmpty()) QSKIP("Set SHOGIBOARDQ_TEST_ENGINE to run the real-engine regression.");
        MatchHarness h;
        QVERIFY(h.start(engine));
        QVERIFY(h.reply() != QPoint(-1, -1));
        QVERIFY(!h.protocol.predictedMove().isEmpty());
        h.commands.clear();
        h.humanMove(h.protocol.predictedMove());
        QVERIFY(h.reply() != QPoint(-1, -1));
        QCOMPARE(h.commands.first(), QStringLiteral("ponderhit"));
        // 先読みと異なる合法な歩突き。最初の予測手は通常2g2fなど。
        const QString predicted = h.protocol.predictedMove();
        if (!predicted.isEmpty()) {
            const QString move = h.position.contains(QStringLiteral("5g5f"))
                || predicted == QStringLiteral("5g5f") ? QStringLiteral("9g9f") : QStringLiteral("5g5f");
            h.commands.clear();
            h.humanMove(move);
            QVERIFY(h.reply() != QPoint(-1, -1));
            QCOMPARE(h.commands.first(), QStringLiteral("stop"));
        }
        QCOMPARE(h.errors, 0);
        QVERIFY(!h.clock.isGameOver());
    }
};

QTEST_MAIN(TestUsiMatchHandler)
#include "tst_usimatchhandler.moc"
