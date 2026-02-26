/// @file mainwindowwiringassembler.cpp
/// @brief MainWindow の配線組み立てロジック実装

#include "mainwindowwiringassembler.h"
#include "mainwindow.h"
#include "mainwindowmatchadapter.h"
#include "mainwindowmatchwiringdepsservice.h"
#include "mainwindowsignalrouter.h"
#include "mainwindowserviceregistry.h"
#include "dialoglaunchwiring.h"
#include "playerinfowiring.h"
#include "kifufilecontroller.h"
#include "timecontrolcontroller.h"
#include "kifunavigationcoordinator.h"
#include "gamerecordupdateservice.h"
#include "matchruntimequeryservice.h"

MatchCoordinatorWiring::Deps MainWindowWiringAssembler::buildMatchWiringDeps(MainWindow& mw)
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    MainWindowMatchWiringDepsService::Inputs in;
    in.evalGraphController = mw.m_evalGraphController;
    in.onTurnChanged = std::bind(&MainWindow::onTurnManagerChanged, &mw, _1);
    in.sendGo = [&mw](Usi* which, const MatchCoordinator::GoTimes& t) {
        if (mw.m_match) mw.m_match->sendGoToEngine(which, t);
    };
    in.sendStop = [&mw](Usi* which) {
        if (mw.m_match) mw.m_match->sendStopToEngine(which);
    };
    in.sendRaw = [&mw](Usi* which, const QString& cmd) {
        if (mw.m_match) mw.m_match->sendRawToEngine(which, cmd);
    };
    auto* adapter = mw.m_matchAdapter.get();
    in.initializeNewGame = std::bind(&MainWindowMatchAdapter::initializeNewGameHook, adapter, _1);
    in.renderBoard = std::bind(&MainWindowMatchAdapter::renderBoardFromGc, adapter);
    in.showMoveHighlights = std::bind(&MainWindowMatchAdapter::showMoveHighlights, adapter, _1, _2);
    in.appendKifuLine = [&mw](const QString& text, const QString& elapsed) {
        mw.m_registry->ensureGameRecordUpdateService();
        if (mw.m_gameRecordUpdateService) {
            mw.m_gameRecordUpdateService->appendKifuLine(text, elapsed);
        }
    };
    in.showGameOverDialog = std::bind(&MainWindowMatchAdapter::showGameOverMessageBox, adapter, _1, _2);
    in.remainingMsFor = std::bind(&MatchRuntimeQueryService::getRemainingMsFor, mw.m_queryService, _1);
    in.incrementMsFor = std::bind(&MatchRuntimeQueryService::getIncrementMsFor, mw.m_queryService, _1);
    in.byoyomiMs = std::bind(&MatchRuntimeQueryService::getByoyomiMs, mw.m_queryService);
    in.setPlayersNames = std::bind(&PlayerInfoWiring::onSetPlayersNames, mw.m_playerInfoWiring, _1, _2);
    in.setEngineNames = std::bind(&PlayerInfoWiring::onSetEngineNames, mw.m_playerInfoWiring, _1, _2);
    mw.ensureKifuFileController();
    in.autoSaveKifu = std::bind(&KifuFileController::autoSaveKifuToFile, mw.m_kifuFileController, _1, _2, _3, _4, _5, _6);

    mw.ensureKifuNavigationCoordinator();
    in.updateHighlightsForPly = std::bind(&KifuNavigationCoordinator::syncBoardAndHighlightsAtRow, mw.m_kifuNavCoordinator, _1);
    in.updateTurnAndTimekeepingDisplay = std::bind(&MainWindowMatchAdapter::updateTurnAndTimekeepingDisplay, adapter);
    in.isHumanSide = std::bind(&MatchRuntimeQueryService::isHumanSide, mw.m_queryService, _1);

    in.gc    = mw.m_gameController;
    in.view  = mw.m_shogiView;
    in.usi1  = mw.m_usi1;
    in.usi2  = mw.m_usi2;
    in.comm1  = mw.m_models.commLog1;
    in.think1 = mw.m_models.thinking1;
    in.comm2  = mw.m_models.commLog2;
    in.think2 = mw.m_models.thinking2;
    in.sfenRecord = mw.m_queryService->sfenRecord();
    in.playMode   = &mw.m_state.playMode;
    in.timePresenter   = mw.m_timePresenter;
    in.boardController = mw.m_boardController;
    in.kifuRecordModel  = mw.m_models.kifuRecord;
    in.gameMoves        = &mw.m_kifu.gameMoves;
    in.positionStrList  = &mw.m_kifu.positionStrList;
    in.currentMoveIndex = &mw.m_state.currentMoveIndex;

    in.ensureTimeController           = std::bind(&MainWindow::ensureTimeController, &mw);
    in.ensureEvaluationGraphController = std::bind(&MainWindowServiceRegistry::ensureEvaluationGraphController, mw.m_registry);
    in.ensurePlayerInfoWiring         = std::bind(&MainWindow::ensurePlayerInfoWiring, &mw);
    in.ensureUsiCommandController     = std::bind(&MainWindowServiceRegistry::ensureUsiCommandController, mw.m_registry);
    in.ensureUiStatePolicyManager     = std::bind(&MainWindow::ensureUiStatePolicyManager, &mw);
    in.connectBoardClicks             = std::bind(&MainWindowSignalRouter::connectBoardClicks, mw.m_signalRouter);
    in.connectMoveRequested           = std::bind(&MainWindowSignalRouter::connectMoveRequested, mw.m_signalRouter);

    in.getClock = [&mw]() -> ShogiClock* {
        return mw.m_timeController ? mw.m_timeController->clock() : nullptr;
    };
    in.getTimeController = [&mw]() -> TimeControlController* {
        return mw.m_timeController;
    };
    in.getEvalGraphController = [&mw]() -> EvaluationGraphController* {
        return mw.m_evalGraphController;
    };
    in.getUiStatePolicy = [&mw]() -> UiStatePolicyManager* {
        return mw.m_uiStatePolicy;
    };

    const MainWindowMatchWiringDepsService depsService;
    return depsService.buildDeps(in);
}

