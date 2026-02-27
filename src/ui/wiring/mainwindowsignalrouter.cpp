/// @file mainwindowsignalrouter.cpp
/// @brief MainWindow のシグナル配線ルーター実装

#include "mainwindowsignalrouter.h"
#include "mainwindow.h"
#include "mainwindowappearancecontroller.h"

#include "boardinteractioncontroller.h"
#include "boardsetupcontroller.h"
#include "dialogcoordinatorwiring.h"
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

namespace {
/// ダブルポインタを安全にデリファレンスするヘルパー
template<typename T>
T* deref(T** pp) { return pp ? *pp : nullptr; }
} // namespace

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
    // DialogCoordinatorWiring を先行生成（cancelKifuAnalysis 接続先として必要）
    if (m_deps.dialogCoordinatorWiringPtr && !deref(m_deps.dialogCoordinatorWiringPtr)) {
        *m_deps.dialogCoordinatorWiringPtr = new DialogCoordinatorWiring(m_deps.mainWindow);
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
    if (m_deps.actionsWiringPtr && !deref(m_deps.actionsWiringPtr)) {
        UiActionsWiring::Deps d;
        d.ui  = m_deps.ui;
        d.ctx = m_deps.mainWindow;
        d.dlw = deref(m_deps.dialogLaunchWiringPtr);
        d.dcw = deref(m_deps.dialogCoordinatorWiringPtr);
        d.kfc = deref(m_deps.kifuFileControllerPtr);
        d.gso = deref(m_deps.gameSessionOrchestratorPtr);
        d.appearance = m_deps.appearanceController;
        d.shogiView = m_deps.shogiView;
        d.evalChart = m_deps.evalChart;
        d.getKifuExportController = m_deps.getKifuExportController;
        *m_deps.actionsWiringPtr = new UiActionsWiring(d, m_deps.mainWindow);
    }
    auto* actionsWiring = deref(m_deps.actionsWiringPtr);
    if (actionsWiring) {
        actionsWiring->wire();
    }
}

void MainWindowSignalRouter::connectCoreSignals()
{
    qCDebug(lcApp) << "connectCoreSignals called";

    auto* dlw = deref(m_deps.dialogLaunchWiringPtr);

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

    // ErrorBus のエラーを UiNotificationService に直接接続
    if (m_deps.ensureUiNotificationService) {
        m_deps.ensureUiNotificationService();
    }
    auto* ns = deref(m_deps.notificationServicePtr);
    if (ns) {
        connect(&ErrorBus::instance(), &ErrorBus::errorOccurred,
                ns, &UiNotificationService::displayErrorMessage, Qt::UniqueConnection);
    }
}

void MainWindowSignalRouter::connectBoardClicks()
{
    if (m_deps.ensureBoardSetupController) {
        m_deps.ensureBoardSetupController();
    }
    auto* bsc = deref(m_deps.boardSetupControllerPtr);
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
