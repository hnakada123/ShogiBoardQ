#include <QtTest>

#include "usimove.h"
#include "timecontrol_test_helper.h"
#include <QGraphicsView>
#include <QTableView>

#include "undoflowservice.h"
#include "matchundohandler.h"
#include "recordline.h"
#include "livegamesession.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifudisplaycoordinator.h"
#include "gamerecordpresenter.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifudisplay.h"
#include "recordpane.h"
#include "branchtreemanager.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "sfenpositiontracer.h"
#include "sfenutils.h"
#include "playmode.h"
#include "shogiclock.h"

namespace {

const QStringList opening = QStringLiteral(
    "7g7f 4a3b 2g2f 7a6b 2f2e 5a4a 2e2d 2c2d 2h2d P*2c").split(' ');

struct Harness : QObject {
    KifuBranchTree tree;
    KifuNavigationState state;
    KifuNavigationController nav;
    KifuRecordListModel record;
    KifuBranchListModel branches;
    RecordPane pane;
    QGraphicsView treeView;
    BranchTreeManager treeManager;
    KifuDisplayCoordinator display{&tree, &state, &nav};
    ShogiGameController gc;
    LiveGameSession session;
    GameRecordPresenter presenter{{&record, &pane}};
    MatchUndoHandler handler;
    UndoFlowService flow;
    ShogiClock clock;
    int timerRestarts = 0;
    QStringList sfens;
    QList<ShogiMove> moves;
    QString position() const
    {
        RecordLine line;
        line.sfens = sfens;
        line.moves = moves;
        return line.positionCommand(line.moveCount());
    }
    QString currentSfen;
    int highlightPly = -1;
    int undoCalls = 0;
    bool dirty = false;
    bool coherentUndo = false;
    void observeUndo()
    {
        coherentUndo = session.currentSfen() == sfens.last() && currentSfen == sfens.last()
            && session.totalPly() == sfens.size() - 1 && moves.size() == sfens.size() - 1
            && state.currentNode() == session.liveNode();
    }
    PlayMode mode = PlayMode::EvenHumanVsEngine;
    Turn human = Turn::Black;

    Harness()
    {
        QString initial = SfenUtils::hirateSfen();
        gc.newGame(initial);
        sfens.append(initial);
        currentSfen = initial;
        tree.setRootSfen(initial);
        nav.setTreeAndState(&tree, &state);
        pane.setModels(&record, &branches);
        treeManager.setView(&treeView);
        display.setRecordPane(&pane);
        display.setRecordModel(&record);
        display.setBranchModel(&branches);
        display.setBranchTreeManager(&treeManager);
        display.setBoardSfenProvider([this]() {
            return QStringLiteral("%1 %2 %3 1").arg(gc.board()->convertBoardToSfen(),
                turnToSfen(gc.board()->currentPlayer()), gc.board()->convertStandToSfen());
        });
        display.setLiveGameSession(&session);
        display.wireSignals();
        display.onTreeChanged();
        session.setTree(&tree);
        session.startFromRoot();
        QObject::connect(&session, &LiveGameSession::movesUndone, this, &Harness::observeUndo);

        MatchUndoHandler::Refs refs;
        refs.gc = &gc;
        refs.sfenHistory = &sfens;
        refs.gameMoves = &moves;
        handler.setRefs(refs);
        MatchUndoHandler::Hooks clockHooks;
        clockHooks.clockProvider = [this]() { return &clock; };
        clockHooks.restartHumanTurnTimer = [this]() { ++timerRestarts; };
        handler.setHooks(clockHooks);
        MatchUndoHandler::UndoRefs undoRefs;
        MatchUndoHandler::UndoHooks hooks;
        hooks.canUndoRecord = [this] { return session.canUndoMoves(2); };
        hooks.commitRecordUndo = [this] {
            currentSfen = sfens.last();
            QVERIFY(session.undoMoves(2));
        };
        hooks.isHumanSide = [this](Turn player) { return player == human; };
        hooks.updateHighlightsForPly = [this](int ply) { highlightPly = ply; };
        handler.setUndoBindings(undoRefs, hooks);

        UndoFlowService::Deps deps;
        deps.undoTwoPlies = [this]() { ++undoCalls; return handler.undoTwoPlies(); };
        deps.markGameRecordDirty = [this]() { dirty = true; };
        deps.liveSession = &session;
        deps.playMode = &mode;
        deps.sfenRecord = &sfens;
        flow.updateDeps(deps);

    }

