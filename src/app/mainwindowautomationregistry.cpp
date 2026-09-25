/// @file mainwindowautomationregistry.cpp
/// @brief MainWindowServiceRegistry の自動化 API（--automation）生成部

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"

#include "automationcommands.h"
#include "automationcontext.h"
#include "automationserver.h"
#include "kifusubregistry.h"
#include "logcategories.h"
#include "mainwindowfoundationregistry.h"
#include "matchruntimequeryservice.h"
#include "kifuexportcontroller.h"

#include <QTextStream>

#include <cstdio>

// 自動化 API のサーバーを生成し、MainWindow の状態とコントローラを注入して待ち受けを始める。
// 生成は 1 回だけ。失敗しても GUI の起動は続ける。
void MainWindowServiceRegistry::ensureAutomationServer(const QString& socketPath)
{
    if (m_mw.m_automationServer) return;

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once when --automation is given, never recreated
    auto* server = new AutomationServer(&m_mw);

    AutomationContext ctx;
    ctx.mainWindow = &m_mw;
    ctx.gameController = m_mw.m_gameController;
    ctx.shogiView = m_mw.m_shogiView;
    ctx.branchTree = m_mw.m_branchNav.branchTree;
    ctx.playMode = &m_mw.m_state.playMode;
    ctx.currentMoveIndex = &m_mw.m_state.currentMoveIndex;
    ctx.startSfenStr = &m_mw.m_state.startSfenStr;
    ctx.currentSfenStr = &m_mw.m_state.currentSfenStr;
    ctx.saveFileName = &m_mw.m_kifu.saveFileName;
    ctx.engineName1 = &m_mw.m_player.engineName1;
    ctx.engineName2 = &m_mw.m_player.engineName2;
    ctx.sfenRecord = [this]() -> const QStringList* {
        return m_mw.m_queryService ? m_mw.m_queryService->sfenRecord() : nullptr;
    };
    ctx.navigationController = [this]() { return m_mw.m_branchNav.kifuNavController; };
    ctx.uiStatePolicy = [this]() {
        m_foundation->ensureUiStatePolicyManager();
        return m_mw.m_uiStatePolicy;
    };
    ctx.kifuFileController = [this]() {
        m_kifu->ensureKifuFileController();
        return m_mw.m_kifuFileController;
    };
    ctx.kifuExportController = [this]() {
        m_kifu->ensureGameRecordModel();
        m_kifu->ensureKifuExportController();
        m_kifu->updateKifuExportDeps();
        return m_mw.m_kifuExportController.get();
    };
    ctx.gameRecordModel = [this]() {
        m_kifu->ensureGameRecordModel();
        return m_mw.m_models.gameRecord;
    };
    ctx.quitApplication = [this]() {
        // 自動化からの終了では未保存確認ダイアログを出さない
        m_mw.m_isShuttingDown = true;
        m_mw.saveSettingsAndClose();
    };
    AutomationCommands::registerAll(server->dispatcher(), ctx);

    QString error;
    if (!server->listen(socketPath, &error)) {
        qCWarning(lcApp) << "automation API not started:" << error;
        QTextStream(stderr) << "automation: " << error << '\n';
        server->deleteLater();
        return;
    }
    m_mw.m_automationServer = server;
    QTextStream(stdout) << "automation: listening on " << server->socketPath() << Qt::endl;
}
