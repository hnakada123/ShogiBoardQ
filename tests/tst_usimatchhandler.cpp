#include <QtTest>
#include <QSettings>
#include <QFile>
#include <algorithm>

#include "engineprocessmanager.h"
#include "usiprotocolhandler.h"
#include "usimatchhandler.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "thinkinginfopresenter.h"
#include "settingscommon.h"
#include "enginepondersettings.h"
#include "matchtimekeeper.h"
#include "usi.h"
#include "sfenutils.h"
#include <QTemporaryDir>

// 本体と同じ時計・USI・子プロセスを使い、応答シグナルで結果を検証する。
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
    int completed = 0;
    QPoint lastTo{-1, -1};

    MatchHarness()
    {
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(ShogiGameController::Player2);
        protocol.setProcessManager(&process);
        protocol.setGameController(&game);
        match.setClock(&clock);
        match.setHooks({[this] { ++errors; protocol.cancelCurrentOperation(); },
            [this](const QPoint&, const QPoint& to, const QString& result, const QString& nextPonder) {
                lastTo = to;
                position = result;
                ponder = nextPonder;
                ++completed;
            }});
        connect(&protocol, &UsiProtocolHandler::bestMoveReceived, this, &MatchHarness::acceptMove);
        connect(&process, &EngineProcessManager::commandSent, this, &MatchHarness::recordCommand);
    }
    ~MatchHarness() override
    {
        protocol.sendQuit();
        process.stopProcessAsync();
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
        return initialize(path);
    }
    bool initialize(const QString& path)
    {
        protocol.loadEngineOptions(QStringLiteral("MatchTest"));
        QSignalSpy started(&process, &EngineProcessManager::processStarted);
        if (!process.startProcessAsync(path)) return false;
        if (started.isEmpty() && !started.wait(5000)) return false;
        QSignalSpy initialized(&protocol, &UsiProtocolHandler::initializationFinished);
        protocol.initializeEngineAsync();
        if (initialized.isEmpty() && !initialized.wait(5000)) return false;
        if (!initialized.first().first().toBool()) return false;
        clock.setPlayerTimes(300, 0, 0, byoyomi / 1000, 0, 0, true);
        clock.setCurrentPlayer(2);
        clock.startClock();
        return true;
    }
    QPoint reply()
    {
        const int previous = completed;
        lastTo = QPoint(-1, -1);
        const UsiTimingParams timing{byoyomi, QStringLiteral("300000"), QStringLiteral("0"), 0, 0, true};
        match.requestMove(position, ponder, timing);
        const bool finished = QTest::qWaitFor([&] {
            return completed > previous || errors > 0 || clock.isGameOver();
        }, byoyomi + 2000);
        return finished ? lastTo : QPoint(-1, -1);
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
    QTemporaryDir m_config;
    QString mockPath() const
    {
        QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
#ifdef Q_OS_WIN
        path += QStringLiteral(".exe");
#endif
        return path;
    }
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }
    void init()
    {
        qunsetenv("SBQ_MATCH_PONDER_MODE");
        QFile::remove(SettingsCommon::settingsFilePath());
    }
    void cleanup()
    {
        qunsetenv("SBQ_MATCH_PONDER_MODE");
    }
    void ponderSettings_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<bool>("configured");
        QTest::addColumn<bool>("enabled");
        QTest::addColumn<bool>("sendUnreported");
        QTest::addColumn<bool>("expectOption");
        QTest::addColumn<bool>("expectPonder");
        QTest::newRow("unconfigured-hidden") << QStringLiteral("hidden") << false << false << true << true << false;
        QTest::newRow("hidden-on") << QStringLiteral("hidden") << true << true << true << true << true;
        QTest::newRow("hidden-off") << QStringLiteral("hidden") << true << false << true << true << false;
        QTest::newRow("gui-only-on") << QStringLiteral("gui-only") << true << true << false << false << true;
        QTest::newRow("gui-only-off-ignores-prediction") << QStringLiteral("gui-only") << true << false << false << false << false;
        QTest::newRow("reported-on") << QStringLiteral("reported") << true << true << false << true << true;
        QTest::newRow("reported-off") << QStringLiteral("reported") << true << false << false << true << false;
        QTest::newRow("no-prediction") << QStringLiteral("no-prediction") << true << true << true << true << false;
    }
    void ponderSettings()
    {
        QFETCH(QString, mode);
        QFETCH(bool, configured);
        QFETCH(bool, enabled);
        QFETCH(bool, sendUnreported);
        QFETCH(bool, expectOption);
        QFETCH(bool, expectPonder);
        qputenv("SBQ_MATCH_PONDER_MODE", mode.toUtf8());
        {
            QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
            settings.beginWriteArray(QStringLiteral("MatchTest"), 1);
            settings.setArrayIndex(0);
            settings.setValue(QStringLiteral("name"), QStringLiteral("UnsupportedOption"));
            settings.setValue(QStringLiteral("type"), QStringLiteral("check"));
            settings.setValue(QStringLiteral("value"), QStringLiteral("true"));
            settings.endArray();
        }
        if (configured) EnginePonderSettings::save(QStringLiteral("MatchTest"), enabled, sendUnreported);
        MatchHarness h;
        QVERIFY(h.initialize(mockPath()));
        QCOMPARE(h.protocol.isPonderEnabled(), enabled);
        QStringList expected{QStringLiteral("usi")};
        if (expectOption) {
            expected.append(enabled ? QStringLiteral("setoption name USI_Ponder value true")
                                    : QStringLiteral("setoption name USI_Ponder value false"));
        }
        expected.append({QStringLiteral("isready"), QStringLiteral("usinewgame")});
        QCOMPARE(h.commands, expected);
        QCOMPARE(h.reply(), QPoint(8, 4));
        QCOMPARE(h.protocol.currentPhase() == UsiProtocolHandler::SearchPhase::Ponder, expectPonder);
        QCOMPARE(h.commands.last().startsWith(QStringLiteral("go ponder ")), expectPonder);
        QCOMPARE(h.errors, 0);
    }
    void incrementBudgetIncludesCurrentMove()
    {
        MatchTimekeeper keeper;
        MatchTimekeeper::Hooks hooks;
        hooks.remainingMsFor = [](int player) { return player == 1 ? 10000LL : 17000LL; };
        keeper.setHooks(hooks);
        keeper.setTimeControlConfig(false, 0, 0, 10000, 7000, true);
        const auto times = keeper.computeGoTimes();
        QCOMPARE(times.btime, 10000LL);
        QCOMPARE(times.wtime, 17000LL);
        QCOMPARE(times.binc, 10000LL);
        QCOMPARE(times.winc, 7000LL);
    }
    void incrementOnlyFirstMove()
    {
        MatchHarness h;
        const QString realEngine = qEnvironmentVariable("SHOGIBOARDQ_TEST_ENGINE");
        QVERIFY(h.start(realEngine.isEmpty() ? mockPath() : realEngine, false));
        h.clock.stopClock();
        // 対局開始時に加算された初手の10秒を渡す。
        h.clock.setPlayerTimes(10, 10, 0, 0, 10, 10, true);
        h.clock.setCurrentPlayer(1);
        h.game.setCurrentPlayer(ShogiGameController::Player1);
        h.clock.startClock();
        const UsiTimingParams timing{0, QStringLiteral("10000"), QStringLiteral("10000"), 10000, 10000, false};
        h.match.requestMove(QStringLiteral("position startpos"), {}, timing);
        QTRY_VERIFY_WITH_TIMEOUT(h.completed > 0 || h.errors > 0 || h.clock.isGameOver(), 11000);
        QCOMPARE(h.errors, 0);
        QVERIFY(!h.clock.isGameOver());
        QCOMPARE(h.completed, 1);
        QVERIFY(h.lastTo != QPoint(-1, -1));
        qInfo() << "first move:" << h.position << "remaining ms:" << h.clock.getPlayer1TimeIntMs();
        const auto go = std::find_if(h.commands.cbegin(), h.commands.cend(), [](const QString& cmd) {
            return cmd.startsWith(QStringLiteral("go "));
        });
        QVERIFY(go != h.commands.cend());
        qInfo() << "time command:" << *go;
        QVERIFY(go->contains(QStringLiteral("binc 10000 winc 10000")));
    }
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
    void guiSettingOverridesLegacyOption()
    {
        EnginePonderSettings::save(QStringLiteral("MatchTest"), true, true);
        MatchHarness h;
        QVERIFY(h.start(mockPath(), false)); // 古いUSI_Ponder=falseよりGUI設定を優先
        QVERIFY(h.protocol.isPonderEnabled());
        QCOMPARE(h.commands.count(QStringLiteral("setoption name USI_Ponder value true")), 1);
        QVERIFY(!h.commands.contains(QStringLiteral("setoption name USI_Ponder value false")));
        QCOMPARE(h.reply(), QPoint(8, 4));
        QCOMPARE(h.protocol.currentPhase(), UsiProtocolHandler::SearchPhase::Ponder);
    }
    void ponderTransition_data()
    {
        QTest::addColumn<bool>("hit");
        QTest::addColumn<QString>("mode");
        QTest::newRow("legacy-hit") << true << QStringLiteral("legacy");
        QTest::newRow("legacy-miss-discards-resign") << false << QStringLiteral("legacy");
        QTest::newRow("hidden-hit") << true << QStringLiteral("hidden");
        QTest::newRow("hidden-miss-discards-resign") << false << QStringLiteral("hidden");
        QTest::newRow("gui-only-hit") << true << QStringLiteral("gui-only");
        QTest::newRow("gui-only-miss-discards-resign") << false << QStringLiteral("gui-only");
    }
    void ponderTransition()
    {
        QFETCH(bool, hit);
        QFETCH(QString, mode);
        MatchHarness h;
        qputenv("SBQ_MATCH_PONDER_MODE", mode.toUtf8());
        if (mode == QLatin1String("legacy")) {
            QVERIFY(h.start(mockPath()));
        } else {
            EnginePonderSettings::save(QStringLiteral("MatchTest"), true, mode != QLatin1String("gui-only"));
            QVERIFY(h.initialize(mockPath()));
        }
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
    void externallyStoppedPonderStartsNewSearch_data()
    {
        QTest::addColumn<bool>("responseReceived");
        QTest::newRow("stop-response-pending") << false;
        QTest::newRow("stop-response-received") << true;
    }
    void externallyStoppedPonderStartsNewSearch()
    {
        QFETCH(bool, responseReceived);
        MatchHarness h;
        QVERIFY(h.start(mockPath()));
        QCOMPARE(h.reply(), QPoint(8, 4));
        QSignalSpy resign(&h.protocol, &UsiProtocolHandler::bestMoveResignReceived);
        QSignalSpy stopped(&h.protocol, &UsiProtocolHandler::bestMoveReceived);
        h.protocol.sendStop();
        if (responseReceived) QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 2000);
        h.commands.clear();
        h.humanMove(QStringLiteral("2g2f")); // 元の予測に一致しても新しい探索が必要
        QCOMPARE(h.reply(), QPoint(3, 4));
        QCOMPARE(h.errors, 0);
        QCOMPARE(resign.count(), 0);
        QVERIFY(h.commands.first().startsWith(QStringLiteral("position ")));
        QVERIFY(h.commands.at(1).startsWith(QStringLiteral("go btime ")));
        QVERIFY(!h.commands.contains(QStringLiteral("ponderhit")));
        QVERIFY(!h.commands.contains(QStringLiteral("stop")));
    }
    void humanMoveApiRecognizesPonderHit()
    {
        EnginePonderSettings::save(QStringLiteral("MatchTest"), true, true);
        ShogiGameController game;
        QString sfen = SfenUtils::hirateSfen();
        game.newGame(sfen);
        game.setCurrentPlayer(ShogiGameController::Player2);
        Usi engine(nullptr, nullptr, &game);
        QSignalSpy initialized(&engine, &Usi::engineInitialized);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        QVERIFY(engine.startAndInitializeEngineAsync(mockPath(), QStringLiteral("MatchTest")));
        QTRY_COMPARE_WITH_TIMEOUT(initialized.count(), 1, 5000);
        QString position = QStringLiteral("position startpos");
        QStringList history;
        const UsiTimingParams timing{5000, QStringLiteral("300000"), QStringLiteral("0"), 0, 0, true};
        engine.requestHumanReply(position, {}, QPoint(7, 7), QPoint(7, 6), timing, history);
        QCOMPARE(history, QStringList{QStringLiteral("position startpos moves 7g7f")});
        QTRY_COMPARE_WITH_TIMEOUT(moves.count(), 1, 5000);
        QCOMPARE(moves.first().at(1).toPoint(), QPoint(8, 4));
        position = moves.first().at(2).toString();
        const QString ponder = moves.first().at(3).toString();
        engine.requestHumanReply(position, ponder, QPoint(2, 7), QPoint(2, 6), timing, history);
        QTRY_COMPARE_WITH_TIMEOUT(moves.count(), 2, 5000);
        QCOMPARE(moves.last().at(1).toPoint(), QPoint(3, 4));
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.last(), ponder);
        engine.cleanupEngineProcessAndThread();
    }
    void actualDeadlineRejectsLateMove()
    {
        MatchHarness h;
        h.byoyomi = 1000;
        QVERIFY(h.start(mockPath(), false));
        h.protocol.sendSetOption(QStringLiteral("ReplyDelay"), QStringLiteral("1100"));
        QCOMPARE(h.reply(), QPoint(-1, -1));
        QVERIFY(h.clock.isGameOver());
        QCOMPARE(h.errors, 0);
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
    void unreportedPonderRealProcess_data()
    {
        QTest::addColumn<bool>("sendUnreported");
        QTest::newRow("notify-reserved-option") << true;
        QTest::newRow("gui-only") << false;
    }
    void unreportedPonderRealProcess()
    {
        const QString engine = qEnvironmentVariable("SHOGIBOARDQ_TEST_PONDER_ENGINE");
        if (engine.isEmpty()) QSKIP("Set SHOGIBOARDQ_TEST_PONDER_ENGINE to an engine such as Gikou 2.");
        QFETCH(bool, sendUnreported);
        {
            QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
            settings.beginWriteArray(QStringLiteral("MatchTest"), 2);
            settings.setArrayIndex(0);
            settings.setValue(QStringLiteral("name"), QStringLiteral("OwnBook"));
            settings.setValue(QStringLiteral("type"), QStringLiteral("check"));
            settings.setValue(QStringLiteral("value"), QStringLiteral("false"));
            settings.setArrayIndex(1);
            settings.setValue(QStringLiteral("name"), QStringLiteral("Threads"));
            settings.setValue(QStringLiteral("type"), QStringLiteral("spin"));
            settings.setValue(QStringLiteral("value"), QStringLiteral("1"));
            settings.endArray();
        }
        EnginePonderSettings::save(QStringLiteral("MatchTest"), true, sendUnreported);
        MatchHarness h;
        h.byoyomi = 1000;
        QVERIFY(h.initialize(engine));
        QCOMPARE(h.commands.contains(QStringLiteral("setoption name USI_Ponder value true")), sendUnreported);
        QVERIFY(h.reply() != QPoint(-1, -1));
        QVERIFY(!h.protocol.predictedMove().isEmpty());
        QVERIFY(h.commands.last().startsWith(QStringLiteral("go ponder ")));
        QTest::qWait(100); // 相手の手番中に実際に先読みさせる
        h.commands.clear();
        h.humanMove(h.protocol.predictedMove());
        QVERIFY(h.reply() != QPoint(-1, -1));
        QCOMPARE(h.commands.first(), QStringLiteral("ponderhit"));
        QVERIFY(!h.protocol.predictedMove().isEmpty());
        QVERIFY(h.commands.last().startsWith(QStringLiteral("go ponder ")));
        QTest::qWait(100);
        h.commands.clear();
        const QString move = h.position.contains(QStringLiteral("5g5f"))
            || h.protocol.predictedMove() == QLatin1String("5g5f") ? QStringLiteral("9g9f") : QStringLiteral("5g5f");
        h.humanMove(move);
        QVERIFY(h.reply() != QPoint(-1, -1));
        QCOMPARE(h.commands.first(), QStringLiteral("stop"));
        QCOMPARE(h.errors, 0);
        QVERIFY(!h.clock.isGameOver());
    }
};

QTEST_MAIN(TestUsiMatchHandler)
#include "tst_usimatchhandler.moc"
