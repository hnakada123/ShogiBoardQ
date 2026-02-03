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

    // ★ ここからが3局目の開始シミュレーション ★

    // 本譜の6手目にナビゲート（ユーザーが分岐ツリーの本譜をクリック）
    state.resetPreferredLineIndex();  // 本譜へ移動するのでリセット
    nav.goToNode(main6);

    QCOMPARE(state.currentLineIndex(), 0);  // 本譜にいることを確認
    QCOMPARE(state.currentNode(), main6);

    // 3局目をライブセッション開始（本譜の6手目から）
    qDebug() << "Starting live session from main6, sfen:" << main6->sfen();
    coordinator.onLiveGameSessionStarted(main6);

    // ★ 検証: 棋譜欄が本譜の1-6手目を表示していること ★
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

    // ★ 重要な検証: 棋譜欄が本譜の手を表示していること
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
    // ★注意: 本譜と同じ7手目だが、異なるノードとして追加される
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

    // ★ 重要な検証: 3手目が本譜の▲2六歩であること（分岐1の▲1六歩ではない）
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

    // ★ 検証: 分岐ツリーのライン構造が正しいことを確認
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

    // ★ 重要な検証: 分岐2のbranchPointが依然として△9四歩(93)であること
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

    // ★ 追加検証: ResolvedRowLiteに変換した場合のparent値を確認
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

    // ★ 重要: 分岐1の4手目「△9四歩」にナビゲート
    int br1LineIndex = tree.findLineIndexForNode(br1_4);
    qDebug() << "br1_4 belongs to line index:" << br1LineIndex;

    state.setPreferredLineIndex(br1LineIndex);
    nav.goToNode(br1_4);

    QCOMPARE(state.currentNode(), br1_4);
    QCOMPARE(state.currentLineIndex(), br1LineIndex);

    // ★ 分岐1の4手目からライブセッション開始
    qDebug() << "Starting live session from br1_4 (△９四歩)";
    liveSession.startFromNode(br1_4);

    // 新しい指し手を追加
    // この指し手は br1_4（△9四歩）の子として追加されるべき
    liveSession.addMove(ShogiMove(), QStringLiteral("▲６八玉(59)"), QStringLiteral("new_5_from_br1"), QString());
    liveSession.addMove(ShogiMove(), QStringLiteral("△８四歩(83)"), QStringLiteral("new_6_from_br1"), QString());

    // ★ 検証: 分岐ツリーのライン構造が正しいことを確認
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

    // ★ 重要な検証: 新しい分岐のbranchPointが△9四歩(93)（分岐1の4手目）であること
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

            // ★ 追加検証: branchPointのSFENが分岐1の4手目のSFENと一致すること
            QCOMPARE(line.branchPoint->sfen(), br1_4->sfen());

            // ★ 追加検証: branchPointが本譜の4手目ではないこと
            QVERIFY2(line.branchPoint != main4,
                     "New branch branchPoint should be br1_4, not main4");
            break;
        }
    }
    QVERIFY2(foundNewBranch, "Could not find new branch (line with ▲6八玉 at ply 5)");

    // ★ 追加検証: 新しい指し手が br1_4 の子として追加されていること
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

QTEST_GUILESS_MAIN(TestKifuDisplayCoordinator)
#include "tst_kifudisplaycoordinator.moc"
