#ifndef SESSIONLIFECYCLECOORDINATOR_H
#define SESSIONLIFECYCLECOORDINATOR_H

/// @file sessionlifecyclecoordinator.h
/// @brief セッションのライフサイクル（リセット・開始・終局）を管理するコーディネータの定義

#include <QObject>
#include <QString>

#include "gamestartcoordinator.h"
#include "mainwindowresetservice.h"
#include "matchcoordinator.h"

struct AppState;
class ConsecutiveGamesController;
class CsaGameWiring;
class GameStateController;
class JosekiWindowWiring;
class PlayerInfoWiring;
class PreStartCleanupHandler;
class ReplayController;
class ShogiGameController;
class ShogiView;
class TimeControlController;
class UiStatePolicyManager;

/**
 * @brief セッションのライフサイクル（リセット・開始・終局）を管理する
 *
 * - resetToInitialState: 「新規」の完全リセット
 * - prepareNextGame: 連続対局の次局の準備
 * - handleGameStarted / handleGameEnded / handleGameEndProcessed: 司令塔・開始担当からの通知
 *
 * 部品はすべて MainWindowComposer が起動時に作って渡す（非所有）。状態は AppState を直接読み書きする。
 */
class SessionLifecycleCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        AppState* state = nullptr;
        MatchCoordinator* match = nullptr;
        CsaGameWiring* csaGameWiring = nullptr;
        ReplayController* replayController = nullptr;
        ShogiGameController* gameController = nullptr;
        GameStateController* gameStateController = nullptr;
        TimeControlController* timeController = nullptr;
        ConsecutiveGamesController* consecutiveGamesController = nullptr;
        ShogiView* shogiView = nullptr;
        PreStartCleanupHandler* preStartCleanupHandler = nullptr;
        PlayerInfoWiring* playerInfoWiring = nullptr;
        JosekiWindowWiring* josekiWiring = nullptr;
        UiStatePolicyManager* uiStatePolicy = nullptr;
        MainWindowResetService::SessionUiDeps sessionUi;   ///< 対局ごとに消すログ・思考表示
        MainWindowResetService::ModelResetDeps models;     ///< 「新規」で作り直す棋譜まわり
    };

    explicit SessionLifecycleCoordinator(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    /// セッションを初期状態にリセットする（メニュー「新規」。未保存の確認は呼び出し側）
    void resetToInitialState();

    /// 対局の状態（モード・対局者名・作業用の履歴）とコントローラをリセットする
    void resetGameState();

    /// 連続対局の次局を独立した棋譜・開始局面として準備する
    void prepareNextGame(const QString& startSfen);


    /// 棋譜まわりのモデルを sfen の局面から作り直す
    void resetModels(const QString& sfen);

    /// 棋譜を読み込む前に、前の対局のログ・グラフ・コメントを消す
    void clearUiBeforeKifuLoad();

public slots:
    /// 対局開始前のクリーンアップとセッション依存 UI のクリア
    void performPreStartCleanup(const GameStartPreparation& preparation);
    /// 時間設定を適用する（保存・時計設定・対局情報への反映）
    void applyTimeControl(const GameStartCoordinator::TimeSettings& tc);
    /// 対局開始後の状態同期
    void handleGameStarted(const MatchCoordinator::StartOptions& opt);
    /// ライブ対局セッションが有効なら確定する
    void commitLiveGameSessionIfActive();
    /// 終局処理（UI ロック・終局時刻）
    void handleGameEnded(const MatchCoordinator::GameEndInfo& info);
    /// 終局表示・棋譜保存の後に、連続対局の次局を予約する
    void handleGameEndProcessed(const MatchCoordinator::GameEndInfo& info);
    /// 評価値グラフとスコアを消す
    void clearEvalState();

private:
    void clearSessionDependentUi();
    void resetUiState(const QString& sfen);
    void stopEnginesAndNetwork();

    Deps m_deps;
    GameStartCoordinator::TimeSettings m_lastTimeControl;  ///< 連続対局の次局に引き継ぐ時間設定
};

#endif // SESSIONLIFECYCLECOORDINATOR_H
