/// @file tst_ui_display_consistency.cpp
/// @brief 表示中の手（KifuNavigationState）と、棋譜欄・分岐候補欄・分岐ツリー・コメント欄の表示が揃うことのテスト
///
/// 表示する部品は状態の写しを持たず、表示中の手が変わるたびにそこから作り直す。
/// ここでは本体と同じ部品（KifuDisplayPresenter・CommentCoordinator）を組み立て、
/// 操作・対局の追記・ツリーの変更のあとに、各表示が表示中の手と一致するかを確かめる。

#include <QtTest>
#include <QCoreApplication>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QPushButton>
#include <QScrollBar>
#include <QTableView>
#include <QTextEdit>
#include <QVBoxLayout>
#include <memory>

#include "branchtreeitemroles.h"
#include "branchtreeview.h"
#include "commentcoordinator.h"
#include "commenteditorpanel.h"
#include "evaluationchartwidget.h"
#include "kifudocument.h"
#include "kifubranchlistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifudisplaypresenter.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifupresentation.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "record_rows_helper.h"
#include "recordpane.h"
#include "shogimove.h"
#include "test_replay.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

namespace {

/// 本譜5手（▲７六歩 △３四歩 ▲２六歩 △８四歩 ▲２五歩）と、2手目から分かれる変化3手（▲６六歩 △８四歩 ▲６五歩）
struct Scenario {
    int branchLineIndex = -1;
    KifuBranchNode* branchFirstNode = nullptr;   ///< 変化の3手目 ▲６六歩
    KifuBranchNode* branchLeafNode = nullptr;    ///< 変化の5手目 ▲６五歩
};

Scenario buildScenarioTree(KifuBranchTree& tree)
{
    Scenario scenario;
    tree.setRootSfen(kHirateSfen);
    const QStringList mainUsi = {QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
                                 QStringLiteral("8c8d"), QStringLiteral("2f2e")};
    const QStringList mainText = {QStringLiteral("▲７六歩(77)"), QStringLiteral("△３四歩(33)"),
                                  QStringLiteral("▲２六歩(27)"), QStringLiteral("△８四歩(83)"),
                                  QStringLiteral("▲２五歩(26)")};
    const QStringList mainSfens = TestReplay::buildSfenRecord(kHirateSfen, mainUsi, false);
    KifuBranchNode* current = tree.root();
    for (int i = 0; i < mainUsi.size(); ++i)
        current = TestTree::addMove(tree, current, ShogiMove(), mainText.at(i), mainSfens.at(i + 1));

    KifuBranchNode* branchParent = TestTree::findByPlyOnMainLine(tree, 2);
    const QStringList branchUsi = {QStringLiteral("6g6f"), QStringLiteral("8c8d"), QStringLiteral("6f6e")};
    const QStringList branchText = {QStringLiteral("▲６六歩(67)"), QStringLiteral("△８四歩(83)"),
                                    QStringLiteral("▲６五歩(66)")};
    const QStringList branchSfens = TestReplay::buildSfenRecord(branchParent->sfen(), branchUsi, false);
    current = branchParent;
    for (int i = 0; i < branchUsi.size(); ++i) {
        current = TestTree::addMove(tree, current, ShogiMove(), branchText.at(i), branchSfens.at(i + 1));
        if (i == 0) scenario.branchFirstNode = current;
    }
    scenario.branchLeafNode = current;
    scenario.branchLineIndex = tree.findLineIndexForNode(current).value_or(-1);
    return scenario;
}

/// 本体と同じ組み立ての、棋譜の表示に関わる部品一式
struct UiHarness {
    KifuBranchTree tree;
    KifuNavigationState state{&tree};
    KifuNavigationController nav{&state};
    LiveGameSession live{&tree, &state};
    KifuRecordListModel recordModel{&tree};
    KifuBranchListModel branchModel{&tree};
    RecordPane recordPane{nullptr};
    BranchTreeView branchTreeView;
    std::unique_ptr<KifuDisplayPresenter> display;
    Scenario scenario;

