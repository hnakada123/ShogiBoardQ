/// @brief 実際の盤・時計・司令塔で共通の着手確定経路を検証する。
#include <QtTest>
#include <QTemporaryDir>
#include "matchcoordinator.h"
#include "strategycontext.h"
#include "humanvshumanstrategy.h"
#include "humanvsenginestrategy.h"
#include "enginevsenginestrategy.h"
#include "shogiclock.h"
#include "sfenutils.h"
#include "timecontrol_test_helper.h"

struct StrategyHarness {
    ShogiGameController game;
    ShogiClock clock;
    QStringList sfens{SfenUtils::hirateSfen()};
    QList<ShogiMove> moves;
    QList<RecordEntry> records;
    QStringList events;
    std::unique_ptr<MatchCoordinator> match;

    StrategyHarness()
    {
        QString initial = sfens.first();
        game.newGame(initial);
        clock.setTimeControl(TestTime::rules(600, 600, 0, 0, 2, 2));
        MatchCoordinator::Deps deps;
        deps.gc = &game;
        deps.clock = &clock;
        deps.sfenRecord = &sfens;
        deps.gameMoves = &moves;
        deps.hooks.game.appendRecord = [this](const RecordEntry& entry) {
            records.append(entry); events.append("record");
        };
        deps.hooks.ui.renderBoardFromGc = [this]() { events.append("render"); };
        deps.hooks.ui.updateTurnDisplay = [this](Turn) { events.append("turn"); };
        deps.hooks.ui.showGameOverDialog = [this](const QString&, const QString&) { events.append("end"); };
        deps.hooks.game.appendEval = [this](Turn) { events.append("eval"); };
        match = std::make_unique<MatchCoordinator>(deps);
    }
    bool applyHuman(const QString& text, QString& pretty)
    {
        const auto move = UsiMove::parse(text);
        return move && game.applyMove(*move, pretty, static_cast<int>(sfens.size()), &sfens, moves);
    }
};

