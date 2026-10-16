#include <QtTest>
#include <QSettings>
#include <QFile>
#include <algorithm>

#include "enginesettingsconstants.h"
#include "usilineparser.h"
#include "boardcoordinates.h"
#include "engineprocessmanager.h"
#include "usiprotocolhandler.h"
#include "usimatchhandler.h"
#include "matchsearchsnapshot.h"
#include "timecontrol_test_helper.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "thinkinginfoformatter.h"
#include "settingscommon.h"
#include "enginepondersettings.h"
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
    UsiProtocolHandler protocol{&process};
    ThinkingInfoFormatter formatter;
    UsiMatchHandler match{&protocol};
    QStringList commands;
    UsiPosition position = *UsiPosition::parse(u"position startpos moves 7g7f");
    int errors = 0;
    int resigns = 0;
    int byoyomi = 5000;
    int completed = 0;
    QPoint lastTo{-1, -1};

    MatchHarness()
    {
        QString initial = SfenUtils::hirateSfen();
        game.newGame(initial);
        game.setCurrentPlayer(Turn::White);
        match.setClock(&clock);
        connect(&protocol, &UsiProtocolHandler::positionChanged, &formatter, &ThinkingInfoFormatter::setSearchPosition);
        connect(&match, &UsiMatchHandler::moveReady, this, &MatchHarness::acceptMove);
        connect(&match, &UsiMatchHandler::timedOut, this, &MatchHarness::onTimeout);
        connect(&match, &UsiMatchHandler::resigned, this, &MatchHarness::onResigned);
        connect(&process, &EngineProcessManager::commandSent, this, &MatchHarness::recordCommand);
    }
    ~MatchHarness() override
    {
        process.stopProcessAsync();
        QFile::remove(SettingsCommon::settingsFilePath());
    }
    void acceptMove(const UsiMatchMove& move)
    {
        lastTo = BoardCoordinates::toSquare(move.move);
        position = *position.withMove(move.move);
        ++completed;
    }
    void onTimeout()
    {
        ++errors;
        protocol.cancelCurrentOperation();
    }
    void onResigned(Turn) { ++resigns; }
    bool isPondering() const
    {
        return protocol.isSearching() && protocol.searchKind() == UsiProtocolHandler::SearchKind::Ponder;
    }
    /// 先読みで予想している相手の手
    QString predictedMove() const { return protocol.searchCandidate().predictedMove; }
    void recordCommand(const QString& command) { commands.append(command); }
    bool start(const QString& path, bool ponderEnabled = true)
    {
        {
            QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
            settings.beginWriteArray(EngineSettingsConstants::engineOptionsGroup(QStringLiteral("MatchTest")), 1);
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
        clock.setTimeControl(TestTime::rules(300, 0, 0, byoyomi / 1000, 0, 0));
        clock.setCurrentPlayer(Turn::White);
        clock.startClock();
        return true;
    }
    QPoint reply()
    {
        const int previous = completed;
        lastTo = QPoint(-1, -1);
        // 思考時間はつないだ時計から求める
        match.requestMove(position, UsiTimingParams{});
        const bool finished = QTest::qWaitFor([&] {
            return completed > previous || errors > 0 || clock.isGameOver();
        }, byoyomi + 2000);
        return finished ? lastTo : QPoint(-1, -1);
    }
    void humanMove(const QString& move)
    {
        clock.setCurrentPlayer(Turn::Black);
        clock.commitMove(Turn::White);
        position = *position.withMove(*UsiMove::parse(move));
        clock.setCurrentPlayer(Turn::White);
    }
};