    UiHarness()
    {
        scenario = buildScenarioTree(tree);
        recordPane.setModels(&recordModel, &branchModel);
        KifuDisplayPresenter::Deps deps;
        deps.navState = &state;
        deps.liveSession = &live;
        deps.recordModel = &recordModel;
        deps.branchModel = &branchModel;
        deps.recordView = recordPane.kifuView();
        deps.treeView = &branchTreeView;
        display = std::make_unique<KifuDisplayPresenter>(deps);
        // 本体（MainWindowComposer::wireKifu）と同じ接続
        QObject::connect(&recordPane, &RecordPane::mainRowChanged, &nav, &KifuNavigationController::selectRecordRow);
        QObject::connect(&recordPane, &RecordPane::branchRowActivated,
                         &nav, &KifuNavigationController::selectBranchCandidateRow);
        QObject::connect(&branchTreeView, &BranchTreeView::nodeActivated,
                         &nav, &KifuNavigationController::selectBranchTreeNode);
        QObject::connect(&nav, &KifuNavigationController::navigationRejected,
                         display.get(), &KifuDisplayPresenter::refresh);
        display->refresh();
    }

    /// 対局の1手（表記と局面だけ）
    void play(const QString& text, const QString& usi)
    {
        const QString sfen = TestReplay::buildSfenRecord(live.liveNode()->sfen(), {usi}, false).at(1);
        live.appendRecord(RecordEntry::played(ShogiMove(), text, sfen));
    }

    /// 表示が表示中の手と揃っているか（揃っていれば空文字、違えば理由）
    QString inconsistency() const
    {
        const KifuBranchNode* current = state.currentNode();
        const QList<KifuBranchNode*> path = live.isActive() ? tree.pathToNode(live.liveNode()) : state.selectedPath();
        if (recordModel.rowCount() != path.size())
            return QStringLiteral("棋譜欄の行数 %1 != 手順 %2").arg(recordModel.rowCount()).arg(path.size());
        for (int row = 0; row < path.size(); ++row) {
            if (recordModel.nodeAt(row) != path.at(row)) return QStringLiteral("棋譜欄の %1 行目が手順と違う").arg(row);
        }
        const int row = static_cast<int>(path.indexOf(current));
        if (recordModel.currentHighlightRow() != row)
            return QStringLiteral("棋譜欄の強調 %1 != %2").arg(recordModel.currentHighlightRow()).arg(row);
        if (recordPane.kifuView()->currentIndex().row() != row)
            return QStringLiteral("棋譜欄の選択 %1 != %2").arg(recordPane.kifuView()->currentIndex().row()).arg(row);
        const QList<KifuBranchNode*> candidates = state.branchCandidatesAtCurrent();
        if (branchModel.candidateCount() != candidates.size()) return QStringLiteral("分岐候補の数が違う");
        if (branchModel.hasBackToMainRow() != state.offersBackToMainLine()) return QStringLiteral("「本譜へ戻る」が違う");
        if (branchModel.currentHighlightRow() != candidates.indexOf(current)) return QStringLiteral("分岐候補の強調が違う");
        if (!current || branchTreeView.currentNodeId() != current->nodeId()) return QStringLiteral("分岐ツリーの強調が表示中の手と違う");
        return QString();
    }

    /// 分岐ツリーで目立たせている手の手数（目立たせていなければ -1）
    int highlightedPly() const
    {
        const KifuBranchNode* node = tree.nodeAt(branchTreeView.currentNodeId());
        return node ? node->ply() : -1;
    }

    /// 棋譜欄で分岐の印（＋）の付いた行
    QSet<int> markedRows() const
    {
        QSet<int> rows;
        for (int row = 1; row < recordModel.rowCount(); ++row) {
            if (recordModel.data(recordModel.index(row, KifuRecordListModel::MoveColumn)).toString()
                    .endsWith(KifuPresentation::branchMark()))
                rows.insert(row);
        }
        return rows;
    }
};

} // namespace

#define VERIFY_CONSISTENT(h) \
    do { const QString why = (h).inconsistency(); QVERIFY2(why.isEmpty(), qPrintable(why)); } while (false)

