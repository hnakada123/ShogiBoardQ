/// @file mainwindowwiringassembler.cpp
/// @brief WiringAssembler系メソッドの実装（ServiceRegistry統合版）
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "gamesubregistry.h"
#include "gamewiringsubregistry.h"
#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowsignalrouter.h"
#include "dialoglaunchwiring.h"
#include "kifuloadcoordinator.h"            // IWYU pragma: keep (QPointer の完全型)
#include "playerinfowiring.h"
#include "kifufilecontroller.h"
#include "matchruntimequeryservice.h"

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
    d.getJishogiController = [this]() { m_foundation->ensureJishogiController(); return m_mw.m_jishogiController.get(); };
    d.getNyugyokuHandler   = [this]() { m_foundation->ensureNyugyokuHandler(); return m_mw.m_nyugyokuHandler.get(); };
    d.getCsaGameWiring     = [this]() { m_game->wiring()->ensureCsaGameWiring(); return m_mw.m_csaGameWiring.get(); };
    d.getBoardSetupController = [this]() { ensureBoardSetupController(); return m_mw.m_boardSetupController; };
    d.getPlayerInfoWiring  = [this]() { m_foundation->ensurePlayerInfoWiring(); return m_mw.m_playerInfoWiring; };
    d.getAnalysisPresenter = [this]() { m_foundation->ensureAnalysisPresenter(); return m_mw.m_analysisPresenter.get(); };
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

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_dialogLaunchWiring = new DialogLaunchWiring(d, &m_mw);

    // シグナル中継: SFEN局面集の選択をKifuFileControllerに接続
    m_kifu->ensureKifuFileController();
    QObject::connect(m_mw.m_dialogLaunchWiring, &DialogLaunchWiring::sfenCollectionPositionSelected,
                     m_mw.m_kifuFileController, &KifuFileController::onSfenCollectionPositionSelected);
}