    /// 着手し、その手の消費時間を時計に確定する（対局中の着手と同じ順序）
    bool playTimed(const QString& usi, qint64 ms)
    {
        const Turn player = gc.currentPlayer();
        if (!play(usi)) return false;
        clock.setCurrentPlayer(player);
        clock.setMeasuredConsiderationTime(player, ms);
        clock.commitMove(player);
        clock.setCurrentPlayer(player == Turn::Black ? Turn::White : Turn::Black);
        return true;
    }

    bool play(const QString& usi)
    {
        const auto move = UsiMove::parse(usi);
        if (!move) return false;
        QString pretty;
        if (!gc.applyMove(*move, pretty, static_cast<int>(sfens.size()), &sfens, moves)) {
            return false;
        }
        RecordEntry entry;
        entry.move = moves.last();
        entry.displayText = pretty;
        entry.sfen = sfens.last();
        entry.time = RecordTime::fromMilliseconds(1000, 1000);
        session.appendRecord(entry);
        currentSfen = sfens.last();
        return true;
    }
};

} // namespace

class TestUndoFlow : public QObject
{
    Q_OBJECT

    static void verifyPosition(Harness& h, int ply)
    {
        QCOMPARE(h.sfens.size(), ply + 1);
        QCOMPARE(h.record.rowCount(), ply + 1);
        QCOMPARE(h.record.currentHighlightRow(), ply);
        QCOMPARE(h.pane.kifuView()->currentIndex().row(), ply);
        QCOMPARE(h.state.currentPly(), ply);
        QCOMPARE(h.session.totalPly(), ply);
        QCOMPARE(h.session.currentSfen(), h.sfens.last());
        QCOMPARE(h.currentSfen, h.sfens.last());
        QCOMPARE(h.treeManager.lastHighlightedPly(), ply);
        QString reason;
        QVERIFY2(h.display.verifyDisplayConsistencyDetailed(&reason), qPrintable(reason));
    }

private slots:
    void undoAndReplay_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<bool>("humanBlack");
        QTest::newRow("human-black-captures-and-drop") << 10 << true;
        QTest::newRow("human-white") << 9 << false;
        QTest::newRow("human-white-first-turn") << 3 << false;
        QTest::newRow("back-to-root") << 2 << true;
    }

    void undoAndReplay()
    {
        QFETCH(int, count);
        QFETCH(bool, humanBlack);
        Harness h;
        h.human = humanBlack ? Turn::Black : Turn::White;
        h.mode = humanBlack ? PlayMode::EvenHumanVsEngine : PlayMode::EvenEngineVsHuman;
        for (int i = 0; i < count; ++i) QVERIFY(h.play(opening.at(i)));
        const QString targetSfen = h.sfens.at(count - 2);
        const int undoneId = h.session.liveNode()->nodeId();
        h.flow.undoLastTwoMoves();
        verifyPosition(h, count - 2);
        QVERIFY(h.coherentUndo);
        QCOMPARE(h.sfens.last(), targetSfen);
        // 取り消した手はツリーに残し、棋譜欄は現在手までを表示する。
        QCOMPARE(h.tree.nodeCount(), count + 1);
        QCOMPARE(h.tree.displayItemsForLine(0).size(), count + 1);
        QVERIFY(h.tree.nodeAt(undoneId) != nullptr);
        QCOMPARE(h.moves.size(), count - 2);
        // 戻した局面までの指し手は、分岐ツリー（棋譜の正本）の表示中の手順から取れる
        const RecordLine line = h.state.currentRecordLine();
        QCOMPARE(line.currentPly, count - 2);
        QCOMPARE(line.usiMoves(line.currentPly), opening.mid(0, count - 2));
        QCOMPARE(h.highlightPly, count - 2);
        QVERIFY(h.dirty);
        const QString expectedPosition = count == 2 ? QStringLiteral("position startpos")
            : QStringLiteral("position startpos moves ") + opening.mid(0, count - 2).join(' ');
        QCOMPARE(h.position(), expectedPosition);
        QVERIFY(h.play(opening.at(count - 2)));
        QVERIFY(h.play(opening.at(count - 1)));
        verifyPosition(h, count);
        // 同じ手を指し直すと取り消した手のノードを再利用し、分岐は増えない。
        QCOMPARE(h.session.liveNode()->nodeId(), undoneId);
        QCOMPARE(h.tree.nodeCount(), count + 1);
        QCOMPARE(h.tree.lineCount(), 1);
        // 同じ局面へ戻るので、駒取り・駒打ちを含めて全盤面と持ち駒が一致する。
        QCOMPARE(h.sfens, SfenPositionTracer::buildSfenRecord(SfenUtils::hirateSfen(), opening.mid(0, count), false));
    }

    void repeatedUndoStopsAtRoot()
    {
        Harness h;
        for (const QString& move : opening) QVERIFY(h.play(move));
        for (int ply = 8; ply >= 0; ply -= 2) {
            h.flow.undoLastTwoMoves();
            verifyPosition(h, ply);
        }
        h.dirty = false;
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 0);
        QCOMPARE(h.tree.nodeCount(), opening.size() + 1);
        QVERIFY(!h.dirty);
        QVERIFY(h.play(QStringLiteral("2g2f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        verifyPosition(h, 2);
        // 指し直した手順が本譜、取り消した手順が分岐になる。
        QCOMPARE(h.tree.lineCount(), 2);
        QCOMPARE(h.tree.mainLine().size(), 3);
        QCOMPARE(h.tree.mainLine().last(), h.session.liveNode());
        QCOMPARE(h.tree.root()->childAt(1)->displayText(), h.tree.displayItemsForLine(1).at(1).prettyMove);
        QCOMPARE(h.tree.displayItemsForLine(1).size(), opening.size() + 1);
    }

    // 「待った」で、時計も取り消した自分の手を指し始めたときへ戻る。続けて使うと2手ずつ戻る。
    void undoRestoresClockOfRetractedTurn()
    {
        Harness h;
        h.clock.setTimeControl(TestTime::rules(600, 600, 0, 0, 10, 10)); // 持ち時間10分＋加算10秒
        h.clock.setCurrentPlayer(Turn::Black);
        const auto verifyClock = [&h](const ShogiClock::Snapshot& expected) {
            const auto now = h.clock.pauseAndSnapshot();
            QCOMPARE(now.currentPlayer, Turn::Black);
            QCOMPARE(now.of(Turn::Black).remainingMs, expected.of(Turn::Black).remainingMs);
            QCOMPARE(now.of(Turn::White).remainingMs, expected.of(Turn::White).remainingMs);
            QCOMPARE(now.of(Turn::Black).totalConsiderationMs, expected.of(Turn::Black).totalConsiderationMs);
            QCOMPARE(now.of(Turn::White).totalConsiderationMs, expected.of(Turn::White).totalConsiderationMs);
            QCOMPARE(now.of(Turn::Black).considerationMs, 0LL);
        };

        const auto firstTurn = h.clock.pauseAndSnapshot();
        QVERIFY(h.playTimed(opening.at(0), 3000));
        // エンジンの手番では「待った」できず、時計も変わらない
        const auto engineTurn = h.clock.pauseAndSnapshot();
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 1);
        QCOMPARE(h.clock.pauseAndSnapshot().of(Turn::Black).remainingMs, engineTurn.of(Turn::Black).remainingMs);
        QCOMPARE(h.timerRestarts, 0);

        QVERIFY(h.playTimed(opening.at(1), 1000));
        const auto secondTurn = h.clock.pauseAndSnapshot();
        QVERIFY(h.playTimed(opening.at(2), 5000));
        QVERIFY(h.playTimed(opening.at(3), 2000));
        h.clock.setConsiderationTimeMs(Turn::Black, 700); // 考え中の時間も返す

        h.flow.undoLastTwoMoves();
        verifyPosition(h, 2);
        verifyClock(secondTurn);
        QCOMPARE(h.timerRestarts, 1);
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 0);
        verifyClock(firstTurn);
        QCOMPARE(h.timerRestarts, 2);
        h.flow.undoLastTwoMoves(); // 開始局面より前には戻らない
        verifyPosition(h, 0);
        verifyClock(firstTurn);
        QCOMPARE(h.timerRestarts, 2);

        // 指し直した後の「待った」も、指し直した手の前へ戻る
        QVERIFY(h.playTimed(QStringLiteral("2g2f"), 4000));
        QVERIFY(h.playTimed(QStringLiteral("8c8d"), 1000));
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 0);
        verifyClock(firstTurn);
    }

    void undoThenDifferentMoveKeepsUndoneLineAsVariation()
    {
        Harness h;
        for (int i = 0; i < 6; ++i) QVERIFY(h.play(opening.at(i)));
        KifuBranchNode* undoneHead = h.session.liveNode()->parent();
        const QString undoneText = undoneHead->displayText();
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 4);
        QCOMPARE(h.tree.lineCount(), 1);
        QVERIFY(h.play(QStringLiteral("6g6f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        verifyPosition(h, 6);

        // 実際に指した手順が本譜（先頭の子）、待ったで取り消した手順が分岐として残る。
        QCOMPARE(h.tree.lineCount(), 2);
        QCOMPARE(h.tree.nodeCount(), 9);
        QCOMPARE(h.tree.mainLine().last(), h.session.liveNode());
        KifuBranchNode* branchPoint = h.tree.findByPlyOnMainLine(4);
        QCOMPARE(branchPoint->childCount(), 2);
        QCOMPARE(branchPoint->childAt(1), undoneHead);
        QCOMPARE(h.tree.displayItemsForLine(1).at(5).prettyMove, undoneText);
        QCOMPARE(h.tree.displayItemsForLine(1).size(), 7);
        QCOMPARE(h.record.rowCount(), 7);
        // 分岐のある手は棋譜欄で「+」付きになる。
        QVERIFY(h.record.data(h.record.index(5, 0)).toString().endsWith(QLatin1Char('+')));

        // 対局終了後も実際の手順が本譜のまま確定する。
        KifuBranchNode* end = h.session.commit();
        QCOMPARE(end, h.tree.mainLine().last());
        QCOMPARE(h.tree.mainLine().size(), 7);
    }

    void replayingUndoneLineRestoresItAsMainLine()
    {
        Harness h;
        for (int i = 0; i < 4; ++i) QVERIFY(h.play(opening.at(i)));
        KifuBranchNode* firstHead = h.session.liveNode()->parent();
        h.flow.undoLastTwoMoves();
        QVERIFY(h.play(QStringLiteral("6g6f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        KifuBranchNode* secondHead = h.session.liveNode()->parent();
        KifuBranchNode* branchPoint = h.tree.findByPlyOnMainLine(2);
        QCOMPARE(branchPoint->childAt(0), secondHead);
        QCOMPARE(branchPoint->childAt(1), firstHead);

        // 最初の手順に戻ると、その手順が再び本譜になる。
        h.flow.undoLastTwoMoves();
        QVERIFY(h.play(opening.at(2)));
        QVERIFY(h.play(opening.at(3)));
        verifyPosition(h, 4);
        QCOMPARE(h.tree.nodeCount(), 7);
        QCOMPARE(h.tree.lineCount(), 2);
        QCOMPARE(branchPoint->childAt(0), firstHead);
        QCOMPARE(branchPoint->childAt(1), secondHead);
        QCOMPARE(h.tree.mainLine().last(), h.session.liveNode());
    }

    void terminalAfterUndoBecomesMainLine()
    {
        Harness h;
        for (int i = 0; i < 4; ++i) QVERIFY(h.play(opening.at(i)));
        KifuBranchNode* undoneHead = h.session.liveNode()->parent();
        h.flow.undoLastTwoMoves();
        // 待ったの直後に投了しても、投了で終わる手順が本譜になる。
        h.session.appendRecord(RecordEntry::ending(TerminalType::Resign, Turn::Black,
            RecordTime::fromMilliseconds(1000, 1000)));
        KifuBranchNode* end = h.session.commit();
        QVERIFY(end != nullptr && end->isTerminal());
        QCOMPARE(h.tree.mainLine().last(), end);
        QCOMPARE(h.tree.findByPlyOnMainLine(2)->childAt(1), undoneHead);
        QCOMPARE(h.tree.lineCount(), 2);
    }

    void insufficientMovesAndEngineTurnDoNotChangeState()
    {
        Harness h;
        QVERIFY(!h.handler.undoTwoPlies());
        h.flow.undoLastTwoMoves();
        QCOMPARE(h.record.rowCount(), 1);
        QVERIFY(h.play(opening.at(0)));
        QVERIFY(!h.handler.undoTwoPlies());
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 1);
        QVERIFY(!h.dirty);
        QVERIFY(h.play(opening.at(1)));
        QVERIFY(h.play(opening.at(2))); // エンジンの応答待ちに相当する手番
        const QString position = h.position();
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 3);
        QCOMPARE(h.position(), position);
        QVERIFY(!h.dirty);
    }

    void midgameUndoPreservesExistingContinuation()
    {
        Harness h;
        for (int i = 0; i < 4; ++i) QVERIFY(h.play(opening.at(i)));
        auto* originalEnd = h.session.commit();
        QVERIFY(originalEnd != nullptr);
        auto* anchor = h.tree.findByPlyOnMainLine(2);
        h.sfens.resize(3);
        h.moves.resize(2);
        h.gc.board()->setSfen(h.sfens.last());
        h.gc.setCurrentPlayer(Turn::Black);
        h.session.startFromNode(anchor);
        // 同じ手を再利用しても、以前の棋譜のノードは削除しない。
        QVERIFY(h.play(opening.at(2)));
        QVERIFY(h.play(opening.at(3)));
        QCOMPARE(h.session.liveNode(), originalEnd);
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 2);
        QCOMPARE(h.tree.nodeCount(), 5);
        QCOMPARE(h.tree.mainLine().last(), originalEnd);
        QCOMPARE(h.position(), QStringLiteral("position startpos moves ") + opening.mid(0, 2).join(QLatin1Char(' ')));
        const int calls = h.undoCalls;
        h.flow.undoLastTwoMoves();
        QCOMPARE(h.undoCalls, calls); // 起点より前の棋譜を取り消さない
        // 別の手を指し直すと以前の棋譜の分岐になり、取り消しても分岐として残る。
        QVERIFY(h.play(QStringLiteral("6g6f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        QCOMPARE(h.tree.lineCount(), 2);
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 2);
        QCOMPARE(h.tree.lineCount(), 2);
        QCOMPARE(h.tree.mainLine().last(), originalEnd);
        // 以前の棋譜の本譜より前には並べない。
        QVERIFY(h.play(QStringLiteral("9g9f")));
        QVERIFY(h.play(QStringLiteral("3c3d")));
        QCOMPARE(h.tree.lineCount(), 3);
        QCOMPARE(h.tree.mainLine().last(), originalEnd);
        QCOMPARE(anchor->childAt(1), h.session.liveNode()->parent());
    }

    void undoRestoresPreviousDestinationForSameSquareNotation()
    {
        Harness h;
        for (const QString& move : opening) QVERIFY(h.play(move));
        h.flow.undoLastTwoMoves();
        QVERIFY(h.play(QStringLiteral("2h2d")));
        QVERIFY(h.record.item(9)->currentMove().contains(QStringLiteral("同")));
    }
};

QTEST_MAIN(TestUndoFlow)
#include "tst_undo_flow.moc"
