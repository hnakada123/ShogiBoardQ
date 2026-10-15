/// @file turnstatesyncservice.cpp
/// @brief 手番同期サービスの実装

#include "turnstatesyncservice.h"

#include "logcategories.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogiview.h"
#include "timedisplaypresenter.h"
#include "turnmanager.h"

TurnStateSyncService::TurnStateSyncService(QObject* parent)
    : QObject(parent)
    , m_turnManager(new TurnManager(this))
{
    m_turnManager->setObjectName(QStringLiteral("TurnManager"));
    connect(m_turnManager, &TurnManager::changed, this, &TurnStateSyncService::onTurnManagerChanged);
}

void TurnStateSyncService::updateDeps(const Deps& deps)
{
    m_deps = deps;
    if (m_connectedGc != m_deps.gameController) {
        if (m_connectedGc) disconnect(m_connectedGc, nullptr, m_turnManager, nullptr);
        m_connectedGc = m_deps.gameController;
        if (m_connectedGc) {
            connect(m_connectedGc, &ShogiGameController::currentPlayerChanged,
                    m_turnManager, &TurnManager::set);
        }
    }
}

void TurnStateSyncService::setCurrentTurn()
{
    // 対局者は終局後も残るため、実際の対局進行状態で同期元を選ぶ
    const bool gameActive = m_deps.isGameActivelyInProgress && m_deps.isGameActivelyInProgress();

    // ライブ対局中は GC を手番の単一ソースとする。盤面モデルの手番は GC の手番更新から
    // 同期されるが、同期前に board 側を参照すると古い手番へ巻き戻ることがあるため GC だけを見る
    if (gameActive && m_deps.gameController) {
        m_turnManager->set(m_deps.gameController->currentPlayer());
        return;
    }

    // 非ライブ用途（棋譜ナビゲーション等）は盤面 SFEN の手番を優先する
    const Turn boardTurn = (m_deps.shogiView && m_deps.shogiView->board())
                               ? m_deps.shogiView->board()->currentPlayer()
                               : Turn::Black;
    m_turnManager->set(boardTurn);

    // 盤面起点の同期時のみ GC へ反映する
    if (m_deps.gameController) m_deps.gameController->setCurrentPlayer(m_turnManager->side());
}

void TurnStateSyncService::onTurnManagerChanged(Turn now)
{
    // 時計の手番。手番側の表示は下の updateTurnIndicator が行う。
    // ここで時計の setActiveSide を呼ぶと秒読みの警告色が次の秒まで消える
    if (m_deps.clock) {
        m_deps.clock->setCurrentPlayer(now);
    } else {
        qCWarning(lcApp) << "ShogiClock not ready yet";
    }

    // 盤モデルの手番は GC に集約
    if (m_deps.gameController) m_deps.gameController->setCurrentPlayer(now);
    if (m_deps.shogiView) m_deps.shogiView->updateTurnIndicator(now);
}

void TurnStateSyncService::updateTurnAndTimekeepingDisplay()
{
    setCurrentTurn();

    // 時計の再描画（Presenter に現在値を流し直し）
    ShogiClock* clk = m_deps.clock;
    if (m_deps.timePresenter && clk) {
        const Turn sideToMove = m_deps.gameController ? m_deps.gameController->currentPlayer() : Turn::Black;
        m_deps.timePresenter->onMatchTimeUpdated(clk->remainingMs(Turn::Black), clk->remainingMs(Turn::White),
                                                 sideToMove, /*urgencyMs*/ 0);
    }
}
