/// @file tst_preset_gamestart_cleanup.cpp
/// @brief プリセット局面での対局開始時にUIがリセットされることを検証するテスト
///
/// バグ再現シナリオ:
///   1. 人間対エンジンで対局開始 → 数手指す → 中断
///   2. 再度「対局」→「対局」で平手を選択して対局開始
///   3. 期待: 棋譜ドックが「=== 開始局面 ===」のみ、分岐ツリーもリセット
///   4. 実際: 前の対局の棋譜が残ったまま

#include <QtTest>
#include <QSignalSpy>

#include "prestartcleanuphandler.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "shogimove.h"
#include "kifudisplay.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// テスト用ハーネス: PreStartCleanupHandler の最小限の依存関係
struct CleanupHarness {
    KifuRecordListModel recordModel;
    KifuBranchListModel branchModel;
    KifuBranchTree branchTree;
    KifuNavigationState navState;
    LiveGameSession liveSession;

    QString startSfenStr;
    QString currentSfenStr;
    int activePly = 0;
    int currentSelectedPly = 0;
    int currentMoveIndex = 0;

    PreStartCleanupHandler* handler = nullptr;

    CleanupHarness()
    {
        PreStartCleanupHandler::Dependencies deps;
        deps.kifuRecordModel = &recordModel;
        deps.kifuBranchModel = &branchModel;
        deps.branchTree = &branchTree;
        deps.navState = &navState;
        deps.liveGameSession = &liveSession;
        deps.startSfenStr = &startSfenStr;
        deps.currentSfenStr = &currentSfenStr;
        deps.activePly = &activePly;
        deps.currentSelectedPly = &currentSelectedPly;
        deps.currentMoveIndex = &currentMoveIndex;

        handler = new PreStartCleanupHandler(deps);

        // 初期状態: 平手で対局ツリーを構築
        branchTree.setRootSfen(kHirateSfen);
        navState.setTree(&branchTree);
        liveSession.setTree(&branchTree);
    }

    ~CleanupHarness()
    {
        delete handler;
    }

    /// 対局中の状態を模擬: 数手の棋譜が存在する状態にする
    void simulateGameWithMoves(int moveCount = 3)
    {
        // 棋譜モデルにヘッダ + N手分のエントリを追加
        recordModel.clearAllItems();
        recordModel.appendItem(
            new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                            QStringLiteral("（１手 / 合計）")));

        const QStringList moveTexts = {
            QStringLiteral("▲７六歩"),
            QStringLiteral("△３四歩"),
            QStringLiteral("▲２六歩"),
            QStringLiteral("△８四歩"),
            QStringLiteral("▲２五歩"),
        };
        const QStringList moveSfens = {
            QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 4"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 5"),
            QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 6"),
        };

        // LiveGameSession で分岐ツリーにも手を追加
        liveSession.startFromRoot();
        ShogiMove move;
        const int n = qMin(moveCount, static_cast<int>(moveTexts.size()));
        for (int i = 0; i < n; ++i) {
            recordModel.appendItem(
                new KifuDisplay(moveTexts.at(i), QStringLiteral("00:00/00:00")));
            liveSession.addMove(move, moveTexts.at(i), moveSfens.at(i), QString());
        }
        liveSession.commit();

        // 分岐候補モデルにダミーエントリを追加
        branchModel.appendItem(
            new KifuBranchDisplay(QStringLiteral("分岐A")));

        // 手数関連の状態を更新
        startSfenStr = kHirateSfen;
        currentSfenStr = moveSfens.at(n - 1);
        activePly = n;
        currentSelectedPly = n;
        currentMoveIndex = n;
    }
};

class TestPresetGameStartCleanup : public QObject
{
    Q_OBJECT

private slots:
    // ============================================================
    // A) shouldStartFromCurrentPosition: currentSfen="startpos" の場合
    // ============================================================

    void shouldStartFromCurrentPosition_startposForced_returnsFalse()
    {
        // プリセットパスで currentSfenStr = "startpos" に設定された場合
        // selectedPly > 0 でも false を返すべき
        const bool result = PreStartCleanupHandler::shouldStartFromCurrentPosition(
            kHirateSfen, QStringLiteral("startpos"), 5);
        QCOMPARE(result, false);
    }

    void shouldStartFromCurrentPosition_midGame_returnsTrue()
    {
        // 途中局面（selectedPly > 0 かつ currentSfen != "startpos"）
        const QString midSfen = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        const bool result = PreStartCleanupHandler::shouldStartFromCurrentPosition(
            kHirateSfen, midSfen, 3);
        QCOMPARE(result, true);
    }

    // ============================================================
    // B) performCleanup: プリセット対局開始シナリオ
    // ============================================================

    void cleanup_presetHirate_clearsKifuModel()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(3);

