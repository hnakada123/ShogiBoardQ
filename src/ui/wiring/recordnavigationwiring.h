#ifndef RECORDNAVIGATIONWIRING_H
#define RECORDNAVIGATIONWIRING_H

/// @file recordnavigationwiring.h
/// @brief 棋譜ナビゲーション配線クラスの定義


#include <QObject>

class MainWindow;
class RecordNavigationHandler;
class KifuNavigationState;
class KifuBranchTree;
class KifuDisplayCoordinator;
class KifuRecordListModel;
class ShogiView;
class EvaluationGraphController;
class CsaGameCoordinator;
class MatchCoordinator;
class KifuNavigationCoordinator;
class UiStatePolicyManager;
class ConsiderationPositionService;
enum class PlayMode;

/**
 * @brief RecordNavigationHandler の生成・配線・依存設定を担当するクラス
 *
 * 責務:
 * - RecordNavigationHandler の遅延生成
 * - シグナル/スロット接続（6 connect）
 * - Deps 構造体の構築と更新
 */
class RecordNavigationWiring : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        MainWindow* mainWindow = nullptr;                  ///< MainWindow（シグナル接続先）
        KifuNavigationState* navState = nullptr;           ///< ナビゲーション状態（非所有）
        KifuBranchTree* branchTree = nullptr;              ///< 分岐ツリー（非所有）
        KifuDisplayCoordinator* displayCoordinator = nullptr; ///< 棋譜表示コーディネータ（非所有）
        KifuRecordListModel* kifuRecordModel = nullptr;    ///< 棋譜リストモデル（非所有）
        ShogiView* shogiView = nullptr;                    ///< 盤面ビュー（非所有）
        EvaluationGraphController* evalGraphController = nullptr; ///< 評価値グラフ制御（非所有）
        QStringList* sfenRecord = nullptr;                 ///< SFEN記録（外部所有）
        int* activePly = nullptr;                          ///< アクティブ手数（外部所有）
        int* currentSelectedPly = nullptr;                 ///< 選択中手数（外部所有）
        int* currentMoveIndex = nullptr;                   ///< 現在の手インデックス（外部所有）
        QString* currentSfenStr = nullptr;                 ///< 現在局面SFEN文字列（外部所有）
        bool* skipBoardSyncForBranchNav = nullptr;         ///< 分岐ナビ中の盤面同期スキップフラグ（外部所有）
        CsaGameCoordinator* csaGameCoordinator = nullptr;  ///< CSA対局コーディネータ（非所有）
        PlayMode* playMode = nullptr;                      ///< 対局モード（外部所有）
        MatchCoordinator* match = nullptr;                 ///< 対局調整（非所有）
    };

    /// wireSignals で使用するシグナル接続先（呼び出し元で ensure 済み）
    struct WiringTargets {
        KifuNavigationCoordinator* kifuNav = nullptr;               ///< 棋譜ナビゲーション（非所有）
        UiStatePolicyManager* uiStatePolicy = nullptr;              ///< UI状態ポリシー（非所有）
        ConsiderationPositionService* considerationPosition = nullptr; ///< 検討局面サービス（非所有）
    };

    explicit RecordNavigationWiring(QObject* parent = nullptr);

    /// RecordNavigationHandler を必要に応じて生成し、依存関係を更新する
    void ensure(const Deps& deps, const WiringTargets& targets);

    /// RecordNavigationHandler を返す（未生成時は nullptr）
    RecordNavigationHandler* handler() const { return m_handler; }

private:
    void wireSignals(const WiringTargets& targets);
    void bindDeps(const Deps& deps);

    RecordNavigationHandler* m_handler = nullptr;
    MainWindow* m_mainWindow = nullptr;
};

#endif // RECORDNAVIGATIONWIRING_H