void MainWindowWiringAssembler::initializeDialogLaunchWiring(MainWindow& mw)
{
    DialogLaunchWiring::Deps d;
    d.parentWidget = &mw;
    d.playMode = &mw.m_state.playMode;

    // 遅延初期化ゲッター
    d.getDialogCoordinator = [&mw]() { mw.ensureDialogCoordinator(); return mw.m_dialogCoordinator; };
    d.getGameController    = [&mw]() { return mw.m_gameController; };
    d.getMatch             = [&mw]() { return mw.m_match; };
    d.getShogiView         = [&mw]() { return mw.m_shogiView; };
    d.getJishogiController = [&mw]() { mw.m_registry->ensureJishogiController(); return mw.m_jishogiController; };
    d.getNyugyokuHandler   = [&mw]() { mw.m_registry->ensureNyugyokuHandler(); return mw.m_nyugyokuHandler; };
    d.getCsaGameWiring     = [&mw]() { mw.m_registry->ensureCsaGameWiring(); return mw.m_csaGameWiring; };
    d.getBoardSetupController = [&mw]() { mw.ensureBoardSetupController(); return mw.m_boardSetupController; };
    d.getPlayerInfoWiring  = [&mw]() { mw.ensurePlayerInfoWiring(); return mw.m_playerInfoWiring; };
    d.getAnalysisPresenter = [&mw]() { mw.ensureAnalysisPresenter(); return mw.m_analysisPresenter; };
    d.getUsi1              = [&mw]() { return mw.m_usi1; };
    d.getAnalysisTab       = [&mw]() { return mw.m_analysisTab; };
    d.getLineEditModel1    = [&mw]() { return mw.m_models.commLog1; };
    d.getModelThinking1    = [&mw]() { return mw.m_models.thinking1; };
    d.getKifuRecordModel   = [&mw]() { return mw.m_models.kifuRecord; };
    d.getKifuLoadCoordinator = [&mw]() { return mw.m_kifuLoadCoordinator; };
    d.getEvalChart         = [&mw]() { return mw.m_evalChart; };
    d.getSfenRecord        = [&mw]() { return mw.m_queryService->sfenRecord(); };

    // 値型メンバーへのポインタ
    d.moveRecords   = &mw.m_kifu.moveRecords;
    d.gameUsiMoves  = &mw.m_kifu.gameUsiMoves;
    d.activePly     = &mw.m_kifu.activePly;

    // ダブルポインタ
    d.csaGameDialog      = &mw.m_csaGameDialog;
    d.csaGameCoordinator = &mw.m_csaGameCoordinator;
    d.gameInfoController = &mw.m_gameInfoController;
    d.analysisModel      = &mw.m_models.analysis;

    // メニューウィンドウドック
    d.menuWindowDock     = &mw.m_docks.menuWindow;
    d.createMenuWindowDock = [&mw]() { mw.m_registry->createMenuWindowDock(); };

    // 局面集ダイアログ
    d.sfenCollectionDialog = &mw.m_sfenCollectionDialog;

    // CSA通信対局のエンジン評価値グラフ用
    d.getCsaGameCoordinator = [&mw]() { return mw.m_csaGameCoordinator; };

    mw.m_dialogLaunchWiring = new DialogLaunchWiring(d, &mw);

    // シグナル中継: SFEN局面集の選択をKifuFileControllerに接続
    mw.ensureKifuFileController();
    QObject::connect(mw.m_dialogLaunchWiring, &DialogLaunchWiring::sfenCollectionPositionSelected,
                     mw.m_kifuFileController, &KifuFileController::onSfenCollectionPositionSelected);
}
