#include "gamestatecontroller.h"

#include <QDebug>
#include <QAbstractItemView>

#include "shogiview.h"
#include "recordpane.h"
#include "replaycontroller.h"
#include "timecontrolcontroller.h"
#include "kifuloadcoordinator.h"
#include "kifurecordlistmodel.h"
#include "shogiclock.h"

GameStateController::GameStateController(QObject* parent)
    : QObject(parent)
{
}

GameStateController::~GameStateController() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void GameStateController::setMatchCoordinator(MatchCoordinator* match)
{
    m_match = match;
}

void GameStateController::setShogiView(ShogiView* view)
{
    m_shogiView = view;
}

void GameStateController::setRecordPane(RecordPane* pane)
{
    m_recordPane = pane;
}

void GameStateController::setReplayController(ReplayController* replay)
{
    m_replayController = replay;
}

void GameStateController::setTimeController(TimeControlController* tc)
{
    m_timeController = tc;
}

void GameStateController::setKifuLoadCoordinator(KifuLoadCoordinator* kc)
{
    m_kifuLoadCoordinator = kc;
}

void GameStateController::setKifuRecordModel(KifuRecordListModel* model)
{
    m_kifuRecordModel = model;
}

void GameStateController::setPlayMode(PlayMode mode)
{
    m_playMode = mode;
}

// --------------------------------------------------------
// コールバック設定
// --------------------------------------------------------

void GameStateController::setEnableArrowButtonsCallback(EnableArrowButtonsCallback cb)
{
    m_enableArrowButtons = std::move(cb);
}

void GameStateController::setSetReplayModeCallback(SetReplayModeCallback cb)
{
    m_setReplayMode = std::move(cb);
}

void GameStateController::setRefreshBranchTreeCallback(RefreshBranchTreeCallback cb)
{
    m_refreshBranchTree = std::move(cb);
}

void GameStateController::setUpdatePlyStateCallback(UpdatePlyStateCallback cb)
{
    m_updatePlyState = std::move(cb);
}

// --------------------------------------------------------
// 投了・中断処理
// --------------------------------------------------------

void GameStateController::handleResignation()
{
    if (m_match) {
        m_match->handleResign();
    }
}

void GameStateController::handleBreakOffGame()
{
    if (!m_match || m_match->gameOverState().isOver) return;
    m_match->handleBreakOff();
}

// --------------------------------------------------------
// 終局処理
// --------------------------------------------------------

void GameStateController::setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne)
{
    if (!m_match || !m_match->gameOverState().isOver) return;

    m_match->appendGameOverLineAndMark(
        cause,
        loserIsPlayerOne ? MatchCoordinator::P1 : MatchCoordinator::P2);

    // UI 後処理
    if (m_shogiView) {
        m_shogiView->update();
    }
    if (m_recordPane) {
        if (auto* view = m_recordPane->kifuView()) {
            view->setSelectionMode(QAbstractItemView::SingleSelection);
        }
    }
    if (m_setReplayMode) {
        m_setReplayMode(true);
    }
    if (m_replayController) {
        m_replayController->exitLiveAppendMode();
    }
}

// --------------------------------------------------------
// 人間/エンジン判定
// --------------------------------------------------------

bool GameStateController::isHvH() const
{
    return (m_playMode == HumanVsHuman);
}

bool GameStateController::isHumanSide(ShogiGameController::Player p) const
{
    switch (m_playMode) {
    case HumanVsHuman:
        return true;
    case EvenHumanVsEngine:
    case HandicapHumanVsEngine:
        return (p == ShogiGameController::Player1);
    case EvenEngineVsHuman:
    case HandicapEngineVsHuman:
        return (p == ShogiGameController::Player2);
    case EvenEngineVsEngine:
    case HandicapEngineVsEngine:
        return false;
    default:
        return true;
    }
}

bool GameStateController::isGameOver() const
{
    return m_match && m_match->gameOverState().isOver;
}

// --------------------------------------------------------
// シグナルハンドラ
// --------------------------------------------------------

void GameStateController::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    qDebug().nospace()
        << "[GameState] onMatchGameEnded ENTER cause="
        << ((info.cause == MatchCoordinator::Cause::Timeout) ? "Timeout" : "Resign")
        << " loser=" << ((info.loser == MatchCoordinator::P1) ? "P1" : "P2");

    // 対局終了時のスタイル維持を有効化
    if (m_shogiView) {
        m_shogiView->setGameOverStyleLock(true);
    }

    // UI 後始末
    if (m_match) {
        m_match->disarmHumanTimerIfNeeded();
    }
    if (ShogiClock* clk = m_timeController ? m_timeController->clock() : nullptr) {
        clk->stopClock();
    }
    if (m_shogiView) {
        m_shogiView->setMouseClickMode(false);
    }

    // 棋譜追記＋時間確定
    const bool loserIsP1 = (info.loser == MatchCoordinator::P1);
    setGameOverMove(info.cause, loserIsP1);

    // UIの後処理
    if (m_enableArrowButtons) {
        m_enableArrowButtons();
    }
    if (m_recordPane && m_recordPane->kifuView()) {
        m_recordPane->kifuView()->setSelectionMode(QAbstractItemView::SingleSelection);
    }

    qDebug() << "[GameState] onMatchGameEnded LEAVE";
}

void GameStateController::onGameOverStateChanged(const MatchCoordinator::GameOverState& st)
{
    if (!st.isOver) return;

    // 投了行がまだ追加されていない場合は何もしない
    if (!st.moveAppended) {
        return;
    }

    // ライブ追記モードを終了
    if (m_replayController) {
        m_replayController->exitLiveAppendMode();
    }

    // 分岐コンテキストをリセット
    if (m_kifuLoadCoordinator) {
        m_kifuLoadCoordinator->resetBranchContext();
    }

    // 分岐ツリーを再構築
    if (m_refreshBranchTree) {
        m_refreshBranchTree();
    }

    // 対局終了時に現在の手数を設定
    if (m_kifuRecordModel) {
        const int rows = m_kifuRecordModel->rowCount();
        if (rows > 0) {
            const int lastRow = rows - 1;
            if (m_updatePlyState) {
                m_updatePlyState(lastRow, lastRow, lastRow);
            }
        }
    }

    // UI 遷移（閲覧モードへ）
    if (m_enableArrowButtons) {
        m_enableArrowButtons();
    }
    if (m_setReplayMode) {
        m_setReplayMode(true);
    }

    // ハイライト消去
    if (m_shogiView) {
        m_shogiView->removeHighlightAllData();
    }

    Q_EMIT gameOverProcessed();
}

void GameStateController::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info)
{
    const bool loserIsP1 = (info.loser == MatchCoordinator::P1);
    setGameOverMove(info.cause, loserIsP1);
}
