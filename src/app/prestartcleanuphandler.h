#ifndef PRESTARTCLEANUPHANDLER_H
#define PRESTARTCLEANUPHANDLER_H

/// @file prestartcleanuphandler.h
/// @brief 対局開始前クリーンアップハンドラクラスの定義

#include <QObject>
#include "startoptions.h"
#include <QString>
#include <QHash>
#include <QMap>

class BoardInteractionController;
class ShogiView;
class KifuRecordListModel;
class KifuBranchListModel;
class UsiCommLogModel;
class TimeControlController;
class EvaluationChartWidget;
class EvaluationGraphController;
class RecordPane;
class LiveGameSession;
class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;

/**
 * @brief 対局開始前のクリーンアップ処理を担当するクラス
 *
 * 責務:
 * - 盤面/ハイライトのビジュアル初期化
 * - 棋譜モデルの部分削除または全消去
 * - 手数トラッキング更新
 * - 分岐モデルクリア
 * - USIログ初期化
 * - 時間制御リセット
 *
 * GameStartCoordinator::requestPreStartCleanupシグナルに接続して使用する。
 *
 */
class PreStartCleanupHandler : public QObject
{
    Q_OBJECT

public:
    /// 依存オブジェクト構造体
    struct Dependencies {
        BoardInteractionController* boardController = nullptr;  ///< 盤面操作コントローラ（非所有）
        ShogiView* shogiView = nullptr;                         ///< 盤面ビュー（非所有）
        KifuRecordListModel* kifuRecordModel = nullptr;         ///< 棋譜モデル（非所有）
        KifuBranchListModel* kifuBranchModel = nullptr;         ///< 分岐モデル（非所有）
        UsiCommLogModel* lineEditModel1 = nullptr;              ///< エンジン1のUSIログモデル（非所有）
        UsiCommLogModel* lineEditModel2 = nullptr;              ///< エンジン2のUSIログモデル（非所有）
        TimeControlController* timeController = nullptr;        ///< 時間制御コントローラ（非所有）
        EvaluationChartWidget* evalChart = nullptr;             ///< 評価値グラフウィジェット（非所有）
        EvaluationGraphController* evalGraphController = nullptr; ///< 評価値グラフコントローラ（非所有）
        RecordPane* recordPane = nullptr;                       ///< 棋譜ペイン（非所有）

        // --- 分岐ツリー関連 ---
        LiveGameSession* liveGameSession = nullptr;             ///< ライブゲームセッション（非所有）
        KifuBranchTree* branchTree = nullptr;                   ///< 分岐ツリー（非所有）
        KifuNavigationState* navState = nullptr;                ///< ナビゲーション状態（非所有）
    };

    explicit PreStartCleanupHandler(const Dependencies& deps, QObject* parent = nullptr);
    ~PreStartCleanupHandler() override = default;
    void updateDependencies(const Dependencies& deps);

    /// 表示と棋譜を開始要求に合わせる。ライブセッションの開始は呼び出し側が行う。
    KifuBranchNode* performCleanup(const GameStartPreparation& preparation);

private:
    void clearBoardAndHighlights();
    void clearClockDisplay();
    void cleanupKifuModel(KifuBranchNode* startNode);
    void clearBranchModel();
    void resetUsiLogModels();
    void resetTimeControl();
    void resetEvaluationGraph(bool preserve, int ply);
    void selectStartRow(int row);

    // --- メンバー変数 ---

    BoardInteractionController* m_boardController = nullptr;    ///< 盤面操作コントローラ（非所有）
    ShogiView* m_shogiView = nullptr;                           ///< 盤面ビュー（非所有）
    KifuRecordListModel* m_kifuRecordModel = nullptr;           ///< 棋譜モデル（非所有）
    KifuBranchListModel* m_kifuBranchModel = nullptr;           ///< 分岐モデル（非所有）
    UsiCommLogModel* m_lineEditModel1 = nullptr;                ///< エンジン1 USIログ（非所有）
    UsiCommLogModel* m_lineEditModel2 = nullptr;                ///< エンジン2 USIログ（非所有）
    TimeControlController* m_timeController = nullptr;          ///< 時間制御コントローラ（非所有）
    EvaluationChartWidget* m_evalChart = nullptr;               ///< 評価値グラフ（非所有）
    EvaluationGraphController* m_evalGraphController = nullptr; ///< 評価値グラフコントローラ（非所有）
    RecordPane* m_recordPane = nullptr;                         ///< 棋譜ペイン（非所有）

    // --- 分岐ツリー関連 ---

    LiveGameSession* m_liveGameSession = nullptr;  ///< ライブゲームセッション（非所有）
    KifuBranchTree* m_branchTree = nullptr;        ///< 分岐ツリー（非所有）
    KifuNavigationState* m_navState = nullptr;     ///< ナビゲーション状態（非所有）


};

#endif // PRESTARTCLEANUPHANDLER_H
