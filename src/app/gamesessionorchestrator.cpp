/// @file gamesessionorchestrator.cpp
/// @brief 対局ライフサイクルのイベントオーケストレータ実装

#include "gamesessionorchestrator.h"

#include "consecutivegamescontroller.h"
#include "csagamecoordinator.h"
#include "dialogcoordinator.h"
#include "gamestatecontroller.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "mainwindowgamestartservice.h"
#include "matchcoordinator.h"
#include "prestartcleanuphandler.h"
#include "replaycontroller.h"
#include "sessionlifecyclecoordinator.h"
#include "startgamedatabridge.h"
#include "startgamedialog.h"
#include "timecontrolcontroller.h"

namespace {

/// ダブルポインタを安全にデリファレンスするヘルパー
template<typename T>
T* deref(T** pp) { return pp ? *pp : nullptr; }

/// 局面編集後かどうかを判定する（非平手・非startposの場合 true）
bool detectHasEditedStart(const QString* startSfenStr, const QStringList* sfenRecord)
{
    static const QString kHirateSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const auto isEditedStart = [](const QString& raw) -> bool {
        const QString s = raw.trimmed();
        return !s.isEmpty()
               && s != kHirateSfen
               && s != QLatin1String("startpos");
    };

    if (startSfenStr && isEditedStart(*startSfenStr)) {
        return true;
    }
    if (sfenRecord && !sfenRecord->isEmpty() && isEditedStart(sfenRecord->first())) {
        return true;
    }
    return false;
}

/// StartGameDialog の入力値を StartGameDialogData に変換する
StartGameDialogData extractDialogData(const StartGameDialog& dlg)
{
    StartGameDialogData data;
    data.isHuman1  = dlg.isHuman1();
    data.isHuman2  = dlg.isHuman2();
    data.isEngine1 = dlg.isEngine1();
    data.isEngine2 = dlg.isEngine2();

    data.engineName1 = dlg.engineName1();
    data.engineName2 = dlg.engineName2();
    data.humanName1  = dlg.humanName1();
    data.humanName2  = dlg.humanName2();
    data.engineNumber1 = dlg.engineNumber1();
    data.engineNumber2 = dlg.engineNumber2();

    for (const auto& e : std::as_const(dlg.getEngineList())) {
        StartGameDialogData::Engine eng;
        eng.name = e.name;
        eng.path = e.path;
        data.engineList.append(eng);
    }

    data.basicTimeHour1    = dlg.basicTimeHour1();
    data.basicTimeMinutes1 = dlg.basicTimeMinutes1();
    data.byoyomiSec1       = dlg.byoyomiSec1();
    data.addEachMoveSec1   = dlg.addEachMoveSec1();
    data.basicTimeHour2    = dlg.basicTimeHour2();
    data.basicTimeMinutes2 = dlg.basicTimeMinutes2();
    data.byoyomiSec2       = dlg.byoyomiSec2();
    data.addEachMoveSec2   = dlg.addEachMoveSec2();
    data.isLoseOnTimeout   = dlg.isLoseOnTimeout();

    data.startingPositionNumber = dlg.startingPositionNumber();
    data.maxMoves               = dlg.maxMoves();
    data.autoSaveKifu           = dlg.isAutoSaveKifu();
    data.kifuSaveDir            = dlg.kifuSaveDir();
    data.isShowHumanInFront     = dlg.isShowHumanInFront();
    data.consecutiveGames       = dlg.consecutiveGames();
    data.isSwitchTurnEachGame   = dlg.isSwitchTurnEachGame();
    data.jishogiRule            = dlg.jishogiRule();

    return data;
}

} // anonymous namespace

GameSessionOrchestrator::GameSessionOrchestrator(QObject* parent)
    : QObject(parent)
{
}

void GameSessionOrchestrator::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

// ============================================================
// 対局開始（QAction → actionStartGame）
// ============================================================

