/// @file sessionlifecyclecoordinator.cpp
/// @brief セッションライフサイクル（リセット・終局処理）コーディネータの実装

#include "sessionlifecyclecoordinator.h"

#include "consecutivegamescontroller.h"
#include "gamestatecontroller.h"
#include "logcategories.h"
#include "replaycontroller.h"
#include "shogigamecontroller.h"
#include "timecontrolcontroller.h"

SessionLifecycleCoordinator::SessionLifecycleCoordinator(QObject* parent)
    : QObject(parent)
{
}

void SessionLifecycleCoordinator::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void SessionLifecycleCoordinator::resetToInitialState()
{
    static const QString kHirateStartSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    // 「新規」のため局面情報を初期化してから完全クリーンアップを実行する。
    // performCleanup() は currentSelectedPly > 0 かつ currentSfenStr が
    // 非初期局面の場合、棋譜を保持しようとする（途中局面開始の対局用）。
    // ここで事前にリセットすることで、完全クリアを保証する。
    if (m_deps.startSfenStr) {
        *m_deps.startSfenStr = kHirateStartSfen;
    }
    if (m_deps.currentSfenStr) {
        *m_deps.currentSfenStr = QStringLiteral("startpos");
    }
    if (m_deps.currentSelectedPly) {
        *m_deps.currentSelectedPly = 0;
    }

    if (m_deps.resetEngineState) {
        m_deps.resetEngineState();
    }
    if (m_deps.onPreStartCleanupRequested) {
        m_deps.onPreStartCleanupRequested();
    }

    resetGameState();

    if (m_deps.resetModels) {
        m_deps.resetModels(kHirateStartSfen);
    }
    if (m_deps.resetUiState) {
        m_deps.resetUiState(kHirateStartSfen);
    }
}

void SessionLifecycleCoordinator::resetGameState()
{
    // PlayMode を NotStarted にリセット
    if (m_deps.playMode) {
        *m_deps.playMode = PlayMode::NotStarted;
    }

    // MainWindow 側の m_state/m_player/m_kifu フィールドをクリア
    if (m_deps.clearGameStateFields) {
        m_deps.clearGameStateFields();
    }

    // リプレイコントローラのリセット
    if (m_deps.replayController) {
        m_deps.replayController->setReplayMode(false);
        m_deps.replayController->exitLiveAppendMode();
        m_deps.replayController->setResumeFromCurrent(false);
    }

    // ゲームコントローラの結果リセット
    if (m_deps.gameController) {
        m_deps.gameController->resetResult();
    }

    // ゲーム状態コントローラへの反映
    if (m_deps.gameStateController) {
        m_deps.gameStateController->setPlayMode(PlayMode::NotStarted);
    }
}

void SessionLifecycleCoordinator::startNewGame()
{
    const bool resume = m_deps.replayController
                            ? m_deps.replayController->isResumeFromCurrent()
                            : false;

    // 対局終了時のスタイルロック解除
    if (m_deps.unlockGameOverStyle) {
        m_deps.unlockGameOverStyle();
    }

    // 評価値グラフ等の初期化（途中再開でない場合）
    if (!resume && m_deps.clearEvalState) {
        m_deps.clearEvalState();
    }

    // 対局開始（MatchCoordinator の遅延初期化含む）
    if (m_deps.startGame) {
        m_deps.startGame();
    }
}

void SessionLifecycleCoordinator::applyTimeControl(const GameStartCoordinator::TimeControl& tc)
{
    qCDebug(lcApp).noquote()
        << "applyTimeControl:"
        << " enabled=" << tc.enabled
        << " P1{base=" << tc.p1.baseMs << " byoyomi=" << tc.p1.byoyomiMs << " inc=" << tc.p1.incrementMs << "}"
        << " P2{base=" << tc.p2.baseMs << " byoyomi=" << tc.p2.byoyomiMs << " inc=" << tc.p2.incrementMs << "}";

    // 連続対局用に時間設定を保存
    if (m_deps.lastTimeControl) {
        *m_deps.lastTimeControl = tc;
    }

    // 時間設定の適用を TimeControlController に委譲
    if (m_deps.timeController && m_deps.startSfenStr && m_deps.currentSfenStr) {
        m_deps.timeController->applyTimeControl(
            tc, m_deps.match, *m_deps.startSfenStr, *m_deps.currentSfenStr, m_deps.shogiView);
    }

    // 対局情報ドックに持ち時間を追加
    if (tc.enabled && m_deps.updateGameInfoWithTimeControl) {
        m_deps.updateGameInfoWithTimeControl(
            tc.enabled, tc.p1.baseMs, tc.p1.byoyomiMs, tc.p1.incrementMs);
    }
}

void SessionLifecycleCoordinator::handleGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    // GameStateController への終局通知（UI状態ロック・タイマー停止・手追記等）
    if (m_deps.gameStateController) {
        m_deps.gameStateController->onMatchGameEnded(info);
    }

    // 終局時刻の記録とゲーム情報への反映
    //   onRequestAppendGameOverMove の後で行う（投了手を含めるため）
    if (m_deps.timeController) {
        m_deps.timeController->recordGameEndTime();
        const QDateTime endTime = m_deps.timeController->gameEndDateTime();
        if (endTime.isValid() && m_deps.updateEndTime) {
            m_deps.updateEndTime(endTime);
        }
    }

    // 連続対局判定（EvE モードのみ）
    if (m_deps.playMode) {
        const bool isEvE = (*m_deps.playMode == PlayMode::EvenEngineVsEngine ||
                            *m_deps.playMode == PlayMode::HandicapEngineVsEngine);
        if (isEvE && m_deps.consecutiveGamesController
            && m_deps.consecutiveGamesController->shouldStartNextGame()) {
            qCDebug(lcApp) << "EvE game ended, starting next consecutive game...";
            if (m_deps.startNextConsecutiveGame) {
                m_deps.startNextConsecutiveGame();
            }
        }
    }
}
