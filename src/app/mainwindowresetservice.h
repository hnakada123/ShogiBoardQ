#ifndef MAINWINDOWRESETSERVICE_H
#define MAINWINDOWRESETSERVICE_H

/// @file mainwindowresetservice.h
/// @brief MainWindow の状態初期化ロジックを分離したサービス

#include <functional>
#include <QString>
#include <QStringList>

class UsiCommLogModel;
class ShogiEngineThinkingModel;
class EngineAnalysisTab;
class KifuAnalysisListModel;
class EvaluationChartWidget;
class EvaluationGraphController;

class KifuNavigationState;
class GameRecordPresenter;
class KifuDisplayCoordinator;
class BoardInteractionController;
class GameRecordModel;
class TimeControlController;
class GameInfoPaneController;
class KifuLoadCoordinator;
class KifuBranchTree;
class LiveGameSession;

class ShogiGameController;
class ShogiView;
class UiStatePolicyManager;

/**
 * @brief MainWindow のリセット処理を担当するサービス
 *
 * MainWindow から状態クリア/初期化処理を分離し、
 * UI ハブとしての責務を薄く保つことを目的とする。
 */
class MainWindowResetService
{
public:
    struct SessionUiDeps {
        UsiCommLogModel* commLog1 = nullptr;
        UsiCommLogModel* commLog2 = nullptr;
        ShogiEngineThinkingModel* thinking1 = nullptr;
        ShogiEngineThinkingModel* thinking2 = nullptr;
        ShogiEngineThinkingModel* consideration = nullptr;
        EngineAnalysisTab* analysisTab = nullptr;
        KifuAnalysisListModel* analysisModel = nullptr;
        EvaluationChartWidget* evalChart = nullptr;
        EvaluationGraphController* evalGraphController = nullptr;
        std::function<void(const QString&, bool)> broadcastComment;
    };

    struct ModelResetDeps {
        KifuNavigationState* navState = nullptr;
        GameRecordPresenter* recordPresenter = nullptr;
        KifuDisplayCoordinator* displayCoordinator = nullptr;
        BoardInteractionController* boardController = nullptr;
        GameRecordModel* gameRecord = nullptr;
        EvaluationGraphController* evalGraphController = nullptr;
        TimeControlController* timeController = nullptr;
        QStringList* sfenRecord = nullptr;
        GameInfoPaneController* gameInfoController = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        KifuBranchTree* branchTree = nullptr;
        LiveGameSession* liveGameSession = nullptr;
    };

    struct UiResetDeps {
        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;
        UiStatePolicyManager* uiStatePolicy = nullptr;
        std::function<void()> updateJosekiWindow;
    };

    void clearSessionDependentUi(const SessionUiDeps& deps) const;
    void clearUiBeforeKifuLoad(const SessionUiDeps& deps) const;
    void resetModels(const ModelResetDeps& deps, const QString& hirateStartSfen) const;
    void resetUiState(const UiResetDeps& deps, const QString& hirateStartSfen) const;

private:
    void clearPresentationState(const ModelResetDeps& deps) const;
    void clearGameDataModels(const ModelResetDeps& deps, const QString& hirateStartSfen) const;
    void resetBranchTreeForNewState(const ModelResetDeps& deps, const QString& hirateStartSfen) const;
};

#endif // MAINWINDOWRESETSERVICE_H
