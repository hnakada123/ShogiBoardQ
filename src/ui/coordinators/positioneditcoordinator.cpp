/// @file positioneditcoordinator.cpp
/// @brief 局面編集コーディネータクラスの実装

#include "positioneditcoordinator.h"
#include "appstate.h"
#include "uistatepolicymanager.h"
#include "kifunavigationstate.h"

#include <QAction>
#include <QPushButton>

#include "logcategories.h"

#include "shogiview.h"
#include "shogigamecontroller.h"
#include "boardinteractioncontroller.h"
#include "positioneditcontroller.h"
#include "matchcoordinator.h"
#include "replaycontroller.h"

PositionEditCoordinator::PositionEditCoordinator(QObject* parent)
    : QObject(parent)
{
}

PositionEditCoordinator::~PositionEditCoordinator() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void PositionEditCoordinator::setShogiView(ShogiView* view)
{
    m_shogiView = view;
}

void PositionEditCoordinator::setGameController(ShogiGameController* gc)
{
    m_gameController = gc;
}

void PositionEditCoordinator::setBoardController(BoardInteractionController* bic)
{
    m_boardController = bic;
}

void PositionEditCoordinator::setPositionEditController(PositionEditController* posEdit)
{
    m_posEdit = posEdit;
}

void PositionEditCoordinator::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void PositionEditCoordinator::setReplayController(ReplayController* replay)
{
    m_replayController = replay;
}

// --------------------------------------------------------
// 状態参照の設定
// --------------------------------------------------------

void PositionEditCoordinator::setNavigationState(const KifuNavigationState* state)
{
    m_navState = state;
}

void PositionEditCoordinator::setAppState(AppState* state)
{
    m_state = state;
}

// --------------------------------------------------------
// 編集用アクションの設定
// --------------------------------------------------------

void PositionEditCoordinator::setEditActions(const EditActions& actions)
{
    m_editActions = actions;
}

// --------------------------------------------------------
// 局面編集処理
// --------------------------------------------------------

void PositionEditCoordinator::beginPositionEditing()
{
    if (!m_posEdit || !m_shogiView || !m_gameController) return;

    PositionEditController::BeginEditContext ctx;
    ctx.view       = m_shogiView;
    ctx.gc         = m_gameController;
    ctx.bic        = m_boardController;

    const RecordLine line = m_navState ? m_navState->currentRecordLine() : RecordLine{};
    ctx.displayedSfen   = line.sfenAt(line.currentPly);
    ctx.recordStartSfen = line.startSfen;
    ctx.gameOver    = (m_match ? m_match->gameOverState().isOver : false);

    if (m_state) ctx.currentSfen = m_state->currentSfenStr;

    // メニュー表示：編集メニューに遷移
    ctx.onEnterEditMenu = [this]() {
        if (m_uiStatePolicy) m_uiStatePolicy->transitionToDuringPositionEdit();
    };

    // 「編集終了」ボタン表示
    ctx.onShowEditExitButton = [this]() {
        if (m_posEdit && m_shogiView) {
            m_posEdit->showEditExitButtonOnBoard(m_shogiView);
            if (auto* exitBtn = m_shogiView->findChild<QPushButton*>(QStringLiteral("editExitButton"))) {
                QObject::disconnect(m_exitBtnConnection);
                m_exitBtnConnection = QObject::connect(exitBtn, &QPushButton::clicked,
                                                       this, &PositionEditCoordinator::finishPositionEditing);
            }
        }
    };

    // 実行
    const QString startSfen = m_posEdit->beginPositionEditing(ctx);
    if (m_state && !startSfen.isEmpty()) m_state->startSfenStr = startSfen;

    // 編集元の局面が確定した後に、棋譜の初期化を通知する。
    if (m_shogiView->positionEditMode()) {
        emit positionEditingStarted();
    }

    // 編集用アクション配線
    if (m_editActions.actionReturnAllPiecesToStand) {
        QObject::connect(m_editActions.actionReturnAllPiecesToStand, &QAction::triggered,
                         m_posEdit, &PositionEditController::onReturnAllPiecesToBoxTriggered,
                         Qt::UniqueConnection);
    }

    if (m_editActions.actionSetHiratePosition) {
        QObject::connect(m_editActions.actionSetHiratePosition, &QAction::triggered,
                         m_posEdit, &PositionEditController::onFlatHandInitialPositionTriggered,
                         Qt::UniqueConnection);
    }

    if (m_editActions.actionSetTsumePosition) {
        QObject::connect(m_editActions.actionSetTsumePosition, &QAction::triggered,
                         m_posEdit, &PositionEditController::onShogiProblemInitialPositionTriggered,
                         Qt::UniqueConnection);
    }

    if (m_editActions.actionChangeTurn) {
        QObject::connect(m_editActions.actionChangeTurn, &QAction::triggered,
                         m_posEdit, &PositionEditController::onToggleSideToMoveTriggered,
                         Qt::UniqueConnection);
    }

}

void PositionEditCoordinator::finishPositionEditing()
{
    if (!m_posEdit || !m_shogiView || !m_gameController) return;

    // Controller に委譲
    PositionEditController::FinishEditContext ctx;
    ctx.view       = m_shogiView;
    ctx.gc         = m_gameController;
    ctx.bic        = m_boardController;

    // 「編集終了」ボタンの後片付け
    ctx.onHideEditExitButton = [this]() {
        if (m_posEdit && m_shogiView) {
            m_posEdit->hideEditExitButtonOnBoard(m_shogiView);
        }
        if (m_replayController) {
            m_replayController->setResumeFromCurrent(false);
        }
    };

    // メニューを元に戻す
    ctx.onLeaveEditMenu = [this]() {
        if (m_uiStatePolicy) m_uiStatePolicy->transitionToIdle();
    };

    // 実行
    const bool wasEditing = m_shogiView->positionEditMode();
    const QString editedSfen = m_posEdit->finishPositionEditing(ctx);
    // 編集した局面が開始局面になる（棋譜と対局の局面列は positionEditingFinished を受けて作り直す）
    if (m_state && !editedSfen.isEmpty()) m_state->startSfenStr = editedSfen;
    if (wasEditing && !m_shogiView->positionEditMode()) {
        emit positionEditingFinished();
    }

    qCDebug(lcUi) << "finishPositionEditing: editMode="
             << (m_shogiView ? m_shogiView->positionEditMode() : false)
             << " startSfen=" << editedSfen;
}
