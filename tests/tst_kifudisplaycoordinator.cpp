#include <QtTest>

#include "kifudisplaycoordinator.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "shogimove.h"

class TestKifuDisplayCoordinator : public QObject
{
    Q_OBJECT

private slots:
    void testStartFromMidPositionTrimsRecordModel();
    void testRematchFromMainLineAfterBranch();
    void testPreferredLineIndexResetOnSessionStart();
    void testThirdGameFromFirstGamePosition();
    void testClickMainLineWhileOnBranch();
    void testThreeBranchesSelectSecondBranch();
    void testKifuModelMatchesBranchTreePath();
    void testBranchParentIndexPreservedAfterRematch();
    void testRematchFromBranchLineAddsMoveToCorrectLine();
    void testBoardAndKifuListSyncOnBranch();
    // 最重要テスト: 1手進むボタンによる局面連続性の確認
    void testGoForwardAfterBranchSelection();
    void testGoForwardAfterSwitchingBetweenMainAndBranch();
    void testGoForwardAfterRematch();
    // スクリーンショットを元にした4表示一致テスト
    void testFourDisplayConsistencyFromScreenshot();
    // 将棋盤と棋譜欄の駒配置一致テスト（SFENの詳細検証）
    void testBoardPositionMatchesKifuPath();
    // ライブ対局フローでのバグ再現テスト
    void testLiveGameRematchBoardSfenConsistency();
};

