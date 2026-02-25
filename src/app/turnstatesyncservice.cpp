/// @file turnstatesyncservice.cpp
/// @brief 手番同期サービスの実装

#include "turnstatesyncservice.h"

#include "playmode.h"
#include "shogiclock.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "timedisplaypresenter.h"
#include "timecontrolcontroller.h"
#include "turnmanager.h"

void TurnStateSyncService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void TurnStateSyncService::setCurrentTurn()
{
    if (!m_deps.turnManagerParent) return;

    // TurnManager を確保
    TurnManager* tm = m_deps.turnManagerParent->findChild<TurnManager*>(QStringLiteral("TurnManager"));
    if (!tm) {
        tm = new TurnManager(m_deps.turnManagerParent);
        tm->setObjectName(QStringLiteral("TurnManager"));
        if (m_deps.onTurnManagerCreated) {
            m_deps.onTurnManagerCreated(tm);
        }
    }

    const PlayMode mode = m_deps.playMode ? *m_deps.playMode : PlayMode::NotStarted;
    const bool isLivePlayMode =
        (mode == PlayMode::HumanVsHuman) ||
        (mode == PlayMode::EvenHumanVsEngine) ||
        (mode == PlayMode::EvenEngineVsHuman) ||
        (mode == PlayMode::EvenEngineVsEngine) ||
        (mode == PlayMode::HandicapEngineVsHuman) ||
        (mode == PlayMode::HandicapHumanVsEngine) ||
        (mode == PlayMode::HandicapEngineVsEngine) ||
        (mode == PlayMode::CsaNetworkMode);

    // ライブ対局中は GC を手番の単一ソースとする。
    // ShogiBoard::currentPlayer() は setSfen() 経由でのみ更新されるため、
    // 指し手適用直後に board 側を参照すると古い手番へ巻き戻ることがある。
    if (isLivePlayMode && m_deps.gameController) {
        const auto gcTurn = m_deps.gameController->currentPlayer();
        if (gcTurn == ShogiGameController::Player1 || gcTurn == ShogiGameController::Player2) {
            tm->setFromGc(gcTurn);
            return;
        }
    }

    // 非ライブ用途（棋譜ナビゲーション等）は盤面SFENの手番を優先。
    const Turn boardTurn = (m_deps.shogiView && m_deps.shogiView->board())
                               ? m_deps.shogiView->board()->currentPlayer()
                               : Turn::Black;
    tm->setFromTurn(boardTurn);

    // 盤面起点の同期時のみ GC へ反映する。
    if (m_deps.gameController) m_deps.gameController->setCurrentPlayer(tm->toGc());
}

void TurnStateSyncService::onTurnManagerChanged(ShogiGameController::Player now)
{
    // 1) UI側（先手=1, 後手=2）に反映
    const int cur = (now == ShogiGameController::Player2) ? 2 : 1;
    if (m_deps.updateTurnStatus) {
        m_deps.updateTurnStatus(cur);
    }

    // 2) 盤モデルの手番は GC に集約
    if (m_deps.gameController) {
        m_deps.gameController->setCurrentPlayer(now);
    }

    if (m_deps.shogiView) {
        m_deps.shogiView->updateTurnIndicator(now);
    }
}

void TurnStateSyncService::updateTurnAndTimekeepingDisplay()
{
    // 手番ラベルなど
    setCurrentTurn();

    // 時計の再描画（Presenterに現在値を流し直し）
    ShogiClock* clk = m_deps.timeController ? m_deps.timeController->clock() : nullptr;
    if (m_deps.timePresenter && clk) {
        const qint64 p1 = clk->getPlayer1TimeIntMs();
        const qint64 p2 = clk->getPlayer2TimeIntMs();
        const bool p1turn =
            (m_deps.gameController
                 ? (m_deps.gameController->currentPlayer() == ShogiGameController::Player1)
                 : true);
        m_deps.timePresenter->onMatchTimeUpdated(p1, p2, p1turn, /*urgencyMs*/ 0);
    }
}
