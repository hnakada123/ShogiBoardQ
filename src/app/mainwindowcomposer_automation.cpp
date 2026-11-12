/// @file mainwindowcomposer_automation.cpp
/// @brief 自動化 API（--automation）のサーバー生成

#ifdef SBQ_ENABLE_P2P
#include "p2pgameuicontroller.h"
#endif
#include "mainwindowcomposer.h"
#include "mainwindow.h"

#include "analysissession.h"
#include "automationactionpolicy.h"
#include "automationcommands.h"
#include "automationcontext.h"
#include "automationserver.h"
#include "csagamecoordinator.h"
#include "gameboard.h"
#include "menuwindow.h"
#include "menuwindowcontroller.h"
#include "logcategories.h"

#include <QTextStream>

#include <cstdio>

// 自動化 API のサーバーを生成し、状態と部品を渡して待ち受けを始める。
// 生成は1回だけ。失敗しても GUI の起動は続ける。
void MainWindowComposer::startAutomationServer(const QString& socketPath)
{
    if (m_c.automationServer) return;
#ifdef SBQ_ENABLE_P2P
    // 自動化 API を有効にしたときは P2P対局の部品を作らない（ローカルのソケットから招待・対局・確定を始めさせない）。
    // ガイドの画像を撮るためのビルド（ENABLE_P2P_DOCS_AUTOMATION）だけは、自動化 API から操作できる
    if (m_c.p2pUi) {
        if (AutomationActionPolicy::allowsP2pPlay()) {
            QTextStream(stderr) << "automation: this build is for guide screenshots; the automation API can drive P2P play. "
                                   "Do not distribute it.\n";
        } else {
            m_c.p2pUi->disableForAutomation();
        }
    }
#endif

    auto* server = new AutomationServer(&m_window);

    AutomationContext ctx;
    ctx.mainWindow = &m_window;
    ctx.board = m_c.gameBoard->board();
    ctx.shogiView = m_c.shogiView;
    ctx.branchTree = m_c.branchTree;
    ctx.activity = &m_activity;
    ctx.document = m_c.document;
    ctx.analysis = m_c.analysisSession;
    ctx.navState = m_c.navState;
    ctx.navigationController = m_c.kifuNavController;
    ctx.operationPolicy = m_c.operationPolicy.get();
    ctx.kifuFileController = m_c.kifuFileController;
    ctx.csaGame = m_c.csaGame;
    // メニューウィンドウは MenuWindowController が起動時に作る
    ctx.menuFavorites = [this]() { return m_c.menuWindowController->menuWindow()->favorites(); };
    ctx.availableMenuActions = [this]() { return m_c.menuWindowController->menuWindow()->availableActions(); };
    ctx.setMenuFavorites = [this](const QStringList& names) {
        m_c.menuWindowController->menuWindow()->setFavorites(names);
    };
    ctx.quitApplication = [this]() {
        // 自動化からの終了では未保存確認ダイアログを出さない
        m_window.quitWithoutConfirmation();
    };
    AutomationCommands::registerAll(server->dispatcher(), ctx);

    QString error;
    if (!server->listen(socketPath, &error)) {
        qCWarning(lcApp) << "automation API not started:" << error;
        QTextStream(stderr) << "automation: " << error << '\n';
        server->deleteLater();
        return;
    }
    m_c.automationServer = server;
    QTextStream(stdout) << "automation: listening on " << server->socketPath() << Qt::endl;
}
