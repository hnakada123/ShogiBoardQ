/// @file mainwindowwiringassembler.cpp
/// @brief WiringAssembler系メソッドの実装（ServiceRegistry統合版）
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowmatchadapter.h"
#include "mainwindowmatchwiringdepsservice.h"
#include "mainwindowsignalrouter.h"
#include "dialoglaunchwiring.h"
#include "playerinfowiring.h"
#include "kifufilecontroller.h"
#include "timecontrolcontroller.h"
#include "kifunavigationcoordinator.h"
#include "gamerecordupdateservice.h"
#include "matchruntimequeryservice.h"

MatchCoordinatorWiring::Deps MainWindowServiceRegistry::buildMatchWiringDeps()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    MainWindowMatchWiringDepsService::Inputs in;
    in.evalGraphController = m_mw.m_evalGraphController.get();
    in.onTurnChanged = std::bind(&MainWindow::onTurnManagerChanged, &m_mw, _1);
    in.sendGo = [this](Usi* which, const MatchCoordinator::GoTimes& t) {
        if (m_mw.m_match) m_mw.m_match->sendGoToEngine(which, t);
    };
    in.sendStop = [this](Usi* which) {
        if (m_mw.m_match) m_mw.m_match->sendStopToEngine(which);
    };
    in.sendRaw = [this](Usi* which, const QString& cmd) {
        if (m_mw.m_match) m_mw.m_match->sendRawToEngine(which, cmd);
    };
    auto* adapter = m_mw.m_matchAdapter.get();
    in.initializeNewGame = std::bind(&MainWindowMatchAdapter::initializeNewGameHook, adapter, _1);
    in.renderBoard = std::bind(&MainWindowMatchAdapter::renderBoardFromGc, adapter);
    in.showMoveHighlights = std::bind(&MainWindowMatchAdapter::showMoveHighlights, adapter, _1, _2);
    in.appendKifuLine = [this](const QString& text, const QString& elapsed) {
        ensureGameRecordUpdateService();
        if (m_mw.m_gameRecordUpdateService) {
            m_mw.m_gameRecordUpdateService->appendKifuLine(text, elapsed);
        }
    };
    in.showGameOverDialog = std::bind(&MainWindowMatchAdapter::showGameOverMessageBox, adapter, _1, _2);
    in.remainingMsFor = std::bind(&MatchRuntimeQueryService::getRemainingMsFor, m_mw.m_queryService.get(), _1);
    in.incrementMsFor = std::bind(&MatchRuntimeQueryService::getIncrementMsFor, m_mw.m_queryService.get(), _1);
    in.byoyomiMs = std::bind(&MatchRuntimeQueryService::getByoyomiMs, m_mw.m_queryService.get());
    in.setPlayersNames = std::bind(&PlayerInfoWiring::onSetPlayersNames, m_mw.m_playerInfoWiring, _1, _2);
    in.setEngineNames = std::bind(&PlayerInfoWiring::onSetEngineNames, m_mw.m_playerInfoWiring, _1, _2);
    ensureKifuFileController();
    in.autoSaveKifu = std::bind(&KifuFileController::autoSaveKifuToFile, m_mw.m_kifuFileController, _1, _2, _3, _4, _5, _6);

    ensureKifuNavigationCoordinator();
    in.updateHighlightsForPly = std::bind(&KifuNavigationCoordinator::syncBoardAndHighlightsAtRow, m_mw.m_kifuNavCoordinator.get(), _1);
    in.updateTurnAndTimekeepingDisplay = std::bind(&MainWindowMatchAdapter::updateTurnAndTimekeepingDisplay, adapter);
    in.isHumanSide = std::bind(&MatchRuntimeQueryService::isHumanSide, m_mw.m_queryService.get(), _1);

    in.gc    = m_mw.m_gameController;
    in.view  = m_mw.m_shogiView;
    in.usi1  = m_mw.m_usi1;
    in.usi2  = m_mw.m_usi2;
    in.comm1  = m_mw.m_models.commLog1;
    in.think1 = m_mw.m_models.thinking1;
    in.comm2  = m_mw.m_models.commLog2;
    in.think2 = m_mw.m_models.thinking2;
    in.sfenRecord = m_mw.m_queryService->sfenRecord();
    in.playMode   = &m_mw.m_state.playMode;
    in.timePresenter   = m_mw.m_timePresenter;
    in.boardController = m_mw.m_boardController;
    in.kifuRecordModel  = m_mw.m_models.kifuRecord;
    in.gameMoves        = &m_mw.m_kifu.gameMoves;
    in.positionStrList  = &m_mw.m_kifu.positionStrList;
    in.currentMoveIndex = &m_mw.m_state.currentMoveIndex;

    in.ensureTimeController           = std::bind(&MainWindowServiceRegistry::ensureTimeController, this);
    in.ensureEvaluationGraphController = std::bind(&MainWindowServiceRegistry::ensureEvaluationGraphController, this);
    in.ensurePlayerInfoWiring         = std::bind(&MainWindowServiceRegistry::ensurePlayerInfoWiring, this);
    in.ensureUsiCommandController     = std::bind(&MainWindowServiceRegistry::ensureUsiCommandController, this);
    in.ensureUiStatePolicyManager     = std::bind(&MainWindowServiceRegistry::ensureUiStatePolicyManager, this);
    in.connectBoardClicks             = std::bind(&MainWindowSignalRouter::connectBoardClicks, m_mw.m_signalRouter.get());
    in.connectMoveRequested           = std::bind(&MainWindowSignalRouter::connectMoveRequested, m_mw.m_signalRouter.get());

    in.getClock = [this]() -> ShogiClock* {
        return m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr;
    };
    in.getTimeController = [this]() -> TimeControlController* {
        return m_mw.m_timeController;
    };
    in.getEvalGraphController = [this]() -> EvaluationGraphController* {
        return m_mw.m_evalGraphController.get();
    };
    in.getUiStatePolicy = [this]() -> UiStatePolicyManager* {
        return m_mw.m_uiStatePolicy;
    };

    const MainWindowMatchWiringDepsService depsService;
    return depsService.buildDeps(in);
}

