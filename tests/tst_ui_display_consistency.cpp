#include <QtTest>
#include <QCoreApplication>
#include <QGraphicsView>

#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifudisplaycoordinator.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "recordpane.h"
#include "branchtreemanager.h"
#include "sfenpositiontracer.h"
#include "shogiboard.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static QString buildCurrentBoardSfen(ShogiBoard* board)
{
    if (board == nullptr) {
        return QString();
    }
    QString standPart = board->convertStandToSfen();
    if (standPart.isEmpty()) {
        standPart = QStringLiteral("-");
    }
    const QString turnPart = turnToSfen(board->currentPlayer());
    return QStringLiteral("%1 %2 %3 1")
        .arg(board->convertBoardToSfen(), turnPart, standPart);
}

class TestUiDisplayConsistency : public QObject
{
    Q_OBJECT

private:
    struct Scenario {
        int branchLineIndex = -1;
        KifuBranchNode* branchFirstNode = nullptr;
        KifuBranchNode* branchLeafNode = nullptr;
    };

    static Scenario buildScenarioTree(KifuBranchTree& tree)
    {
        Scenario scenario;
        tree.setRootSfen(kHirateSfen);

        const QStringList mainUsiMoves = {
            QStringLiteral("7g7f"),
            QStringLiteral("3c3d"),
            QStringLiteral("2g2f"),
            QStringLiteral("8c8d"),
            QStringLiteral("2f2e"),
        };
        const QStringList mainDisplayMoves = {
            QStringLiteral("▲７六歩(77)"),
            QStringLiteral("△３四歩(33)"),
            QStringLiteral("▲２六歩(27)"),
            QStringLiteral("△８四歩(83)"),
            QStringLiteral("▲２五歩(26)"),
        };

        const QStringList mainSfens =
            SfenPositionTracer::buildSfenRecord(kHirateSfen, mainUsiMoves, /*hasTerminal=*/false);
        if (mainSfens.size() != mainUsiMoves.size() + 1) {
            qWarning("buildScenarioTree: main SFEN record size mismatch");
            return scenario;
        }

        ShogiMove dummyMove;
        auto* current = tree.root();
        QList<KifuBranchNode*> mainNodes;
        mainNodes.append(current);  // ply 0
        for (int i = 0; i < mainUsiMoves.size(); ++i) {
            current = tree.addMove(current, dummyMove, mainDisplayMoves.at(i), mainSfens.at(i + 1));
            if (current == nullptr) {
                qWarning("buildScenarioTree: failed to add main move");
                return scenario;
            }
            mainNodes.append(current);
        }

        // ply 2（2手目後）から分岐を追加
        const QString branchBaseSfen = mainSfens.at(2);
        const QStringList branchUsiMoves = {
            QStringLiteral("6g6f"),
            QStringLiteral("8c8d"),
            QStringLiteral("6f6e"),
        };
        const QStringList branchDisplayMoves = {
            QStringLiteral("▲６六歩(67)"),
            QStringLiteral("△８四歩(83)"),
            QStringLiteral("▲６五歩(66)"),
        };
        const QStringList branchSfens =
            SfenPositionTracer::buildSfenRecord(branchBaseSfen, branchUsiMoves, /*hasTerminal=*/false);
        if (branchSfens.size() != branchUsiMoves.size() + 1) {
            qWarning("buildScenarioTree: branch SFEN record size mismatch");
            return scenario;
        }

        auto* branchParent = mainNodes.at(2);  // ply 2
        if (branchParent == nullptr) {
            qWarning("buildScenarioTree: branch parent is null");
            return scenario;
        }

        current = branchParent;
        for (int i = 0; i < branchUsiMoves.size(); ++i) {
            current = tree.addMove(current, dummyMove, branchDisplayMoves.at(i), branchSfens.at(i + 1));
            if (current == nullptr) {
                qWarning("buildScenarioTree: failed to add branch move");
                return scenario;
            }
            if (i == 0) {
                scenario.branchFirstNode = current;  // ply 3 の分岐ノード
            }
        }

        scenario.branchLeafNode = current;  // ply 5
        scenario.branchLineIndex = tree.findLineIndexForNode(current);
        return scenario;
    }

    struct UiHarness {
        KifuBranchTree tree;
        KifuNavigationState state;
        KifuNavigationController nav;
        KifuRecordListModel recordModel;
        KifuBranchListModel branchModel;
        RecordPane recordPane;
        BranchTreeManager branchTreeManager;
        QGraphicsView branchTreeView;
        KifuDisplayCoordinator coordinator;
        ShogiBoard board;
        Scenario scenario;