class TestGameStrategy : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
    StrategyHarness* m_observed = nullptr;
    bool m_coherentMove = false;
    void observeCommittedMove(Turn mover, int ply)
    {
        const auto& h = *m_observed;
        m_coherentMove = mover == Turn::Black && ply == 1 && h.records.size() == 1
            && h.game.currentPlayer() == Turn::White && h.sfens.size() == 2 && h.moves.size() == 1
            && h.records.last().sfen == h.sfens.last()
            && h.records.last().time.toText() == h.clock.recordTime(Turn::Black).toText();
    }
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }
    void committedNotificationSeesWholeMove()
    {
        StrategyHarness h;
        m_observed = &h;
        m_coherentMove = false;
        h.match->setPlayMode(PlayMode::EvenEngineVsEngine);
        connect(&h.game, &ShogiGameController::moveCommitted, this, &TestGameStrategy::observeCommittedMove);
        QVERIFY(h.match->strategyCtx().applyEngineMove(*UsiMove::parse(u"7g7f"), Turn::Black, 3000, true));
        QVERIFY(m_coherentMove);
        m_observed = nullptr;
    }
    void acceptedMove_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("human-human") << int(PlayMode::HumanVsHuman);
        QTest::newRow("human-engine") << int(PlayMode::EvenHumanVsEngine);
        QTest::newRow("engine-human") << int(PlayMode::EvenEngineVsHuman);
        QTest::newRow("engine-engine") << int(PlayMode::EvenEngineVsEngine);
    }
    void acceptedMove()
    {
        QFETCH(int, mode);
        StrategyHarness h;
        h.match->setPlayMode(static_cast<PlayMode>(mode));
        h.clock.setMeasuredConsiderationTime(Turn::Black, 3000);
        if (mode == int(PlayMode::HumanVsHuman) || mode == int(PlayMode::EvenHumanVsEngine)) {
            QString pretty;
            QVERIFY(h.applyHuman("7g7f", pretty));
            if (mode == int(PlayMode::HumanVsHuman)) {
                HumanVsHumanStrategy strategy(h.match->strategyCtx());
                strategy.onHumanMove(QPoint(7, 7), QPoint(7, 6), pretty);
            } else {
                HumanVsEngineStrategy strategy(h.match->strategyCtx(), Turn::White, {}, {});
                strategy.onHumanMove(QPoint(7, 7), QPoint(7, 6), pretty);
            }
        } else {
            QVERIFY(h.match->strategyCtx().applyEngineMove(*UsiMove::parse(u"7g7f"), Turn::Black, 3000, true));
            QVERIFY(h.events.contains("eval"));
        }
        QCOMPARE(h.records.size(), 1);
        const auto& recorded = h.records.first();
        QCOMPARE(recorded.terminal, TerminalType::None);
        QCOMPARE(recorded.mover, Turn::Black);
        QCOMPARE(recorded.sfen, h.sfens.last());
        QCOMPARE(recorded.move.movingPiece, Piece::BlackPawn);
        QCOMPARE(recorded.time.elapsedMs, 3000);
        QCOMPARE(recorded.time.totalMs, 3000);
        QCOMPARE(h.moves.size(), 1);
        QCOMPARE(h.sfens.size(), 2);
        QCOMPARE(h.match->strategyCtx().currentMoveIndex(), 1);
        QCOMPARE(h.match->strategyCtx().currentTurn(), Turn::White);
        QVERIFY(h.events.indexOf("record") < h.events.indexOf("render"));
        QVERIFY(h.events.indexOf("record") < h.events.indexOf("turn"));
    }
    void illegalEngineMoveDoesNotRecordNormalMove()
    {
        StrategyHarness h;
        h.match->setPlayMode(PlayMode::EvenEngineVsEngine);
        QSignalSpy ending(h.match.get(), &MatchCoordinator::requestAppendGameOverMove);
        QVERIFY(!h.match->strategyCtx().applyEngineMove(*UsiMove::parse(u"7g7e"), Turn::Black, 3000, false));
        QCOMPARE(h.sfens.size(), 1);
        QVERIFY(h.moves.isEmpty());
        QVERIFY(h.records.isEmpty());
        QCOMPARE(ending.count(), 1);
        const auto result = ending.first().first().value<MatchCoordinator::GameEndInfo>();
        QCOMPARE(result.cause, MatchCoordinator::Cause::IllegalMove);
        QCOMPARE(result.loser, Turn::Black);
        QVERIFY(h.match->gameOverState().isOver);
    }
    void limitRecordsMoveBeforeEnding()
    {
        StrategyHarness h;
        MatchStartOptions opt;
        opt.mode = PlayMode::HumanVsHuman;
        opt.sfenStart = h.sfens.first();
        opt.maxMoves = 1;
        h.match->configureAndStart(opt);
        h.events.clear();
        QString pretty;
        QVERIFY(h.applyHuman("7g7f", pretty));
        h.match->onHumanMove(QPoint(7, 7), QPoint(7, 6), pretty);
        QCOMPARE(h.records.size(), 2);
        QCOMPARE(h.records.first().terminal, TerminalType::None);
        QCOMPARE(h.records.last().terminal, TerminalType::Impasse);
        QVERIFY(h.match->gameOverState().isOver);
        QCOMPARE(h.events.count("end"), 1);
    }
    void repetitionUsesSharedHistory()
    {
        StrategyHarness h;
        h.match->setPlayMode(PlayMode::EvenEngineVsEngine);
        const QStringList cycle{"2h3h", "8b7b", "3h2h", "7b8b"};
        for (int i = 0; i < 12; ++i) {
            const Turn mover = i % 2 ? Turn::White : Turn::Black;
            const bool continuing = h.match->strategyCtx().applyEngineMove(
                *UsiMove::parse(cycle.at(i % 4)), mover, 1000, false);
            QCOMPARE(continuing, i < 11);
        }
        QCOMPARE(h.moves.size(), 12);
        QCOMPARE(h.records.size(), 13);
        QCOMPARE(h.records.last().terminal, TerminalType::Repetition);
        QCOMPARE(h.events.count("end"), 1);
    }
    void terminalStopsFurtherEngineMoves()
    {
        StrategyHarness h;
        h.match->setPlayMode(PlayMode::EvenEngineVsEngine);
        h.match->handleResign();
        const auto count = h.records.size();
        QVERIFY(!h.match->strategyCtx().applyEngineMove(*UsiMove::parse(u"7g7f"), Turn::Black, 1000, false));
        QCOMPARE(h.records.size(), count);
        QVERIFY(h.moves.isEmpty());
    }
    void eveRejectsHumanInput()
    {
        StrategyHarness h;
        EngineVsEngineStrategy strategy(h.match->strategyCtx());
        strategy.onHumanMove({}, {}, "unexpected");
        QVERIFY(h.records.isEmpty());
        QVERIFY(h.moves.isEmpty());
        QVERIFY(h.events.isEmpty());
    }
};
QTEST_MAIN(TestGameStrategy)
#include "tst_gamestrategy.moc"
