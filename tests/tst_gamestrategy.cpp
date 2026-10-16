/// @brief 実際の盤・時計・司令塔で、人間の手とエンジンの手が同じ着手確定の経路を通ることを検証する。
#include <QtTest>
#include <QTemporaryDir>
#include "enginevsenginestrategy.h"
#include "match_test_helper.h"
#include "terminaltype.h"
#include "timecontrol_test_helper.h"

class TestGameStrategy : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
    MatchHarness* m_observed = nullptr;
    bool m_coherentMove = false;
    int m_recordsAtCommit = -1;

    /// 一手を確定し終えてから着手の確定を知らせる（盤・手順・棋譜・時計が揃っている）
    void observeCommittedMove(Turn mover, int ply)
    {
        const auto& h = *m_observed;
        m_coherentMove = mover == Turn::Black && ply == 1 && h.sink.records.size() == 1
            && h.gc.currentPlayer() == Turn::White && h.mc->record().moveCount() == 1
            && h.sink.records.last().sfen == h.mc->record().lastSfen()
            && h.sink.records.last().time.toText() == h.clock.recordTime(Turn::Black).toText();
    }
    void observeCommit() { m_recordsAtCommit = static_cast<int>(m_observed->sink.records.size()); }

    static bool engineMove(MatchHarness& h, const QString& usi, Turn mover, qint64 elapsedMs)
    {
        return h.mc->strategyHost().applyEngineMove(::engineMove(usi, mover, elapsedMs));
    }

    static void startTimed(MatchHarness& h, int maxMoves = 0)
    {
        h.clock.setTimeControl(TestTime::rules(600, 600, 0, 0, 2, 2));
        MatchCoordinator::StartOptions opt;
        opt.sfenStart = SfenUtils::hirateSfen();
        opt.maxMoves = maxMoves;
        h.mc->start(opt);
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
        MatchHarness h;
        startTimed(h);
        m_observed = &h;
        m_coherentMove = false;
        connect(&h.gc, &ShogiGameController::moveCommitted, this, &TestGameStrategy::observeCommittedMove);
        QVERIFY(engineMove(h, QStringLiteral("7g7f"), Turn::Black, 3000));
        QVERIFY(m_coherentMove);
        m_observed = nullptr;
    }
    void acceptedMove_data()
    {
        QTest::addColumn<bool>("human");
        QTest::newRow("human") << true;
        QTest::newRow("engine") << false;
    }
    /// 人間の手とエンジンの手は同じように記録し、記録してから着手の確定を知らせる
    /// （棋譜欄・盤の強調は記録された行から表示中の手を作る）
    void acceptedMove()
    {
        QFETCH(bool, human);
        MatchHarness h;
        startTimed(h);
        m_observed = &h;
        m_recordsAtCommit = -1;
        connect(&h.gc, &ShogiGameController::moveCommitted, this, &TestGameStrategy::observeCommit);
        QSignalSpy evaluated(h.mc.get(), &MatchCoordinator::engineMovePlayed);
        if (human) {
            h.clock.setMeasuredConsiderationTime(Turn::Black, 3000);
            QVERIFY(h.playHuman(QStringLiteral("7g7f")));
            QCOMPARE(evaluated.count(), 0);
        } else {
            QVERIFY(engineMove(h, QStringLiteral("7g7f"), Turn::Black, 3000));
            QCOMPARE(evaluated.count(), 1);
            QCOMPARE(evaluated.first().at(0).value<Turn>(), Turn::Black);
            QCOMPARE(evaluated.first().at(1).toInt(), 1);
        }
        QCOMPARE(h.sink.records.size(), 1);
        const auto& recorded = h.sink.records.first();
        QCOMPARE(recorded.terminal, TerminalType::None);
        QCOMPARE(recorded.mover, Turn::Black);
        QCOMPARE(recorded.sfen, h.mc->record().lastSfen());
        QCOMPARE(recorded.move.movingPiece, Piece::BlackPawn);
        QCOMPARE(recorded.time.elapsedMs, 3000);
        QCOMPARE(recorded.time.totalMs, 3000);
        QCOMPARE(h.mc->currentPly(), 1);
        QCOMPARE(h.gc.currentPlayer(), Turn::White);
        QCOMPARE(m_recordsAtCommit, 1);
        m_observed = nullptr;
    }
    void illegalEngineMoveDoesNotRecordNormalMove()
    {
        MatchHarness h;
        startTimed(h);
        QVERIFY(!engineMove(h, QStringLiteral("7g7e"), Turn::Black, 3000));
        QCOMPARE(h.mc->currentPly(), 0);
        QCOMPARE(h.sink.records.size(), 1);
        QCOMPARE(h.sink.records.first().terminal, TerminalType::IllegalLoss);
        QVERIFY(h.mc->gameOverState().isOver);
        QCOMPARE(h.mc->gameOverState().lastInfo.cause, MatchCoordinator::Cause::IllegalMove);
        QCOMPARE(h.mc->gameOverState().lastInfo.loser, Turn::Black);
    }
    void limitRecordsMoveBeforeEnding()
    {
        MatchHarness h;
        startTimed(h, 1);
        QVERIFY(h.playHuman(QStringLiteral("7g7f")));
        QCOMPARE(h.sink.records.size(), 2);
        QCOMPARE(h.sink.records.first().terminal, TerminalType::None);
        QCOMPARE(h.sink.records.last().terminal, TerminalType::Impasse);
        QVERIFY(h.mc->gameOverState().isOver);
        QCOMPARE(h.messageCount(), 1);
    }
    void repetitionUsesMatchRecord()
    {
        MatchHarness h;
        startTimed(h);
        const QStringList cycle{"2h3h", "8b7b", "3h2h", "7b8b"};
        for (int i = 0; i < 12; ++i) {
            const Turn mover = i % 2 ? Turn::White : Turn::Black;
            QCOMPARE(engineMove(h, cycle.at(i % 4), mover, 1000), i < 11);
        }
        QCOMPARE(h.mc->currentPly(), 12);
        QCOMPARE(h.sink.records.size(), 13);
        QCOMPARE(h.sink.records.last().terminal, TerminalType::Repetition);
        QCOMPARE(h.messageCount(), 1);
    }
    void terminalStopsFurtherEngineMoves()
    {
        MatchHarness h;
        startTimed(h);
        h.mc->handleResign();
        const auto count = h.sink.records.size();
        QVERIFY(!engineMove(h, QStringLiteral("7g7f"), Turn::Black, 1000));
        QCOMPARE(h.sink.records.size(), count);
        QCOMPARE(h.mc->currentPly(), 0);
    }
    void eveRejectsHumanInput()
    {
        MatchHarness h;
        EngineVsEngineStrategy strategy(h.mc->strategyHost(), {});
        QSignalSpy board(h.gc.board(), &ShogiBoard::turnChanged);
        strategy.onHumanMove(RecordEntry::played(ShogiMove(*UsiMove::parse(u"7g7f"), Piece::BlackPawn),
                                                 QStringLiteral("unexpected"), SfenUtils::hirateSfen()));
        QVERIFY(h.sink.records.isEmpty());
        QCOMPARE(h.mc->currentPly(), 0);
        QCOMPARE(board.count(), 0);
    }
};
QTEST_MAIN(TestGameStrategy)
#include "tst_gamestrategy.moc"