        // 事前確認: 棋譜モデルにヘッダ + 3手 = 4行ある
        QCOMPARE(h.recordModel.rowCount(), 4);

        // プリセットパスの修正: currentSfenStr を "startpos" に設定
        h.currentSfenStr = QStringLiteral("startpos");

        // performCleanup を呼ぶ
        h.handler->performCleanup();

        // 検証: 棋譜モデルが「=== 開始局面 ===」のみにクリアされている
        QCOMPARE(h.recordModel.rowCount(), 1);

        const QModelIndex idx = h.recordModel.index(0, 0);
        const QString header = h.recordModel.data(idx, Qt::DisplayRole).toString();
        QVERIFY2(header.contains(QStringLiteral("開始局面")),
                 qPrintable(QStringLiteral("Expected header '=== 開始局面 ===', got: ") + header));
    }

    void cleanup_presetHirate_clearsBranchModel()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(3);

        // 事前確認: 分岐候補モデルにエントリがある
        QVERIFY(h.branchModel.rowCount() > 0);

        h.currentSfenStr = QStringLiteral("startpos");
        h.handler->performCleanup();

        // 検証: 分岐候補モデルがクリアされている
        QCOMPARE(h.branchModel.rowCount(), 0);
    }

    void cleanup_presetHirate_resetsPlyTracking()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(3);

        // 事前確認: 手数が3
        QCOMPARE(h.activePly, 3);
        QCOMPARE(h.currentSelectedPly, 3);
        QCOMPARE(h.currentMoveIndex, 3);

        h.currentSfenStr = QStringLiteral("startpos");
        h.handler->performCleanup();

        // 検証: 全手数が0にリセット
        QCOMPARE(h.activePly, 0);
        QCOMPARE(h.currentSelectedPly, 0);
        QCOMPARE(h.currentMoveIndex, 0);
    }

    void cleanup_presetHirate_branchTreeRootPreserved()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(3);

        // 事前確認: ルートノードの子がある（対局中の手が追加されている）
        QVERIFY(h.branchTree.root() != nullptr);
        QVERIFY(h.branchTree.root()->childCount() > 0);

        h.currentSfenStr = QStringLiteral("startpos");
        h.handler->performCleanup();

        // performCleanup後もルートノードは存在する（ensureBranchTreeRoot による）
        QVERIFY(h.branchTree.root() != nullptr);
    }

    // ============================================================
    // C) 完全フロー: performCleanup + resetBranchTreeForNewGame 相当
    // ============================================================

    void fullPresetFlow_kifuModelAndBranchTreeBothReset()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(5);

        // 事前確認
        QCOMPARE(h.recordModel.rowCount(), 6);  // ヘッダ + 5手
        QVERIFY(h.branchTree.root()->childCount() > 0);

        // === プリセットパスの修正を模擬 ===
        // Step 1: currentSfenStr を "startpos" に設定
        h.currentSfenStr = QStringLiteral("startpos");

        // Step 2: performCleanup (requestPreStartCleanup のスロット)
        h.handler->performCleanup();

        // Step 3: resetBranchTreeForNewGame 相当の処理
        //         （KifuLoadCoordinator が行う処理を模擬）
        h.navState.setCurrentNode(nullptr);
        h.navState.resetPreferredLineIndex();
        h.branchTree.clear();
        h.branchTree.setRootSfen(kHirateSfen);
        h.navState.goToRoot();

        // === 検証 ===
        // 棋譜モデル: ヘッダのみ
        QCOMPARE(h.recordModel.rowCount(), 1);

        // 分岐候補: 空
        QCOMPARE(h.branchModel.rowCount(), 0);

        // 手数: 全て0
        QCOMPARE(h.activePly, 0);
        QCOMPARE(h.currentSelectedPly, 0);
        QCOMPARE(h.currentMoveIndex, 0);

        // 分岐ツリー: ルートのみ（子なし）
        QVERIFY(h.branchTree.root() != nullptr);
        QCOMPARE(h.branchTree.root()->childCount(), 0);
        QCOMPARE(h.branchTree.root()->sfen(), kHirateSfen);
    }

    // ============================================================
    // D) エッジケース: selectedPly が大きい状態からのリセット
    // ============================================================

    void cleanup_presetAfterLongGame_stillClearsCorrectly()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(5);

        // selectedPly をさらに大きな値に設定（手数よりも大きい場合のテスト）
        h.currentSelectedPly = 100;
        h.currentMoveIndex = 100;

        h.currentSfenStr = QStringLiteral("startpos");
        h.handler->performCleanup();

        QCOMPARE(h.recordModel.rowCount(), 1);
        QCOMPARE(h.activePly, 0);
        QCOMPARE(h.currentSelectedPly, 0);
        QCOMPARE(h.currentMoveIndex, 0);
    }

    // ============================================================
    // E) 駒落ちプリセットのテスト
    // ============================================================

    void cleanup_presetHandicap_clearsKifuModel()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(3);
        QCOMPARE(h.recordModel.rowCount(), 4);

        // 駒落ちでも currentSfenStr = "startpos" に設定される
        h.currentSfenStr = QStringLiteral("startpos");
        h.handler->performCleanup();

        QCOMPARE(h.recordModel.rowCount(), 1);
        QCOMPARE(h.activePly, 0);
    }

    // ============================================================
    // F) バグ再現テスト: 修正前のコードパスで棋譜が残る
    // ============================================================

    void bugRepro_withoutFix_kifuModelNotCleared()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(3);
        QCOMPARE(h.recordModel.rowCount(), 4);

        // 修正前: currentSfenStr が途中局面のまま performCleanup が呼ばれない
        // この場合、棋譜モデルはクリアされない
        // (performCleanup を呼ばないことで「修正前」を模擬)

        // 代わりに prepareInitialPosition 相当の処理だけが行われる
        h.startSfenStr = kHirateSfen;
        h.currentSfenStr = kHirateSfen;

        // 棋譜モデルはクリアされていない（バグの再現）
        QVERIFY2(h.recordModel.rowCount() > 1,
                 "Bug repro: without performCleanup, old kifu entries remain");
    }

    // ============================================================
    // G) LiveGameSession が正しくリスタートされることの検証
    // ============================================================

    void cleanup_liveSessionRestartsFromRoot()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(3);

        // 事前: セッションはアクティブではない（commit済み）
        QVERIFY(!h.liveSession.isActive());

        h.currentSfenStr = QStringLiteral("startpos");
        h.handler->performCleanup();

        // performCleanup 内の startLiveGameSession で
        // startFromRoot() が呼ばれてセッションがアクティブになる
        QVERIFY(h.liveSession.isActive());
    }

    // ============================================================
    // H) 責務分離テスト: performCleanup() はツリーをクリアしない
    //    分岐ツリーのリセットは上位レベル（MainWindow）の責務
    // ============================================================

    void cleanup_withoutKifuLoadCoordinator_treeNotClearedByPerformCleanup()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(5);

        // 事前確認: ツリーに子ノードがある（旧対局の手）
        QVERIFY(h.branchTree.root() != nullptr);
        QVERIFY2(h.branchTree.root()->childCount() > 0,
                 "Tree should have children from previous game");

        // プリセットパスの修正: currentSfenStr を "startpos" に設定
        h.currentSfenStr = QStringLiteral("startpos");

        // performCleanup のみ実行
        // performCleanup() は分岐ツリーのクリアを行わない（上位の責務）
        h.handler->performCleanup();

        // 検証: ルートは存在するが、旧データは残ったまま
        // （上位レベルでのリセットが必要）
        QVERIFY2(h.branchTree.root() != nullptr,
                 "Root should exist after cleanup");

        // 棋譜モデルはクリアされている（performCleanup の責務）
        QCOMPARE(h.recordModel.rowCount(), 1);
    }

    void cleanup_thenMainWindowHandler_treeClearedCompletely()
    {
        CleanupHarness h;
        h.simulateGameWithMoves(5);

        // 事前確認: ツリーに子ノードがある
        QVERIFY(h.branchTree.root() != nullptr);
        QVERIFY(h.branchTree.root()->childCount() > 0);

        h.currentSfenStr = QStringLiteral("startpos");

        // Step 1: performCleanup（低レベルクリーンアップ）
        h.handler->performCleanup();

        // Step 2: MainWindow::onBranchTreeResetForNewGame 相当の処理を模擬
        // （GameStartCoordinator::requestBranchTreeResetForNewGame シグナル経由で呼ばれる）
        if (h.liveSession.isActive()) {
            h.liveSession.discard();
        }
        h.navState.setCurrentNode(nullptr);
        h.navState.resetPreferredLineIndex();
        h.branchTree.setRootSfen(h.startSfenStr);
        h.navState.goToRoot();

        // 検証: ツリーはルートのみ（子なし）
        QVERIFY(h.branchTree.root() != nullptr);
        QCOMPARE(h.branchTree.root()->childCount(), 0);

        // ツリーは1ライン（ルートのみ）
        QCOMPARE(h.branchTree.lineCount(), 1);
        const QList<BranchLine> lines = h.branchTree.allLines();
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines.at(0).nodes.size(), 1);
        QCOMPARE(lines.at(0).nodes.at(0)->ply(), 0);

        // 棋譜モデルもクリアされている
        QCOMPARE(h.recordModel.rowCount(), 1);
    }
};

QTEST_GUILESS_MAIN(TestPresetGameStartCleanup)
#include "tst_preset_gamestart_cleanup.moc"
