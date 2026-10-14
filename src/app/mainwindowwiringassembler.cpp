/// @file mainwindowwiringassembler.cpp
/// @brief WiringAssembler系メソッドの実装（ServiceRegistry統合版）
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowsignalrouter.h"
#include "dialoglaunchwiring.h"
#include "kifuloadcoordinator.h"            // IWYU pragma: keep (QPointer の完全型)
#include "playerinfowiring.h"
#include "kifufilecontroller.h"

void MainWindowServiceRegistry::initializeDialogLaunchWiring()
{
    DialogLaunchWiring::Deps d;
    d.confirmDiscardUnsavedKifu = [this]() { return confirmDiscardUnsavedKifu(); };
    d.parentWidget = &m_mw;
    d.playMode = &m_mw.m_state.playMode;

    // 遅延初期化ゲッター
    d.getDialogCoordinator = [this]() { ensureDialogCoordinator(); return m_mw.m_dialogCoordinator; };
    d.getGameController    = [this]() { return m_mw.m_gameController; };
    d.getMatch             = [this]() { return m_mw.m_match; };
    d.getShogiView         = [this]() { return m_mw.m_shogiView; };
    d.getJishogiController = [this]() { m_foundation->ensureJishogiController(); return m_mw.m_jishogiController.get(); };
    d.getNyugyokuHandler   = [this]() { m_foundation->prepareNyugyokuHandler(); return m_mw.m_nyugyokuHandler.get(); };
    d.getCsaGameWiring     = [this]() { ensureCsaGameWiring(); return m_mw.m_csaGameWiring.get(); };
    d.getBoardSetupController = [this]() { ensureBoardSetupController(); return m_mw.m_boardSetupController; };
    d.getGameInfoController = [this]() -> GameInfoPaneController* {
        m_foundation->ensurePlayerInfoWiring();
        if (!m_mw.m_playerInfoWiring) return nullptr;
        m_mw.m_playerInfoWiring->ensureGameInfoController();
        m_mw.m_gameInfoController = m_mw.m_playerInfoWiring->gameInfoController();
        return m_mw.m_gameInfoController;
    };
    d.getAnalysisPresenter = [this]() { m_foundation->ensureAnalysisPresenter(); return m_mw.m_analysisPresenter.get(); };
    d.getAnalysisTab       = [this]() { return m_mw.m_analysisTab; };
    d.getLineEditModel1    = [this]() { return m_mw.m_models.commLog1; };
    d.getModelThinking1    = [this]() { return m_mw.m_models.thinking1; };
    d.getKifuRecordModel   = [this]() { return m_mw.m_models.kifuRecord; };
    d.getNavState          = [this]() { return m_mw.m_branchNav.navState; };
    d.getEvalChart         = [this]() { return m_mw.m_evalChart; };
    d.getPieceSoundPlayer  = [this]() { m_foundation->ensurePieceSoundPlayer(); return m_mw.m_registryParts.pieceSoundPlayer; };
    d.getAnalysisModel     = [this]() { return m_mw.m_models.analysis; };
    d.getMenuWindowDock    = [this]() {
        if (!m_mw.m_docks.menuWindow) createMenuWindowDock();
        return m_mw.m_docks.menuWindow;
    };
    d.getCsaGameCoordinator = [this]() { return csaGameCoordinator(); };

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_dialogLaunchWiring = new DialogLaunchWiring(d, &m_mw);

    // シグナル中継: SFEN局面集の選択をKifuFileControllerに接続
    m_kifu->ensureKifuFileController();
    QObject::connect(m_mw.m_dialogLaunchWiring, &DialogLaunchWiring::sfenCollectionPositionSelected,
                     m_mw.m_kifuFileController, &KifuFileController::onSfenCollectionPositionSelected);
}