        UiHarness()
            : recordPane(nullptr)
            , coordinator(&tree, &state, &nav)
            , board(9, 9, nullptr)
        {
            scenario = buildScenarioTree(tree);
            nav.setTreeAndState(&tree, &state);

            recordPane.setModels(&recordModel, &branchModel);
            branchTreeManager.setView(&branchTreeView);

            coordinator.setRecordPane(&recordPane);
            coordinator.setRecordModel(&recordModel);
            coordinator.setBranchModel(&branchModel);
            coordinator.setBranchTreeManager(&branchTreeManager);
            coordinator.setBoardSfenProvider([this]() {
                return buildCurrentBoardSfen(&board);
            });
            QObject::connect(
                &coordinator, &KifuDisplayCoordinator::boardWithHighlightsRequired,
                &coordinator, [this](const QString& currentSfen, const QString&) {
                    board.setSfen(currentSfen);
                });

            coordinator.wireSignals();

            board.setSfen(kHirateSfen);
            coordinator.onTreeChanged();
        }
    };

private slots:
    void branchLine_consistencyIncludesBoardRecordAndTree()
    {
        UiHarness h;
        QVERIFY(h.scenario.branchLineIndex > 0);
        QVERIFY(h.scenario.branchLeafNode != nullptr);
        h.nav.goToNode(h.scenario.branchLeafNode);
        QCoreApplication::processEvents();

        QString reason;
        QVERIFY2(
            h.coordinator.verifyDisplayConsistencyDetailed(&reason),
            qPrintable(reason + QStringLiteral("\n") + h.coordinator.getConsistencyReport()));

        const auto snapshot = h.coordinator.captureDisplaySnapshot();
        QCOMPARE(snapshot.stateLineIndex, h.scenario.branchLineIndex);
        QCOMPARE(snapshot.statePly, h.scenario.branchLeafNode->ply());
        QCOMPARE(snapshot.modelHighlightRow, h.scenario.branchLeafNode->ply());
        QCOMPARE(snapshot.treeHighlightLineIndex, h.scenario.branchLineIndex);
        QCOMPARE(snapshot.treeHighlightPly, h.scenario.branchLeafNode->ply());
        QCOMPARE(snapshot.boardSfenNormalized, snapshot.stateSfenNormalized);
        QVERIFY(snapshot.displayedMoveAtPly.contains(snapshot.expectedMoveAtPly));
    }

    void branchLine_goBackOneMove_staysConsistent()
    {
        UiHarness h;
        QVERIFY(h.scenario.branchLineIndex > 0);
        QVERIFY(h.scenario.branchLeafNode != nullptr);
        h.nav.goToNode(h.scenario.branchLeafNode);
        h.nav.goBack(1);
        QCoreApplication::processEvents();

        QString reason;
        QVERIFY2(
            h.coordinator.verifyDisplayConsistencyDetailed(&reason),
            qPrintable(reason + QStringLiteral("\n") + h.coordinator.getConsistencyReport()));

        const auto snapshot = h.coordinator.captureDisplaySnapshot();
        QCOMPARE(snapshot.stateLineIndex, h.scenario.branchLineIndex);
        QCOMPARE(snapshot.statePly, h.scenario.branchLeafNode->ply() - 1);
        QCOMPARE(snapshot.treeHighlightLineIndex, h.scenario.branchLineIndex);
        QCOMPARE(snapshot.treeHighlightPly, h.scenario.branchLeafNode->ply() - 1);
    }

    void detectsTreeHighlightMismatch()
    {
        UiHarness h;
        QVERIFY(h.scenario.branchLeafNode != nullptr);
        h.nav.goToNode(h.scenario.branchLeafNode);
        QCoreApplication::processEvents();

        h.branchTreeManager.highlightBranchTreeAt(0, 0, /*centerOn=*/false);

        QString reason;
        QVERIFY(!h.coordinator.verifyDisplayConsistencyDetailed(&reason));
        QVERIFY(reason.contains(QStringLiteral("分岐ツリーハイライト")));
    }

    void detectsRecordMainBranchMismatch()
    {
        UiHarness h;
        QVERIFY(h.scenario.branchFirstNode != nullptr);
        // 初期状態では本譜が棋譜欄に表示されている状態
        QCOMPARE(h.state.currentLineIndex(), 0);

        // onPositionChanged は状態・分岐候補・分岐ツリーハイライトを更新するが、
        // 棋譜欄モデルは再構築しない（本譜のまま）ため、意図的に不一致を作る。
        const int branchLine = h.tree.findLineIndexForNode(h.scenario.branchFirstNode);
        QVERIFY(branchLine > 0);
        h.coordinator.onPositionChanged(
            branchLine,
            h.scenario.branchFirstNode->ply(),
            h.scenario.branchFirstNode->sfen());

        QString reason;
        QVERIFY(!h.coordinator.verifyDisplayConsistencyDetailed(&reason));
        QVERIFY(reason.contains(QStringLiteral("棋譜欄")));
    }