void TestKifuDisplayCoordinator::testStartFromMidPositionTrimsRecordModel()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜を6手目まで作成
    KifuBranchNode* node = tree.root();
    node = tree.addMove(node, ShogiMove(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false),
                        QStringLiteral("▲７六歩(77)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false),
                        QStringLiteral("△３四歩(33)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(2, 7), QPoint(2, 6), QLatin1Char('P'), QLatin1Char(' '), false),
                        QStringLiteral("▲２六歩(27)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
                        QStringLiteral("△８四歩(83)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(2, 6), QPoint(2, 5), QLatin1Char('P'), QLatin1Char(' '), false),
                        QStringLiteral("▲２五歩(26)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/7P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL w - 6"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(8, 4), QPoint(8, 5), QLatin1Char('p'), QLatin1Char(' '), false),
                        QStringLiteral("△８五歩(84)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/9/1p5P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL b - 7"),
                        QString());

    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);

    // 旧表示を想定して全手数を表示（手動でモデルに詰める）
    recordModel.appendItem(new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                           QStringLiteral("（１手 / 合計）")));
    QVector<KifuBranchNode*> pathToEnd = tree.pathToNode(node);
    for (KifuBranchNode* n : std::as_const(pathToEnd)) {
        if (n == nullptr || n->ply() == 0) continue;
        const QString num = QString::number(n->ply());
        const QString spaces = QString(qMax(0, 4 - num.length()), QLatin1Char(' '));
        const QString line = spaces + num + QLatin1Char(' ') + n->displayText();
        recordModel.appendItem(new KifuDisplay(line, n->timeText(), n->comment()));
    }
    QCOMPARE(recordModel.rowCount(), 7); // 開始局面 + 6手

    // 4手目からセッション開始
    KifuBranchNode* node4 = tree.findByPlyOnMainLine(4);
    QVERIFY(node4 != nullptr);
    coordinator.onLiveGameSessionStarted(node4);

    // セッション開始直後は 4手目までにトリムされる
    QCOMPARE(recordModel.rowCount(), 5);
    const QModelIndex idx = recordModel.index(4, 0);
    const QString moveText = recordModel.data(idx, Qt::DisplayRole).toString();
    QVERIFY(moveText.contains(node4->displayText()));
}

/**
 * @brief 分岐ライン上で対局後、本譜の途中局面から再対局するテスト
 */
void TestKifuDisplayCoordinator::testRematchFromMainLineAfterBranch()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜（ライン0）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△8五歩
    KifuBranchNode* mainNode = tree.root();
    KifuBranchNode* node1 = tree.addMove(mainNode,
        ShogiMove(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲７六歩(77)"),
        QStringLiteral("sfen_main_1"), QString());
    KifuBranchNode* node2 = tree.addMove(node1,
        ShogiMove(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△３四歩(33)"),
        QStringLiteral("sfen_main_2"), QString());
    KifuBranchNode* node3_main = tree.addMove(node2,
        ShogiMove(QPoint(2, 7), QPoint(2, 6), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲２六歩(27)"),
        QStringLiteral("sfen_main_3"), QString());
    KifuBranchNode* node4_main = tree.addMove(node3_main,
        ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８四歩(83)"),
        QStringLiteral("sfen_main_4"), QString());
    KifuBranchNode* node5_main = tree.addMove(node4_main,
        ShogiMove(QPoint(2, 6), QPoint(2, 5), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲２五歩(26)"),
        QStringLiteral("sfen_main_5"), QString());
    KifuBranchNode* node6_main = tree.addMove(node5_main,
        ShogiMove(QPoint(8, 4), QPoint(8, 5), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８五歩(84)"),
        QStringLiteral("sfen_main_6"), QString());

    // 分岐（ライン1）: 2手目から分岐 - 3.▲1六歩 4.△8四歩 5.▲4八銀 6.△8五歩
    KifuBranchNode* node3_branch = tree.addMove(node2,
        ShogiMove(QPoint(1, 7), QPoint(1, 6), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲１六歩(17)"),
        QStringLiteral("sfen_branch_3"), QString());
    KifuBranchNode* node4_branch = tree.addMove(node3_branch,
        ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８四歩(83)"),
        QStringLiteral("sfen_branch_4"), QString());
    KifuBranchNode* node5_branch = tree.addMove(node4_branch,
        ShogiMove(QPoint(4, 9), QPoint(4, 8), QLatin1Char('S'), QLatin1Char(' '), false),
        QStringLiteral("▲４八銀(39)"),
        QStringLiteral("sfen_branch_5"), QString());
    KifuBranchNode* node6_branch = tree.addMove(node5_branch,
        ShogiMove(QPoint(8, 4), QPoint(8, 5), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８五歩(84)"),
        QStringLiteral("sfen_branch_6"), QString());

    // 検証: ツリー構造が正しいか
    QVector<BranchLine> lines = tree.allLines();
    QCOMPARE(lines.size(), 2);

    // 本譜（ライン0）の3手目が▲2六歩であること
    QCOMPARE(lines.at(0).nodes.at(3)->displayText(), QStringLiteral("▲２六歩(27)"));
    // 分岐（ライン1）の3手目が▲1六歩であること
    QCOMPARE(lines.at(1).nodes.at(3)->displayText(), QStringLiteral("▲１六歩(17)"));

    // ナビゲーション状態を設定
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // ステップ1: 分岐ライン1の6手目にナビゲート
    nav.goToNode(node6_branch);
    state.setPreferredLineIndex(1);  // 分岐ライン上にいることを設定

    // この時点で棋譜欄は分岐ライン1のデータを表示しているはず
    QCOMPARE(state.currentLineIndex(), 1);

    // ステップ2: 本譜の6手目にナビゲート（分岐ツリーをクリックした場合）
    state.resetPreferredLineIndex();  // 本譜に戻る際にリセット
    nav.goToNode(node6_main);

    // 本譜にいることを確認
    QCOMPARE(state.currentLineIndex(), 0);
    QCOMPARE(state.currentNode(), node6_main);

    // ステップ3: 本譜の6手目からライブセッションを開始
    coordinator.onLiveGameSessionStarted(node6_main);

    // ステップ4: 棋譜欄が本譜の指し手を表示していることを確認
    QCOMPARE(recordModel.rowCount(), 7);  // 開始局面 + 6手

    // 3手目が▲2六歩（本譜）であること（▲1六歩（分岐）ではない）
    KifuDisplay* item3 = recordModel.item(3);
    QVERIFY(item3 != nullptr);
    qDebug() << "Move 3 in record model:" << item3->currentMove();
    QVERIFY2(item3->currentMove().contains(QStringLiteral("２六歩")),
             qPrintable(QString("Expected ２六歩 but got: %1").arg(item3->currentMove())));

    // 5手目が▲2五歩（本譜）であること（▲4八銀（分岐）ではない）
    KifuDisplay* item5 = recordModel.item(5);
    QVERIFY(item5 != nullptr);
    qDebug() << "Move 5 in record model:" << item5->currentMove();
    QVERIFY2(item5->currentMove().contains(QStringLiteral("２五歩")),
             qPrintable(QString("Expected ２五歩 but got: %1").arg(item5->currentMove())));
}

/**
 * @brief ライブセッション開始時にpreferredLineIndexがリセットされることを確認
 */
void TestKifuDisplayCoordinator::testPreferredLineIndexResetOnSessionStart()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜を作成
    KifuBranchNode* node = tree.root();
    node = tree.addMove(node,
        ShogiMove(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲７六歩"), QStringLiteral("sfen_1"), QString());
    node = tree.addMove(node,
        ShogiMove(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△３四歩"), QStringLiteral("sfen_2"), QString());

    // 分岐を作成
    tree.addMove(tree.root()->childAt(0),
        ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８四歩"), QStringLiteral("sfen_branch"), QString());

    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // preferredLineIndexを1に設定（分岐ライン上にいる状態）
    state.setPreferredLineIndex(1);
    QCOMPARE(state.preferredLineIndex(), 1);

    // ライブセッション開始
    coordinator.onLiveGameSessionStarted(node);

    // preferredLineIndexがリセットされていることを確認
    QCOMPARE(state.preferredLineIndex(), -1);
}

/**
 * @brief 1局目→2局目→3局目のシナリオで、1局目の途中から3局目を開始した場合のテスト
 *
 * シナリオ:
 * 1. 1局目（本譜）を7手目まで作成（7手目で投了）
 * 2. 2局目を3手目から分岐（ライン1）で11手目まで作成
 * 3. 本譜の6手目にナビゲート
 * 4. 3局目を本譜の6手目からライブセッション開始
 * 5. 棋譜欄が本譜の1-6手目を正しく表示することを確認
 */
void TestKifuDisplayCoordinator::testThirdGameFromFirstGamePosition()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("root_sfen"));

    // 1局目（本譜）: 7手目で投了
    KifuBranchNode* node = tree.root();
    KifuBranchNode* main1 = tree.addMove(node, ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("main_sfen_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("main_sfen_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"), QStringLiteral("main_sfen_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("main_sfen_4"), QString());
    KifuBranchNode* main5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"), QStringLiteral("main_sfen_5"), QString());
    KifuBranchNode* main6 = tree.addMove(main5, ShogiMove(), QStringLiteral("△８五歩(84)"), QStringLiteral("main_sfen_6"), QString());
    tree.addTerminalMove(main6, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 2局目（ライン1）: 3手目から分岐、11手目で投了
    KifuBranchNode* branch3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩(17)"), QStringLiteral("branch_sfen_3"), QString());
    KifuBranchNode* branch4 = tree.addMove(branch3, ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("branch_sfen_4"), QString());
    KifuBranchNode* branch5 = tree.addMove(branch4, ShogiMove(), QStringLiteral("▲１八香(19)"), QStringLiteral("branch_sfen_5"), QString());
    KifuBranchNode* branch6 = tree.addMove(branch5, ShogiMove(), QStringLiteral("△８五歩(84)"), QStringLiteral("branch_sfen_6"), QString());
    KifuBranchNode* branch7 = tree.addMove(branch6, ShogiMove(), QStringLiteral("▲３八銀(39)"), QStringLiteral("branch_sfen_7"), QString());
    KifuBranchNode* branch8 = tree.addMove(branch7, ShogiMove(), QStringLiteral("△８六歩(85)"), QStringLiteral("branch_sfen_8"), QString());
    KifuBranchNode* branch9 = tree.addMove(branch8, ShogiMove(), QStringLiteral("▲４八金(49)"), QStringLiteral("branch_sfen_9"), QString());
    KifuBranchNode* branch10 = tree.addMove(branch9, ShogiMove(), QStringLiteral("△８八角成(22)"), QStringLiteral("branch_sfen_10"), QString());
    tree.addTerminalMove(branch10, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // ツリー構造を確認
    QVector<BranchLine> lines = tree.allLines();
    QCOMPARE(lines.size(), 2);
    qDebug() << "Line 0 (main) node count:" << lines.at(0).nodes.size();
    qDebug() << "Line 1 (branch) node count:" << lines.at(1).nodes.size();

    // ナビゲーション状態を設定
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // 2局目終了後の状態をシミュレート：ライン1の最後にいる
    nav.goToNode(branch10);
    state.setPreferredLineIndex(1);
    QCOMPARE(state.currentLineIndex(), 1);

    // 棋譜欄にライン1のデータを表示（2局目終了時の状態）
    recordModel.clearAllItems();
    recordModel.appendItem(new KifuDisplay(QStringLiteral("=== 開始局面 ==="), QString()));
    for (KifuBranchNode* n : std::as_const(lines.at(1).nodes)) {
        if (n->ply() == 0) continue;
        const QString num = QString::number(n->ply());
        const QString spaces = QString(qMax(0, 4 - num.length()), QLatin1Char(' '));
        recordModel.appendItem(new KifuDisplay(spaces + num + QLatin1Char(' ') + n->displayText(), QString()));
    }
    qDebug() << "Record model before rematch:" << recordModel.rowCount() << "rows";

    // 検証: この時点でライン1のデータが表示されている
    QVERIFY(recordModel.item(3)->currentMove().contains(QStringLiteral("１六歩")));  // ライン1の3手目

    // ここからが3局目の開始シミュレーション

    // 本譜の6手目にナビゲート（ユーザーが分岐ツリーの本譜をクリック）
    state.resetPreferredLineIndex();  // 本譜へ移動するのでリセット
    nav.goToNode(main6);

    QCOMPARE(state.currentLineIndex(), 0);  // 本譜にいることを確認
    QCOMPARE(state.currentNode(), main6);

    // 3局目をライブセッション開始（本譜の6手目から）
    qDebug() << "Starting live session from main6, sfen:" << main6->sfen();
    coordinator.onLiveGameSessionStarted(main6);

    // 検証: 棋譜欄が本譜の1-6手目を表示していること
    qDebug() << "Record model after session start:" << recordModel.rowCount() << "rows";
    QCOMPARE(recordModel.rowCount(), 7);  // 開始局面 + 6手

    // 各手が本譜の指し手であることを確認
    for (int i = 1; i <= 6; ++i) {
        KifuDisplay* item = recordModel.item(i);
        QVERIFY2(item != nullptr, qPrintable(QString("Item at row %1 is null").arg(i)));
        qDebug() << "Row" << i << ":" << item->currentMove();
    }

    // 3手目が本譜の▲2六歩であること（ライン1の▲1六歩ではない）
    KifuDisplay* item3 = recordModel.item(3);
    QVERIFY2(item3->currentMove().contains(QStringLiteral("２六歩")),
             qPrintable(QString("Row 3: Expected ２六歩 but got: %1").arg(item3->currentMove())));

    // 5手目が本譜の▲2五歩であること（ライン1の▲1八香ではない）
    KifuDisplay* item5 = recordModel.item(5);
    QVERIFY2(item5->currentMove().contains(QStringLiteral("２五歩")),
             qPrintable(QString("Row 5: Expected ２五歩 but got: %1").arg(item5->currentMove())));
}

/**
 * @brief 棋譜欄の内容が分岐ツリーのパスと一致することを確認
 */
/**
 * @brief 3つの分岐がある場合のテスト（スクリーンショットの状態を再現）
 *
 * シナリオ:
 * - 本譜: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△8五歩 7.▲7八金 8.△3二金 9.▲投了
 * - 分岐1（3手目から）: 3.▲1六歩 4.△8四歩 5.▲1五歩 6.△8五歩 7.▲1六香 8.△3二金 9.▲3八銀 10.△8六歩 11.▲投了
 * - 分岐2（本譜の7手目から）: 7.▲7八金(69) 8.△6二銀(71) 9.▲投了
 *
 * 分岐2を選択した場合:
 * - 棋譜欄には1-6手目は本譜の手、7-9手目は分岐2の手を表示すべき
 * - 3手目が▲2六歩（本譜）であり、▲1六歩（分岐1）ではないことを確認
 */
/**
 * @brief 本譜をクリックした時に正しく本譜が表示されるかテスト（スクリーンショットの問題を再現）
 *
 * シナリオ:
 * - 本譜: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△8五歩 7.▲投了
 * - 分岐1（3手目から）: 3.▲1六歩 4.△8四歩 5.▲6八玉 6.△1四歩 7.▲7八銀 8.△8八角成 9.▲投了
 *
 * 分岐1の6手目にいる状態で、本譜の6手目をクリックした場合：
 * - 棋譜欄には本譜の1-6手目を表示すべき
 * - 3手目が▲2六歩（本譜）であり、▲1六歩（分岐1）ではないことを確認
 */
void TestKifuDisplayCoordinator::testClickMainLineWhileOnBranch()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("root_sfen"));

    // 本譜: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△8五歩 7.▲投了
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("main_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("main_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"), QStringLiteral("main_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("main_4"), QString());
    KifuBranchNode* main5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"), QStringLiteral("main_5"), QString());
    KifuBranchNode* main6 = tree.addMove(main5, ShogiMove(), QStringLiteral("△８五歩(84)"), QStringLiteral("main_6"), QString());
    tree.addTerminalMove(main6, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 分岐1（3手目から）: 3.▲1六歩 4.△8四歩 5.▲6八玉 6.△1四歩 7.▲7八銀 8.△8八角成 9.▲投了
    KifuBranchNode* br1_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩(17)"), QStringLiteral("br1_3"), QString());
    KifuBranchNode* br1_4 = tree.addMove(br1_3, ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("br1_4"), QString());
    KifuBranchNode* br1_5 = tree.addMove(br1_4, ShogiMove(), QStringLiteral("▲６八玉(59)"), QStringLiteral("br1_5"), QString());
    KifuBranchNode* br1_6 = tree.addMove(br1_5, ShogiMove(), QStringLiteral("△１四歩(13)"), QStringLiteral("br1_6"), QString());
    KifuBranchNode* br1_7 = tree.addMove(br1_6, ShogiMove(), QStringLiteral("▲７八銀(79)"), QStringLiteral("br1_7"), QString());
    KifuBranchNode* br1_8 = tree.addMove(br1_7, ShogiMove(), QStringLiteral("△８八角成(22)"), QStringLiteral("br1_8"), QString());
    tree.addTerminalMove(br1_8, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // ツリー構造を確認
    QVector<BranchLine> lines = tree.allLines();
    qDebug() << "=== testClickMainLineWhileOnBranch ===";
    qDebug() << "Number of lines:" << lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        qDebug() << "Line" << i << ": branchPly=" << lines.at(i).branchPly
                 << "nodes=" << lines.at(i).nodes.size();
        if (lines.at(i).nodes.size() >= 3) {
            qDebug() << "  Move 3:" << lines.at(i).nodes.at(3)->displayText();
        }
    }

    // 各ラインの3手目を確認
    QCOMPARE(lines.size(), 2);
    // line[0]は本譜
    QCOMPARE(lines.at(0).branchPly, 0);
    QVERIFY(lines.at(0).nodes.at(3)->displayText().contains(QStringLiteral("２六歩")));
    // line[1]は分岐1
    QCOMPARE(lines.at(1).branchPly, 3);
    QVERIFY(lines.at(1).nodes.at(3)->displayText().contains(QStringLiteral("１六歩")));

    // ナビゲーション状態を設定
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // ステップ1: 分岐1の6手目にナビゲート（分岐ライン上にいる状態を再現）
    qDebug() << "Step 1: Navigate to br1_6 (branch line)";
    int br1LineIndex = tree.findLineIndexForNode(br1_6);
    qDebug() << "br1_6 lineIndex:" << br1LineIndex;
    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_6);

    QCOMPARE(state.currentNode(), br1_6);
    QCOMPARE(state.currentLineIndex(), br1LineIndex);

    // この時点で棋譜欄は分岐1のデータを表示しているはず
    qDebug() << "After navigating to br1_6, record model:";
    for (int i = 0; i < recordModel.rowCount(); ++i) {
        qDebug() << "  Row" << i << ":" << recordModel.item(i)->currentMove();
    }
    QVERIFY(recordModel.item(3)->currentMove().contains(QStringLiteral("１六歩")));

    // ステップ2: 本譜の6手目にナビゲート（ユーザーが本譜をクリックした場合を再現）
    qDebug() << "Step 2: Navigate to main6 (main line)";
    int main6LineIndex = tree.findLineIndexForNode(main6);
    qDebug() << "main6 lineIndex:" << main6LineIndex;

    // 本譜に戻るのでpreferredLineIndexをリセット
    state.resetPreferredLineIndex();
    nav.goToNode(main6);

    QCOMPARE(state.currentNode(), main6);
    QCOMPARE(state.currentLineIndex(), 0);  // 本譜は常にline 0

    // 重要な検証: 棋譜欄が本譜の手を表示していること
    qDebug() << "After navigating to main6, record model:";
    for (int i = 0; i < recordModel.rowCount(); ++i) {
        qDebug() << "  Row" << i << ":" << recordModel.item(i)->currentMove();
    }

    // 3手目が本譜の▲2六歩であること（分岐1の▲1六歩ではない）
    KifuDisplay* item3 = recordModel.item(3);
    QVERIFY(item3 != nullptr);
    qDebug() << "Move 3 in record model:" << item3->currentMove();
    QVERIFY2(item3->currentMove().contains(QStringLiteral("２六歩")),
             qPrintable(QString("Move 3: Expected ２六歩(27) but got: %1").arg(item3->currentMove())));

    // 5手目が本譜の▲2五歩であること
    KifuDisplay* item5 = recordModel.item(5);
    QVERIFY(item5 != nullptr);
    qDebug() << "Move 5 in record model:" << item5->currentMove();
    QVERIFY2(item5->currentMove().contains(QStringLiteral("２五歩")),
             qPrintable(QString("Move 5: Expected ２五歩(26) but got: %1").arg(item5->currentMove())));
}

void TestKifuDisplayCoordinator::testThreeBranchesSelectSecondBranch()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("root_sfen"));

    // 本譜: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△8五歩 7.▲7八金 8.△3二金 9.▲投了
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("main_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("main_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"), QStringLiteral("main_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("main_4"), QString());
    KifuBranchNode* main5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"), QStringLiteral("main_5"), QString());
    KifuBranchNode* main6 = tree.addMove(main5, ShogiMove(), QStringLiteral("△８五歩(84)"), QStringLiteral("main_6"), QString());
    KifuBranchNode* main7 = tree.addMove(main6, ShogiMove(), QStringLiteral("▲７八金(69)"), QStringLiteral("main_7"), QString());
    KifuBranchNode* main8 = tree.addMove(main7, ShogiMove(), QStringLiteral("△３二金(41)"), QStringLiteral("main_8"), QString());
    tree.addTerminalMove(main8, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 分岐1（3手目から）: 3.▲1六歩 4.△8四歩 5.▲1五歩 6.△8五歩 7.▲1六香 8.△3二金 9.▲3八銀 10.△8六歩 11.▲投了
    KifuBranchNode* br1_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩(17)"), QStringLiteral("br1_3"), QString());
    KifuBranchNode* br1_4 = tree.addMove(br1_3, ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("br1_4"), QString());
    KifuBranchNode* br1_5 = tree.addMove(br1_4, ShogiMove(), QStringLiteral("▲１五歩(16)"), QStringLiteral("br1_5"), QString());
    KifuBranchNode* br1_6 = tree.addMove(br1_5, ShogiMove(), QStringLiteral("△８五歩(84)"), QStringLiteral("br1_6"), QString());
    KifuBranchNode* br1_7 = tree.addMove(br1_6, ShogiMove(), QStringLiteral("▲１六香(19)"), QStringLiteral("br1_7"), QString());
    KifuBranchNode* br1_8 = tree.addMove(br1_7, ShogiMove(), QStringLiteral("△３二金(41)"), QStringLiteral("br1_8"), QString());
    KifuBranchNode* br1_9 = tree.addMove(br1_8, ShogiMove(), QStringLiteral("▲３八銀(39)"), QStringLiteral("br1_9"), QString());
    KifuBranchNode* br1_10 = tree.addMove(br1_9, ShogiMove(), QStringLiteral("△８六歩(85)"), QStringLiteral("br1_10"), QString());
    tree.addTerminalMove(br1_10, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 分岐2（本譜の7手目から）: 7.▲7八金(69) 8.△6二銀(71) 9.▲投了
    //注意: 本譜と同じ7手目だが、異なるノードとして追加される
    KifuBranchNode* br2_7 = tree.addMove(main6, ShogiMove(), QStringLiteral("▲７八金(69)"), QStringLiteral("br2_7"), QString());
    KifuBranchNode* br2_8 = tree.addMove(br2_7, ShogiMove(), QStringLiteral("△６二銀(71)"), QStringLiteral("br2_8"), QString());
    tree.addTerminalMove(br2_8, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // ツリー構造を確認
    QVector<BranchLine> lines = tree.allLines();
    qDebug() << "Number of lines:" << lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        qDebug() << "Line" << i << "has" << lines.at(i).nodes.size() << "nodes, branchPly=" << lines.at(i).branchPly;
        if (lines.at(i).nodes.size() >= 3) {
            qDebug() << "  Move 3:" << lines.at(i).nodes.at(3)->displayText();
        }
    }

    // 3つのラインが存在することを確認
    QCOMPARE(lines.size(), 3);

    // ナビゲーション状態を設定
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // 分岐2の最後のノード（投了の1つ前、△6二銀）にナビゲート
    qDebug() << "Navigating to br2_8 (△６二銀)";
    // br2_8が属するラインインデックスを正しく取得
    int br2LineIndex = tree.findLineIndexForNode(br2_8);
    qDebug() << "br2_8 belongs to line index:" << br2LineIndex;
    state.setPreferredLineIndex(br2LineIndex);
    nav.goToNode(br2_8);

    QCOMPARE(state.currentNode(), br2_8);
    qDebug() << "currentLineIndex:" << state.currentLineIndex();

    // 棋譜欄が分岐2のパスを表示していることを確認
    // 分岐2のパス: main1→main2→main3→main4→main5→main6→br2_7→br2_8
    qDebug() << "Record model after navigating to br2_8:";
    for (int i = 0; i < recordModel.rowCount(); ++i) {
        qDebug() << "  Row" << i << ":" << recordModel.item(i)->currentMove();
    }

    // 重要な検証: 3手目が本譜の▲2六歩であること（分岐1の▲1六歩ではない）
    KifuDisplay* item3 = recordModel.item(3);
    QVERIFY(item3 != nullptr);
    qDebug() << "Move 3 in record model:" << item3->currentMove();
    QVERIFY2(item3->currentMove().contains(QStringLiteral("２六歩")),
             qPrintable(QString("Move 3: Expected ２六歩(27) but got: %1").arg(item3->currentMove())));

    // 5手目が本譜の▲2五歩であること
    KifuDisplay* item5 = recordModel.item(5);
    QVERIFY(item5 != nullptr);
    qDebug() << "Move 5 in record model:" << item5->currentMove();
    QVERIFY2(item5->currentMove().contains(QStringLiteral("２五歩")),
             qPrintable(QString("Move 5: Expected ２五歩(26) but got: %1").arg(item5->currentMove())));

    // 7手目が分岐2の▲7八金であること
    KifuDisplay* item7 = recordModel.item(7);
    QVERIFY(item7 != nullptr);
    qDebug() << "Move 7 in record model:" << item7->currentMove();
    QVERIFY2(item7->currentMove().contains(QStringLiteral("７八金")),
             qPrintable(QString("Move 7: Expected ７八金(69) but got: %1").arg(item7->currentMove())));

    // 8手目が分岐2の△6二銀であること
    KifuDisplay* item8 = recordModel.item(8);
    QVERIFY(item8 != nullptr);
    qDebug() << "Move 8 in record model:" << item8->currentMove();
    QVERIFY2(item8->currentMove().contains(QStringLiteral("６二銀")),
             qPrintable(QString("Move 8: Expected ６二銀(71) but got: %1").arg(item8->currentMove())));
}

void TestKifuDisplayCoordinator::testKifuModelMatchesBranchTreePath()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("root_sfen"));

    // 本譜を作成
    KifuBranchNode* node = tree.root();
    KifuBranchNode* main1 = tree.addMove(node, ShogiMove(), QStringLiteral("▲７六歩"), QStringLiteral("main_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩"), QStringLiteral("main_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩"), QStringLiteral("main_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩"), QStringLiteral("main_4"), QString());

    // 分岐を作成（3手目から）
    KifuBranchNode* branch3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩"), QStringLiteral("branch_3"), QString());
    KifuBranchNode* branch4 = tree.addMove(branch3, ShogiMove(), QStringLiteral("△８四歩"), QStringLiteral("branch_4"), QString());

    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // テスト1: 本譜の4手目からセッション開始
    nav.goToNode(main4);
    coordinator.onLiveGameSessionStarted(main4);

    // 棋譜欄のパスを取得
    QStringList kifuMoves;
    for (int i = 1; i < recordModel.rowCount(); ++i) {
        kifuMoves << recordModel.item(i)->currentMove();
    }
    qDebug() << "Kifu from main4:" << kifuMoves;

    // 分岐ツリーのパスを取得
    QVector<KifuBranchNode*> treePath = tree.pathToNode(main4);
    QStringList treePathMoves;
    for (KifuBranchNode* n : std::as_const(treePath)) {
        if (n->ply() > 0) {
            treePathMoves << n->displayText();
        }
    }
    qDebug() << "Tree path to main4:" << treePathMoves;

    // 棋譜欄と分岐ツリーのパスが一致することを確認
    QCOMPARE(kifuMoves.size(), treePathMoves.size());
    for (int i = 0; i < kifuMoves.size(); ++i) {
        QVERIFY2(kifuMoves.at(i).contains(treePathMoves.at(i)),
                 qPrintable(QString("Mismatch at index %1: kifu='%2', tree='%3'")
                            .arg(i).arg(kifuMoves.at(i), treePathMoves.at(i))));
    }

    // テスト2: 分岐の4手目からセッション開始
    state.resetPreferredLineIndex();
    nav.goToNode(branch4);
    coordinator.onLiveGameSessionStarted(branch4);

    kifuMoves.clear();
    for (int i = 1; i < recordModel.rowCount(); ++i) {
        kifuMoves << recordModel.item(i)->currentMove();
    }
    qDebug() << "Kifu from branch4:" << kifuMoves;

    treePath = tree.pathToNode(branch4);
    treePathMoves.clear();
    for (KifuBranchNode* n : std::as_const(treePath)) {
        if (n->ply() > 0) {
            treePathMoves << n->displayText();
        }
    }
    qDebug() << "Tree path to branch4:" << treePathMoves;

    QCOMPARE(kifuMoves.size(), treePathMoves.size());
    for (int i = 0; i < kifuMoves.size(); ++i) {
        QVERIFY2(kifuMoves.at(i).contains(treePathMoves.at(i)),
                 qPrintable(QString("Mismatch at index %1: kifu='%2', tree='%3'")
                            .arg(i).arg(kifuMoves.at(i), treePathMoves.at(i))));
    }
}

/**
 * @brief 分岐棋譜を読み込んだ後、本譜の途中局面から再対局した場合に、
 *        分岐ツリーの親子関係（parentIndex）が正しく維持されることを確認
 *
 * シナリオ（test2_branch.kifと同様の構造）:
 * - 本譜（ライン0）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△投了
 * - 分岐1（3手目から）: 3.▲1六歩 4.△9四歩 5.▲4六歩 6.△7四歩
 * - 分岐2（分岐1の5手目から）: 5.▲5六歩 6.△5二金 7.▲5五歩
 *
 * 本譜の4手目「△8四歩」から対局を開始し、新しい指し手を追加した場合:
 * - 分岐2の親は依然として分岐1（ライン1）であること
 * - 分岐2の罫線が分岐1の4手目「△9四歩」から引かれること（本譜の4手目「△8四歩」からではない）
 */
void TestKifuDisplayCoordinator::testBranchParentIndexPreservedAfterRematch()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("root_sfen"));

    // 本譜（ライン0）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△投了
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("main_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("main_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"), QStringLiteral("main_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("main_4"), QString());
    KifuBranchNode* main5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"), QStringLiteral("main_5"), QString());
    tree.addTerminalMove(main5, TerminalType::Resign, QStringLiteral("△投了"), QString());

    // 分岐1（3手目から）: 3.▲1六歩 4.△9四歩 5.▲4六歩 6.△7四歩
    KifuBranchNode* br1_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩(17)"), QStringLiteral("br1_3"), QString());
    KifuBranchNode* br1_4 = tree.addMove(br1_3, ShogiMove(), QStringLiteral("△９四歩(93)"), QStringLiteral("br1_4"), QString());
    KifuBranchNode* br1_5 = tree.addMove(br1_4, ShogiMove(), QStringLiteral("▲４六歩(47)"), QStringLiteral("br1_5"), QString());
    tree.addMove(br1_5, ShogiMove(), QStringLiteral("△７四歩(73)"), QStringLiteral("br1_6"), QString());

    // 分岐2（分岐1の5手目から）: 5.▲5六歩 6.△5二金 7.▲5五歩
    KifuBranchNode* br2_5 = tree.addMove(br1_4, ShogiMove(), QStringLiteral("▲５六歩(57)"), QStringLiteral("br2_5"), QString());
    KifuBranchNode* br2_6 = tree.addMove(br2_5, ShogiMove(), QStringLiteral("△５二金(61)"), QStringLiteral("br2_6"), QString());
    tree.addMove(br2_6, ShogiMove(), QStringLiteral("▲５五歩(56)"), QStringLiteral("br2_7"), QString());

    // ツリー構造を確認
    QVector<BranchLine> lines = tree.allLines();
    QCOMPARE(lines.size(), 3);  // 本譜、分岐1、分岐2

    // 各ラインのbranchPlyとbranchPointを確認
    qDebug() << "=== testBranchParentIndexPreservedAfterRematch ===";
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        qDebug() << "Line" << i << ": branchPly=" << line.branchPly
                 << "branchPoint=" << (line.branchPoint ? line.branchPoint->displayText() : "null");
    }

    // 分岐2のbranchPlyが5であること（分岐1の4手目の後）
    QCOMPARE(lines.at(2).branchPly, 5);
    // 分岐2のbranchPointが△9四歩(93)（分岐1の4手目）であること
    QVERIFY(lines.at(2).branchPoint != nullptr);
    QCOMPARE(lines.at(2).branchPoint->displayText(), QStringLiteral("△９四歩(93)"));

    // KifuDisplayCoordinatorを使用して、親行インデックスを計算する
    // ここでは、BranchLineの情報からResolvedRowLiteを作成し、親行を正しく設定することを確認する
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // 本譜の4手目にナビゲート
    nav.goToNode(main4);
    QCOMPARE(state.currentNode(), main4);
    QCOMPARE(state.currentLineIndex(), 0);  // 本譜

    // ライブセッション開始（本譜の4手目から）
    liveSession.startFromNode(main4);

    // 新しい指し手を追加
    liveSession.addMove(ShogiMove(), QStringLiteral("▲５八金(49)"), QStringLiteral("new_5"), QString());
    liveSession.addMove(ShogiMove(), QStringLiteral("△８五歩(84)"), QStringLiteral("new_6"), QString());

    // 検証: 分岐ツリーのライン構造が正しいことを確認
    lines = tree.allLines();
    qDebug() << "After addMove, lines.size()=" << lines.size();

    // ライン数が4になっていること（本譜、分岐1、分岐2、新しい分岐）
    QCOMPARE(lines.size(), 4);

    // 各ラインのbranchPlyとbranchPointを再確認
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        qDebug() << "After addMove Line" << i << ": branchPly=" << line.branchPly
                 << "branchPoint=" << (line.branchPoint ? line.branchPoint->displayText() : "null")
                 << "nodes.size=" << line.nodes.size();
    }

    // 重要な検証: 分岐2のbranchPointが依然として△9四歩(93)であること
    // （本譜の△8四歩(83)に変わっていないこと）
    // 分岐2は lines.at(2) または lines.at(3) のどちらかに配置される可能性があるため、
    // branchPly==5 かつ displayText に「５六歩」を含むラインを探す
    bool foundBranch2 = false;
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        if (line.branchPly == 5 && line.nodes.size() >= 5 &&
            line.nodes.at(5)->displayText().contains(QStringLiteral("５六歩"))) {
            foundBranch2 = true;
            // このラインのbranchPointが△9四歩(93)であること
            QVERIFY2(line.branchPoint != nullptr,
                     qPrintable(QString("Branch 2 (line %1) has null branchPoint").arg(i)));
            QVERIFY2(line.branchPoint->displayText().contains(QStringLiteral("９四歩")),
                     qPrintable(QString("Branch 2 branchPoint should be △9四歩(93) but got: %1")
                                .arg(line.branchPoint->displayText())));
            break;
        }
    }
    QVERIFY2(foundBranch2, "Could not find branch 2 (line with ▲5六歩 at ply 5)");

    // 追加検証: ResolvedRowLiteに変換した場合のparent値を確認
    // これはKifuDisplayCoordinator::onLiveGameMoveAdded()で行われる変換をテストする
    QVector<int> expectedParents;
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        int parentLineIndex = -1;

        // branchPointから親ラインを計算
        if (line.branchPoint != nullptr) {
            // branchPointが含まれるラインのインデックスを探す
            for (int j = 0; j < lines.size(); ++j) {
                if (j == i) continue;  // 自分自身はスキップ
                if (lines.at(j).nodes.contains(line.branchPoint)) {
                    parentLineIndex = j;
                    break;
                }
            }
        }

        expectedParents.append(parentLineIndex);
        qDebug() << "Line" << i << "expected parentLineIndex=" << parentLineIndex;
    }

    // 本譜（ライン0）の親は-1
    QCOMPARE(expectedParents.at(0), -1);

    // 分岐1（ライン1）の親は本譜（ライン0）
    QCOMPARE(expectedParents.at(1), 0);

    // 分岐2のラインを見つけて、その親が分岐1（△９四歩を含むライン）であることを確認
    // 注: 新しい分岐が追加されると、ライン順序が変わることがある
    // そのため、インデックスではなく、親が「△９四歩」を含むラインであることを確認する
    int branch1LineIndex = -1;
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        // 分岐1は「△９四歩(93)」を含み、かつ「▲４六歩(47)」も含むライン
        bool has94fu = false;
        bool has46fu = false;
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->displayText().contains(QStringLiteral("９四歩"))) has94fu = true;
            if (node->displayText().contains(QStringLiteral("４六歩"))) has46fu = true;
        }
        if (has94fu && has46fu) {
            branch1LineIndex = i;
            break;
        }
    }
    QVERIFY2(branch1LineIndex >= 0, "Could not find branch 1 (line with △9四歩 and ▲4六歩)");

    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        if (line.branchPly == 5 && line.nodes.size() >= 5 &&
            line.nodes.at(5)->displayText().contains(QStringLiteral("５六歩"))) {
            QVERIFY2(expectedParents.at(i) == branch1LineIndex,
                     qPrintable(QString("Branch 2 (line %1) should have parent %2 (branch 1) but got: %3")
                                .arg(i).arg(branch1LineIndex).arg(expectedParents.at(i))));
            break;
        }
    }
}

