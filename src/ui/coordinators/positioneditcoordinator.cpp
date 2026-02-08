/// @file positioneditcoordinator.cpp
/// @brief 局面編集コーディネータクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "positioneditcoordinator.h"

#include <QDebug>
#include <QAction>

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

void PositionEditCoordinator::setSfenRecord(QStringList* sfenRecord)
{
    m_sfenRecord = sfenRecord;
}

// --------------------------------------------------------
// 状態参照の設定
// --------------------------------------------------------

void PositionEditCoordinator::setCurrentSelectedPly(const int* currentSelectedPly)
{
    m_currentSelectedPly = currentSelectedPly;
}

void PositionEditCoordinator::setActivePly(const int* activePly)
{
    m_activePly = activePly;
}

void PositionEditCoordinator::setStartSfenStr(QString* startSfenStr)
{
    m_startSfenStr = startSfenStr;
}

void PositionEditCoordinator::setCurrentSfenStr(QString* currentSfenStr)
{
    m_currentSfenStr = currentSfenStr;
}

void PositionEditCoordinator::setResumeSfenStr(QString* resumeSfenStr)
{
    m_resumeSfenStr = resumeSfenStr;
}

void PositionEditCoordinator::setOnMainRowGuard(bool* onMainRowGuard)
{
    m_onMainRowGuard = onMainRowGuard;
}

// --------------------------------------------------------
// コールバック設定
// --------------------------------------------------------

void PositionEditCoordinator::setApplyEditMenuStateCallback(ApplyEditMenuStateCallback cb)
{
    m_applyEditMenuState = std::move(cb);
}

void PositionEditCoordinator::setEnsurePositionEditCallback(EnsurePositionEditCallback cb)
{
    m_ensurePositionEdit = std::move(cb);
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
    if (m_ensurePositionEdit) {
        m_ensurePositionEdit();
    }

    if (!m_posEdit || !m_shogiView || !m_gameController) return;

    PositionEditController::BeginEditContext ctx;
    ctx.view       = m_shogiView;
    ctx.gc         = m_gameController;
    ctx.bic        = m_boardController;
    ctx.sfenRecord = m_sfenRecord;

    ctx.selectedPly = m_currentSelectedPly ? *m_currentSelectedPly : 0;
    ctx.activePly   = m_activePly ? *m_activePly : 0;
    ctx.gameOver    = (m_match ? m_match->gameOverState().isOver : false);

    ctx.startSfenStr    = m_startSfenStr;
    ctx.currentSfenStr  = m_currentSfenStr;
    ctx.resumeSfenStr   = m_resumeSfenStr;

    // メニュー表示：編集メニューに遷移
    ctx.onEnterEditMenu = [this]() {
        if (m_applyEditMenuState) {
            m_applyEditMenuState(true);
        }
    };

    // 「編集終了」ボタン表示
    ctx.onShowEditExitButton = [this]() {
        if (m_posEdit && m_shogiView) {
            m_posEdit->showEditExitButtonOnBoard(m_shogiView, this, SLOT(finishPositionEditing()));
        }
    };

    // 実行
    m_posEdit->beginPositionEditing(ctx);

    // 編集用アクション配線
    if (m_editActions.actionReturnAllPiecesToStand) {
        QObject::connect(m_editActions.actionReturnAllPiecesToStand, &QAction::triggered,
                         m_posEdit, &PositionEditController::onReturnAllPiecesOnStandTriggered,
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

    // 先後反転はシグナルで通知
    if (m_editActions.actionSwapSides) {
        QObject::connect(m_editActions.actionSwapSides, &QAction::triggered,
                         this, &PositionEditCoordinator::reversalTriggered,
                         Qt::UniqueConnection);
    }
}

void PositionEditCoordinator::finishPositionEditing()
{
    // 自動同期を一時停止
    bool prevGuard = false;
    if (m_onMainRowGuard) {
        prevGuard = *m_onMainRowGuard;
        *m_onMainRowGuard = true;
    }

    if (m_ensurePositionEdit) {
        m_ensurePositionEdit();
    }

    if (!m_posEdit || !m_shogiView || !m_gameController) {
        if (m_onMainRowGuard) {
            *m_onMainRowGuard = prevGuard;
        }
        return;
    }

    // Controller に委譲
    PositionEditController::FinishEditContext ctx;
    ctx.view       = m_shogiView;
    ctx.gc         = m_gameController;
    ctx.bic        = m_boardController;
    ctx.sfenRecord = m_sfenRecord;
    ctx.startSfenStr        = m_startSfenStr;
    ctx.isResumeFromCurrent = nullptr;

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
        if (m_applyEditMenuState) {
            m_applyEditMenuState(false);
        }
    };

    // 実行
    m_posEdit->finishPositionEditing(ctx);

    // 自動同期を再開
    if (m_onMainRowGuard) {
        *m_onMainRowGuard = prevGuard;
    }

    qDebug() << "[PosEditCoord] finishPositionEditing: editMode="
             << (m_shogiView ? m_shogiView->positionEditMode() : false)
             << " m_startSfenStr=" << (m_startSfenStr ? *m_startSfenStr : QString());
}
