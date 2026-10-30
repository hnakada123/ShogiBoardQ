/// @file mainwindowcomposer.cpp
/// @brief 合成ルートの起動・終了手順と終了確認

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "appsettings.h"
#include "commentcoordinator.h"
#include "considerationpanel.h"
#include "docklayoutmanager.h"
#include "gameinfopane.h"
#include "engineslots.h"
#include "humanturnpolicy.h"
#include "josekiwindowcontroller.h"
#include "kifudisplaypresenter.h"
#include "kifufilecontroller.h"
#include "mainwindowappearancecontroller.h"
#include "playerinfopresenter.h"
#include "sessionresetcoordinator.h"
#include "gameboard.h"
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
    m_c.shogiView->applyBoardAndRender(m_c.gameBoard->board());
    m_c.kifuDisplay->refresh();

    // ウィンドウの大きさ（小さすぎる保存値は使わない）と、起動時のドックのレイアウト
    const QSize windowSize = AppSettings::mainWindowSize();
    if (windowSize.width() > 100 && windowSize.height() > 100) m_window.resize(windowSize);
    m_c.dockLayoutManager->restoreStartupLayoutIfSet();

    m_c.appearance->installAppToolTips();
}

void MainWindowComposer::shutdown()
{
    if (m_shutdownDone) return;
    m_shutdownDone = true;

    // 設定保存（ウィンドウ・ドックの配置。盤のマスの大きさは変わるたびに MainWindowAppearanceController が保存する）
    AppSettings::mainWindowSize.set(m_window.size());
    if (m_c.dockLayoutManager) {
        m_c.dockLayoutManager->saveDockStates();
    }

    // エンジンが起動していれば quit を送って止める（対局・検討のどちらのエンジンも）
    if (m_c.engines) {
        m_c.engines->destroyEngines();
    }

    // 終了処理中に盤がクリックされても、操作できない手番として扱う（盤の操作が参照し続けるので破棄はしない）
    if (m_c.humanTurnPolicy) m_c.humanTurnPolicy->disable();
}

bool MainWindowComposer::confirmClose()
{
    if (m_c.kifuFileController && !m_c.kifuFileController->confirmDiscardUnsaved()) return false;
    return !m_c.joseki || m_c.joseki->confirmClose();
}
