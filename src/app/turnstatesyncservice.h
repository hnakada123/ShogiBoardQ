#ifndef TURNSTATESYNCSERVICE_H
#define TURNSTATESYNCSERVICE_H

/// @file turnstatesyncservice.h
/// @brief 手番同期サービス — TurnManager / GameController / View / 時計の手番同期を集約

#include "shogigamecontroller.h"

#include <functional>

class ShogiView;
class ShogiClock;
class TurnManager;
class TimeControlController;
class TimeDisplayPresenter;
class QObject;
enum class PlayMode;

/**
 * @brief TurnManager / GameController / View / 時計表示の手番同期ロジックを集約するサービス
 *
 * MainWindow から以下の責務を移譲:
 * - setCurrentTurn: TurnManager の生成/取得と手番同期
 * - onTurnManagerChanged: TurnManager 変更通知のハンドリング
 * - updateTurnAndTimekeepingDisplay: 手番表示 + 時計表示の更新
 */
class TurnStateSyncService
{
public:
    struct Deps {
        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;
        TimeControlController* timeController = nullptr;
        TimeDisplayPresenter* timePresenter = nullptr;
        PlayMode* playMode = nullptr;

        /// updateTurnStatus(int currentPlayer) のコールバック
        /// (clock の currentPlayer 設定 + view の activeSide 設定)
        std::function<void(int)> updateTurnStatus;

        /// TurnManager の親オブジェクト（findChild で検索するための QObject*）
        QObject* turnManagerParent = nullptr;

        /// TurnManager 新規生成時に呼ばれるコールバック（シグナル接続用）
        std::function<void(TurnManager*)> onTurnManagerCreated;
    };

    TurnStateSyncService() = default;

    void updateDeps(const Deps& deps);

    /// TurnManager を確保し、手番を同期する
    void setCurrentTurn();

    /// TurnManager 変更通知ハンドラ
    void onTurnManagerChanged(ShogiGameController::Player now);

    /// 手番表示 + 時計表示を更新する
    void updateTurnAndTimekeepingDisplay();

private:
    Deps m_deps;
};

#endif // TURNSTATESYNCSERVICE_H
