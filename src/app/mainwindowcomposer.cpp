/// @file mainwindowcomposer.cpp
/// @brief 合成ルートの起動・終了手順と終了確認

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "appsettings.h"
#include "docklayoutmanager.h"
#include "josekiwindow.h"
#include "josekiwindowwiring.h"
#include "kifufilecontroller.h"
#include "shogiview.h"

// unique_ptr メンバの完全型（デストラクタ用）
#include "analysisresultspresenter.h"
#include "branchnavigationwiring.h"
#include "csagamewiring.h"
#include "dockcreationservice.h"
#include "evaluationgraphcontroller.h"
#include "gamerecordloadservice.h"
#include "gamerecordupdateservice.h"
#include "jishogiscoredialogcontroller.h"
#include "kifuexportcontroller.h"
#include "kifunavigationcoordinator.h"
#include "languagecontroller.h"
#include "mainwindowactions.h"
#include "mainwindowappearancecontroller.h"
#include "matchcoordinatorwiring.h"
#include "nyugyokudeclarationhandler.h"
#include "playmodepolicyservice.h"
#include "turnstatesyncservice.h"
#include "undoflowservice.h"
#include "usicommandcontroller.h"
#ifdef QT_DEBUG
#include "debugscreenshotwiring.h"
#endif

#include <QTimer>

MainWindowComposer::MainWindowComposer(MainWindow& window, Ui::MainWindow& ui)
    : QObject(nullptr)
    , m_window(window)
    , m_ui(ui)
{
}

MainWindowComposer::~MainWindowComposer() = default;

void MainWindowComposer::compose()
{
    // 1) 生成。依存の少ないものから順に、起動時に1回だけ作る
    createModels();
    setupUiSkeleton();
    createBoardAndGameController();
    createPanelsAndDocks();
    createBranchNavigation();
    createRecordAndPlayerInfo();
    createSecondaryDocks();
    createServices();

    // 2) 依存の注入。部品は全部できているので、相互参照も順序を気にせず結べる
    wireFoundation();
    wireMatchCoordinator();
    wireCsaGame();
    wireKifu();
    wireUi();
    wireDialogs();
    wireGame();

    // 3) 部品どうしのシグナル接続と初期表示
    connectSignals();
    finalizeUi();
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

    // エンジンが起動していれば quit を送って止める
    if (m_c.match) {
        m_c.match->destroyEngines();
    }

    // 終了処理中にモード判定を求められても、部品がない状態として扱う
    m_c.playModePolicy.reset();
}

bool MainWindowComposer::confirmClose()
{
    if (m_c.kifuFileController && !m_c.kifuFileController->confirmDiscardUnsaved()) return false;
    JosekiWindow* joseki = m_c.josekiWiring ? m_c.josekiWiring->josekiWindow() : nullptr;
    return !joseki || joseki->confirmClose();
}
