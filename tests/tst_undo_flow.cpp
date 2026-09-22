#include <QtTest>
#include <QGraphicsView>
#include <QTableView>

#include "undoflowservice.h"
#include "matchundohandler.h"
#include "livegamesession.h"
#include "livegamesessionupdater.h"
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

namespace {

const QStringList opening = QStringLiteral(
    "7g7f 4a3b 2g2f 7a6b 2f2e 5a4a 2e2d 2c2d 2h2d P*2c").split(' ');

struct Harness {
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
    LiveGameSessionUpdater updater;
    QStringList sfens;
    QList<ShogiMove> moves;
    QStringList usiMoves;
    QString position = QStringLiteral("position startpos");
    QString ponder;
    QStringList positionHistory;
    QStringList positionList{position}; // 読み込み時の履歴は対局中に追記されない
    QString currentSfen;
    int currentMoveIndex = 0;
    int selectedPly = 0;
    int activePly = 0;
    int highlightPly = -1;
    int undoCalls = 0;
    bool dirty = false;
    PlayMode mode = PlayMode::EvenHumanVsEngine;
    ShogiGameController::Player human = ShogiGameController::Player1;

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

        MatchUndoHandler::Refs refs;
        refs.gc = &gc;
        refs.sfenHistory = &sfens;
        refs.gameMoves = &moves;
        refs.positionStr1 = &position;
        refs.positionPonder1 = &ponder;
        refs.positionStrHistory = &positionHistory;
        handler.setRefs(refs);
        MatchUndoHandler::UndoRefs undoRefs;
        undoRefs.recordModel = &record;
        undoRefs.positionStrList = &positionList;
        undoRefs.currentMoveIndex = &currentMoveIndex;
        MatchUndoHandler::UndoHooks hooks;
        hooks.isHumanSide = [this](ShogiGameController::Player player) { return player == human; };
        hooks.updateHighlightsForPly = [this](int ply) { highlightPly = ply; };
        handler.setUndoBindings(undoRefs, hooks);

        UndoFlowService::Deps deps;
        deps.undoTwoPlies = [this]() { ++undoCalls; return handler.undoTwoPlies(); };
        deps.markGameRecordDirty = [this]() { dirty = true; };
        deps.liveSession = &session;
        deps.recordPresenter = &presenter;
        deps.playMode = &mode;
        deps.sfenRecord = &sfens;
        deps.gameUsiMoves = &usiMoves;
        deps.currentSfenStr = &currentSfen;
        deps.currentSelectedPly = &selectedPly;
        deps.activePly = &activePly;
        flow.updateDeps(deps);

        LiveGameSessionUpdater::Deps updateDeps;
        updateDeps.liveSession = &session;
        updateDeps.branchTree = &tree;
        updateDeps.gameController = &gc;
        updateDeps.sfenRecord = &sfens;
        updater.updateDeps(updateDeps);
    }

