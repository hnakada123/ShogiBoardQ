#ifndef KIFULOADCOORDINATORFACTORY_H
#define KIFULOADCOORDINATORFACTORY_H

/// @file kifuloadcoordinatorfactory.h
/// @brief KifuLoadCoordinator の生成・配線ロジックを担うファクトリ

#include <QList>
#include <QStringList>

struct ShogiMove;
class QWidget;
class QTabWidget;
class KifuLoadCoordinator;
class KifuRecordListModel;
class KifuBranchListModel;
class KifuBranchTree;
class KifuNavigationState;
class ShogiView;
class GameInfoPaneController;
class EngineAnalysisTab;
class BranchNavigationWiring;
class KifuNavigationCoordinator;
class UiStatePolicyManager;
class PlayerInfoWiring;
class UiNotificationService;
class MainWindow;
class RecordPane;

/**
 * @brief KifuLoadCoordinator の生成・依存設定・シグナル接続を担うファクトリ
 *
 * Params 構造体経由で全入力を受け取るため friend 宣言は不要。
 */
class KifuLoadCoordinatorFactory
{
public:
    /// ファクトリメソッドの入力パラメータ
    struct Params {
        // --- KifuLoadCoordinator コンストラクタ引数（ポインタ経由で参照渡し） ---
        QList<ShogiMove>* gameMoves = nullptr;
        QStringList* positionStrList = nullptr;
        int* activePly = nullptr;
        int* currentSelectedPly = nullptr;
        int* currentMoveIndex = nullptr;
        QStringList* sfenRecord = nullptr;
        GameInfoPaneController* gameInfoController = nullptr;
        QTabWidget* tab = nullptr;
        RecordPane* recordPane = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        KifuBranchListModel* kifuBranchModel = nullptr;
        QWidget* parent = nullptr;

        // --- 分岐ツリー設定 ---
        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;

        // --- 追加依存 ---
        EngineAnalysisTab* analysisTab = nullptr;
        ShogiView* shogiView = nullptr;

        // --- 配線先（呼び出し元で ensure 済み） ---
        MainWindow* mainWindow = nullptr;               ///< displayGameRecord 接続先
        BranchNavigationWiring* branchNavWiring = nullptr;
        KifuNavigationCoordinator* kifuNavCoordinator = nullptr;
        UiStatePolicyManager* uiStatePolicy = nullptr;
        PlayerInfoWiring* playerInfoWiring = nullptr;
        UiNotificationService* notificationService = nullptr;
    };

    /// KifuLoadCoordinator を生成し、依存設定・シグナル接続を行い、所有権を返す
    static KifuLoadCoordinator* createAndWire(const Params& p);
};

#endif // KIFULOADCOORDINATORFACTORY_H
