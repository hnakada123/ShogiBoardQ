/// @file mainwindowcomposer.cpp
/// @brief 合成ルートの起動・終了手順と終了確認

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "appsettings.h"
#include "commentcoordinator.h"
#include "considerationtabmanager.h"
#include "docklayoutmanager.h"
#include "gameinfopane.h"
#include "enginelifecyclemanager.h"
#include "humanturnpolicy.h"
#include "josekiwindow.h"
#include "josekiwindowcontroller.h"
#include "kifudisplaypresenter.h"
#include "kifufilecontroller.h"
#include "mainwindowappearancecontroller.h"
#include "playerinfopresenter.h"
#include "sessionresetcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

MainWindowComposer::MainWindowComposer(MainWindow& window, Ui::MainWindow& ui)
    : QObject(nullptr)
    , m_window(window)
    , m_ui(ui)
{
}

MainWindowComposer::~MainWindowComposer() = default;

void MainWindowComposer::compose()
{
    // 1) 生成。依存する部品が先にできている順に、起動時に1回だけ作る
    createRecordAndModels();
    createBoard();
    createPanels();
    createMatchAndAnalysis();
    createPresentersAndDocks();
    createSessions();
    createControllers();

    // 2) 部品どうしのシグナル接続
    connectRecord();
    connectBoard();
    connectGame();
    connectAnalysisAndUi();

    // 3) 初期表示
    finalizeUi();
}

void MainWindowComposer::finalizeUi()
{
    // 盤・棋譜欄・分岐・手番と対局者名の初期表示（対局は始めない。盤の対局者名は対局情報の初期値「先手」「後手」）
    m_c.shogiView->applyBoardAndRender(m_c.gameController->board());
    m_c.kifuDisplay->refresh();
    m_c.playerInfo->showMatchPlayers();

    // ウィンドウの大きさと、起動時のドックのレイアウト
    AppSettings::loadWindowSize(&m_window);
    m_c.dockLayoutManager->restoreStartupLayoutIfSet();

    m_c.appearance->installAppToolTips();
}

bool MainWindowComposer::isHumanTurnNow() const
{
    return m_c.humanTurnPolicy && m_c.humanTurnPolicy->isHumanTurnNow();
}

bool MainWindowComposer::showConsiderationArrows() const
{
    return !m_c.considerationTab || m_c.considerationTab->settings().showArrows;
}

void MainWindowComposer::commitPendingEdits()
{
    if (m_c.gameInfoPane) m_c.gameInfoPane->commitPendingEditor();
    if (m_c.commentCoordinator) m_c.commentCoordinator->commitPendingComment();
}

bool MainWindowComposer::hasPendingEdits() const
{
    return (m_c.gameInfoPane && m_c.gameInfoPane->hasPendingEditor())
        || (m_c.commentCoordinator && m_c.commentCoordinator->hasPendingComment());
}

void MainWindowComposer::shutdown()
{
    if (m_shutdownDone) return;
    m_shutdownDone = true;

    // 設定保存（ウィンドウ・盤のマスサイズ・ドックの配置）
    AppSettings::saveWindowAndBoard(&m_window, m_c.shogiView ? m_c.shogiView->squareSize() : 0);
    if (m_c.dockLayoutManager) {
        m_c.dockLayoutManager->saveDockStates();
    }

    // エンジンが起動していれば quit を送って止める（対局・検討のどちらのエンジンも）
    if (m_c.engines) {
        m_c.engines->destroyEngines();
    }

    // 終了処理中に盤がクリックされても、操作できない手番として扱う
    m_c.humanTurnPolicy.reset();
}

bool MainWindowComposer::confirmClose()
{
    if (m_c.kifuFileController && !m_c.kifuFileController->confirmDiscardUnsaved()) return false;
    JosekiWindow* joseki = m_c.joseki ? m_c.joseki->josekiWindow() : nullptr;
    return !joseki || joseki->confirmClose();
}