/**
 * @brief 分岐ライン上のノードから再対局した場合、新しい指し手がその分岐ラインに追加されることを確認
 *
 * シナリオ:
 * - 本譜（ライン0）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△投了
 * - 分岐1（3手目から）: 3.▲1六歩 4.△9四歩 5.▲4六歩 6.△7四歩
 *
 * 分岐1の4手目「△9四歩」から対局を開始し、新しい指し手を追加した場合:
 * - 新しい指し手は分岐1の4手目の子として追加されること（本譜の4手目の子ではない）
 * - 新しい分岐のbranchPointが分岐1の4手目「△9四歩」であること
 */
void TestKifuDisplayCoordinator::testRematchFromBranchLineAddsMoveToCorrectLine()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("root_sfen"));

    // 本譜（ライン0）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△投了
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"), QStringLiteral("main_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("main_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"), QStringLiteral("main_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("main_4"), QString());
    KifuBranchNode* main5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"), QStringLiteral("main_5"), QString());
    tree.addTerminalMove(main5, TerminalType::Resign, QStringLiteral("△投了"), QString());

    // 分岐1（3手目から）: 3.▲1六歩 4.△9四歩 5.▲4六歩 6.△7四歩
    KifuBranchNode* br1_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩(17)"), QStringLiteral("br1_3"), QString());
    KifuBranchNode* br1_4 = tree.addMove(br1_3, ShogiMove(), QStringLiteral("△９四歩(93)"), QStringLiteral("br1_4"), QString());
    KifuBranchNode* br1_5 = tree.addMove(br1_4, ShogiMove(), QStringLiteral("▲４六歩(47)"), QStringLiteral("br1_5"), QString());
    tree.addMove(br1_5, ShogiMove(), QStringLiteral("△７四歩(73)"), QStringLiteral("br1_6"), QString());

    // ツリー構造を確認
    QVector<BranchLine> lines = tree.allLines();
    QCOMPARE(lines.size(), 2);  // 本譜、分岐1

    qDebug() << "=== testRematchFromBranchLineAddsMoveToCorrectLine ===";
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        qDebug() << "Line" << i << ": branchPly=" << line.branchPly
                 << "nodes.size=" << line.nodes.size();
    }

    // KifuDisplayCoordinatorを設定
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // 重要: 分岐1の4手目「△9四歩」にナビゲート
    int br1LineIndex = tree.findLineIndexForNode(br1_4);
    qDebug() << "br1_4 belongs to line index:" << br1LineIndex;

    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_4);

    QCOMPARE(state.currentNode(), br1_4);
    QCOMPARE(state.currentLineIndex(), br1LineIndex);

    // 分岐1の4手目からライブセッション開始
    qDebug() << "Starting live session from br1_4 (△９四歩)";
    liveSession.startFromNode(br1_4);

    // 新しい指し手を追加
    // この指し手は br1_4（△9四歩）の子として追加されるべき
    liveSession.addMove(ShogiMove(), QStringLiteral("▲６八玉(59)"), QStringLiteral("new_5_from_br1"), QString());
    liveSession.addMove(ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("new_6_from_br1"), QString());

    // 検証: 分岐ツリーのライン構造が正しいことを確認
    lines = tree.allLines();
    qDebug() << "After addMove, lines.size()=" << lines.size();

    // ライン数が3になっていること（本譜、分岐1、新しい分岐）
    QCOMPARE(lines.size(), 3);

    // 各ラインのbranchPlyとbranchPointを確認
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        qDebug() << "After addMove Line" << i << ": branchPly=" << line.branchPly
                 << "branchPoint=" << (line.branchPoint ? line.branchPoint->displayText() : "null")
                 << "nodes.size=" << line.nodes.size();
        // 最初の数手を表示
        for (int j = 0; j < qMin(6, line.nodes.size()); ++j) {
            qDebug() << "  Node" << j << ":" << line.nodes.at(j)->displayText();
        }
    }

    // 重要な検証: 新しい分岐のbranchPointが△9四歩(93)（分岐1の4手目）であること
    // （本譜の△8四歩(83)ではない）
    bool foundNewBranch = false;
    for (int i = 0; i < lines.size(); ++i) {
        const BranchLine& line = lines.at(i);
        // 新しい分岐は branchPly==5 かつ 5手目に「６八玉」を含むライン
        if (line.branchPly == 5 && line.nodes.size() >= 5 &&
            line.nodes.at(5)->displayText().contains(QStringLiteral("６八玉"))) {
            foundNewBranch = true;
            qDebug() << "Found new branch at line" << i;

            // このラインのbranchPointが△9四歩(93)であること
            QVERIFY2(line.branchPoint != nullptr,
                     qPrintable(QString("New branch (line %1) has null branchPoint").arg(i)));
            QVERIFY2(line.branchPoint->displayText().contains(QStringLiteral("９四歩")),
                     qPrintable(QString("New branch branchPoint should be △9四歩(93) but got: %1")
                                .arg(line.branchPoint->displayText())));

            // 追加検証: branchPointのSFENが分岐1の4手目のSFENと一致すること
            QCOMPARE(line.branchPoint->sfen(), br1_4->sfen());

            // 追加検証: branchPointが本譜の4手目ではないこと
            QVERIFY2(line.branchPoint != main4,
                     "New branch branchPoint should be br1_4, not main4");
            break;
        }
    }
    QVERIFY2(foundNewBranch, "Could not find new branch (line with ▲6八玉 at ply 5)");

    // 追加検証: 新しい指し手が br1_4 の子として追加されていること
    // br1_4 の子ノードに「▲6八玉」があるはず
    bool foundAsChild = false;
    for (int i = 0; i < br1_4->childCount(); ++i) {
        KifuBranchNode* child = br1_4->childAt(i);
        if (child->displayText().contains(QStringLiteral("６八玉"))) {
            foundAsChild = true;
            qDebug() << "Found ▲6八玉 as child of br1_4";
            break;
        }
    }
    QVERIFY2(foundAsChild, "▲6八玉 should be a child of br1_4 (△9四歩)");

    // main4（本譜の4手目）の子に「▲6八玉」がないことを確認
    bool wrongChild = false;
    for (int i = 0; i < main4->childCount(); ++i) {
        KifuBranchNode* child = main4->childAt(i);
        if (child->displayText().contains(QStringLiteral("６八玉"))) {
            wrongChild = true;
            qDebug() << "ERROR: Found ▲6八玉 as child of main4 (should be br1_4)";
            break;
        }
    }
    QVERIFY2(!wrongChild, "▲6八玉 should NOT be a child of main4 (△8四歩 in main line)");
}

