/// @file sessionlifecyclecoordinator.cpp
/// @brief セッションのライフサイクル（リセット・開始・終局）コーディネータの実装

#include "sessionlifecyclecoordinator.h"

#include "appstate.h"
#include "consecutivegamescontroller.h"
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "gamestatecontroller.h"
#include "josekiwindowwiring.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "playerinfowiring.h"
#include "prestartcleanuphandler.h"
#include "replaycontroller.h"
#include "sfenutils.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"

SessionLifecycleCoordinator::SessionLifecycleCoordinator(QObject* parent)
    : QObject(parent)
{
}

void SessionLifecycleCoordinator::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void SessionLifecycleCoordinator::stopEnginesAndNetwork()
{
    m_deps.match->stopAnalysisEngine();
    m_deps.match->clearGameOverState();
    if (CsaGameCoordinator* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr) {
        csa->stopGame();
    }
    m_deps.consecutiveGamesController->reset();
}

void SessionLifecycleCoordinator::resetToInitialState()
{
    const QString hirateStartSfen = SfenUtils::hirateSfen();

    // 「新規」のため局面情報を初期化してから完全クリーンアップを実行する。
    // performCleanup() は表示中の手数が 0 より大きく currentSfenStr が
    // 非初期局面の場合、棋譜を保持しようとする（途中局面開始の対局用）。
    // currentSfenStr を初期局面にしておけば、表示中の手数によらず完全クリアになる。
    m_deps.state->startSfenStr = hirateStartSfen;
    m_deps.state->currentSfenStr = QStringLiteral("startpos");
    stopEnginesAndNetwork();
    performPreStartCleanup();
    resetGameState();
    resetModels(hirateStartSfen);
    resetUiState(hirateStartSfen);
}

void SessionLifecycleCoordinator::resetGameState()
{
    m_deps.state->playMode = PlayMode::NotStarted;
    MainWindowResetService().clearGameStateFields(*m_deps.state);
    m_deps.replayController->setReplayMode(false);
    m_deps.replayController->setResumeFromCurrent(false);
    m_deps.gameController->resetResult();
}

void SessionLifecycleCoordinator::performPreStartCleanup()
{
    m_deps.preStartCleanupHandler->performCleanup();
    clearSessionDependentUi();
}

void SessionLifecycleCoordinator::clearSessionDependentUi()
{
    MainWindowResetService().clearSessionDependentUi(m_deps.sessionUi);
}

void SessionLifecycleCoordinator::clearUiBeforeKifuLoad()
{
    MainWindowResetService().clearUiBeforeKifuLoad(m_deps.sessionUi);
}

void SessionLifecycleCoordinator::resetModels(const QString& sfen)
{
    MainWindowResetService().resetModels(m_deps.models, sfen);
}

void SessionLifecycleCoordinator::resetUiState(const QString& sfen)
{
    MainWindowResetService::UiResetDeps deps;
    deps.gameController = m_deps.gameController;
    deps.shogiView = m_deps.shogiView;
    deps.uiStatePolicy = m_deps.uiStatePolicy;
    deps.josekiWiring = m_deps.josekiWiring;
    deps.playerInfoWiring = m_deps.playerInfoWiring;
    MainWindowResetService().resetUiState(deps, sfen);
}

void SessionLifecycleCoordinator::clearEvalState()
{
    if (m_deps.sessionUi.evalChart) m_deps.sessionUi.evalChart->clearAll();
    if (m_deps.sessionUi.evalGraphController) m_deps.sessionUi.evalGraphController->clearScores();
}

void SessionLifecycleCoordinator::prepareNextGame(const QString& startSfen)
{
    const QString sfen = startSfen.isEmpty() || startSfen == QLatin1String("startpos")
                             ? SfenUtils::hirateSfen() : startSfen;
    m_deps.state->startSfenStr = sfen;
    m_deps.state->currentSfenStr = sfen;

    resetGameState();
    resetModels(sfen);
    performPreStartCleanup();
    m_deps.shogiView->setGameOverStyleLock(false);
}

void SessionLifecycleCoordinator::applyTimeControl(const GameStartCoordinator::TimeSettings& tc)
{
    qCDebug(lcApp).noquote()
        << "applyTimeControl:"
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.rules.black.mainMs << " byoyomi=" << tc.rules.black.byoyomiMs << " inc=" << tc.rules.black.incrementMs << "}"
        << " P2{base=" << tc.rules.white.mainMs << " byoyomi=" << tc.rules.white.byoyomiMs << " inc=" << tc.rules.white.incrementMs << "}";

    // 連続対局用に時間設定を保存
    m_lastTimeControl = tc;

    m_deps.timeController->applyTimeControl(
        tc, m_deps.state->startSfenStr, m_deps.state->currentSfenStr, m_deps.shogiView);

    // 対局情報ドックに持ち時間を追加
    if (tc.enabled) {
        m_deps.playerInfoWiring->updateGameInfoWithTimeControl(tc.rules);
    }
}

void SessionLifecycleCoordinator::handleGameStarted(const MatchCoordinator::StartOptions& info)
{
    m_deps.state->playMode = info.mode;

    if (!info.sfenStart.isEmpty()) {
        m_deps.state->currentSfenStr = info.sfenStart;
    } else if (!m_deps.state->sfenRecord.isEmpty()) {
        m_deps.state->currentSfenStr = m_deps.state->sfenRecord.first();
    }

    m_deps.josekiWiring->updateJosekiWindow();
    m_deps.consecutiveGamesController->onGameStarted(info, m_lastTimeControl);
}

void SessionLifecycleCoordinator::commitLiveGameSessionIfActive()
{
    LiveGameSession* session = m_deps.models.liveGameSession;
    if (session != nullptr && session->isActive()) {
        qCDebug(lcApp) << "commitLiveGameSessionIfActive: committing live game session";
        session->commit();
    }
}

void SessionLifecycleCoordinator::handleGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    // GameStateController への終局通知（UI状態ロック・タイマー停止・手追記等）
    m_deps.gameStateController->onMatchGameEnded(info);

    // 終局時刻の記録とゲーム情報への反映（投了手を含めるため、終局手の追記の後で行う）
    m_deps.timeController->recordGameEndTime();
    const QDateTime endTime = m_deps.timeController->gameEndDateTime();
    if (endTime.isValid()) {
        m_deps.playerInfoWiring->updateGameInfoWithEndTime(endTime);
    }

    if (info.cause == MatchCoordinator::Cause::BreakOff) {
        m_deps.consecutiveGamesController->reset();
    }
}

void SessionLifecycleCoordinator::handleGameEndProcessed(const MatchCoordinator::GameEndInfo& info)
{
    if (info.cause == MatchCoordinator::Cause::BreakOff) return;

    // 終局表示・保存が完了してから次局を予約する（EvE モードのみ）
    const PlayMode mode = m_deps.state->playMode;
    const bool isEvE = (mode == PlayMode::EvenEngineVsEngine || mode == PlayMode::HandicapEngineVsEngine);
    if (isEvE && m_deps.consecutiveGamesController->shouldStartNextGame()) {
        qCDebug(lcApp) << "EvE game ended, starting next consecutive game...";
        m_deps.consecutiveGamesController->startNextGame();
    }
}
