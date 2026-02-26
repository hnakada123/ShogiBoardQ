/// @file mainwindowsignalrouter.cpp
/// @brief MainWindow のシグナル配線ルーター実装

#include "mainwindowsignalrouter.h"
#include "mainwindow.h"
#include "mainwindowappearancecontroller.h"
#include "mainwindowserviceregistry.h"

#include "boardinteractioncontroller.h"
#include "boardsetupcontroller.h"
#include "dialogcoordinatorwiring.h"
#include "dialoglaunchwiring.h"
#include "errorbus.h"
#include "kifufilecontroller.h"
#include "uinotificationservice.h"
#include "mainwindowwiringassembler.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "uiactionswiring.h"
#include "kifuexportcontroller.h"
#include "ui_mainwindow.h"

#include "logcategories.h"

MainWindowSignalRouter::MainWindowSignalRouter(MainWindow& mw, QObject* parent)
    : QObject(parent)
    , m_mw(mw)
{
}

void MainWindowSignalRouter::connectAll()
{
    // ダイアログ起動配線（connectAllActions より先に初期化）
    initializeDialogLaunchWiring();

    // メニュー/アクションのconnect（関数ポインタで統一）
    connectAllActions();

    // コアシグナル（昇格ダイアログ・エラー・ドラッグ終了・指し手確定 等）
    connectCoreSignals();
}

void MainWindowSignalRouter::initializeDialogLaunchWiring()
{
    MainWindowWiringAssembler::initializeDialogLaunchWiring(m_mw);
}

void MainWindowSignalRouter::connectAllActions()
{
    // DialogCoordinatorWiring を先行生成（cancelKifuAnalysis 接続先として必要）
    if (!m_mw.m_dialogCoordinatorWiring) {
        m_mw.m_dialogCoordinatorWiring = new DialogCoordinatorWiring(&m_mw);
    }

    // KifuFileController を先行生成（ファイル操作スロット接続先として必要）
    m_mw.ensureKifuFileController();

    // GameSessionOrchestrator を先行生成（対局ライフサイクル接続先として必要）
    m_mw.m_registry->ensureGameSessionOrchestrator();

    // 既存があれば使い回し
    if (!m_mw.m_actionsWiring) {
        UiActionsWiring::Deps d;
        d.ui  = m_mw.ui.get();
        d.ctx = &m_mw;
        d.dlw = m_mw.m_dialogLaunchWiring;
        d.dcw = m_mw.m_dialogCoordinatorWiring;
        d.kfc = m_mw.m_kifuFileController;
        d.gso = m_mw.m_gameSessionOrchestrator;
        d.appearance = m_mw.m_appearanceController;
        d.shogiView = m_mw.m_shogiView;
        d.evalChart = m_mw.m_evalChart;
        d.getKifuExportController = [this]() -> KifuExportController* {
            m_mw.m_registry->ensureKifuExportController();
            return m_mw.m_kifuExportController;
        };
        m_mw.m_actionsWiring = new UiActionsWiring(d, &m_mw);
    }
    m_mw.m_actionsWiring->wire();
}

void MainWindowSignalRouter::connectCoreSignals()
{
    qCDebug(lcApp) << "connectCoreSignals called";

    // 将棋盤表示・昇格・ドラッグ終了・指し手確定
    if (m_mw.m_gameController) {
        connect(m_mw.m_gameController, &ShogiGameController::showPromotionDialog,
                m_mw.m_dialogLaunchWiring, &DialogLaunchWiring::displayPromotionDialog, Qt::UniqueConnection);

        connect(m_mw.m_gameController, &ShogiGameController::endDragSignal,
                m_mw.m_shogiView,      &ShogiView::endDrag, Qt::UniqueConnection);

        connect(m_mw.m_gameController, &ShogiGameController::moveCommitted,
                &m_mw, &MainWindow::onMoveCommitted, Qt::UniqueConnection);
    }
    if (m_mw.m_shogiView && m_mw.m_appearanceController) {
        bool connected = connect(m_mw.m_shogiView, &ShogiView::fieldSizeChanged,
                m_mw.m_appearanceController, &MainWindowAppearanceController::onBoardSizeChanged, Qt::UniqueConnection);
        qCDebug(lcApp) << "fieldSizeChanged -> onBoardSizeChanged connected:" << connected
                 << "m_shogiView=" << m_mw.m_shogiView;
    } else {
        qCDebug(lcApp) << "m_shogiView is NULL, cannot connect fieldSizeChanged";
    }

    // ErrorBus のエラーを UiNotificationService に直接接続
    m_mw.m_registry->ensureUiNotificationService();
    connect(&ErrorBus::instance(), &ErrorBus::errorOccurred,
            m_mw.m_notificationService, &UiNotificationService::displayErrorMessage, Qt::UniqueConnection);
}

void MainWindowSignalRouter::connectBoardClicks()
{
    m_mw.ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->connectBoardClicks();
    }
}

void MainWindowSignalRouter::connectMoveRequested()
{
    // BoardInteractionControllerからのシグナルをMainWindow経由で処理
    if (m_mw.m_boardController) {
        QObject::connect(
            m_mw.m_boardController, &BoardInteractionController::moveRequested,
            &m_mw,                  &MainWindow::onMoveRequested,
            Qt::UniqueConnection);
    }
}
