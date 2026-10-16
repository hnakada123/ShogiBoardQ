/// @file positioneditsession.cpp
/// @brief 局面編集の実装

#include "positioneditsession.h"

#include <QAction>

#include "appactivitystate.h"
#include "boardchromewidget.h"
#include "boardinteractioncontroller.h"
#include "kifunavigationstate.h"
#include "logcategories.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"

namespace {
/// 詰将棋の初期局面の見本（使わない駒は玉方の持駒）
constexpr char kTsumeSample[] = "5+r1kl/6p2/6Bpn/9/7P1/9/9/9/9 b RSb4g3s3n3l15p 1";
} // namespace

PositionEditSession::PositionEditSession(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.view && m_deps.gameController && m_deps.board && m_deps.navState && m_deps.activity);
    connect(m_deps.view->chrome(), &BoardChromeWidget::editExitRequested, this, &PositionEditSession::finish);

    const EditActions& a = m_deps.actions;
    if (a.returnAllPiecesToStand)
        connect(a.returnAllPiecesToStand, &QAction::triggered, this, &PositionEditSession::returnAllPiecesToBox);
    if (a.setHiratePosition)
        connect(a.setHiratePosition, &QAction::triggered, this, &PositionEditSession::setHiratePosition);
    if (a.setTsumePosition)
        connect(a.setTsumePosition, &QAction::triggered, this, &PositionEditSession::setTsumePosition);
    if (a.changeTurn)
        connect(a.changeTurn, &QAction::triggered, this, &PositionEditSession::toggleSideToMove);
}

bool PositionEditSession::isEditing() const
{
    return m_deps.activity->is(AppActivityState::Activity::DuringPositionEdit);
}

// ============================================================
// 開始・終了
// ============================================================

void PositionEditSession::begin()
{
    ShogiBoard* board = m_deps.view->board();
    if (!board || isEditing()) return;

    // 表示中の局面を、表示している手番のまま1手目として編集する（棋譜を読み込んだ直後も同じ）
    const Turn turn = board->currentPlayer();
    const std::optional<Position> displayed = Position::fromSfen(m_deps.navState->currentSfen());
    const QString sfen = displayed ? displayed->toSfen(turn, 1) : board->position().toSfen(turn, 1);

    resetInteraction();
    board->setSfen(sfen);
    m_deps.activity->setActivity(AppActivityState::Activity::DuringPositionEdit);
    m_deps.view->setPositionEditMode(true);
    m_deps.view->setEditExitButtonVisible(true);
    m_deps.view->update();
    emit editingStarted(sfen);
}

void PositionEditSession::finish()
{
    if (!isEditing()) return;
    ShogiBoard* board = m_deps.view->board();

    // 編集中に替えた手番で、編集した局面を1手目にする
    const QString sfen = board->position().toSfen(m_deps.gameController->currentPlayer(), 1);
    resetInteraction();
    board->setSfen(sfen);
    m_deps.view->setPositionEditMode(false);
    m_deps.view->setEditExitButtonVisible(false);
    m_deps.activity->setActivity(AppActivityState::Activity::Idle);
    m_deps.view->update();
    qCDebug(lcUi) << "finishPositionEditing: startSfen=" << sfen;
    emit editingFinished(sfen);
}

void PositionEditSession::resetInteraction()
{
    m_deps.view->endDrag();
    m_deps.board->clearAllHighlights();
}

// ============================================================
// 編集中の着手
// ============================================================

bool PositionEditSession::applyEditMove(const QPoint& from, const QPoint& to)
{
    QPoint hFrom = from, hTo = to;
    const bool ok = m_deps.gameController->editPosition(hFrom, hTo, m_deps.view->pieceBoxSide());
    // 盤・駒台・駒箱の変化は ShogiBoard の通知で変わった所だけ描き直されるので、ここで盤全体は描き直さない
    m_deps.view->endDrag();
    m_deps.board->onMoveApplied(hFrom, hTo, ok);
    return ok;
}

// ============================================================
// 編集メニュー
// ============================================================

void PositionEditSession::returnAllPiecesToBox()
{
    if (!isEditing()) return;
    resetInteraction();
    m_deps.view->board()->resetGameBoard();
}

void PositionEditSession::setHiratePosition()
{
    if (!isEditing()) return;
    resetInteraction();
    m_deps.view->board()->setSfen(SfenUtils::hirateSfen());
    m_deps.gameController->setCurrentPlayer(Turn::Black);
}

void PositionEditSession::setTsumePosition()
{
    if (!isEditing()) return;
    resetInteraction();
    m_deps.view->board()->setSfen(QString::fromLatin1(kTsumeSample));
    m_deps.gameController->setCurrentPlayer(Turn::Black);
}

void PositionEditSession::toggleSideToMove()
{
    if (!isEditing()) return;
    // 編集中の配置は作り直さず、手番だけ替える
    const Turn next = m_deps.gameController->currentPlayer() == Turn::Black ? Turn::White : Turn::Black;
    m_deps.gameController->setCurrentPlayer(next);
    m_deps.view->update();
}
