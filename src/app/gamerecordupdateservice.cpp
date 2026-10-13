/// @file gamerecordupdateservice.cpp
/// @brief 棋譜追記・ライブセッション更新ロジックの実装

#include "gamerecordupdateservice.h"

#include "kifdisplayitem.h"
#include "kifubranchnode.h"
#include "matchcoordinator.h"
#include "gamerecordpresenter.h"
#include "livegamesession.h"
#include "livegamesessionupdater.h"
#include "shogimove.h"
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

void GameRecordUpdateService::updateGameRecord(const QString& moveText, const QString& clockElapsedTime,
                                             const QString& recordedSfen)
{
    const bool gameOverAppended =
        (m_deps.match && m_deps.match->gameOverState().isOver && m_deps.match->gameOverState().moveAppended);
    if (gameOverAppended) return;

    // 棋譜の途中から対局した場合も、累計時間は棋譜上の直前の累計から続ける
    const QString elapsedTime = (m_deps.liveGameSession != nullptr)
        ? m_deps.liveGameSession->continuedElapsedText(clockElapsedTime) : clockElapsedTime;

    GameRecordPresenter* presenter = nullptr;
    if (m_deps.ensureRecordPresenter) {
        presenter = m_deps.ensureRecordPresenter();
    }

    if (presenter) {
        QString beforeSfen, usiMove;
        if (detectTerminalType(moveText) == TerminalType::None && m_deps.gameMoves && !m_deps.gameMoves->isEmpty()
            && m_deps.sfenRecord && m_deps.sfenRecord->size() >= 2) {
            const qsizetype previousIndex = recordedSfen.isEmpty() ? m_deps.sfenRecord->size() - 2
                : m_deps.sfenRecord->indexOf(recordedSfen) - 1;
            beforeSfen = m_deps.sfenRecord->value(previousIndex);
            usiMove = m_deps.gameMoves->last().toUsiMove().toString();
        }
        presenter->appendMoveLine(moveText, elapsedTime, beforeSfen, usiMove);
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
            updater->appendMove(move, moveText, elapsedTime, recordedSfen);
        }
    }
    if (!moveText.isEmpty() && m_deps.markGameRecordDirty) m_deps.markGameRecordDirty();
}

void GameRecordUpdateService::updateCurrentSfenAfterMove()
{
    // currentSfenStrを現在の局面に更新
    if (m_deps.sfenRecord && m_deps.currentSfenStr && !m_deps.sfenRecord->isEmpty()) {
        *m_deps.currentSfenStr = m_deps.sfenRecord->last();
        qCDebug(lcApp) << "updateCurrentSfenAfterMove:"
                 << "sfenRecord.size()=" << m_deps.sfenRecord->size()
                 << "using last sfen=" << *m_deps.currentSfenStr;
    }
}