    bool play(const QString& usi)
    {
        const auto parsed = SfenPositionTracer::buildGameMoves(sfens.last(), {usi});
        if (parsed.size() != 1) return false;
        QPoint from = parsed.first().fromSquare + QPoint(1, 1);
        QPoint to = parsed.first().toSquare + QPoint(1, 1);
        const bool humanMove = gc.currentPlayer() == human;
        gc.setForcedPromotion(true, usi.endsWith('+'));
        QString pretty;
        if (!gc.validateAndMove(from, to, pretty, mode, static_cast<int>(sfens.size()), &sfens, moves)) {
            return false;
        }
        if (!position.contains(QStringLiteral(" moves "))) position += QStringLiteral(" moves");
        position += QLatin1Char(' ') + usi;
        if (humanMove) positionHistory.append(position);
        presenter.appendMoveLine(pretty, QStringLiteral("00:01/00:00:01"));
        presenter.addLiveKifItem(pretty, QStringLiteral("00:01/00:00:01"));
        updater.appendMove(moves.last(), pretty, QStringLiteral("00:01/00:00:01"));
        usiMoves.append(usi);
        currentMoveIndex = static_cast<int>(sfens.size()) - 1;
        selectedPly = activePly = currentMoveIndex;
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
        QCOMPARE(h.currentMoveIndex, ply);
        QCOMPARE(h.selectedPly, ply);
        QCOMPARE(h.activePly, ply);
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
        h.human = humanBlack ? ShogiGameController::Player1 : ShogiGameController::Player2;
        h.mode = humanBlack ? PlayMode::EvenHumanVsEngine : PlayMode::EvenEngineVsHuman;
        for (int i = 0; i < count; ++i) QVERIFY(h.play(opening.at(i)));
        const QString targetSfen = h.sfens.at(count - 2);
        const int removedId = h.session.liveNode()->nodeId();
        h.flow.undoLastTwoMoves();
        verifyPosition(h, count - 2);
        QCOMPARE(h.sfens.last(), targetSfen);
        QCOMPARE(h.tree.nodeCount(), count - 1);
        QCOMPARE(h.tree.displayItemsForLine(0).size(), count - 1);
        QVERIFY(h.tree.nodeAt(removedId) == nullptr);
        QCOMPARE(h.moves.size(), count - 2);
        QCOMPARE(h.usiMoves, opening.mid(0, count - 2));
        QCOMPARE(h.presenter.liveDisp().size(), count - 2);
        QCOMPARE(h.highlightPly, count - 2);
        QVERIFY(h.dirty);
        const QString expectedPosition = count == 2 ? QStringLiteral("position startpos")
            : QStringLiteral("position startpos moves ") + opening.mid(0, count - 2).join(' ');
        QCOMPARE(h.position, expectedPosition);
        QCOMPARE(h.ponder, expectedPosition);
        QCOMPARE(h.positionList.last(), expectedPosition);
        QVERIFY(h.play(opening.at(count - 2)));
        QVERIFY(h.play(opening.at(count - 1)));
        verifyPosition(h, count);
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
        QCOMPARE(h.tree.nodeCount(), 1);
        QVERIFY(!h.dirty);
        QVERIFY(h.play(QStringLiteral("2g2f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        verifyPosition(h, 2);
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
        const QString position = h.position;
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 3);
        QCOMPARE(h.position, position);
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
        h.moves.clear();
        h.usiMoves.resize(2);
        h.presenter.clearLiveDisp();
        h.gc.board()->setSfen(h.sfens.last());
        h.gc.setCurrentPlayer(ShogiGameController::Player1);
        h.position = QStringLiteral("position sfen ") + h.sfens.last();
        h.currentMoveIndex = 2;
        h.session.startFromNode(anchor);
        // 同じ手を再利用しても、以前の棋譜のノードは削除しない。
        QVERIFY(h.play(opening.at(2)));
        QVERIFY(h.play(opening.at(3)));
        QCOMPARE(h.session.liveNode(), originalEnd);
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 2);
        QCOMPARE(h.tree.nodeCount(), 5);
        QCOMPARE(h.tree.mainLine().last(), originalEnd);
        QCOMPARE(h.position, QStringLiteral("position sfen ") + h.sfens.last());
        const int calls = h.undoCalls;
        h.flow.undoLastTwoMoves();
        QCOMPARE(h.undoCalls, calls); // 起点より前の棋譜を取り消さない
        // 別の手を指し直し、分岐だけを取り消す。
        QVERIFY(h.play(QStringLiteral("6g6f")));
        QVERIFY(h.play(QStringLiteral("8c8d")));
        QCOMPARE(h.tree.lineCount(), 2);
        h.flow.undoLastTwoMoves();
        verifyPosition(h, 2);
        QCOMPARE(h.tree.lineCount(), 1);
        QCOMPARE(h.tree.mainLine().last(), originalEnd);
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