class TestUiDisplayConsistency : public QObject
{
    Q_OBJECT

private slots:
    /// 表示を表示中の手に合わせるとき（行が減る・増える・対局で手が足される）、棋譜欄の選択の変化を
    /// 利用者が行を選んだ操作として知らせない（知らせると、その行へ移り直してしまう）
    void displayUpdatesDoNotReportRowSelection()
    {
        UiHarness h;
        QSignalSpy rows(&h.recordPane, &RecordPane::mainRowChanged);
        h.nav.goToNode(h.scenario.branchLeafNode);
        h.nav.goBack(2);
        h.nav.goToMainLineAtCurrentPly();
        h.nav.goToLast();
        h.live.startFromNode(h.state.currentNode());
        h.play(QStringLiteral("△８五歩(84)"), QStringLiteral("8d8e"));
        h.live.discard();
        h.nav.goToNode(h.scenario.branchFirstNode);
        QCOMPARE(rows.count(), 0);
        QCOMPARE(h.state.currentNode(), h.scenario.branchFirstNode);
        VERIFY_CONSISTENT(h);
    }

    /// 棋譜欄の指し手列の幅は、末尾に行を足すたびに全行を測り直した場合と同じになる
    /// （1手ごとに全行を測り直さず、足した行だけ測って今の幅と比べるため）
    void kifuColumnWidthMatchesFullMeasure()
    {
        RecordRows rows;
        KifuBranchListModel branchModel(&rows.tree);
        RecordPane pane(nullptr);
        pane.setModels(&rows.model, &branchModel);
        pane.resize(420, 320);
        pane.show();
        QVERIFY(QTest::qWaitForWindowExposed(&pane));
        QTableView* kifu = pane.kifuView();
        QVERIFY(kifu);
        const QStringList moves = {QStringLiteral("▲７六歩(77)"), QStringLiteral("△同　歩(34)"),
                                   QStringLiteral("▲２二角成(88)"), QStringLiteral("△同　銀(31)"),
                                   QStringLiteral("▲３三角打"), QStringLiteral("△投了")};
        for (int i = 0; i < 30; ++i) {
            rows.append(moves.at(i % moves.size()), QStringLiteral("00:01/00:00:10"));
            const int incremental = kifu->columnWidth(0);
            kifu->resizeColumnToContents(0);
            QCOMPARE(incremental, kifu->columnWidth(0));
            QCoreApplication::processEvents();
        }
    }

    void branchLine_isShownEverywhere()
    {
        UiHarness h;
        QVERIFY(h.scenario.branchLineIndex > 0);
        h.nav.goToNode(h.scenario.branchLeafNode);
        VERIFY_CONSISTENT(h);
        QCOMPARE(h.state.currentLineIndex(), h.scenario.branchLineIndex);
        QCOMPARE(h.recordModel.currentHighlightRow(), h.scenario.branchLeafNode->ply());
        QCOMPARE(h.highlightedPly(), h.scenario.branchLeafNode->ply());
        QVERIFY(h.recordModel.data(h.recordModel.index(5, KifuRecordListModel::MoveColumn)).toString()
                    .contains(QStringLiteral("６五歩")));
    }

    void branchLine_goBackOneMove_staysOnTheBranch()
    {
        UiHarness h;
        h.nav.goToNode(h.scenario.branchLeafNode);
        h.nav.goBack(1);
        VERIFY_CONSISTENT(h);
        QCOMPARE(h.state.currentLineIndex(), h.scenario.branchLineIndex);
        QCOMPARE(h.state.currentPly(), h.scenario.branchLeafNode->ply() - 1);
        // 戻っても、棋譜欄は変化の終わりまで出したまま
        QCOMPARE(h.recordModel.rowCount(), h.scenario.branchLeafNode->ply() + 1);
    }

