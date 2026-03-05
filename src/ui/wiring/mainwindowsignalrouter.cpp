/// @file mainwindowsignalrouter.cpp
/// @brief MainWindow のシグナル配線ルーター実装

#include "mainwindowsignalrouter.h"
#include "mainwindow.h"
#include "mainwindowappearancecontroller.h"

#include "boardinteractioncontroller.h"
#include "boardsetupcontroller.h"
#include "dialoglaunchwiring.h"
#include "errorbus.h"
#include "kifufilecontroller.h"
#include "uinotificationservice.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "uiactionswiring.h"
#include "kifuexportcontroller.h"
#include "ui_mainwindow.h"

#include "logcategories.h"

MainWindowSignalRouter::MainWindowSignalRouter(QObject* parent)
    : QObject(parent)
{
}

void MainWindowSignalRouter::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void MainWindowSignalRouter::connectAll()
{
    // ダイアログ起動配線（connectAllActions より先に初期化）
    if (m_deps.initializeDialogLaunchWiring) {
        m_deps.initializeDialogLaunchWiring();
    }

    // メニュー/アクションのconnect（関数ポインタで統一）
    connectAllActions();

    // コアシグナル（昇格ダイアログ・エラー・ドラッグ終了・指し手確定 等）
    connectCoreSignals();
}

void MainWindowSignalRouter::connectAllActions()
{
    // DialogCoordinatorWiring を CompositionRoot/Registry 経由で確保
    if (m_deps.ensureDialogCoordinator) {
        m_deps.ensureDialogCoordinator();
    }

    // KifuFileController を先行生成（ファイル操作スロット接続先として必要）
    if (m_deps.ensureKifuFileController) {
        m_deps.ensureKifuFileController();
    }

    // GameSessionOrchestrator を先行生成（対局ライフサイクル接続先として必要）
    if (m_deps.ensureGameSessionOrchestrator) {
        m_deps.ensureGameSessionOrchestrator();
    }

    // 既存があれば使い回し
    UiActionsWiring* actionsWiring = m_deps.getActionsWiring ? m_deps.getActionsWiring() : nullptr;
    if (!actionsWiring) {
        UiActionsWiring::Deps d;
        d.ui  = m_deps.ui;
        d.ctx = m_deps.mainWindow;
        d.dlw = m_deps.getDialogLaunchWiring ? m_deps.getDialogLaunchWiring() : nullptr;
        d.dcw = m_deps.getDialogCoordinatorWiring ? m_deps.getDialogCoordinatorWiring() : nullptr;
        if (Q_UNLIKELY(!d.dcw)) {
            qCWarning(lcApp, "connectAllActions: DialogCoordinatorWiring is null after ensure");
        }
        d.kfc = m_deps.getKifuFileController ? m_deps.getKifuFileController() : nullptr;
        d.gso = m_deps.getGameSessionOrchestrator ? m_deps.getGameSessionOrchestrator() : nullptr;
        d.appearance = m_deps.appearanceController;
        d.shogiView = m_deps.shogiView;
        d.evalChart = m_deps.evalChart;
        d.getKifuExportController = m_deps.getKifuExportController;
        actionsWiring = new UiActionsWiring(d, m_deps.mainWindow);
        if (m_deps.setActionsWiring) {
            m_deps.setActionsWiring(actionsWiring);
        }
    }
    if (actionsWiring) {
        actionsWiring->wire();
    }
}

void MainWindowSignalRouter::connectCoreSignals()
{
    qCDebug(lcApp) << "connectCoreSignals called";

    auto* dlw = m_deps.getDialogLaunchWiring ? m_deps.getDialogLaunchWiring() : nullptr;

    // 将棋盤表示・昇格・ドラッグ終了・指し手確定
    if (m_deps.gameController && dlw) {
        connect(m_deps.gameController, &ShogiGameController::showPromotionDialog,
                dlw, &DialogLaunchWiring::displayPromotionDialog, Qt::UniqueConnection);
    }
    if (m_deps.gameController) {
        connect(m_deps.gameController, &ShogiGameController::endDragSignal,
                m_deps.shogiView,      &ShogiView::endDrag, Qt::UniqueConnection);

        connect(m_deps.gameController, &ShogiGameController::moveCommitted,
                m_deps.mainWindow, &MainWindow::onMoveCommitted, Qt::UniqueConnection);
    }
    if (m_deps.shogiView && m_deps.appearanceController) {
        bool connected = connect(m_deps.shogiView, &ShogiView::fieldSizeChanged,
                m_deps.appearanceController, &MainWindowAppearanceController::onBoardSizeChanged, Qt::UniqueConnection);
        qCDebug(lcApp) << "fieldSizeChanged -> onBoardSizeChanged connected:" << connected
                 << "m_shogiView=" << m_deps.shogiView;
    } else {
        qCDebug(lcApp) << "m_shogiView is NULL, cannot connect fieldSizeChanged";
    }

    // ErrorBus のメッセージを UiNotificationService に直接接続
    if (m_deps.ensureUiNotificationService) {
        m_deps.ensureUiNotificationService();
    }
    auto* ns = m_deps.getNotificationService ? m_deps.getNotificationService() : nullptr;
    if (ns) {
        connect(&ErrorBus::instance(), &ErrorBus::messagePosted,
                ns, &UiNotificationService::displayMessage, Qt::UniqueConnection);
    }
}

void MainWindowSignalRouter::connectBoardClicks()
{
    if (m_deps.ensureBoardSetupController) {
        m_deps.ensureBoardSetupController();
    }
    auto* bsc = m_deps.getBoardSetupController ? m_deps.getBoardSetupController() : nullptr;
    if (bsc) {
        bsc->connectBoardClicks();
    }
}

void MainWindowSignalRouter::connectMoveRequested()
{
    // BoardInteractionControllerからのシグナルをMainWindow経由で処理
    if (m_deps.boardController) {
        QObject::connect(
            m_deps.boardController, &BoardInteractionController::moveRequested,
            m_deps.mainWindow,      &MainWindow::onMoveRequested,
            Qt::UniqueConnection);
    }
}