class TestUsiMatchHandler : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    /// 送信したコマンドのうち、指定の接頭辞で始まる最初の1件を数値の項目に分けて返す
    static QHash<QString, qint64> goFields(const QStringList& commands, const QString& prefix)
    {
        QHash<QString, qint64> fields;
        const auto go = std::find_if(commands.cbegin(), commands.cend(), [&prefix](const QString& cmd) {
            return cmd.startsWith(prefix);
        });
        if (go == commands.cend()) return fields;
        const QStringList tokens = go->mid(prefix.size()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (qsizetype i = 0; i + 1 < tokens.size(); i += 2) fields.insert(tokens.at(i), tokens.at(i + 1).toLongLong());
        return fields;
    }
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
            settings.beginWriteArray(EngineSettingsConstants::engineOptionsGroup(QStringLiteral("MatchTest")), 1);
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
        QCOMPARE(h.isPondering(), expectPonder);
        QCOMPARE(h.commands.last().startsWith(QStringLiteral("go ponder ")), expectPonder);
        QCOMPARE(h.errors, 0);
    }
    void incrementOnlyFirstMove()
    {
        MatchHarness h;
        const QString realEngine = qEnvironmentVariable("SHOGIBOARDQ_TEST_ENGINE");
        QVERIFY(h.start(realEngine.isEmpty() ? mockPath() : realEngine, false));
        h.clock.stopClock();
        // 持ち時間0・加算10秒。時計は初手の加算を足して10秒から始まる。
        h.clock.setTimeControl(TestTime::rules(0, 0, 0, 0, 10, 10));
        h.clock.setCurrentPlayer(Turn::Black);
        h.game.setCurrentPlayer(Turn::Black);
        h.clock.startClock();
        h.match.requestMove(*UsiPosition::parse(u"position startpos"), UsiTimingParams{});
        QTRY_VERIFY_WITH_TIMEOUT(h.completed > 0 || h.errors > 0 || h.clock.isGameOver(), 11000);
        QCOMPARE(h.errors, 0);
        QVERIFY(!h.clock.isGameOver());
        QCOMPARE(h.completed, 1);
        QVERIFY(h.lastTo != QPoint(-1, -1));
        qInfo() << "first move:" << h.position.command() << "remaining ms:" << h.clock.remainingMs(Turn::Black);
        const auto go = std::find_if(h.commands.cbegin(), h.commands.cend(), [](const QString& cmd) {
            return cmd.startsWith(QStringLiteral("go "));
        });
        QVERIFY(go != h.commands.cend());
        qInfo() << "time command:" << *go;
        // 持ち時間0なので今回使えるのは加算分だけ。余裕を引いた分だけ手番側の加算を減らし、
        // btime + binc が実際に使える時間を超えないようにする。
        const auto fields = goFields(h.commands, QStringLiteral("go "));
        QCOMPARE(fields.value(QStringLiteral("btime"), -1), 0LL);
        QCOMPARE(fields.value(QStringLiteral("wtime"), -1), 0LL);
        QVERIFY2(fields.value(QStringLiteral("binc")) > 9000 && fields.value(QStringLiteral("binc")) <= 9750,
                 qPrintable(*go));
        QCOMPARE(fields.value(QStringLiteral("winc"), -1), 10000LL);
    }
    void incrementWithMainTimeSendsFullIncrement()
    {
        MatchHarness h;
        QVERIFY(h.start(mockPath(), false));
        h.clock.stopClock();
        // 持ち時間300秒＋加算10秒。時計は初手の加算を含めて310秒から始まる。
        h.clock.setTimeControl(TestTime::rules(300, 300, 0, 0, 10, 10));
        h.clock.setCurrentPlayer(Turn::Black);
        h.game.setCurrentPlayer(Turn::Black);
        h.clock.startClock();
        h.match.requestMove(*UsiPosition::parse(u"position startpos"), UsiTimingParams{});
        QTRY_VERIFY_WITH_TIMEOUT(h.completed > 0 || h.errors > 0, 5000);
        QCOMPARE(h.errors, 0);
        const auto fields = goFields(h.commands, QStringLiteral("go "));
        const qint64 btime = fields.value(QStringLiteral("btime"), -1);
        QVERIFY2(btime > 299000 && btime <= 299750, qPrintable(h.commands.join(QLatin1Char('\n'))));
        QCOMPARE(fields.value(QStringLiteral("wtime"), -1), 300000LL);
        QCOMPARE(fields.value(QStringLiteral("binc"), -1), 10000LL);
        QCOMPARE(fields.value(QStringLiteral("winc"), -1), 10000LL);
    }
    void incrementPonderExcludesNextIncrement()
    {
        EnginePonderSettings::save(QStringLiteral("MatchTest"), true, true);
        MatchHarness h;
        QVERIFY(h.start(mockPath(), true));
        h.clock.stopClock();
        h.clock.setTimeControl(TestTime::rules(300, 300, 0, 0, 10, 10));
        h.clock.setCurrentPlayer(Turn::Black);
        h.game.setCurrentPlayer(Turn::Black);
        h.clock.startClock();
        h.match.requestMove(*UsiPosition::parse(u"position startpos"), UsiTimingParams{});
        QTRY_VERIFY_WITH_TIMEOUT(h.completed > 0 || h.errors > 0, 5000);
        QCOMPARE(h.errors, 0);
        // 先読み開始時は自分の着手の加算前なので、自分の残り時間は加算を含まない。
        // 相手（次に指す側）の残り時間は次の加算分を含むので差し引く。
        const auto fields = goFields(h.commands, QStringLiteral("go ponder "));
        QVERIFY2(!fields.isEmpty(), qPrintable(h.commands.join(QLatin1Char('\n'))));
        const qint64 btime = fields.value(QStringLiteral("btime"), -1);
        QVERIFY2(btime > 305000 && btime <= 309750, qPrintable(QString::number(btime)));
        QCOMPARE(fields.value(QStringLiteral("wtime"), -1), 300000LL);
        QCOMPARE(fields.value(QStringLiteral("binc"), -1), 10000LL);
        QCOMPARE(fields.value(QStringLiteral("winc"), -1), 10000LL);
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
        QVERIFY(h.isPondering());
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
        QCOMPARE(h.predictedMove(), QStringLiteral("2g2f"));
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
    // 先読み中の読み筋は、エンジンの指した手と予想手を進めた局面から日本語にする
    void ponderPvUsesPredictedPosition()
    {
        MatchHarness h;
        QVERIFY(h.start(mockPath()));
        QCOMPARE(h.reply(), QPoint(8, 4));
        QVERIFY(h.isPondering());
        // 通信した局面（7g7f後）から、bestmoveと予測手を進めた局面。表示中の盤には依存しない。
        QCOMPARE(h.protocol.searchCandidate().baseSfen,
                 QStringLiteral("lnsgkgsnl/1r5b1/p1ppppppp/1p7/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"));
        QSignalSpy rows(&h.formatter, &ThinkingInfoFormatter::thinkingInfoUpdated);
        h.formatter.processInfo(*UsiLineParser::parseInfo(QStringLiteral("info depth 1 score cp 0 pv 8d8e 2f2e 8e8f")));
        QCOMPARE(rows.count(), 1);
        const auto row = qvariant_cast<ThinkingInfoRow>(rows.at(0).at(0));
        QCOMPARE(row.pvKanji, QStringLiteral("△８五歩(84)▲２五歩(26)△８六歩(85)"));
        QCOMPARE(row.usiPv, QStringLiteral("8d8e 2f2e 8e8f"));
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
        QSignalSpy stopped(&h.protocol, &UsiProtocolHandler::bestMoveReceived);
        h.protocol.sendStop();
        if (responseReceived) QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 2000);
        h.commands.clear();
        h.humanMove(QStringLiteral("2g2f")); // 元の予測に一致しても新しい探索が必要
        QCOMPARE(h.reply(), QPoint(3, 4));
        QCOMPARE(h.errors, 0);
        QCOMPARE(h.resigns, 0);
        QVERIFY(h.commands.first().startsWith(QStringLiteral("position ")));
        QVERIFY(h.commands.at(1).startsWith(QStringLiteral("go btime ")));
        QVERIFY(!h.commands.contains(QStringLiteral("ponderhit")));
        QVERIFY(!h.commands.contains(QStringLiteral("stop")));
    }
    void committedPositionRecognizesPonderHit()
    {
        EnginePonderSettings::save(QStringLiteral("MatchTest"), true, true);
        ShogiGameController game;
        QString sfen = SfenUtils::hirateSfen();
        game.newGame(sfen);
        game.setCurrentPlayer(Turn::White);
        Usi engine;
        QSignalSpy initialized(&engine, &Usi::engineInitialized);
        QSignalSpy moves(&engine, &Usi::matchMoveReady);
        QVERIFY(engine.startEngine(mockPath(), QStringLiteral("MatchTest")));
        QTRY_COMPARE_WITH_TIMEOUT(initialized.count(), 1, 5000);
        // 時計がないので、残り時間と秒読みを条件で渡す
        UsiTimingParams timing;
        timing.timeControl.black.byoyomiMs = timing.timeControl.white.byoyomiMs = 5000;
        timing.btimeMs = 300000;
        UsiPosition position = *UsiPosition::parse(u"position startpos moves 7g7f");
        engine.requestMatchMove(position, timing);
        QTRY_COMPARE_WITH_TIMEOUT(moves.count(), 1, 5000);
        QCOMPARE(moves.first().at(0).value<UsiMatchMove>().move.to, (BoardSquare{8, 4}));
        position = *position.withMove(moves.first().at(0).value<UsiMatchMove>().move);
        QCOMPARE(position.command(), QStringLiteral("position startpos moves 7g7f 8c8d"));
        // 先読みの予想（2g2f）どおりに指した局面は、先読みの局面と同じ値になる
        position = *position.withMove(*UsiMove::parse(u"2g2f"));
        engine.requestMatchMove(position, timing);
        QTRY_COMPARE_WITH_TIMEOUT(moves.count(), 2, 5000);
        QCOMPARE(moves.last().at(0).value<UsiMatchMove>().move.to, (BoardSquare{3, 4}));
        engine.quit();
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
        QCOMPARE(h.position.command(), QStringLiteral("position startpos moves 7g7f"));
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
        QVERIFY(!h.predictedMove().isEmpty());
        h.commands.clear();
        h.humanMove(h.predictedMove());
        QVERIFY(h.reply() != QPoint(-1, -1));
        QCOMPARE(h.commands.first(), QStringLiteral("ponderhit"));
        // 先読みと異なる合法な歩突き。最初の予測手は通常2g2fなど。
        const QString predicted = h.predictedMove();
        if (!predicted.isEmpty()) {
            const QString move = h.position.moves().contains(QStringLiteral("5g5f"))
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
            settings.beginWriteArray(EngineSettingsConstants::engineOptionsGroup(QStringLiteral("MatchTest")), 2);
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
        QVERIFY(!h.predictedMove().isEmpty());
        QVERIFY(h.commands.last().startsWith(QStringLiteral("go ponder ")));
        QTest::qWait(100); // 相手の手番中に実際に先読みさせる
        h.commands.clear();
        h.humanMove(h.predictedMove());
        QVERIFY(h.reply() != QPoint(-1, -1));
        QCOMPARE(h.commands.first(), QStringLiteral("ponderhit"));
        QVERIFY(!h.predictedMove().isEmpty());
        QVERIFY(h.commands.last().startsWith(QStringLiteral("go ponder ")));
        QTest::qWait(100);
        h.commands.clear();
        const QString move = h.position.moves().contains(QStringLiteral("5g5f"))
            || h.predictedMove() == QLatin1String("5g5f") ? QStringLiteral("9g9f") : QStringLiteral("5g5f");
        h.humanMove(move);
        QVERIFY(h.reply() != QPoint(-1, -1));
        QCOMPARE(h.commands.first(), QStringLiteral("stop"));
        QCOMPARE(h.errors, 0);
        QVERIFY(!h.clock.isGameOver());
    }
};

QTEST_MAIN(TestUsiMatchHandler)
#include "tst_usimatchhandler.moc"
