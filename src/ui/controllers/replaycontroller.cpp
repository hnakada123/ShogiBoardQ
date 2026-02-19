/// @file replaycontroller.cpp
/// @brief 棋譜再生コントローラクラスの実装

#include "replaycontroller.h"

#include "loggingcategory.h"
#include <QTableView>
#include <QAbstractItemView>

#include "shogiclock.h"
#include "shogiview.h"
#include "shogigamecontroller.h"
#include "matchcoordinator.h"
#include "recordpane.h"

ReplayController::ReplayController(QObject* parent)
    : QObject(parent)
{
}

ReplayController::~ReplayController() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void ReplayController::setClock(ShogiClock* clock)
{
    m_clock = clock;
}

void ReplayController::setShogiView(ShogiView* view)
{
    m_view = view;
}

void ReplayController::setGameController(ShogiGameController* gc)
{
    m_gc = gc;
}

void ReplayController::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void ReplayController::setRecordPane(RecordPane* pane)
{
    m_recordPane = pane;
}

// --------------------------------------------------------
// リプレイモード管理
// --------------------------------------------------------

void ReplayController::setReplayMode(bool on)
{
    if (m_isReplayMode == on) return;

    m_isReplayMode = on;

    qCDebug(lcUi).noquote() << "setReplayMode:" << on;

    // 再生中は時計を止め、表示だけ整える
    if (m_clock) {
        m_clock->stopClock();
        m_clock->updateClock();  // 表示だけは最新化
    }

    if (m_match) {
        m_match->pokeTimeUpdateNow();  // 残時間ラベル等の静的更新だけ反映
    }

    // 再生モードの入/出でハイライト方針を切替
    if (m_view) {
        m_view->setUiMuted(on);
        if (on) {
            m_view->clearTurnHighlight();  // 中立に
        } else {
            // 対局に戻る: 現手番・残時間から再適用
            const bool p1turn = (m_gc &&
                                 m_gc->currentPlayer() == ShogiGameController::Player1);
            m_view->setActiveSide(p1turn);

            // Urgency は時計側の更新イベントで再適用させる
            if (m_clock) {
                m_clock->updateClock();
            }
        }
    }

}

bool ReplayController::isReplayMode() const
{
    return m_isReplayMode;
}

// --------------------------------------------------------
// ライブ追記モード管理
// --------------------------------------------------------

void ReplayController::enterLiveAppendMode()
{
    if (m_isLiveAppendMode) return;

    m_isLiveAppendMode = true;

    qCDebug(lcUi).noquote() << "enterLiveAppendMode";

    // 棋譜ビューの選択を無効化（ライブ中はスクロール追従のみ）
    if (m_recordPane) {
        if (QTableView* view = m_recordPane->kifuView()) {
            view->setSelectionMode(QAbstractItemView::NoSelection);
            view->setFocusPolicy(Qt::NoFocus);
        }
    }

}

void ReplayController::exitLiveAppendMode()
{
    if (!m_isLiveAppendMode) return;

    m_isLiveAppendMode = false;

    qCDebug(lcUi).noquote() << "exitLiveAppendMode";

    // 棋譜ビューの選択を再有効化
    if (m_recordPane) {
        if (QTableView* view = m_recordPane->kifuView()) {
            view->setSelectionMode(QAbstractItemView::SingleSelection);
            view->setFocusPolicy(Qt::StrongFocus);
        }
    }

}

bool ReplayController::isLiveAppendMode() const
{
    return m_isLiveAppendMode;
}

// --------------------------------------------------------
// 中断からの再開モード
// --------------------------------------------------------

void ReplayController::setResumeFromCurrent(bool on)
{
    m_isResumeFromCurrent = on;
    qCDebug(lcUi).noquote() << "setResumeFromCurrent:" << on;
}

bool ReplayController::isResumeFromCurrent() const
{
    return m_isResumeFromCurrent;
}