void GameSessionOrchestrator::initializeGame()
{
    if (m_deps.ensureGameStartCoordinator) m_deps.ensureGameStartCoordinator();
    auto* gameStart = deref(m_deps.gameStart);
    if (!gameStart) return;

    MainWindowGameStartService::PrepareDeps prep;
    prep.branchTree = m_deps.branchTree;
    prep.navState = m_deps.navState;
    prep.sfenRecord = m_deps.sfenRecord ? m_deps.sfenRecord() : nullptr;
    prep.currentSelectedPly = m_deps.currentSelectedPly ? *m_deps.currentSelectedPly : 0;
    prep.currentSfenStr = m_deps.currentSfenStr;

    const MainWindowGameStartService gameStartService;
    gameStartService.prepareForInitializeGame(prep);

    qCDebug(lcApp).noquote() << "initializeGame: FINAL currentSfenStr="
                             << (m_deps.currentSfenStr ? m_deps.currentSfenStr->left(50) : QStringLiteral("null"))
                             << " startSfenStr=" << (m_deps.startSfenStr ? m_deps.startSfenStr->left(50) : QStringLiteral("null"))
                             << " currentSelectedPly=" << (m_deps.currentSelectedPly ? *m_deps.currentSelectedPly : -1);

    // --- ダイアログ表示（app/ 層の責務） ---
    QStringList* sfenRecPtr = m_deps.sfenRecord ? m_deps.sfenRecord() : nullptr;
    const bool hasEditedStart = detectHasEditedStart(m_deps.startSfenStr, sfenRecPtr);

    StartGameDialog dlg;
    if (hasEditedStart) {
        dlg.forceCurrentPositionSelection();
    }
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    StartGameDialogData dialogData = extractDialogData(dlg);

    // 局面編集後の場合、開始局面を「現在の局面」(0) に強制
    if (hasEditedStart && dialogData.startingPositionNumber != 0) {
        dialogData.startingPositionNumber = 0;
    }

    auto* tc = deref(m_deps.timeController);

    MainWindowGameStartService::ContextDeps ctx;
    ctx.gc = m_deps.gameController;
    ctx.clock = tc ? tc->clock() : nullptr;
    ctx.sfenRecord = sfenRecPtr;
    ctx.kifuModel = m_deps.kifuModel;
    ctx.kifuLoadCoordinator = m_deps.kifuLoadCoordinator;
    ctx.currentSfenStr = m_deps.currentSfenStr;
    ctx.startSfenStr = m_deps.startSfenStr;
    ctx.selectedPly = m_deps.currentSelectedPly ? *m_deps.currentSelectedPly : 0;
    auto* rc = deref(m_deps.replayController);
    ctx.isReplayMode = rc ? rc->isReplayMode() : false;
    ctx.bottomIsP1 = m_deps.bottomIsP1 ? *m_deps.bottomIsP1 : true;

    GameStartCoordinator::Ctx c = gameStartService.buildContext(ctx);
    c.dialogData = dialogData;
    gameStart->initializeGame(c);
}

// ============================================================
// 対局操作（QAction から接続）
// ============================================================

void GameSessionOrchestrator::handleResignation()
{
    // CSA通信対局モードの場合
    auto* csa = deref(m_deps.csaGameCoordinator);
    if (m_deps.playMode && *m_deps.playMode == PlayMode::CsaNetworkMode && csa) {
        csa->onResign();
        return;
    }

    // 通常対局モードの場合
    if (m_deps.ensureGameStateController) m_deps.ensureGameStateController();
    auto* gsc = deref(m_deps.gameStateController);
    if (gsc) {
        gsc->handleResignation();
    }
}

void GameSessionOrchestrator::handleBreakOffGame()
{
    if (m_deps.ensureGameStateController) m_deps.ensureGameStateController();
    auto* gsc = deref(m_deps.gameStateController);
    if (gsc) {
        gsc->handleBreakOffGame();
    }
}

void GameSessionOrchestrator::movePieceImmediately()
{
    auto* mc = deref(m_deps.match);
    if (mc) {
        mc->forceImmediateMove();
    }
}

void GameSessionOrchestrator::stopTsumeSearch()
{
    qCDebug(lcApp).noquote() << "stopTsumeSearch called";
    auto* mc = deref(m_deps.match);
    if (mc) {
        mc->stopAnalysisEngine();
    }
}

void GameSessionOrchestrator::openWebsiteInExternalBrowser()
{
    if (m_deps.ensureDialogCoordinator) m_deps.ensureDialogCoordinator();
    auto* dc = deref(m_deps.dialogCoordinator);
    if (dc) {
        dc->openProjectWebsite();
    }
}

// ============================================================
// 対局ライフサイクルイベント（MatchCoordinatorWiring から接続）
// ============================================================

void GameSessionOrchestrator::onMatchGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    if (m_deps.ensureGameStateController) m_deps.ensureGameStateController();
    if (m_deps.ensureConsecutiveGamesController) m_deps.ensureConsecutiveGamesController();
    if (m_deps.ensureSessionLifecycleCoordinator) m_deps.ensureSessionLifecycleCoordinator();
    auto* slc = deref(m_deps.sessionLifecycle);
    if (slc) {
        slc->handleGameEnded(info);
    }
}

void GameSessionOrchestrator::onGameOverStateChanged(const MatchCoordinator::GameOverState& st)
{
    if (m_deps.ensureGameStateController) m_deps.ensureGameStateController();
    auto* gsc = deref(m_deps.gameStateController);
    if (gsc) {
        gsc->onGameOverStateChanged(st);
    }
}

void GameSessionOrchestrator::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info)
{
    if (m_deps.ensureGameStateController) m_deps.ensureGameStateController();
    auto* gsc = deref(m_deps.gameStateController);
    if (gsc) {
        gsc->onRequestAppendGameOverMove(info);
    }

    auto* lgs = deref(m_deps.liveGameSession);
    if (lgs != nullptr && lgs->isActive()) {
        qCDebug(lcApp) << "onRequestAppendGameOverMove: committing live game session";
        lgs->commit();
    }
}

