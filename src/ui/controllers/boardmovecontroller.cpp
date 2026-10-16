/// @file boardmovecontroller.cpp
/// @brief 盤上で指した手の処理の実装

#include "boardmovecontroller.h"

#include "appactivitystate.h"
#include "boardinteractioncontroller.h"
#include "branchtreeeditcontroller.h"
#include "logcategories.h"
#include "matchcoordinator.h"
#include "positioneditcontroller.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

BoardMoveController::BoardMoveController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.view && m_deps.gameController && m_deps.board && m_deps.activity && m_deps.match
             && m_deps.positionEdit && m_deps.branchEdit);
    connect(m_deps.board, &BoardInteractionController::moveRequested, this, &BoardMoveController::onMoveRequested);
}

void BoardMoveController::onMoveRequested(const QPoint& from, const QPoint& to)
{
    (void)playMove(from, to);
}

bool BoardMoveController::playMove(const QPoint& from, const QPoint& to)
{
    qCDebug(lcUi) << "playMove from=" << from << " to=" << to;
    BoardInteractionController* board = m_deps.board;
    using Activity = AppActivityState::Activity;

    switch (m_deps.activity->activity()) {
    case Activity::DuringPositionEdit: {
        const bool ok = m_deps.positionEdit->applyEditMove(from, to, m_deps.view, m_deps.gameController, board);
        if (!ok) qCDebug(lcUi) << "edit-mode move rejected";
        return ok;
    }
    case Activity::Idle: {
        // 対局していないとき：指した手を表示中の手の次の手（変化）として記録する
        QPoint hFrom = from, hTo = to;
        const bool ok = m_deps.branchEdit->recordBoardMove(hFrom, hTo, m_deps.gameController);
        board->onMoveApplied(hFrom, hTo, ok);
        return ok;
    }
    case Activity::DuringCsaGame:
        // 通信対局は確認を受けてから確定する。入力側は盤面や履歴を変更しない
        emit csaMoveRequested(from, to);
        board->onMoveApplied(from, to, false);
        return true;
    case Activity::DuringGame:
        break;
    default:
        board->onMoveApplied(from, to, false);
        return false;
    }

    // 対局（人間の手番）
    MatchCoordinator* match = m_deps.match;
    if (!match->canAcceptMove() || !match->isHumanSide(m_deps.gameController->currentPlayer())) {
        board->onMoveApplied(from, to, false);
        return false;
    }
    // validateAndMove は成りの選択などで座標を書き換えるのでローカルに退避
    QPoint hFrom = from, hTo = to;
    const std::optional<RecordEntry> played = m_deps.gameController->validateAndMove(hFrom, hTo, match->lastMoveDestination());
    board->onMoveApplied(hFrom, hTo, played.has_value());
    if (!played) {
        qCDebug(lcUi) << "validateAndMove failed (human move rejected)";
        return false;
    }
    // 考慮時間の確定・棋譜への追記・千日手と最大手数の判定・エンジンへの手番渡しは対局の司令塔が行う
    match->onHumanMove(*played);
    return true;
}
