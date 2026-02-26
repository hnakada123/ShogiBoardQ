#ifndef KIFUNAVIGATIONCOORDINATOR_H
#define KIFUNAVIGATIONCOORDINATOR_H

/// @file kifunavigationcoordinator.h
/// @brief 棋譜ナビゲーション同期コーディネータの定義

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class RecordPane;
class KifuRecordListModel;
class BoardSyncPresenter;
class ShogiView;
class KifuNavigationState;
class KifuBranchTree;
class MatchCoordinator;
class EngineAnalysisTab;
class UiStatePolicyManager;
enum class PlayMode;

/**
 * @brief 棋譜ナビゲーション同期コーディネータ
 *
 * MainWindow から navigateKifuViewToRow / syncBoardAndHighlightsAtRow /
 * onBranchNodeHandled のロジックを集約し、本譜/分岐/検討モードの
 * 条件分岐を一元管理する。
 */
class KifuNavigationCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // --- UI ---
        RecordPane* recordPane = nullptr;                      ///< 棋譜欄ウィジェット（非所有）
        KifuRecordListModel* kifuRecordModel = nullptr;        ///< 棋譜リストモデル（非所有）

        // --- Board sync ---
        BoardSyncPresenter* boardSync = nullptr;               ///< 盤面同期プレゼンタ（非所有）
        ShogiView* shogiView = nullptr;                        ///< 将棋盤ビュー（非所有）

        // --- Navigation state ---
        KifuNavigationState* navState = nullptr;               ///< ナビゲーション状態（非所有）
        KifuBranchTree* branchTree = nullptr;                  ///< 分岐ツリー（非所有）

        // --- External state pointers ---
        int* activePly = nullptr;                              ///< アクティブ手数（外部所有）
        int* currentSelectedPly = nullptr;                     ///< 選択中手数（外部所有）
        int* currentMoveIndex = nullptr;                       ///< 現在の手インデックス（外部所有）
        QString* currentSfenStr = nullptr;                     ///< 現在局面SFEN文字列（外部所有）
        bool* skipBoardSyncForBranchNav = nullptr;             ///< 分岐ナビ中の盤面同期スキップフラグ（外部所有）
        PlayMode* playMode = nullptr;                          ///< 対局モード（外部所有）

        // --- Coordinators ---
        MatchCoordinator* match = nullptr;                     ///< 対局調整（非所有）
        EngineAnalysisTab* analysisTab = nullptr;              ///< エンジン解析タブ（非所有）
        UiStatePolicyManager* uiStatePolicy = nullptr;         ///< UI状態ポリシーマネージャ（非所有）

        // --- Callbacks ---
        std::function<void()> setCurrentTurn;                  ///< 手番表示更新
        std::function<void()> updateJosekiWindow;              ///< 定跡ウィンドウ更新
        std::function<void()> ensureBoardSyncPresenter;        ///< BoardSyncPresenter 遅延初期化
        std::function<bool()> isGameActivelyInProgress;        ///< 対局進行中判定
        std::function<QStringList*()> getSfenRecord;           ///< SFEN履歴取得
    };

    explicit KifuNavigationCoordinator(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    /// 棋譜ビューを指定手数の行にスクロールし盤面を同期する
    void navigateToRow(int ply);

    /// 指定手数の盤面・ハイライト・関連UI状態を同期する
    void syncBoardAndHighlightsAtRow(int ply);

    /// 分岐ノード処理完了後の盤面・ハイライト更新
    void handleBranchNodeHandled(int ply, const QString& sfen,
                                 int previousFileTo, int previousRankTo,
                                 const QString& lastUsiMove);

    /// 棋譜欄の指定行を選択し盤面を同期する
    void selectKifuRow(int row);

    /// ナビゲーション状態を手数に同期する
    void syncNavStateToPly(int selectedPly);

    /// 分岐ナビゲーション中の盤面同期スキップガードを開始する
    void beginBranchNavGuard();

private slots:
    /// 分岐ナビゲーションガードを解除する（QTimer::singleShot から呼ばれる）
    void clearBranchNavGuard();

private:
    Deps m_deps;
};

#endif // KIFUNAVIGATIONCOORDINATOR_H