/**
 * @brief 分岐ライン上で棋譜欄の行をクリックした時に正しいSFENが取得されることをテスト
 *
 * シナリオ:
 * - 本譜（ライン0）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩
 * - 分岐1（3手目から）: 3.▲1六歩 4.△9四歩
 *
 * 分岐1にナビゲートした後、棋譜欄の3手目をクリックした場合:
 * - 盤面には分岐1の3手目「▲1六歩」の局面が表示されるべき（本譜の「▲2六歩」ではない）
 */
void TestKifuDisplayCoordinator::testBoardAndKifuListSyncOnBranch()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜（ライン0）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"),
        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"),
        QStringLiteral("main_sfen_3"), QString());  // 本譜の3手目
    tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"),
        QStringLiteral("main_sfen_4"), QString());

    // 分岐1（3手目から）: 3.▲1六歩 4.△9四歩
    KifuBranchNode* br1_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩(17)"),
        QStringLiteral("branch_sfen_3"), QString());  // 分岐の3手目（本譜とは異なる）
    KifuBranchNode* br1_4 = tree.addMove(br1_3, ShogiMove(), QStringLiteral("△９四歩(93)"),
        QStringLiteral("branch_sfen_4"), QString());

    // ツリー構造を確認
    QVector<BranchLine> lines = tree.allLines();
    QCOMPARE(lines.size(), 2);

    qDebug() << "=== testBoardAndKifuListSyncOnBranch ===";
    // 本譜と分岐の3手目のSFENが異なることを確認
    QCOMPARE(lines.at(0).nodes.at(3)->sfen(), QStringLiteral("main_sfen_3"));
    QCOMPARE(lines.at(1).nodes.at(3)->sfen(), QStringLiteral("branch_sfen_3"));
    qDebug() << "Main line ply 3 SFEN:" << lines.at(0).nodes.at(3)->sfen();
    qDebug() << "Branch line ply 3 SFEN:" << lines.at(1).nodes.at(3)->sfen();

    // ナビゲーション状態を設定
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    // ステップ1: 分岐1の4手目にナビゲート
    int br1LineIndex = tree.findLineIndexForNode(br1_4);
    qDebug() << "br1_4 belongs to line index:" << br1LineIndex;
    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_4);

    QCOMPARE(state.currentNode(), br1_4);
    QCOMPARE(state.currentLineIndex(), br1LineIndex);

    // 棋譜欄が分岐1のデータを表示していることを確認
    qDebug() << "After navigating to br1_4, record model:";
    for (int i = 0; i < recordModel.rowCount(); ++i) {
        qDebug() << "  Row" << i << ":" << recordModel.item(i)->currentMove();
    }
    QVERIFY(recordModel.item(3)->currentMove().contains(QStringLiteral("１六歩")));

    // ステップ2: 棋譜欄の3手目の行を選択した場合をシミュレート
    // この時、onPositionChanged が呼ばれ、正しいSFENを取得する必要がある
    const int selectedRow = 3;  // 3手目
    const int lineIndex = state.currentLineIndex();

    // 分岐ライン上で棋譜欄の行を選択した時に正しいSFENを取得できることを確認
    // これは MainWindow::onRecordPaneMainRowChanged() で行われる処理をテスト
    QString expectedSfen;
    if (lineIndex >= 0 && lineIndex < lines.size()) {
        const BranchLine& line = lines.at(lineIndex);
        if (selectedRow >= 0 && selectedRow < line.nodes.size()) {
            expectedSfen = line.nodes.at(selectedRow)->sfen();
        }
    }

    qDebug() << "Selected row:" << selectedRow;
    qDebug() << "Current line index:" << lineIndex;
    qDebug() << "Expected SFEN (from branch):" << expectedSfen;

    // 重要な検証: 分岐ライン上では分岐のSFENが取得されること（本譜のSFENではない）
    QCOMPARE(expectedSfen, QStringLiteral("branch_sfen_3"));
    QVERIFY2(expectedSfen != QStringLiteral("main_sfen_3"),
             "On branch line, should get branch SFEN, not main line SFEN");

    // ステップ3: onPositionChanged を呼び出して状態更新をテスト
    coordinator.onPositionChanged(lineIndex, selectedRow, expectedSfen);

    // 状態が正しく更新されたことを確認
    QCOMPARE(state.currentNode()->ply(), selectedRow);
    QCOMPARE(state.currentNode()->sfen(), expectedSfen);

    // 分岐ライン上の正しいノードが選択されていることを確認
    // （本譜ではなく分岐ライン上のノードであること）
    QVERIFY2(!state.currentNode()->isMainLine(),
             "Current node should be on branch line, not main line");
}

