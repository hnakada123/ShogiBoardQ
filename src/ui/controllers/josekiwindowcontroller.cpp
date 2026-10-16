/// @file josekiwindowcontroller.cpp
/// @brief 定跡ウィンドウの表示・更新と、定跡手の着手の実装

#include "josekiwindowcontroller.h"

#include "logcategories.h"

#include "boardmovecontroller.h"
#include "josekiwindow.h"
#include "shogigamecontroller.h"
#include "boardcoordinates.h"
#include "kifunavigationstate.h"

#include <QDockWidget>
#include <functional>

JosekiWindowController::JosekiWindowController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_gameController(deps.gameController)
    , m_navState(deps.navState)
    , m_isHumanTurnNow(deps.isHumanTurnNow)
    , m_boardMoves(deps.boardMoves)
{
    Q_ASSERT(m_gameController && m_navState && m_boardMoves);
    m_josekiWindow = new JosekiWindow(m_parentWidget);

    // 定跡手選択シグナルを接続
    connect(m_josekiWindow, &JosekiWindow::josekiMoveSelected,
            this, &JosekiWindowController::onJosekiMoveSelected);

    // 定跡手を指せるかは、表示を作り直すたびに盤面クリックと同じ判定で問い合わせる
    m_josekiWindow->setHumanCanPlayProvider(std::bind(&JosekiWindowController::determineHumanCanPlay, this));

    // 棋譜データ要求シグナルを接続
    connect(m_josekiWindow, &JosekiWindow::requestKifuDataForMerge,
            this, &JosekiWindowController::onRequestKifuDataForMerge);
    connect(m_navState, &KifuNavigationState::changed, this, &JosekiWindowController::updateJosekiWindow);
}

void JosekiWindowController::attachDock(QDockWidget* dock)
{
    m_dockWidget = dock;
    m_dockShown = dock && dock->isVisible();
    if (dock) {
        connect(dock, &QDockWidget::visibilityChanged,
                this, &JosekiWindowController::onDockVisibilityChanged);
    }
}

void JosekiWindowController::onDockVisibilityChanged(bool visible)
{
    m_dockShown = visible;
    // 裏に回っている間に進んだ局面は、前面に戻ったときに反映する
    if (visible) updateJosekiWindow();
}

void JosekiWindowController::updateJosekiWindow()
{
    // 定跡ウィンドウが存在し、表示されている場合のみ更新。タブの裏に回ったドックの中身は
    // isVisible() が真のままなので、ドックが前面にあるかも見る（1手ごとに呼ばれる）
    if (!m_josekiWindow->isVisible()) {
        return;
    }
    if (m_dockWidget && !m_dockShown) return;

    // 表示中の手の局面を定跡ウィンドウに設定する。人間が着手可能かどうかは、定跡ウィンドウが
    // 表示を作り直すたびに setHumanCanPlayProvider の判定で問い合わせる
    m_josekiWindow->setCurrentSfen(m_navState->currentSfen());
}

bool JosekiWindowController::determineHumanCanPlay() const
{
    // 盤面クリックと同じ判定を使う。対局中は人間の手番だけ、対局外（終局後を含む）は
    // 指した手を棋譜の変化として記録できる待機中だけ指せる
    return m_isHumanTurnNow && m_isHumanTurnNow();
}

void JosekiWindowController::onJosekiMoveSelected(const QString& usiMove)
{
    qCDebug(lcUi) << "onJosekiMoveSelected: usiMove=" << usiMove;

    if (usiMove.isEmpty()) {
        qCDebug(lcUi) << "onJosekiMoveSelected: empty move";
        return;
    }
    // 最後に表示を更新した後で終局・手番交代していれば、着手ボタンを消して何もしない
    if (!determineHumanCanPlay()) {
        m_josekiWindow->setHumanCanPlay(false);
        return;
    }

    const auto move = UsiMove::parse(usiMove);
    if (!move) {
        qCWarning(lcUi) << "onJosekiMoveSelected: invalid move format";
        m_josekiWindow->showMoveFailed(usiMove);
        return;
    }
    // 定跡手は成り・不成が決まっているので、成りの選択を出さずにそのまま指す
    const bool moveSuccess = m_boardMoves->playUsiMove(*move);
    qCDebug(lcUi) << "Move result: success=" << moveSuccess;
    if (!moveSuccess) m_josekiWindow->showMoveFailed(usiMove);
}

void JosekiWindowController::onRequestKifuDataForMerge()
{
    // 表示中の手順（分岐を選んでいればその分岐）の局面・指し手・表記をそろえて渡す
    const RecordLine line = m_navState ? m_navState->currentRecordLine() : RecordLine{};
    qCDebug(lcUi) << "onRequestKifuDataForMerge: moves=" << line.moveCount()
                  << "currentPly=" << line.currentPly;
    m_josekiWindow->setKifuDataForMerge(line.sfens(), line.usiMoves(), line.displayTexts(), line.currentPly);
}
