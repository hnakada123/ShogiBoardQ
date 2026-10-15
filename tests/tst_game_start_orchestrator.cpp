/// 対局開始には指定された手順だけを使用する。USIプロセスまで実物の経路で検証する。
#include <QtTest>
#include <QTemporaryDir>
#include "gamestartorchestrator.h"
#include "matchcoordinator.h"
#include "strategycontext.h"
#include "sfenpositiontracer.h"
#include "shogiclock.h"
#include "usi.h"

class TestGameStartOrchestrator : public QObject
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
        RecordLine line;
        line.sfens = SfenPositionTracer::buildSfenRecord(initial, moves, false);
        line.moves = SfenPositionTracer::buildGameMoves(initial, moves);
        line.currentPly = 3; // 後続の4手目を含めない
        ShogiGameController game;
        ShogiClock clock;
        MatchCoordinator::Deps deps;
        deps.gc = &game;
        deps.clock = &clock;
        deps.hooks.game.initializeNewGame = [&game](const QString& sfen) {
            QString position = sfen;
            game.newGame(position);
        };
        MatchCoordinator match(deps);
        // 別の対局が先に存在しても、開始要求で指定した手順を使う。
        MatchStartOptions old;
        old.mode = PlayMode::HumanVsHuman;
        old.sfenStart = SfenUtils::hirateSfen();
        match.configureAndStart(old);
        MatchStartOptions options;
        options.mode = PlayMode::HumanVsHuman;
        options.recordLine = line;
        options.sfenStart = line.sfens.at(3);
        match.configureAndStart(options);
        QCOMPARE(match.gameMoves().size(), 3);
        QCOMPARE(*match.sfenRecordPtr(), line.sfens.mid(0, 4));
        QCOMPARE(match.strategyCtx().currentMoveIndex(), 3);

        Usi engine(nullptr, nullptr, &game);
        QSignalSpy initialized(&engine, &Usi::engineInitialized);
        QSignalSpy ready(&engine, &Usi::matchMoveReady);
        QVERIFY(engine.startAndInitializeEngineAsync(
            QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match"), QStringLiteral("StartTest")));
        QTRY_COMPARE_WITH_TIMEOUT(initialized.count(), 1, 5000);
        match.strategyCtx().requestEngineMove(&engine, 1);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);
        const QString bestMove = qvariant_cast<UsiMove>(ready.first().at(0)).toString();
        QCOMPARE(ready.first().at(1).toString(), expectedPosition + QLatin1Char(' ') + bestMove);
        engine.cleanupEngineProcessAndThread();
    }
    void startOptionsAndNames()
    {
        StartGameDialogData data;
        data.humanName1 = QStringLiteral("先手名");
        data.humanName2 = QStringLiteral("後手名");
        data.maxMoves = 128;
        data.autoSaveKifu = true;
        data.kifuSaveDir = m_config.path();
        const auto options = GameStartOrchestrator::buildStartOptions(
            PlayMode::HumanVsHuman, SfenUtils::hirateSfen(), nullptr, &data);
        MatchSessionState state;
        GameStartOrchestrator orchestrator;
        orchestrator.setRefs({nullptr, &state});
        GameStartOrchestrator::Hooks hooks;
        QStringList names;
        hooks.setPlayersNames = [&names](const QString& black, const QString& white) { names = {black, white}; };
        orchestrator.setHooks(hooks);
        orchestrator.configureAndStart(options);
        QCOMPARE(state.playMode(), PlayMode::HumanVsHuman);
        QCOMPARE(state.maxMoves(), 128);
        QVERIFY(state.options().autoSaveKifu);
        QCOMPARE(state.options().kifuSaveDir, m_config.path());
        QCOMPARE(names, (QStringList{data.humanName1, data.humanName2}));
    }
    void humanSideControlsFlip()
    {
        GameStartOrchestrator orchestrator;
        GameStartOrchestrator::Hooks hooks;
        int flips = 0;
        hooks.flipBoard = [&flips] { ++flips; };
        orchestrator.setHooks(hooks);
        StartGameDialogData data;
        data.isHuman1 = false;
        data.isHuman2 = true;
        data.isShowHumanInFront = true;
        orchestrator.ensureHumanAtBottomIfApplicable(&data, Turn::Black);
        QCOMPARE(flips, 1);
        orchestrator.ensureHumanAtBottomIfApplicable(&data, Turn::White);
        QCOMPARE(flips, 1);
        data.isHuman1 = true;
        orchestrator.ensureHumanAtBottomIfApplicable(&data, Turn::Black);
        QCOMPARE(flips, 1);
    }
};
QTEST_MAIN(TestGameStartOrchestrator)
#include "tst_game_start_orchestrator.moc"
