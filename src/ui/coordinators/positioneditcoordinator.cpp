/// @file positioneditcoordinator.cpp
/// @brief 局面編集の開始・終了の流れ

#include "positioneditcoordinator.h"

#include "appactivitystate.h"
#include "kifunavigationstate.h"
#include "logcategories.h"
#include "positioneditcontroller.h"
#include "shogiview.h"

#include <QAction>
#include <QPushButton>

PositionEditCoordinator::PositionEditCoordinator(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.view && m_deps.gameController && m_deps.posEdit && m_deps.navState && m_deps.activity);

    // 編集メニューの項目は、編集中だけ有効になる（UiStatePolicyManager）
    PositionEditController* posEdit = m_deps.posEdit;
    const EditActions& a = m_deps.actions;
    if (a.returnAllPiecesToStand)
        connect(a.returnAllPiecesToStand, &QAction::triggered, posEdit, &PositionEditController::onReturnAllPiecesToBoxTriggered);
    if (a.setHiratePosition)
        connect(a.setHiratePosition, &QAction::triggered, posEdit, &PositionEditController::onFlatHandInitialPositionTriggered);
    if (a.setTsumePosition)
        connect(a.setTsumePosition, &QAction::triggered, posEdit, &PositionEditController::onShogiProblemInitialPositionTriggered);
    if (a.changeTurn)
        connect(a.changeTurn, &QAction::triggered, posEdit, &PositionEditController::onToggleSideToMoveTriggered);
}

void PositionEditCoordinator::beginPositionEditing()
{
    PositionEditController::BeginEditContext ctx;
    ctx.view = m_deps.view;
    ctx.gc = m_deps.gameController;
    ctx.bic = m_deps.boardController;
    ctx.displayedSfen = m_deps.navState->currentSfen();
    if (!m_deps.posEdit->beginPositionEditing(ctx)) return;

    m_deps.activity->setActivity(AppActivityState::Activity::DuringPositionEdit);
    // 盤の上に「編集終了」ボタンを出す
    m_deps.posEdit->showEditExitButtonOnBoard(m_deps.view);
    if (auto* exitButton = m_deps.view->findChild<QPushButton*>(QStringLiteral("editExitButton"))) {
        connect(exitButton, &QPushButton::clicked, this, &PositionEditCoordinator::finishPositionEditing,
                Qt::UniqueConnection);
    }
    // 編集元の局面が確定した後に、棋譜の初期化を知らせる
    emit positionEditingStarted();
}

void PositionEditCoordinator::finishPositionEditing()
{
    if (!m_deps.view->positionEditMode()) return;

    PositionEditController::FinishEditContext ctx;
    ctx.view = m_deps.view;
    ctx.gc = m_deps.gameController;
    ctx.bic = m_deps.boardController;
    const QString editedSfen = m_deps.posEdit->finishPositionEditing(ctx);
    m_deps.posEdit->hideEditExitButtonOnBoard(m_deps.view);
    m_deps.activity->setActivity(AppActivityState::Activity::Idle);
    qCDebug(lcUi) << "finishPositionEditing: startSfen=" << editedSfen;
    // 編集した局面が開始局面になる（棋譜は positionEditingFinished を受けて作り直す）
    emit positionEditingFinished();
}