    /// 入れ子分岐の先頭の子でも、ボタン操作と棋譜欄の行の選択のどちらでも「本譜へ戻る」が出る
    void nestedBranchFirstChild_showsBackToMainRowOnBothPaths()
    {
        UiHarness h;
        // 変化の4手目に5手目の別の手を足して入れ子分岐を作る。変化の5手目は「親の最初の子」だが本譜ではない
        KifuBranchNode* ply4 = h.scenario.branchFirstNode->childAt(0);
        QCOMPARE(ply4->childAt(0), h.scenario.branchLeafNode);
        const QStringList nestedSfens = TestReplay::buildSfenRecord(ply4->sfen(), {QStringLiteral("5g5f")}, false);
        QVERIFY(TestTree::addMove(h.tree, ply4, ShogiMove(), QStringLiteral("▲５六歩(57)"), nestedSfens.at(1)));
        QVERIFY(h.scenario.branchLeafNode->isMainLine());
        const int leafLineIndex = h.tree.findLineIndexForNode(h.scenario.branchLeafNode).value_or(-1);
        QVERIFY(leafLineIndex > 0);

        // 経路1: ナビゲーション操作
        h.nav.goToNode(h.scenario.branchLeafNode);
        QCOMPARE(h.state.currentLineIndex(), leafLineIndex);
        QVERIFY(!h.state.isOnMainLine());
        QCOMPARE(h.branchModel.candidateCount(), 2);
        QVERIFY(h.branchModel.hasBackToMainRow());
        VERIFY_CONSISTENT(h);

        // 経路2: 棋譜欄の行の選択
        h.nav.goToNode(ply4);
        QVERIFY(!h.branchModel.hasBackToMainRow());   // 4手目は分岐候補なし
        emit h.recordPane.mainRowChanged(h.scenario.branchLeafNode->ply());
        QCOMPARE(h.state.currentNode(), h.scenario.branchLeafNode);
        QCOMPARE(h.branchModel.candidateCount(), 2);
        QVERIFY(h.branchModel.hasBackToMainRow());
        VERIFY_CONSISTENT(h);

        // 本譜側の同じ手数（分岐点の最初の子）では出さない
        h.nav.goToMainLineAtCurrentPly();
        QVERIFY(h.state.isOnMainLine());
        QVERIFY(!h.branchModel.hasBackToMainRow());
        VERIFY_CONSISTENT(h);
    }

    /// 分岐候補欄: シングルクリックで clicked と activated の両方が出る環境でも行の選択は1回だけ知らせ、
    /// キーボードによる activated だけのときも知らせる
    void branchView_clickThenActivateEmitsOnce()
    {
        UiHarness h;
        h.branchModel.setCandidates({TestTree::findByPlyOnMainLine(h.tree, 3), h.scenario.branchFirstNode}, 0, false);
        QTableView* view = h.recordPane.branchView();
        QVERIFY(view != nullptr);
        const QModelIndex idx0 = h.branchModel.index(0, 0);
        const QModelIndex idx1 = h.branchModel.index(1, 0);

        QSignalSpy spy(&h.recordPane, &RecordPane::branchRowActivated);
        emit view->clicked(idx0);
        emit view->activated(idx0);
        QCOMPARE(spy.count(), 1);

        emit view->activated(idx1);
        QCOMPARE(spy.count(), 2);

        QTest::qWait(1);
        emit view->activated(idx0);
        QCOMPARE(spy.count(), 3);
    }

    /// 対局で手を足すと、表示中の手がその手へ移り、棋譜欄・分岐ツリー・分岐候補が合う。
    /// 対局を終えたあと、戻る→進むで対局の手順をたどれる
    void liveGameMove_followsTheNewMove()
    {
        UiHarness h;
        KifuBranchNode* start = TestTree::findByPlyOnMainLine(h.tree, 3);
        h.nav.goToNode(start);
        h.live.startFromNode(start);
        QVERIFY(h.live.isActive());

        // 本譜の4手目は△８四歩なので、別の手で分岐を作る
        QSignalSpy changed(&h.state, &KifuNavigationState::changed);
        h.play(QStringLiteral("△３二金(41)"), QStringLiteral("4a3b"));
        QCOMPARE(changed.count(), 1);   // 1手につき1回だけ描き直す
        KifuBranchNode* live = h.live.liveNode();
        QCOMPARE(live->ply(), 4);
        QCOMPARE(h.state.currentNode(), live);
        QVERIFY(!h.state.isOnMainLine());
        QCOMPARE(h.recordModel.currentHighlightRow(), 4);
        QCOMPARE(h.highlightedPly(), 4);
        QCOMPARE(h.branchModel.candidateCount(), 2);   // △８四歩 / △３二金
        QVERIFY(h.branchModel.hasBackToMainRow());
        VERIFY_CONSISTENT(h);

        h.play(QStringLiteral("▲２五歩(26)"), QStringLiteral("2f2e"));
        QCOMPARE(h.state.currentNode(), h.live.liveNode());
        QCOMPARE(h.recordModel.currentHighlightRow(), 5);
        VERIFY_CONSISTENT(h);

        KifuBranchNode* end = h.live.commit();
        QVERIFY(end != nullptr);
        h.nav.goBack(2);
        QCOMPARE(h.state.currentNode(), start);
        h.nav.goForward(2);
        QCOMPARE(h.state.currentNode(), end);
    }

