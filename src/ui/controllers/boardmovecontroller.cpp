/// @file boardmovecontroller.cpp
/// @brief 盤上で指した手の処理の実装

#include "boardmovecontroller.h"

#include "appactivitystate.h"
#include "boardcoordinates.h"
#include "boardinteractioncontroller.h"
#include "branchtreeeditcontroller.h"
#include "logcategories.h"
#include "matchcoordinator.h"
#include "humanturnpolicy.h"
#include "positioneditsession.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

BoardMoveController::BoardMoveController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.view && m_deps.gameController && m_deps.board && m_deps.activity && m_deps.match
             && m_deps.positionEdit && m_deps.branchEdit && m_deps.turnPolicy);
    m_deps.board->setInputRoute(this);
    // 動作状態が変わったら（対局の開始・局面編集の開始など）、つまんでいた駒を戻す
    connect(m_deps.activity, &AppActivityState::changed,
            m_deps.board, &BoardInteractionController::cancelPendingClick);
}

bool BoardMoveController::acceptsInput() const
{
    return isEditing() || m_deps.turnPolicy->isHumanTurnNow();
}

bool BoardMoveController::isEditing() const
{
    return m_deps.positionEdit->isEditing();
}

void BoardMoveController::submit(const QPoint& from, const QPoint& to)
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
        const bool ok = m_deps.positionEdit->applyEditMove(from, to);
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
    case Activity::DuringGame:
        if (!canPlayInGame()) break;
        [[fallthrough]];
    case Activity::DuringCsaGame: {
        // クリックの移動元・移動先を手にする（成り・不成の両方が合法なら対局者に選ばせる）
        const std::optional<UsiMove> move = m_deps.gameController->prepareMove(from, to);
        if (!move) break;
        return playUsiMove(*move);
    }
    default:
        break;
    }
    board->onMoveApplied(from, to, false);
    return false;
}

bool BoardMoveController::playUsiMove(const UsiMove& move)
{
    qCDebug(lcUi).noquote() << "playUsiMove" << move.toString();
    using Activity = AppActivityState::Activity;
    switch (m_deps.activity->activity()) {
    case Activity::Idle: {
        const bool ok = m_deps.branchEdit->recordMove(move, m_deps.gameController);
        reportResult(move, ok);
        return ok;
    }
    case Activity::DuringCsaGame: {
        // 通信対局は確認を受けてから確定する。入力側は盤面や履歴を変更しない
        const bool legal = m_deps.gameController->isLegal(move);
        if (legal) emit csaMoveRequested(move);
        reportResult(move, false);
        return legal;
    }
    case Activity::DuringGame: {
        if (!canPlayInGame()) break;
        const std::optional<RecordEntry> played =
            m_deps.gameController->applyMove(move, m_deps.match->lastMoveDestination());
        reportResult(move, played.has_value());
        if (!played) {
            qCDebug(lcUi) << "human move rejected";
            return false;
        }
        // 考慮時間の確定・棋譜への追記・千日手と最大手数の判定・エンジンへの手番渡しは対局の司令塔が行う
        m_deps.match->onHumanMove(*played);
        return true;
    }
    default:
        break;
    }
    reportResult(move, false);
    return false;
}

bool BoardMoveController::canPlayInGame() const
{
    return m_deps.match->canAcceptMove() && m_deps.match->isHumanSide(m_deps.gameController->currentPlayer());
}

void BoardMoveController::reportResult(const UsiMove& move, bool ok)
{
    // 指した後は手番が替わっているので、移動元の駒台は指した側（指せなければ今の手番）から決める
    const Turn current = m_deps.gameController->currentPlayer();
    const Turn mover = ok ? (current == Turn::Black ? Turn::White : Turn::Black) : current;
    m_deps.board->onMoveApplied(BoardCoordinates::fromSquare(move, mover), BoardCoordinates::toSquare(move), ok);
}