void MainWindowServiceRegistry::initializeDialogLaunchWiring()
{
    DialogLaunchWiring::Deps d;
    d.parentWidget = &m_mw;
    d.playMode = &m_mw.m_state.playMode;

    // 遅延初期化ゲッター
    d.getDialogCoordinator = [this]() { ensureDialogCoordinator(); return m_mw.m_dialogCoordinator; };
    d.getGameController    = [this]() { return m_mw.m_gameController; };
    d.getMatch             = [this]() { return m_mw.m_match; };
    d.getShogiView         = [this]() { return m_mw.m_shogiView; };
    d.getJishogiController = [this]() { ensureJishogiController(); return m_mw.m_jishogiController.get(); };
    d.getNyugyokuHandler   = [this]() { ensureNyugyokuHandler(); return m_mw.m_nyugyokuHandler.get(); };
    d.getCsaGameWiring     = [this]() { ensureCsaGameWiring(); return m_mw.m_csaGameWiring.get(); };
    d.getBoardSetupController = [this]() { ensureBoardSetupController(); return m_mw.m_boardSetupController; };
    d.getPlayerInfoWiring  = [this]() { ensurePlayerInfoWiring(); return m_mw.m_playerInfoWiring; };
    d.getAnalysisPresenter = [this]() { ensureAnalysisPresenter(); return m_mw.m_analysisPresenter.get(); };
    d.getUsi1              = [this]() { return m_mw.m_usi1; };
    d.getAnalysisTab       = [this]() { return m_mw.m_analysisTab; };
    d.getLineEditModel1    = [this]() { return m_mw.m_models.commLog1; };
    d.getModelThinking1    = [this]() { return m_mw.m_models.thinking1; };
    d.getKifuRecordModel   = [this]() { return m_mw.m_models.kifuRecord; };
    d.getKifuLoadCoordinator = [this]() { return m_mw.m_kifuLoadCoordinator; };
    d.getEvalChart         = [this]() { return m_mw.m_evalChart; };
    d.getSfenRecord        = [this]() { return m_mw.m_queryService->sfenRecord(); };

    // 値型メンバーへのポインタ
    d.moveRecords   = &m_mw.m_kifu.moveRecords;
    d.gameUsiMoves  = &m_mw.m_kifu.gameUsiMoves;
    d.activePly     = &m_mw.m_kifu.activePly;

    // ダブルポインタ
    d.csaGameDialog      = &m_mw.m_csaGameDialog;
    d.csaGameCoordinator = &m_mw.m_csaGameCoordinator;
    d.gameInfoController = &m_mw.m_gameInfoController;
    d.analysisModel      = &m_mw.m_models.analysis;

    // メニューウィンドウドック
    d.menuWindowDock     = &m_mw.m_docks.menuWindow;
    d.createMenuWindowDock = [this]() { createMenuWindowDock(); };

    // 局面集ダイアログ
    d.sfenCollectionDialog = &m_mw.m_sfenCollectionDialog;

    // CSA通信対局のエンジン評価値グラフ用
    d.getCsaGameCoordinator = [this]() { return m_mw.m_csaGameCoordinator; };

    m_mw.m_dialogLaunchWiring = new DialogLaunchWiring(d, &m_mw);

    // シグナル中継: SFEN局面集の選択をKifuFileControllerに接続
    ensureKifuFileController();
    QObject::connect(m_mw.m_dialogLaunchWiring, &DialogLaunchWiring::sfenCollectionPositionSelected,
                     m_mw.m_kifuFileController, &KifuFileController::onSfenCollectionPositionSelected);
}
