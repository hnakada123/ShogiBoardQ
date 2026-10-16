/// @file mainwindowcomposer_automation.cpp
/// @brief 自動化 API（--automation）のサーバー生成

#include "mainwindowcomposer.h"
#include "mainwindow.h"

#include "analysissession.h"
#include "automationcommands.h"
#include "automationcontext.h"
#include "automationserver.h"
#include "csagamecoordinator.h"
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

    auto* server = new AutomationServer(&m_window);

    AutomationContext ctx;
    ctx.mainWindow = &m_window;
    ctx.gameController = m_c.gameController;
    ctx.shogiView = m_c.shogiView;
    ctx.branchTree = m_c.branchTree;
    ctx.state = &m_state;
    ctx.document = m_c.document;
    ctx.analysis = m_c.analysisSession;
    ctx.navState = m_c.navState;
    ctx.navigationController = m_c.kifuNavController;
    ctx.uiStatePolicy = m_c.uiStatePolicy;
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
