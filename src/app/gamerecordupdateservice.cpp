/// @file gamerecordupdateservice.cpp
/// @brief 棋譜追記・ライブセッション更新ロジックの実装

#include "gamerecordupdateservice.h"

#include "kifdisplayitem.h"
#include "matchcoordinator.h"
#include "gamerecordpresenter.h"
#include "livegamesessionupdater.h"
#include "shogimove.h"
#include "shogiutils.h"
#include "logcategories.h"

GameRecordUpdateService::GameRecordUpdateService(QObject* parent)
    : QObject(parent)
{
}

void GameRecordUpdateService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void GameRecordUpdateService::appendKifuLine(const QString& text, const QString& elapsedTime)
{
    qCDebug(lcApp).noquote() << "appendKifuLine ENTER: text=" << text
                       << "elapsedTime=" << elapsedTime;

    updateGameRecord(text, elapsedTime);
}

void GameRecordUpdateService::updateGameRecord(const QString& moveText, const QString& elapsedTime)
{
    const bool gameOverAppended =
        (m_deps.match && m_deps.match->gameOverState().isOver && m_deps.match->gameOverState().moveAppended);
    if (gameOverAppended) return;

    GameRecordPresenter* presenter = nullptr;
    if (m_deps.ensureRecordPresenter) {
        presenter = m_deps.ensureRecordPresenter();
    }

    if (presenter) {
        presenter->appendMoveLine(moveText, elapsedTime);

        if (!moveText.isEmpty()) {
            presenter->addLiveKifItem(moveText, elapsedTime);
        }
    }

    if (m_deps.liveGameSession != nullptr && !moveText.isEmpty()) {
        LiveGameSessionUpdater* updater = nullptr;
        if (m_deps.ensureLiveGameSessionUpdater) {
            updater = m_deps.ensureLiveGameSessionUpdater();
        }
        if (updater) {
            ShogiMove move;
            if (m_deps.gameMoves && !m_deps.gameMoves->isEmpty()) {
                move = m_deps.gameMoves->last();
            }
            updater->appendMove(move, moveText, elapsedTime);
        }
    }
}

void GameRecordUpdateService::recordUsiMoveAndUpdateSfen()
{
    // USI形式の指し手を記録（詰み探索などで使用）
    if (m_deps.gameMoves && m_deps.gameUsiMoves && !m_deps.gameMoves->isEmpty()) {
        const ShogiMove& lastMove = m_deps.gameMoves->last();
        const QString usiMove = ShogiUtils::moveToUsi(lastMove);
        if (!usiMove.isEmpty()) {
            // 重複追加を防ぐ
            if (m_deps.gameUsiMoves->size() == m_deps.gameMoves->size() - 1) {
                m_deps.gameUsiMoves->append(usiMove);
                qCDebug(lcApp).noquote() << "recordUsiMoveAndUpdateSfen: added USI move:" << usiMove
                                   << "gameUsiMoves.size()=" << m_deps.gameUsiMoves->size();
            }
        }
    }

    // currentSfenStrを現在の局面に更新
    if (m_deps.sfenRecord && m_deps.currentSfenStr && !m_deps.sfenRecord->isEmpty()) {
        *m_deps.currentSfenStr = m_deps.sfenRecord->last();
        qCDebug(lcApp) << "recordUsiMoveAndUpdateSfen:"
                 << "sfenRecord.size()=" << m_deps.sfenRecord->size()
                 << "using last sfen=" << *m_deps.currentSfenStr;
    }
}
