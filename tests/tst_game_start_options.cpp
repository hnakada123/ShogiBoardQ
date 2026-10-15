/// 対局開始には指定された手順だけを使用する。USIプロセスまで実物の経路で検証する。
#include <QtTest>
#include <QTemporaryDir>
#include "gamestartoptionsbuilder.h"
#include "match_test_helper.h"
#include "usi.h"

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
        RecordLine line = RecordLine::fromPositions(SfenReplay::buildSfenRecord(initial, moves, false),
                                                    SfenReplay::buildGameMoves(initial, moves));
        line.currentPly = 3; // 後続の4手目を含めない

        MatchHarness h;
        // 別の対局が先に存在しても、開始要求で指定した手順を使う。
        h.startHumanVsHuman();
        QVERIFY(h.playHuman(QStringLiteral("5g5f")));
        MatchStartOptions options;
        options.recordLine = line;
        h.mc->start(options);
        QCOMPARE(h.mc->currentPly(), 3);
        QCOMPARE(h.mc->record().sfens(), line.sfens().mid(0, 4));
        QCOMPARE(h.gc.board()->toSfen(), line.sfenAt(3));
        QCOMPARE(h.gc.currentPlayer(), Turn::White);

        // 手番（後手）のエンジンに、選んだ手順の局面で思考させる
        Usi* engine = h.engines.createEngine(1, Turn::White, QStringLiteral("StartTest"));
        QVERIFY(engine);
        QSignalSpy initialized(engine, &Usi::engineInitialized);
        QSignalSpy ready(engine, &Usi::matchMoveReady);
        h.engines.startEngine(engine, MatchHarness::mockEnginePath(), QStringLiteral("StartTest"));
        QTRY_COMPARE_WITH_TIMEOUT(initialized.count(), 1, 5000);
        h.mc->strategyHost().requestEngineMove(Turn::White);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);
        const QString bestMove = qvariant_cast<UsiMove>(ready.first().at(0)).toString();
        QCOMPARE(ready.first().at(1).toString(), expectedPosition + QLatin1Char(' ') + bestMove);
    }
    void startOptionsAndNames()
    {
        StartGameDialogData data;
        data.humanName1 = QStringLiteral("先手名");
        data.humanName2 = QStringLiteral("後手名");
        data.maxMoves = 128;
        data.autoSaveKifu = true;
        data.kifuSaveDir = m_config.path();
        data.isHuman1 = data.isHuman2 = true;
        const MatchStartOptions options = GameStartOptionsBuilder::buildStartOptions(
            SfenUtils::hirateSfen(), data);
        MatchHarness h;
        QSignalSpy prepared(h.mc.get(), &MatchCoordinator::gamePrepared);
        h.mc->start(options);
        QVERIFY(h.mc->players().humanVsHuman());
        QCOMPARE(h.mc->options().maxMoves, 128);
        QVERIFY(h.mc->options().autoSaveKifu);
        QCOMPARE(h.mc->options().kifuSaveDir, m_config.path());
        QCOMPARE(prepared.count(), 1);
        const auto announced = prepared.first().first().value<MatchStartOptions>();
        QCOMPARE(announced.players.blackHumanName, data.humanName1);
        QCOMPARE(announced.players.whiteHumanName, data.humanName2);
    }
    void humanSideControlsFlip()
    {
        StartGameDialogData data;
        data.isHuman1 = false;
        data.isHuman2 = true;
        data.isShowHumanInFront = true;
        QVERIFY(GameStartOptionsBuilder::needsHumanAtBottomFlip(data, Turn::Black));
        QVERIFY(!GameStartOptionsBuilder::needsHumanAtBottomFlip(data, Turn::White));
        data.isHuman1 = true;
        QVERIFY(!GameStartOptionsBuilder::needsHumanAtBottomFlip(data, Turn::Black));
    }
};
QTEST_MAIN(TestGameStartOptions)
#include "tst_game_start_options.moc"