    // コメント表示テスト: goToNode（矢印キーナビゲーション）経路でコメントが通知されることを確認
    void commentUpdateRequired_emittedOnGoToNode()
    {
        UiHarness h;
        // ply 3 のノードにコメントを設定
        QList<BranchLine> lines = h.tree.allLines();
        QVERIFY(!lines.isEmpty());
        QVERIFY(lines.at(0).nodes.size() > 3);
        KifuBranchNode* node3 = lines.at(0).nodes.at(3);
        QVERIFY(node3 != nullptr);
        node3->setComment(QStringLiteral("テストコメント"));

        // commentUpdateRequired シグナルを監視
        QSignalSpy spy(&h.coordinator, &KifuDisplayCoordinator::commentUpdateRequired);

        // ノードへナビゲート
        h.nav.goToNode(node3);
        QCoreApplication::processEvents();

        // シグナルが発火されたことを確認
        QVERIFY2(spy.count() > 0,
                 qPrintable(QStringLiteral("commentUpdateRequired not emitted (goToNode path). count=%1")
                            .arg(spy.count())));

        // コメント内容を確認（最後に発火されたシグナルを使用）
        const QList<QVariant> lastArgs = spy.last();
        const int emittedPly = lastArgs.at(0).toInt();
        const QString emittedComment = lastArgs.at(1).toString();
        QCOMPARE(emittedPly, 3);
        QVERIFY2(emittedComment.contains(QStringLiteral("テストコメント")),
                 qPrintable(QStringLiteral("Expected comment containing 'テストコメント' but got: '%1'")
                            .arg(emittedComment)));
    }

    // コメント表示テスト: onPositionChanged（棋譜欄行選択）経路でコメントが通知されることを確認
    void commentUpdateRequired_emittedOnPositionChanged()
    {
        UiHarness h;
        // ply 2 のノードにコメントを設定
        QList<BranchLine> lines = h.tree.allLines();
        QVERIFY(!lines.isEmpty());
        QVERIFY(lines.at(0).nodes.size() > 2);
        KifuBranchNode* node2 = lines.at(0).nodes.at(2);
        QVERIFY(node2 != nullptr);
        node2->setComment(QStringLiteral("行選択コメント"));

        // commentUpdateRequired シグナルを監視
        QSignalSpy spy(&h.coordinator, &KifuDisplayCoordinator::commentUpdateRequired);

        // onPositionChanged を直接呼び出し（棋譜欄行選択パス）
        h.coordinator.onPositionChanged(0, 2, node2->sfen());
        QCoreApplication::processEvents();

        // シグナルが発火されたことを確認
        QVERIFY2(spy.count() > 0,
                 qPrintable(QStringLiteral("commentUpdateRequired not emitted (onPositionChanged path). count=%1")
                            .arg(spy.count())));

        // コメント内容を確認
        const QList<QVariant> lastArgs = spy.last();
        const int emittedPly = lastArgs.at(0).toInt();
        const QString emittedComment = lastArgs.at(1).toString();
        QCOMPARE(emittedPly, 2);
        QVERIFY2(emittedComment.contains(QStringLiteral("行選択コメント")),
                 qPrintable(QStringLiteral("Expected comment containing '行選択コメント' but got: '%1'")
                            .arg(emittedComment)));
    }

    // コメントなしノードでは「コメントなし」が通知されることを確認
    void commentUpdateRequired_emitsNoCommentPlaceholder()
    {
        UiHarness h;
        // ply 1 のノードはコメントなし
        QList<BranchLine> lines = h.tree.allLines();
        QVERIFY(!lines.isEmpty());
        QVERIFY(lines.at(0).nodes.size() > 1);
        KifuBranchNode* node1 = lines.at(0).nodes.at(1);
        QVERIFY(node1 != nullptr);
        QVERIFY(node1->comment().isEmpty());

        QSignalSpy spy(&h.coordinator, &KifuDisplayCoordinator::commentUpdateRequired);

        h.coordinator.onPositionChanged(0, 1, node1->sfen());
        QCoreApplication::processEvents();

        QVERIFY2(spy.count() > 0,
                 "commentUpdateRequired not emitted for empty comment node");
    }
};

QTEST_MAIN(TestUiDisplayConsistency)
#include "tst_ui_display_consistency.moc"