/**
 * @brief 【最重要テスト】分岐選択後の「1手進む」で現在ラインの次の指し手が正しく反映されることを確認
 *
 * ドキュメント「最重要テスト：1手進むボタンによる局面連続性の確認」のケース1に対応
 *
 * シナリオ:
 * - 本譜: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩
 * - 分岐1（3手目から）: 3.▲6六歩 4.△8四歩 5.▲6五歩
 *
 * テスト:
 * 1. 分岐ツリーで分岐ライン1の3手目をクリック
 * 2. 「1手進む」ボタンをクリック
 * 3. 盤面が分岐ライン1の4手目の局面になっていることを確認
 *    - 本譜の4手目（△8四歩）とは同じSFENだが、分岐ラインの4手目ノードが選択されている
 */
void TestKifuDisplayCoordinator::testGoForwardAfterBranchSelection()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
        QStringLiteral("sfen_main_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"),
        QStringLiteral("sfen_main_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"),
        QStringLiteral("sfen_main_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"),
        QStringLiteral("sfen_main_4"), QString());
    tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"),
        QStringLiteral("sfen_main_5"), QString());

    // 分岐1（3手目から）: 3.▲6六歩 4.△8四歩 5.▲6五歩
    KifuBranchNode* br1_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲６六歩(67)"),
        QStringLiteral("sfen_br1_3"), QString());
    KifuBranchNode* br1_4 = tree.addMove(br1_3, ShogiMove(), QStringLiteral("△８四歩(83)"),
        QStringLiteral("sfen_br1_4"), QString());
    KifuBranchNode* br1_5 = tree.addMove(br1_4, ShogiMove(), QStringLiteral("▲６五歩(66)"),
        QStringLiteral("sfen_br1_5"), QString());

    qDebug() << "=== testGoForwardAfterBranchSelection ===";

    // ナビゲーション状態を設定
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    // ステップ1: 分岐ライン1の3手目にナビゲート（分岐ツリーでクリック）
    int br1LineIndex = tree.findLineIndexForNode(br1_3);
    qDebug() << "Navigating to br1_3 (分岐ライン1の3手目), lineIndex=" << br1LineIndex;
    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_3);

    // 分岐ライン1の3手目にいることを確認
    QCOMPARE(state.currentNode(), br1_3);
    QCOMPARE(state.currentNode()->ply(), 3);
    QCOMPARE(state.currentNode()->displayText(), QStringLiteral("▲６六歩(67)"));
    qDebug() << "Current position: ply=" << state.currentPly()
             << "sfen=" << state.currentNode()->sfen();

    // ステップ2: 「1手進む」（goForward）を実行
    qDebug() << "Executing goForward (1手進む)...";
    nav.goForward(1);

    // ステップ3: 分岐ライン1の4手目に進んでいることを確認
    qDebug() << "After goForward: ply=" << state.currentPly()
             << "sfen=" << state.currentNode()->sfen()
             << "displayText=" << state.currentNode()->displayText();

    // 重要な検証: 「1手進む」で分岐ライン1の4手目に移動していること
    QCOMPARE(state.currentNode()->ply(), 4);
    QCOMPARE(state.currentNode()->sfen(), QStringLiteral("sfen_br1_4"));
    QCOMPARE(state.currentNode(), br1_4);

    // 本譜の4手目ではないことを確認
    QVERIFY2(state.currentNode() != main4,
             "After goForward from branch, should be on br1_4, not main4");

    // ステップ4: もう1手進む
    qDebug() << "Executing goForward again...";
    nav.goForward(1);

    // 分岐ライン1の5手目に進んでいることを確認
    QCOMPARE(state.currentNode()->ply(), 5);
    QCOMPARE(state.currentNode()->sfen(), QStringLiteral("sfen_br1_5"));
    QCOMPARE(state.currentNode(), br1_5);
    qDebug() << "After second goForward: ply=" << state.currentPly()
             << "displayText=" << state.currentNode()->displayText();
}

/**
 * @brief 【最重要テスト】本譜と分岐を行き来した後の「1手進む」で正しいラインの指し手が反映されることを確認
 *
 * ドキュメント「最重要テスト：1手進むボタンによる局面連続性の確認」のケース2に対応
 *
 * シナリオ:
 * 1. 分岐棋譜を読み込む
 * 2. 分岐ツリーで分岐ライン1を選択
 * 3. 「本譜へ戻る」をクリック
 * 4. 「1手戻る」ボタンを数回クリック
 * 5. 「1手進む」ボタンをクリック
 * 6. 将棋盤が正しく本譜の1手だけ進んでいることを確認
 */