    /// 棋譜欄の分岐の印（＋）は表示中の手順の分岐点だけに付き、別の手順の同じ手数の分岐は拾わない。
    /// 対局で新しい分岐点ができれば付く
    void liveGameBranchMarks_followDisplayedPathOnly()
    {
        UiHarness h;
        // 変化の3手目に4手目の別の手を足す（4手目に分岐点があるが、本譜の4手目は分岐ではない）
        KifuBranchNode* a3 = h.scenario.branchFirstNode;
        const QString nestedSfen = TestReplay::buildSfenRecord(a3->sfen(), {QStringLiteral("4a3b")}, false).at(1);
        QVERIFY(TestTree::addMove(h.tree, a3, ShogiMove(), QStringLiteral("△３二金(41)"), nestedSfen));

        KifuBranchNode* start = TestTree::findByPlyOnMainLine(h.tree, 5);
        QCOMPARE(start->childCount(), 0);
        h.nav.goToNode(start);
        h.live.startFromNode(start);
        QCOMPARE(h.markedRows(), QSet<int>{3});

        h.play(QStringLiteral("△８五歩(84)"), QStringLiteral("8d8e"));
        QCOMPARE(h.markedRows(), QSet<int>{3});
        QCOMPARE(h.recordModel.currentHighlightRow(), 6);

        // 本譜4手目の局面から別の手を指すと、5手目に新しい分岐点ができる
        QVERIFY(h.live.commit() != nullptr);
        KifuBranchNode* main4 = TestTree::findByPlyOnMainLine(h.tree, 4);
        h.nav.goToNode(main4);
        h.live.startFromNode(main4);
        QCOMPARE(h.markedRows(), QSet<int>{3});
        h.play(QStringLiteral("▲６六歩(67)"), QStringLiteral("6g6f"));
        QCOMPARE(h.markedRows(), (QSet<int>{3, 5}));
        QCOMPARE(h.recordModel.currentHighlightRow(), 5);
        VERIFY_CONSISTENT(h);
        h.live.discard();
    }

    /// 途中局面からの対局で既存の続きと同じ手を指し直すと、既存の手を使う。
    /// 対局中の棋譜欄は対局の最新の手まで（既存の続きは出さない）、終われば手順の終わりまで出す
    void liveGameReplayOnExistingLine_showsMovesUpToTheLiveMove()
    {
        UiHarness h;
        KifuBranchNode* start = TestTree::findByPlyOnMainLine(h.tree, 2);
        KifuBranchNode* main3 = TestTree::findByPlyOnMainLine(h.tree, 3);
        h.nav.goToNode(start);
        h.live.startFromNode(start);
        QCOMPARE(h.recordModel.rowCount(), 3);   // 開始局面〜2手目

        h.live.appendRecord(RecordEntry::played(ShogiMove(), QStringLiteral("▲２六歩(27)"), main3->sfen()));
        QCOMPARE(h.live.liveNode(), main3);
        QCOMPARE(h.state.currentLineIndex(), 0);
        QCOMPARE(h.recordModel.rowCount(), 4);
        QCOMPARE(h.recordModel.currentHighlightRow(), 3);
        VERIFY_CONSISTENT(h);

        QVERIFY(h.live.commit() != nullptr);
        QCOMPARE(h.recordModel.rowCount(), 6);   // 続きを含む本譜全体
        VERIFY_CONSISTENT(h);
    }

