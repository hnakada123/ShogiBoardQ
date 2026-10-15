#ifndef MAINWINDOWRESETSERVICE_H
#define MAINWINDOWRESETSERVICE_H

/// @file mainwindowresetservice.h
/// @brief 状態とモデルの初期化をまとめたサービス（GameSession・MainWindowActions が使う）

#include <QString>

class UsiCommLogModel;
class ShogiEngineThinkingModel;
class EngineAnalysisTab;
class KifuAnalysisListModel;
class EvaluationGraphController;
class CommentCoordinator;

class KifuNavigationState;
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
class JosekiWindowController;
class PlayerInfoPresenter;
struct AppState;

/**
 * @brief 「新規」・対局開始前・棋譜読み込み前のリセット処理
 *
 * 状態を持たない。対象の部品は呼び出し側が構造体で渡す（すべて非所有）。
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
        EvaluationGraphController* evalGraphController = nullptr;
        CommentCoordinator* commentCoordinator = nullptr;   ///< コメント欄を空にする
    };

    struct ModelResetDeps {
        KifuNavigationState* navState = nullptr;
        KifuDisplayCoordinator* displayCoordinator = nullptr;
        BoardInteractionController* boardController = nullptr;
        GameRecordModel* gameRecord = nullptr;
        EvaluationGraphController* evalGraphController = nullptr;
        TimeControlController* timeController = nullptr;
        GameInfoPaneController* gameInfoController = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        KifuBranchTree* branchTree = nullptr;
        LiveGameSession* liveGameSession = nullptr;
    };

    struct UiResetDeps {
        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;
        UiStatePolicyManager* uiStatePolicy = nullptr;
        JosekiWindowController* josekiController = nullptr;
        PlayerInfoPresenter* playerInfo = nullptr;  ///< 対局者名ラベルを初期化した対局情報に合わせる
    };

    void clearGameStateFields(AppState& state) const;
    void clearSessionDependentUi(const SessionUiDeps& deps) const;
    void clearUiBeforeKifuLoad(const SessionUiDeps& deps) const;
    void resetModels(const ModelResetDeps& deps, const QString& hirateStartSfen) const;
    void resetUiState(const UiResetDeps& deps, const QString& hirateStartSfen) const;

private:
    void clearPresentationState(const ModelResetDeps& deps) const;
    void clearGameDataModels(const ModelResetDeps& deps) const;
    void resetBranchTreeForNewState(const ModelResetDeps& deps, const QString& hirateStartSfen) const;
};

#endif // MAINWINDOWRESETSERVICE_H