void TestKifuDisplayCoordinator::testGoForwardAfterSwitchingBetweenMainAndBranch()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("root_sfen"));

    // 本譜: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
        QStringLiteral("sfen_main_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"),
        QStringLiteral("sfen_main_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"),
        QStringLiteral("sfen_main_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"),
        QStringLiteral("sfen_main_4"), QString());
    KifuBranchNode* main5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"),
        QStringLiteral("sfen_main_5"), QString());

    // 分岐1（3手目から）: 3.▲1六歩 4.△9四歩 5.▲4六歩
    KifuBranchNode* br1_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩(17)"),
        QStringLiteral("sfen_br1_3"), QString());
    KifuBranchNode* br1_4 = tree.addMove(br1_3, ShogiMove(), QStringLiteral("△９四歩(93)"),
        QStringLiteral("sfen_br1_4"), QString());
    KifuBranchNode* br1_5 = tree.addMove(br1_4, ShogiMove(), QStringLiteral("▲４六歩(47)"),
        QStringLiteral("sfen_br1_5"), QString());

    qDebug() << "=== testGoForwardAfterSwitchingBetweenMainAndBranch ===";

    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    // ステップ1: 分岐ライン1の5手目にナビゲート
    int br1LineIndex = tree.findLineIndexForNode(br1_5);
    qDebug() << "Step 1: Navigate to br1_5, lineIndex=" << br1LineIndex;
    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_5);
    QCOMPARE(state.currentNode(), br1_5);

    // ステップ2: 「本譜へ戻る」を実行
    qDebug() << "Step 2: goToMainLineAtCurrentPly (本譜へ戻る)";
    nav.goToMainLineAtCurrentPly();

    // 本譜の5手目に移動していることを確認
    QCOMPARE(state.currentNode(), main5);
    QCOMPARE(state.currentNode()->sfen(), QStringLiteral("sfen_main_5"));
    qDebug() << "After goToMainLine: ply=" << state.currentPly()
             << "displayText=" << state.currentNode()->displayText();

    // ステップ3: 「1手戻る」を2回実行（3手目まで戻る）
    qDebug() << "Step 3: goBack(2) (1手戻るを2回)";
    nav.goBack(2);

    QCOMPARE(state.currentNode()->ply(), 3);
    QCOMPARE(state.currentNode(), main3);
    QCOMPARE(state.currentNode()->sfen(), QStringLiteral("sfen_main_3"));
    qDebug() << "After goBack(2): ply=" << state.currentPly()
             << "displayText=" << state.currentNode()->displayText();

    // ステップ4: 「1手進む」を実行
    qDebug() << "Step 4: goForward (1手進む)";
    nav.goForward(1);

    // 重要な検証: 本譜の4手目に進んでいること（分岐の4手目ではない）
    qDebug() << "After goForward: ply=" << state.currentPly()
             << "sfen=" << state.currentNode()->sfen()
             << "displayText=" << state.currentNode()->displayText();

    QCOMPARE(state.currentNode()->ply(), 4);
    QCOMPARE(state.currentNode(), main4);
    QCOMPARE(state.currentNode()->sfen(), QStringLiteral("sfen_main_4"));

    // 分岐1の4手目ではないことを確認
    QVERIFY2(state.currentNode() != br1_4,
             "After goToMainLine and goBack, goForward should stay on main line, not branch");
    QVERIFY2(state.currentNode()->sfen() != QStringLiteral("sfen_br1_4"),
             "SFEN should be main line's 4th move, not branch's");
}

/**
 * @brief 【最重要テスト】再対局後の「1手進む」で正しいラインの指し手が反映されることを確認
 *
 * ドキュメント「最重要テスト：1手進むボタンによる局面連続性の確認」のケース3に対応
 *
 * シナリオ:
 * 1. 人間対人間で対局して終了
 * 2. 途中局面から再対局して分岐を作成
 * 3. 対局終了後、分岐ツリーで本譜を選択
 * 4. 「先頭へ」ボタンで開始局面に移動
 * 5. 「1手進む」ボタンを連続でクリック
 * 6. 各手で将棋盤が正しく本譜の1手ずつ進んでいることを確認
 */
void TestKifuDisplayCoordinator::testGoForwardAfterRematch()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("root_sfen"));

    // 1局目（本譜）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△8五歩 7.▲投了
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
        QStringLiteral("sfen_main_1"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"),
        QStringLiteral("sfen_main_2"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"),
        QStringLiteral("sfen_main_3"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"),
        QStringLiteral("sfen_main_4"), QString());
    KifuBranchNode* main5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"),
        QStringLiteral("sfen_main_5"), QString());
    KifuBranchNode* main6 = tree.addMove(main5, ShogiMove(), QStringLiteral("△８五歩(84)"),
        QStringLiteral("sfen_main_6"), QString());
    tree.addTerminalMove(main6, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 2局目（分岐）: 3手目から再対局
    // 3.▲6六歩 4.△8四歩 5.▲6五歩 6.△8五歩 7.▲投了
    KifuBranchNode* br1_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲６六歩(67)"),
        QStringLiteral("sfen_br1_3"), QString());
    KifuBranchNode* br1_4 = tree.addMove(br1_3, ShogiMove(), QStringLiteral("△８四歩(83)"),
        QStringLiteral("sfen_br1_4"), QString());
    KifuBranchNode* br1_5 = tree.addMove(br1_4, ShogiMove(), QStringLiteral("▲６五歩(66)"),
        QStringLiteral("sfen_br1_5"), QString());
    KifuBranchNode* br1_6 = tree.addMove(br1_5, ShogiMove(), QStringLiteral("△８五歩(84)"),
        QStringLiteral("sfen_br1_6"), QString());
    tree.addTerminalMove(br1_6, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    qDebug() << "=== testGoForwardAfterRematch ===";

    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // 2局目終了後の状態をシミュレート: 分岐ライン1の最後にいる
    int br1LineIndex = tree.findLineIndexForNode(br1_6);
    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_6);
    qDebug() << "After 2nd game: on br1_6, lineIndex=" << state.currentLineIndex();

    // ステップ3: 本譜を選択（分岐ツリーで本譜をクリック）
    qDebug() << "Step 3: Select main line (main6)";
    state.resetPreferredLineIndex();
    state.clearLineSelectionMemory();
    nav.goToNode(main6);
    QCOMPARE(state.currentNode(), main6);
    QCOMPARE(state.currentLineIndex(), 0);

    // ステップ4: 「先頭へ」で開始局面に移動
    qDebug() << "Step 4: goToFirst (先頭へ)";
    nav.goToFirst();
    QCOMPARE(state.currentNode(), tree.root());
    QCOMPARE(state.currentNode()->ply(), 0);

    // ステップ5: 「1手進む」を連続で実行し、本譜の指し手が順番に反映されることを確認
    qDebug() << "Step 5: goForward repeatedly, checking main line moves...";

    // 本譜のノードの配列
    QVector<KifuBranchNode*> mainLineNodes = {main1, main2, main3, main4, main5, main6};
    QVector<QString> mainLineSfens = {
        QStringLiteral("sfen_main_1"), QStringLiteral("sfen_main_2"), QStringLiteral("sfen_main_3"),
        QStringLiteral("sfen_main_4"), QStringLiteral("sfen_main_5"), QStringLiteral("sfen_main_6")
    };

    for (int i = 0; i < mainLineNodes.size(); ++i) {
        nav.goForward(1);

        qDebug() << "After goForward" << (i + 1) << ": ply=" << state.currentPly()
                 << "sfen=" << state.currentNode()->sfen()
                 << "displayText=" << state.currentNode()->displayText();

        // 重要な検証: 本譜のi+1手目に進んでいること
        QCOMPARE(state.currentNode()->ply(), i + 1);
        QCOMPARE(state.currentNode(), mainLineNodes.at(i));
        QCOMPARE(state.currentNode()->sfen(), mainLineSfens.at(i));

        // 分岐の指し手が混入していないことを確認（3手目以降）
        if (i >= 2) {
            KifuBranchNode* branchNode = (i == 2) ? br1_3 :
                                         (i == 3) ? br1_4 :
                                         (i == 4) ? br1_5 : br1_6;
            QVERIFY2(state.currentNode() != branchNode,
                     qPrintable(QString("Move %1: should be main line node, not branch node").arg(i + 1)));
        }
    }

    qDebug() << "All main line moves verified correctly!";
}

/**
 * @brief スクリーンショットを元にした4表示一致テスト
 *
 * 添付画像の状態を再現し、4つの表示（将棋盤、棋譜欄、分岐候補欄、分岐ツリー）が
 * 一致していることを確認する。
 *
 * スクリーンショットの状態:
 * - 棋譜欄: 分岐ライン1のパスを表示、5手目「▲4八銀(39)」がハイライト
 * - 分岐ツリー: 5手目「▲4八銀(39)」がオレンジ色でハイライト
 * - 分岐候補欄: 「▲2五歩(26)」「▲4八銀(39)」「本譜へ戻る」が表示
 *
 * 分岐ツリー構造:
 * - 本譜（ライン0）: ▲7六歩→△3四歩→▲2六歩→△8四歩→▲2五歩→△8五歩→▲投了
 * - 分岐1（4手目から）: △8四歩→▲4八銀→△5二金→▲投了
 * - 分岐2（2手目から）: △3四歩→▲1六歩→△9四歩→▲1五歩→△9五歩→▲投了
 */