    /// 対局中の分岐ツリー: 手順の末尾への追加は描き足すだけ、既存の手の再利用は何もしない、
    /// 新しい手順ができたときだけ全体を作り直す
    void liveGameMoves_updateBranchTreeIncrementally()
    {
        UiHarness h;
        // (1) 本譜の末尾（5手目）から2手延長 → 描き足すだけ
        KifuBranchNode* main5 = TestTree::findByPlyOnMainLine(h.tree, 5);
        h.nav.goToNode(main5);
        h.live.startFromNode(main5);
        const int rebuildsBefore = h.branchTreeView.rebuildCount();

        h.play(QStringLiteral("△８五歩(84)"), QStringLiteral("8d8e"));
        QCOMPARE(h.branchTreeView.rebuildCount(), rebuildsBefore);
        QCOMPARE(h.branchTreeView.laneOf(h.live.liveNode()->nodeId()), 0);   // 本譜の行に描き足す
        QCOMPARE(h.highlightedPly(), 6);

        h.play(QStringLiteral("▲７八金(69)"), QStringLiteral("6i7h"));
        QCOMPARE(h.branchTreeView.rebuildCount(), rebuildsBefore);
        QCOMPARE(h.branchTreeView.laneOf(h.live.liveNode()->nodeId()), 0);
        QCOMPARE(h.highlightedPly(), 7);
        VERIFY_CONSISTENT(h);
        QVERIFY(h.live.commit() != nullptr);

        // (2) 本譜2手目から本譜3手目と同じ手を指し直す → 既存の手を使い、作り直しも描き足しも無し
        KifuBranchNode* main2 = TestTree::findByPlyOnMainLine(h.tree, 2);
        KifuBranchNode* main3 = TestTree::findByPlyOnMainLine(h.tree, 3);
        h.nav.goToNode(main2);
        h.live.startFromNode(main2);
        const int rebuilds2 = h.branchTreeView.rebuildCount();
        h.live.appendRecord(RecordEntry::played(ShogiMove(), QStringLiteral("▲２六歩(27)"), main3->sfen()));
        QCOMPARE(h.live.liveNode(), main3);
        QCOMPARE(h.branchTreeView.rebuildCount(), rebuilds2);
        QCOMPARE(h.highlightedPly(), 3);

        // (3) 本譜3手目の局面から別の手 → 新しい手順なので作り直して行が増える。続く手は描き足すだけ
        h.play(QStringLiteral("△３二金(41)"), QStringLiteral("4a3b"));
        QCOMPARE(h.branchTreeView.rebuildCount(), rebuilds2 + 1);
        const int newLane = h.branchTreeView.laneOf(h.live.liveNode()->nodeId());
        QVERIFY(newLane > 0);

        h.play(QStringLiteral("▲２五歩(26)"), QStringLiteral("2f2e"));
        QCOMPARE(h.branchTreeView.rebuildCount(), rebuilds2 + 1);
        QCOMPARE(h.branchTreeView.laneOf(h.live.liveNode()->nodeId()), newLane);
        QCOMPARE(h.highlightedPly(), 5);
        VERIFY_CONSISTENT(h);
        h.live.discard();
    }

    /// 対局外でツリーに手を足したとき（盤上の着手の記録・分岐の編集）も、手順の末尾に1手足すだけなら
    /// 分岐ツリーは描き足すだけにする。新しい変化ができたときは作り直す
    void treeChangeAppendsMoveWithoutRebuild()
    {
        UiHarness h;
        KifuBranchNode* main5 = TestTree::findByPlyOnMainLine(h.tree, 5);
        h.nav.goToNode(main5);
        const int rebuildsBefore = h.branchTreeView.rebuildCount();

        const QString sfen6 = TestReplay::buildSfenRecord(main5->sfen(), {QStringLiteral("8d8e")}, false).at(1);
        KifuBranchNode* added = TestTree::addMove(h.tree, main5, ShogiMove(), QStringLiteral("△８五歩(84)"), sfen6);
        QVERIFY(added != nullptr);
        h.nav.goToNode(added);
        QCOMPARE(h.branchTreeView.rebuildCount(), rebuildsBefore);
        QCOMPARE(h.branchTreeView.laneOf(added->nodeId()), 0);
        QCOMPARE(h.highlightedPly(), 6);
        VERIFY_CONSISTENT(h);

        KifuBranchNode* main3 = TestTree::findByPlyOnMainLine(h.tree, 3);
        const QString sfen4b = TestReplay::buildSfenRecord(main3->sfen(), {QStringLiteral("4a3b")}, false).at(1);
        KifuBranchNode* branch = TestTree::addMove(h.tree, main3, ShogiMove(), QStringLiteral("△３二金(41)"), sfen4b);
        QVERIFY(branch != nullptr);
        h.nav.goToNode(branch);
        QCOMPARE(h.branchTreeView.rebuildCount(), rebuildsBefore + 1);
        VERIFY_CONSISTENT(h);
    }

