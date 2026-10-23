/// 対局開始には指定された手順だけを使用する。USIプロセスまで実物の経路で検証する。
#include <QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include "gamestartoptionsbuilder.h"
#include "match_test_helper.h"
#include "engineprocessmanager.h"
#include "usi.h"
#include "thinkinginfoformatter.h"
#include "test_replay.h"

/// 後手をテスト用の USI エンジンが指す（人間対エンジンではエンジンは E1 の枠）
static void useWhiteEngine(MatchStartOptions& options, const QString& name)
{
    options.players.seat(Turn::White).engine = true;
    options.players.seat(Turn::White).engineName = name;
    options.engine(Turn::White).path = MatchHarness::mockEnginePath();
}

class TestGameStartOptions : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }
    void selectedLineReachesEngine_data()
    {
        QTest::addColumn<QStringList>("moves");
        QTest::addColumn<int>("counter");
        QTest::addColumn<QString>("expectedPosition");
        QTest::newRow("main") << QStringList{"7g7f", "3c3d", "2g2f", "8c8d"} << 1
            << QStringLiteral("position startpos moves 7g7f 3c3d 2g2f");
        QTest::newRow("transposed-variation") << QStringList{"2g2f", "8c8d", "7g7f", "3c3d"} << 1
            << QStringLiteral("position startpos moves 2g2f 8c8d 7g7f");
        QTest::newRow("non-one-sfen-counter") << QStringList{"2g2f", "8c8d", "7g7f", "3c3d"} << 41
            << QStringLiteral("position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 41 moves 2g2f 8c8d 7g7f");
    }
    void selectedLineReachesEngine()
    {
        QFETCH(QStringList, moves);
        QFETCH(int, counter);
        QFETCH(QString, expectedPosition);
        QString initial = SfenUtils::hirateSfen();
        initial = initial.left(initial.lastIndexOf(' ') + 1) + QString::number(counter);
        RecordLine line = TestReplay::recordLine(TestReplay::buildSfenRecord(initial, moves, false),
                                                    TestReplay::buildGameMoves(initial, moves));
        line.currentPly = 3; // 後続の4手目を含めない

        MatchHarness h;
        // 別の対局が先に存在しても、開始要求で指定した手順を使う。
        h.startHumanVsHuman();
        QVERIFY(h.playHuman(QStringLiteral("5g5f")));
        MatchStartOptions options;
        options.recordLine = line;
        useWhiteEngine(options, QStringLiteral("StartTest"));
        h.mc->start(options);
        QCOMPARE(h.mc->currentPly(), 3);
        QCOMPARE(h.mc->record().sfens(), line.sfens().mid(0, 4));
        QCOMPARE(h.gc.board()->toSfen(), line.sfenAt(3));
        QCOMPARE(h.gc.currentPlayer(), Turn::White);

        // 手番（後手）のエンジンに、選んだ手順の局面で思考させる
        Usi* engine = h.engines.engineInSlot(1);
        QVERIFY(engine);
        QSignalSpy initialized(engine, &Usi::engineInitialized);
        QSignalSpy ready(engine, &Usi::matchMoveReady);
        QSignalSpy sent(engine->process(), &EngineProcessManager::commandSent);
        QTRY_COMPARE_WITH_TIMEOUT(initialized.count(), 1, 5000);
        h.mc->strategyHost().requestEngineMove(Turn::White);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);
        QStringList positions;
        for (const QList<QVariant>& args : std::as_const(sent)) {
            const QString command = args.at(0).toString();
            if (command.startsWith(QStringLiteral("position "))) positions << command;
        }
        QCOMPARE(positions, QStringList{expectedPosition});
    }
    /// 定跡を使うエンジンの手番では、探索させずに定跡手を1秒置いて指す（position・go を送らない）
    void bookMoveSkipsEngineSearch()
    {
        const QString initial = SfenUtils::hirateSfen();
        const QStringList moves{QStringLiteral("7g7f")};
        RecordLine line = TestReplay::recordLine(TestReplay::buildSfenRecord(initial, moves, false),
                                                    TestReplay::buildGameMoves(initial, moves));
        line.currentPly = 1;
        MatchStartOptions options;
        options.sfenStart = initial;
        options.recordLine = line;
        options.josekiBook.putMove(line.sfenAt(1), {QStringLiteral("8c8d"), QStringLiteral("none"), 0, 0, 1, {}});
        options.josekiBook.putMove(line.sfenAt(1), {QStringLiteral("3c3d"), QStringLiteral("none"), 0, 0, 2, {}});
        options.engine(Turn::White).joseki.enabled = true;
        options.engine(Turn::White).joseki.method = JosekiPlayOptions::FirstMove;
        useWhiteEngine(options, QStringLiteral("BookTest"));
        MatchHarness h;
        h.mc->start(options);

        Usi* engine = h.engines.engineInSlot(1);
        QVERIFY(engine);
        QSignalSpy initialized(engine, &Usi::engineInitialized);
        QSignalSpy ready(engine, &Usi::matchMoveReady);
        QSignalSpy sent(engine->process(), &EngineProcessManager::commandSent);
        QSignalSpy status(engine->thinking(), &ThinkingInfoFormatter::searchStatusUpdated);
        QTRY_COMPARE_WITH_TIMEOUT(initialized.count(), 1, 5000);
        sent.clear();
        QElapsedTimer elapsed;
        elapsed.start();
        h.mc->strategyHost().requestEngineMove(Turn::White);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);
        QVERIFY(elapsed.elapsed() >= 900);
        const UsiMatchMove played = ready.first().at(0).value<UsiMatchMove>();
        QCOMPARE(played.move.toString(), QStringLiteral("8c8d"));
        QVERIFY(played.score.isNone());
        // 情報欄の「探索手」に、定跡ウィンドウの「比率」と同じ小数1桁の割合を添えて出す
        QVERIFY(!status.isEmpty());
        const QString label = status.last().at(0).value<EngineSearchStatus>().searchedMove;
        QVERIFY2(label.endsWith(QStringLiteral("（定跡 33.3%）")), qPrintable(label));
        for (const QList<QVariant>& args : std::as_const(sent)) {
            const QString command = args.at(0).toString();
            QVERIFY2(!command.startsWith(QStringLiteral("go")) && !command.startsWith(QStringLiteral("position")),
                     qPrintable(command));
        }
    }
    /// 定跡を使う条件は、エンジンの対局者のものだけを対局に渡す
    void bookOptionsOnlyForEngines()
    {
        StartGameDialogData data;
        data.players[0].joseki.enabled = true;
        data.players[1].kind = PlayerChoice::Kind::Engine;
        data.players[1].engine.path = MatchHarness::mockEnginePath();
        data.players[1].joseki.enabled = true;
        data.players[1].joseki.maxPly = 0;  // 範囲に収める
        const MatchStartOptions options = GameStartOptionsBuilder::buildStartOptions(SfenUtils::hirateSfen(), data);
        QVERIFY(!options.engine(Turn::Black).joseki.enabled);
        QVERIFY(options.engine(Turn::White).joseki.enabled);
        QCOMPARE(options.engine(Turn::White).joseki.maxPly, 1);
    }
    void startOptionsAndNames()
    {
        StartGameDialogData data;
        data.players[0].humanName = QStringLiteral("先手名");
        data.players[1].humanName = QStringLiteral("後手名");
        data.maxMoves = 128;
        data.autoSaveKifu = true;
        data.kifuSaveDir = m_config.path();
        const MatchStartOptions options = GameStartOptionsBuilder::buildStartOptions(
            SfenUtils::hirateSfen(), data);
        MatchHarness h;
        h.mc->start(options);
        QVERIFY(h.mc->players().humanVsHuman());
        QCOMPARE(h.mc->options().maxMoves, 128);
        QVERIFY(h.mc->options().autoSave.isActive());
        QCOMPARE(h.mc->options().autoSave.dir, m_config.path());
        QCOMPARE(h.mc->options().autoSave.extension, QStringLiteral("kifu"));
        QCOMPARE(h.mc->players().seat(Turn::Black).humanName, data.players[0].humanName);
        QCOMPARE(h.mc->players().seat(Turn::White).humanName, data.players[1].humanName);
    }
    void humanSideControlsFlip()
    {
        StartGameDialogData data;
        data.players[0].kind = PlayerChoice::Kind::Engine;
        data.isShowHumanInFront = true;
        QVERIFY(GameStartOptionsBuilder::needsHumanAtBottomFlip(data, Turn::Black));
        QVERIFY(!GameStartOptionsBuilder::needsHumanAtBottomFlip(data, Turn::White));
        data.players[0].kind = PlayerChoice::Kind::Human;
        QVERIFY(!GameStartOptionsBuilder::needsHumanAtBottomFlip(data, Turn::Black));
    }
};
QTEST_MAIN(TestGameStartOptions)
#include "tst_game_start_options.moc"