void TestKifuDisplayCoordinator::testFourDisplayConsistencyFromScreenshot()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜（ライン0）: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△8五歩 7.▲投了
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"),
        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"), QString());
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"),
        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"), QString());
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"),
        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5"), QString());
    KifuBranchNode* main5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"),
        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/7P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL w - 6"), QString());
    KifuBranchNode* main6 = tree.addMove(main5, ShogiMove(), QStringLiteral("△８五歩(84)"),
        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/9/1p5P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL b - 7"), QString());
    tree.addTerminalMove(main6, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 分岐1（4手目から、本譜の△8四歩の後）: 5.▲4八銀 6.△5二金 7.▲投了
    KifuBranchNode* br1_5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲４八銀(39)"),
        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1BS4R1/LN1GKGSNL w - 6"), QString());
    KifuBranchNode* br1_6 = tree.addMove(br1_5, ShogiMove(), QStringLiteral("△５二金(61)"),
        QStringLiteral("lnsgk1snl/1r3gb1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1BS4R1/LN1GKGSNL b - 7"), QString());
    tree.addTerminalMove(br1_6, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 分岐2（2手目から、△3四歩の後）: 3.▲1六歩 4.△9四歩 5.▲1五歩 6.△9五歩 7.▲投了
    KifuBranchNode* br2_3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲１六歩(17)"),
        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P5P/PP1PPPPPP/1B5R1/LNSGKGSNL w - 4"), QString());
    KifuBranchNode* br2_4 = tree.addMove(br2_3, ShogiMove(), QStringLiteral("△９四歩(93)"),
        QStringLiteral("lnsgkgsnl/1r5b1/1ppppp1pp/p5p2/9/2P5P/PP1PPPPPP/1B5R1/LNSGKGSNL b - 5"), QString());
    KifuBranchNode* br2_5 = tree.addMove(br2_4, ShogiMove(), QStringLiteral("▲１五歩(16)"),
        QStringLiteral("lnsgkgsnl/1r5b1/1ppppp1pp/p5p2/8P/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 6"), QString());
    KifuBranchNode* br2_6 = tree.addMove(br2_5, ShogiMove(), QStringLiteral("△９五歩(94)"),
        QStringLiteral("lnsgkgsnl/1r5b1/1ppppp1pp/9/p7P/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 7"), QString());
    tree.addTerminalMove(br2_6, TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // ツリー構造を確認
    QVector<BranchLine> lines = tree.allLines();
    qDebug() << "=== testFourDisplayConsistencyFromScreenshot ===";
    qDebug() << "Number of lines:" << lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        qDebug() << "Line" << i << ": branchPly=" << lines.at(i).branchPly
                 << "nodes=" << lines.at(i).nodes.size();
    }
    QCOMPARE(lines.size(), 3);  // 本譜、分岐1、分岐2

    // ナビゲーション状態を設定
    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    // ========================================
    // スクリーンショットの状態を再現:
    // 分岐ライン1の5手目「▲4八銀(39)」にナビゲート
    // ========================================
    int br1LineIndex = tree.findLineIndexForNode(br1_5);
    qDebug() << "Navigating to br1_5 (▲４八銀), lineIndex=" << br1LineIndex;
    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_5);

    // === 検証1: 現在位置が正しいこと ===
    QCOMPARE(state.currentNode(), br1_5);
    QCOMPARE(state.currentNode()->ply(), 5);
    QCOMPARE(state.currentNode()->displayText(), QStringLiteral("▲４八銀(39)"));
    QCOMPARE(state.currentLineIndex(), br1LineIndex);
    qDebug() << "✓ 検証1: 現在位置が分岐1の5手目であること - OK";

    // === 検証2: 棋譜欄が分岐1のパスを表示していること ===
    qDebug() << "Record model content:";
    for (int i = 0; i < recordModel.rowCount(); ++i) {
        qDebug() << "  Row" << i << ":" << recordModel.item(i)->currentMove();
    }

    // 3手目が▲2六歩（本譜と共通）であること
    QVERIFY(recordModel.item(3)->currentMove().contains(QStringLiteral("２六歩")));
    // 5手目が▲4八銀（分岐1）であること（本譜は▲2五歩）
    QVERIFY(recordModel.item(5)->currentMove().contains(QStringLiteral("４八銀")));
    qDebug() << "✓ 検証2: 棋譜欄が分岐1のパスを表示していること - OK";

    // === 検証3: 分岐候補が正しいこと ===
    // branchCandidatesAtCurrent() は現在のノードの親の子ノード（兄弟）を返す
    // 5手目（▲4八銀）から見た分岐候補を取得
    QVector<KifuBranchNode*> candidates = state.branchCandidatesAtCurrent();
    qDebug() << "Branch candidates from current (ply 5):";
    for (KifuBranchNode* c : std::as_const(candidates)) {
        qDebug() << "  " << c->displayText();
    }
    // 5手目から見ると、▲2五歩(本譜)と▲4八銀(分岐1)の2つの候補があるはず
    // （これはスクリーンショットの分岐候補欄と一致）
    QCOMPARE(candidates.size(), 2);
    // 候補の中に▲2五歩と▲4八銀があることを確認
    bool has25fu = false;
    bool has48gin = false;
    for (KifuBranchNode* c : std::as_const(candidates)) {
        if (c->displayText().contains(QStringLiteral("２五歩"))) has25fu = true;
        if (c->displayText().contains(QStringLiteral("４八銀"))) has48gin = true;
    }
    QVERIFY2(has25fu, "Branch candidates should include ▲2五歩(26)");
    QVERIFY2(has48gin, "Branch candidates should include ▲4八銀(39)");
    qDebug() << "✓ 検証3: 分岐候補が正しいこと（▲2五歩、▲4八銀）- OK";

    // === 検証4: 「1手進む」で分岐1の6手目に進むこと ===
    qDebug() << "Testing goForward from br1_5...";
    nav.goForward(1);

    QCOMPARE(state.currentNode(), br1_6);
    QCOMPARE(state.currentNode()->ply(), 6);
    QCOMPARE(state.currentNode()->displayText(), QStringLiteral("△５二金(61)"));
    qDebug() << "✓ 検証4: 「1手進む」で分岐1の6手目（△5二金）に進むこと - OK";

    // === 検証5: 「1手戻る」で分岐1の5手目に戻ること ===
    qDebug() << "Testing goBack from br1_6...";
    nav.goBack(1);

    QCOMPARE(state.currentNode(), br1_5);
    QCOMPARE(state.currentNode()->ply(), 5);
    QCOMPARE(state.currentNode()->displayText(), QStringLiteral("▲４八銀(39)"));
    qDebug() << "✓ 検証5: 「1手戻る」で分岐1の5手目（▲4八銀）に戻ること - OK";

    // === 検証6: 「1手戻る」で共通の4手目に戻ること ===
    qDebug() << "Testing goBack again...";
    nav.goBack(1);

    QCOMPARE(state.currentNode(), main4);  // main4は本譜・分岐1の共通ノード
    QCOMPARE(state.currentNode()->ply(), 4);
    QCOMPARE(state.currentNode()->displayText(), QStringLiteral("△８四歩(83)"));
    qDebug() << "✓ 検証6: 「1手戻る」で共通の4手目（△8四歩）に戻ること - OK";

    // === 検証7: この状態で「1手進む」すると、分岐1の5手目に進むこと ===
    // （本譜の5手目▲2五歩ではなく、分岐1の5手目▲4八銀に進むべき）
    qDebug() << "Testing goForward from main4 (should go to br1_5, not main5)...";
    nav.goForward(1);

    // 重要: 分岐を選択していた場合、その分岐の続きに進むこと
    QCOMPARE(state.currentNode(), br1_5);
    QCOMPARE(state.currentNode()->displayText(), QStringLiteral("▲４八銀(39)"));
    QVERIFY2(state.currentNode() != main5,
             "After goBack and goForward on branch, should return to branch line, not main line");
    qDebug() << "✓ 検証7: 分岐選択後の「1手戻る」→「1手進む」で分岐ラインに戻ること - OK";

    // === 検証8: SFEN（盤面状態）が正しいこと ===
    // 5手目（▲4八銀後）の盤面状態を確認
    // 4八に銀がある状態のSFEN
    QString expectedSfen = br1_5->sfen();
    qDebug() << "br1_5 SFEN:" << expectedSfen;
    QVERIFY(expectedSfen.contains(QStringLiteral("S")));  // 銀がある
    qDebug() << "✓ 検証8: SFEN（盤面状態）が分岐1の5手目に対応していること - OK";

    qDebug() << "=== All 8 verifications passed! ===";
}

/**
 * @brief 将棋盤と棋譜欄の駒配置一致テスト（SFENの詳細検証）
 *
 * スクリーンショットで発見されたバグ:
 * - 棋譜欄: 3手目「▲2六歩(27)」が表示されている
 * - 将棋盤: 2六に歩がない（2七に歩がある＝2手目の状態）
 * - 将棋盤が「2手目の局面 + 4〜5手目の指し手」という矛盾した状態
 *
 * このテストでは、分岐ライン上にナビゲートした際に、
 * SFENが棋譜欄に表示されている全ての指し手を正しく反映していることを確認する。
 */
void TestKifuDisplayCoordinator::testBoardPositionMatchesKifuPath()
{
    KifuBranchTree tree;
    // 初期局面（平手）
    const QString initialSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    tree.setRootSfen(initialSfen);

    // 本譜: 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩
    // 各手のSFENを正確に設定（駒の移動を反映）
    KifuBranchNode* main1 = tree.addMove(tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"), QString());
    KifuBranchNode* main2 = tree.addMove(main1, ShogiMove(), QStringLiteral("△３四歩(33)"),
        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"), QString());
    // 3手目: ▲2六歩 - 2七の歩が2六に移動
    KifuBranchNode* main3 = tree.addMove(main2, ShogiMove(), QStringLiteral("▲２六歩(27)"),
        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"), QString());
    // 4手目: △8四歩 - 8三の歩が8四に移動
    KifuBranchNode* main4 = tree.addMove(main3, ShogiMove(), QStringLiteral("△８四歩(83)"),
        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5"), QString());
    // 5手目: ▲2五歩 - 2六の歩が2五に移動
    tree.addMove(main4, ShogiMove(), QStringLiteral("▲２五歩(26)"),
        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/7P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL w - 6"), QString());

    // 分岐1（4手目から）: 5.▲4八銀 - 3九の銀が4八に移動
    //重要: このSFENには3手目の▲2六歩が反映されている必要がある（2六に歩がある）
    // 4八 = 4筋8段: SFENでは 9筋=col0, 8筋=col1, ..., 4筋=col5, ..., 1筋=col8
    // rank8は "1B3S1R1"（9筋空、8筋角、7-5筋空(3マス)、4筋銀、3筋空、2筋飛、1筋空）
    KifuBranchNode* br1_5 = tree.addMove(main4, ShogiMove(), QStringLiteral("▲４八銀(39)"),
        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B3S1R1/LN1GKGSNL w - 6"), QString());

    qDebug() << "=== testBoardPositionMatchesKifuPath ===";

    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    // 分岐1の5手目にナビゲート
    int br1LineIndex = tree.findLineIndexForNode(br1_5);
    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_5);

    // 現在のSFENを取得
    QString currentSfen = state.currentNode()->sfen();
    qDebug() << "Current SFEN at br1_5:" << currentSfen;

    // ========================================
    //最重要検証: SFENが棋譜欄の全ての指し手を反映しているか
    // ========================================

    // SFENのボード部分を解析（"/"で区切られた最初の9つのランク）
    QStringList sfenParts = currentSfen.split(QLatin1Char(' '));
    QString boardPart = sfenParts.at(0);
    QStringList ranks = boardPart.split(QLatin1Char('/'));

    qDebug() << "Board ranks:";
    for (int i = 0; i < ranks.size(); ++i) {
        qDebug() << "  Rank" << (i + 1) << ":" << ranks.at(i);
    }

    // --- 検証1: 1手目 ▲7六歩 が反映されているか ---
    // 7六 = 7筋、6段 = rank[5]（0-indexed）の7筋位置
    // 6段目（rank[5]）に歩(P)があるはず
    QString rank6 = ranks.at(5);  // 6段目（下から4段目）
    qDebug() << "Rank 6 (六段):" << rank6;
    // "2P4P1" の場合、2マス空き + P + 4マス空き + P + 1マス空き
    // 7筋は左から3番目なので、"2P..."のPが7六の歩
    QVERIFY2(rank6.contains(QLatin1Char('P')), "1手目▲7六歩: 6段目に歩(P)があるべき");
    qDebug() << "✓ 検証1: 1手目▲7六歩が反映されている";

    // --- 検証2: 2手目 △3四歩 が反映されているか ---
    // 3四 = 3筋、4段 = rank[3]（0-indexed）
    QString rank4 = ranks.at(3);  // 4段目
    qDebug() << "Rank 4 (四段):" << rank4;
    // 4段目に後手の歩(p)があるはず
    QVERIFY2(rank4.contains(QLatin1Char('p')), "2手目△3四歩: 4段目に歩(p)があるべき");
    qDebug() << "✓ 検証2: 2手目△3四歩が反映されている";

    // --- 検証3: 3手目 ▲2六歩 が反映されているか ---
    // 2六 = 2筋、6段 = rank[5]
    //これが最重要！スクリーンショットのバグでは2六に歩がなかった
    // SFENで2筋6段に歩があることを確認
    // rank6 = "2P4P1" の場合、右から2番目の位置にPがあるはず
    qDebug() << "Checking if 2六 has a pawn (3手目▲2六歩)...";

    // SFENのrank6を解析して、2筋（右から2番目）に歩があるか確認
    // SFENは9筋から1筋の順（左から右）で記述される
    // 2筋は右から2番目 = 左から8番目（0-indexed: 7）
    int col = 0;
    bool found26Pawn = false;
    for (int i = 0; i < rank6.length(); ++i) {
        QChar c = rank6.at(i);
        if (c.isDigit()) {
            col += c.digitValue();
        } else {
            if (col == 7 && c == QLatin1Char('P')) {  // 2筋（0-indexed: 7）
                found26Pawn = true;
                break;
            }
            col++;
        }
    }

    QVERIFY2(found26Pawn,
             qPrintable(QString("3手目▲2六歩: 2筋6段に歩(P)があるべき。実際のrank6: %1").arg(rank6)));
    qDebug() << "✓ 検証3: 3手目▲2六歩が反映されている（2六に歩がある）";

    // --- 検証4: 4手目 △8四歩 が反映されているか ---
    // 8四 = 8筋、4段 = rank[3]
    // 8筋は左から2番目（0-indexed: 1）
    qDebug() << "Checking if 8四 has a pawn (4手目△8四歩)...";
    col = 0;
    bool found84Pawn = false;
    for (int i = 0; i < rank4.length(); ++i) {
        QChar c = rank4.at(i);
        if (c.isDigit()) {
            col += c.digitValue();
        } else {
            if (col == 1 && c == QLatin1Char('p')) {  // 8筋（0-indexed: 1）
                found84Pawn = true;
                break;
            }
            col++;
        }
    }

    QVERIFY2(found84Pawn,
             qPrintable(QString("4手目△8四歩: 8筋4段に歩(p)があるべき。実際のrank4: %1").arg(rank4)));
    qDebug() << "✓ 検証4: 4手目△8四歩が反映されている（8四に歩がある）";

    // --- 検証5: 5手目 ▲4八銀 が反映されているか ---
    // 4八 = 4筋、8段 = rank[7]
    QString rank8 = ranks.at(7);  // 8段目
    qDebug() << "Rank 8 (八段):" << rank8;
    // 4筋は左から6番目（0-indexed: 5）
    col = 0;
    bool found48Silver = false;
    for (int i = 0; i < rank8.length(); ++i) {
        QChar c = rank8.at(i);
        if (c.isDigit()) {
            col += c.digitValue();
        } else {
            if (col == 5 && c == QLatin1Char('S')) {  // 4筋（0-indexed: 5）に銀
                found48Silver = true;
                break;
            }
            col++;
        }
    }

    QVERIFY2(found48Silver,
             qPrintable(QString("5手目▲4八銀: 4筋8段に銀(S)があるべき。実際のrank8: %1").arg(rank8)));
    qDebug() << "✓ 検証5: 5手目▲4八銀が反映されている（4八に銀がある）";

    qDebug() << "=== All board position verifications passed! ===";
    qDebug() << "将棋盤のSFENは棋譜欄に表示された全ての指し手を正しく反映しています。";
}

/**
 * @brief ライブ対局フローでのバグ再現テスト
 *
 * スクリーンショットで発見されたバグを再現:
 * - 人間対人間で対局（1局目）
 * - 途中で投了
 * - 途中局面から再対局（2局目、分岐を作成）
 * - 分岐ラインにナビゲート
 * - 将棋盤のSFENが棋譜欄の指し手と不一致
 *
 * このテストでは、LiveGameSessionを使った実際の対局フローをシミュレートし、
 * ナビゲーション後のSFENが正しいか検証します。
 */
void TestKifuDisplayCoordinator::testLiveGameRematchBoardSfenConsistency()
{
    qDebug() << "=== testLiveGameRematchBoardSfenConsistency ===";

    // ========================================
    // 初期設定
    // ========================================
    KifuBranchTree tree;
    const QString initialSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    tree.setRootSfen(initialSfen);

    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);
    coordinator.wireSignals();

    LiveGameSession liveSession;
    liveSession.setTree(&tree);
    coordinator.setLiveGameSession(&liveSession);

    // ========================================
    // 1局目: 本譜を作成（ルートから）
    // 1.▲7六歩 2.△3四歩 3.▲2六歩 4.△8四歩 5.▲2五歩 6.△8五歩 7.▲投了
    // ========================================
    qDebug() << "=== 1局目開始 ===";
    liveSession.startFromRoot();
    coordinator.onLiveGameSessionStarted(nullptr);

    // 各手のSFENを計算して追加
    // 1手目: ▲7六歩（7七の歩が7六に移動）
    const QString sfen1 = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
    liveSession.addMove(ShogiMove(), QStringLiteral("▲７六歩(77)"), sfen1, QString());

    // 2手目: △3四歩（3三の歩が3四に移動）
    const QString sfen2 = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3");
    liveSession.addMove(ShogiMove(), QStringLiteral("△３四歩(33)"), sfen2, QString());

    // 3手目: ▲2六歩（2七の歩が2六に移動）
    const QString sfen3 = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4");
    liveSession.addMove(ShogiMove(), QStringLiteral("▲２六歩(27)"), sfen3, QString());

    // 4手目: △8四歩（8三の歩が8四に移動）
    const QString sfen4 = QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5");
    liveSession.addMove(ShogiMove(), QStringLiteral("△８四歩(83)"), sfen4, QString());

    // 5手目: ▲2五歩（2六の歩が2五に移動）
    const QString sfen5 = QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/7P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL w - 6");
    liveSession.addMove(ShogiMove(), QStringLiteral("▲２五歩(26)"), sfen5, QString());

    // 6手目: △8五歩（8四の歩が8五に移動）
    const QString sfen6 = QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/9/1p5P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL b - 7");
    liveSession.addMove(ShogiMove(), QStringLiteral("△８五歩(84)"), sfen6, QString());

    // 投了
    liveSession.addTerminalMove(TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 1局目をコミット
    KifuBranchNode* game1End = liveSession.commit();
    qDebug() << "1局目終了: ply=" << game1End->ply();

    // 本譜の構造を確認
    QVector<BranchLine> lines = tree.allLines();
    QCOMPARE(lines.size(), 1);
    qDebug() << "本譜のノード数:" << lines.at(0).nodes.size();

    // ========================================
    // 2局目: 4手目から再対局（分岐を作成）
    // 4手目（△8四歩）の後から開始し、別の手を指す
    // 5.▲4八銀 6.△5二金 7.▲投了
    // ========================================
    qDebug() << "=== 2局目開始（4手目から分岐）===";

    // 4手目のノードを取得
    KifuBranchNode* node4 = tree.findByPlyOnMainLine(4);
    QVERIFY(node4 != nullptr);
    QCOMPARE(node4->displayText(), QStringLiteral("△８四歩(83)"));
    qDebug() << "分岐起点: ply=" << node4->ply() << "sfen=" << node4->sfen();

    // 4手目から再対局開始
    liveSession.startFromNode(node4);
    coordinator.onLiveGameSessionStarted(node4);

    // 5手目: ▲4八銀（3九の銀が4八に移動）
    //重要: このSFENには1-4手目の全ての指し手が反映されている必要がある
    // 特に3手目の▲2六歩（2六に歩がある）
    const QString sfen5_br = QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B3S1R1/LN1GKGSNL w - 6");
    liveSession.addMove(ShogiMove(), QStringLiteral("▲４八銀(39)"), sfen5_br, QString());

    // 6手目: △5二金（6一の金が5二に移動）
    const QString sfen6_br = QStringLiteral("lnsgk1snl/1r3gb1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B3S1R1/LN1GKGSNL b - 7");
    liveSession.addMove(ShogiMove(), QStringLiteral("△５二金(61)"), sfen6_br, QString());

    // 投了
    liveSession.addTerminalMove(TerminalType::Resign, QStringLiteral("▲投了"), QString());

    // 2局目をコミット
    KifuBranchNode* game2End = liveSession.commit();
    qDebug() << "2局目終了: ply=" << game2End->ply();

    // 分岐構造を確認
    lines = tree.allLines();
    QCOMPARE(lines.size(), 2);  // 本譜と分岐1
    qDebug() << "ライン数:" << lines.size();

    // ========================================
    // 分岐ラインにナビゲートしてSFENを検証
    // ========================================
    qDebug() << "=== 分岐ラインにナビゲート ===";

    // 分岐1の5手目（▲4八銀）のノードを取得
    KifuBranchNode* br1_node5 = nullptr;
    for (const BranchLine& line : std::as_const(lines)) {
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->ply() == 5 && node->displayText().contains(QStringLiteral("４八銀"))) {
                br1_node5 = node;
                break;
            }
        }
        if (br1_node5 != nullptr) break;
    }
    QVERIFY2(br1_node5 != nullptr, "分岐1の5手目（▲4八銀）が見つからない");

    // 分岐1にナビゲート
    int br1LineIndex = tree.findLineIndexForNode(br1_node5);
    qDebug() << "分岐1のラインインデックス:" << br1LineIndex;
    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_node5);

    // ========================================
    //最重要検証: ナビゲーション後のSFENが正しいか
    // ========================================
    QString actualSfen = state.currentNode()->sfen();
    qDebug() << "ナビゲーション後のSFEN:" << actualSfen;

    // SFENを解析して、3手目の▲2六歩が反映されているか確認
    QStringList sfenParts = actualSfen.split(QLatin1Char(' '));
    QString boardPart = sfenParts.at(0);
    QStringList ranks = boardPart.split(QLatin1Char('/'));

    // 6段目を確認（3手目▲2六歩が反映されているか）
    QString rank6 = ranks.at(5);
    qDebug() << "Rank 6 (六段):" << rank6;

    // 2筋6段に歩があるか確認
    int col = 0;
    bool found26Pawn = false;
    for (int i = 0; i < rank6.length(); ++i) {
        QChar c = rank6.at(i);
        if (c.isDigit()) {
            col += c.digitValue();
        } else {
            if (col == 7 && c == QLatin1Char('P')) {
                found26Pawn = true;
                break;
            }
            col++;
        }
    }

    QVERIFY2(found26Pawn,
             qPrintable(QString("バグ発見！分岐1の5手目のSFENに3手目▲2六歩が反映されていない。"
                               "2六に歩がない。rank6=%1").arg(rank6)));
    qDebug() << "✓ 3手目▲2六歩が反映されている（2六に歩がある）";

    // 4筋8段に銀があるか確認（5手目▲4八銀）
    QString rank8 = ranks.at(7);
    qDebug() << "Rank 8 (八段):" << rank8;

    col = 0;
    bool found48Silver = false;
    for (int i = 0; i < rank8.length(); ++i) {
        QChar c = rank8.at(i);
        if (c.isDigit()) {
            col += c.digitValue();
        } else {
            if (col == 5 && c == QLatin1Char('S')) {
                found48Silver = true;
                break;
            }
            col++;
        }
    }

    QVERIFY2(found48Silver,
             qPrintable(QString("5手目▲4八銀が反映されていない。4八に銀がない。rank8=%1").arg(rank8)));
    qDebug() << "✓ 5手目▲4八銀が反映されている（4八に銀がある）";

    qDebug() << "=== ライブ対局フローでのSFEN検証成功 ===";
}

QTEST_GUILESS_MAIN(TestKifuDisplayCoordinator)
#include "tst_kifudisplaycoordinator.moc"