    /// 移る前の確認（未更新のコメントの破棄）でやめたときは、ボタン・分岐候補・分岐ツリーの
    /// どの操作でも移らず、クリックで変わった強調も表示中の手に戻す
    void leaveGuard_cancelKeepsPositionAndHighlights()
    {
        UiHarness h;
        h.nav.goToPly(3);   // 3手目に分岐がある（本譜）
        QCOMPARE(h.branchModel.currentHighlightRow(), 0);

        int asked = 0;
        bool allow = false;
        h.nav.setLeaveGuard([&asked, &allow]() { ++asked; return allow; });

        h.nav.onNextClicked();
        h.nav.onFirstClicked();
        emit h.recordPane.branchRowActivated(1);
        h.branchTreeView.setCurrentNode(h.scenario.branchFirstNode->nodeId());
        emit h.branchTreeView.nodeActivated(h.scenario.branchFirstNode->nodeId());

        QCOMPARE(asked, 4);
        QCOMPARE(h.state.currentPly(), 3);
        QCOMPARE(h.state.currentLineIndex(), 0);
        QCOMPARE(h.branchModel.currentHighlightRow(), 0);
        VERIFY_CONSISTENT(h);

        allow = true;
        h.nav.onNextClicked();
        QCOMPARE(asked, 5);
        QCOMPARE(h.state.currentPly(), 4);
    }

    /// 棋譜欄と評価値グラフの局面送りのボタンは同じ QAction に結び付き、
    /// どちらを押しても同じように移り、有効・無効もそろって変わる
    void stepButtons_shareActions()
    {
        UiHarness h;
        EvaluationChartWidget chart;
        h.nav.bindButtons({h.recordPane.firstButton(), h.recordPane.back10Button(), h.recordPane.prevButton(),
                           h.recordPane.nextButton(), h.recordPane.fwd10Button(), h.recordPane.lastButton()});
        h.nav.bindButtons({chart.firstButton(), chart.back10Button(), chart.prevButton(),
                           chart.nextButton(), chart.fwd10Button(), chart.lastButton()});
        h.nav.goToPly(2);

        h.recordPane.nextButton()->click();
        QCOMPARE(h.state.currentPly(), 3);
        chart.nextButton()->click();
        QCOMPARE(h.state.currentPly(), 4);
        chart.firstButton()->click();
        QCOMPARE(h.state.currentPly(), 0);
        h.recordPane.lastButton()->click();
        const int lastPly = h.state.currentPly();
        QVERIFY(lastPly > 4);

        const QList<QPushButton*> buttons = {h.recordPane.firstButton(), h.recordPane.lastButton(),
                                             chart.prevButton(), chart.fwd10Button()};
        h.nav.setStepActionsEnabled(false);
        QVERIFY(!h.nav.stepActionsEnabled());
        for (QPushButton* const button : buttons) QVERIFY(!button->isEnabled());
        chart.firstButton()->click();
        QCOMPARE(h.state.currentPly(), lastPly);

        h.nav.setStepActionsEnabled(true);
        for (QPushButton* const button : buttons) QVERIFY(button->isEnabled());
        h.nav.action(KifuNavigationController::Back10)->trigger();
        QCOMPARE(h.state.currentPly(), qMax(0, lastPly - 10));
        VERIFY_CONSISTENT(h);
    }