void GameSessionOrchestrator::onResignationTriggered()
{
    // 既存の投了処理に委譲
    handleResignation();
}

void GameSessionOrchestrator::onPreStartCleanupRequested()
{
    if (m_deps.ensurePreStartCleanupHandler) m_deps.ensurePreStartCleanupHandler();
    auto* psch = deref(m_deps.preStartCleanupHandler);
    if (psch) {
        psch->performCleanup();
    }
    if (m_deps.clearSessionDependentUi) {
        m_deps.clearSessionDependentUi();
    }
}

void GameSessionOrchestrator::onApplyTimeControlRequested(const GameStartCoordinator::TimeControl& tc)
{
    if (m_deps.ensureSessionLifecycleCoordinator) m_deps.ensureSessionLifecycleCoordinator();
    auto* slc = deref(m_deps.sessionLifecycle);
    if (slc) {
        slc->applyTimeControl(tc);
    }
}

void GameSessionOrchestrator::onGameStarted(const MatchCoordinator::StartOptions& opt)
{
    // 対局開始時は MatchCoordinator 側の確定モードをUI側にも同期する。
    if (m_deps.playMode) {
        *m_deps.playMode = opt.mode;
    }

    // 開始局面のSFENを同期（定跡ウィンドウ等が参照する）
    QStringList* sfen = m_deps.sfenRecord ? m_deps.sfenRecord() : nullptr;
    if (m_deps.currentSfenStr) {
        if (!opt.sfenStart.isEmpty()) {
            *m_deps.currentSfenStr = opt.sfenStart;
        } else if (sfen && !sfen->isEmpty()) {
            *m_deps.currentSfenStr = sfen->first();
        }
    }
    if (m_deps.updateJosekiWindow) {
        m_deps.updateJosekiWindow();
    }

    if (m_deps.ensureConsecutiveGamesController) m_deps.ensureConsecutiveGamesController();
    auto* cgc = deref(m_deps.consecutiveGamesController);
    if (cgc && m_deps.lastTimeControl) {
        cgc->onGameStarted(opt, *m_deps.lastTimeControl);
    }
}

// ============================================================
// 連続対局
// ============================================================

void GameSessionOrchestrator::onConsecutiveStartRequested(const GameStartCoordinator::StartParams& params)
{
    if (m_deps.ensureGameStartCoordinator) m_deps.ensureGameStartCoordinator();
    auto* gs = deref(m_deps.gameStart);
    if (gs) {
        gs->start(params);
    }
}

void GameSessionOrchestrator::onConsecutiveGamesConfigured(int totalGames, bool switchTurn)
{
    qCDebug(lcApp).noquote() << "onConsecutiveGamesConfigured: totalGames=" << totalGames
                             << " switchTurn=" << switchTurn;

    if (m_deps.ensureConsecutiveGamesController) m_deps.ensureConsecutiveGamesController();
    auto* cgc = deref(m_deps.consecutiveGamesController);
    if (cgc) {
        cgc->configure(totalGames, switchTurn);
    }
}

void GameSessionOrchestrator::startNextConsecutiveGame()
{
    qCDebug(lcApp).noquote() << "startNextConsecutiveGame (delegated to controller)";

    if (m_deps.ensureConsecutiveGamesController) m_deps.ensureConsecutiveGamesController();
    auto* cgc = deref(m_deps.consecutiveGamesController);
    if (cgc && cgc->shouldStartNextGame()) {
        cgc->startNextGame();
    }
}

// ============================================================
// SessionLifecycleCoordinator コールバック用
// ============================================================

void GameSessionOrchestrator::startNewShogiGame()
{
    if (m_deps.ensureReplayController) m_deps.ensureReplayController();
    if (m_deps.ensureSessionLifecycleCoordinator) m_deps.ensureSessionLifecycleCoordinator();
    auto* slc = deref(m_deps.sessionLifecycle);
    if (slc) {
        slc->startNewGame();
    }
}

// ============================================================
// private
// ============================================================

void GameSessionOrchestrator::invokeStartGame()
{
    auto* mc = deref(m_deps.match);
    if (!mc) {
        if (m_deps.initMatchCoordinator) m_deps.initMatchCoordinator();
        mc = deref(m_deps.match);
    }
    if (!mc) return;

    QStringList* sfen = m_deps.sfenRecord ? m_deps.sfenRecord() : nullptr;
    mc->prepareAndStartGame(
        m_deps.playMode ? *m_deps.playMode : PlayMode::NotStarted,
        m_deps.startSfenStr ? *m_deps.startSfenStr : QString(),
        sfen,
        nullptr,
        m_deps.bottomIsP1 ? *m_deps.bottomIsP1 : true
        );
}