    /// 分岐ツリーは非表示の間はスクロールせず、表示したときに現在の手を
    /// 上の手数ラベルごと見える位置に出す（タブの裏で読み込んだ棋譜でもずれない）
    void branchTree_scrollsCurrentNodeIntoViewWhenShown()
    {
        UiHarness h;
        const int main5 = TestTree::findByPlyOnMainLine(h.tree, 5)->nodeId();
        h.branchTreeView.resize(420, 160);
        h.branchTreeView.setCurrentNode(main5, /*center=*/true);
        QCOMPARE(h.branchTreeView.horizontalScrollBar()->value(), 0);
        QCOMPARE(h.branchTreeView.verticalScrollBar()->value(), 0);

        QGraphicsItem* node = nullptr;
        const auto items = h.branchTreeView.scene()->items();
        for (QGraphicsItem* item : items) {
            if (item->data(BranchTreeItemRoles::NodeId).toInt() == main5) node = item;
        }
        QVERIFY(node != nullptr);
        const QRectF nodeArea = node->mapRectToScene(node->boundingRect() | node->childrenBoundingRect());
        const auto visibleArea = [&h]() {
            return h.branchTreeView.mapToScene(h.branchTreeView.viewport()->rect()).boundingRect();
        };

        h.branchTreeView.show();
        QVERIFY(QTest::qWaitForWindowExposed(&h.branchTreeView));
        QTRY_VERIFY(visibleArea().contains(nodeArea));
        QCOMPARE(h.branchTreeView.verticalScrollBar()->value(), 0);

        // 見えている手へ移っても動かさない
        const int x = h.branchTreeView.horizontalScrollBar()->value();
        h.branchTreeView.setCurrentNode(TestTree::findByPlyOnMainLine(h.tree, 4)->nodeId());
        QCOMPARE(h.branchTreeView.horizontalScrollBar()->value(), x);
    }

    /// 棋譜欄の行の選択は、表示中の手順のその手へ移る（ボタンと同じく1回だけ知らせる）。
    /// 無い行は別の手順の同じ手数へ移らない
    void rowSelectionMovesWithinTheShownLine()
    {
        UiHarness h;
        h.nav.goToNode(h.scenario.branchFirstNode);
        QSignalSpy changed(&h.state, &KifuNavigationState::changed);
        emit h.recordPane.mainRowChanged(h.scenario.branchLeafNode->ply());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(h.state.currentNode(), h.scenario.branchLeafNode);
        VERIFY_CONSISTENT(h);
        h.nav.selectRecordRow(999);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(h.state.currentNode(), h.scenario.branchLeafNode);
    }

    /// コメント欄は、どの操作で移っても表示中の手のコメントを出す（空なら空のまま）
    void commentFollowsTheCurrentMove()
    {
        UiHarness h;
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        // パネルは host の子にする（host が消す）
        CommentEditorPanel& editor = *new CommentEditorPanel(&host);
        layout->addWidget(&editor);
        auto* edit = host.findChild<QTextEdit*>(QStringLiteral("kifuCommentEdit"));
        QVERIFY(edit != nullptr);
        KifuDocument document(&h.tree);
        CommentCoordinator::Deps deps;
        deps.editor = &editor;
        deps.navState = &h.state;
        deps.document = &document;
        CommentCoordinator comments(deps);

        KifuBranchNode* node3 = TestTree::findByPlyOnMainLine(h.tree, 3);
        document.setComment(node3, QStringLiteral("テストコメント"));
        document.setComment(h.scenario.branchFirstNode, QStringLiteral("変化のコメント"));

        h.nav.goToNode(node3);
        QCOMPARE(edit->toPlainText(), QStringLiteral("テストコメント"));
        emit h.recordPane.mainRowChanged(1);
        QVERIFY(edit->toPlainText().isEmpty());
        emit h.branchTreeView.nodeActivated(h.scenario.branchFirstNode->nodeId());
        QCOMPARE(edit->toPlainText(), QStringLiteral("変化のコメント"));
        // 表示中の手のコメントを書き換えると、コメント欄も変わる
        document.setComment(h.scenario.branchFirstNode, QStringLiteral("書き換えた"));
        QCOMPARE(edit->toPlainText(), QStringLiteral("書き換えた"));
    }
};

QTEST_MAIN(TestUiDisplayConsistency)
#include "tst_ui_display_consistency.moc"
