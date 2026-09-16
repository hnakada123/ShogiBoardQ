/// @file mainwindowserviceregistry.cpp
/// @brief サービスレジストリのコンストラクタ

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"
#include "kifusubregistry.h"
#include "kifufilecontroller.h"
#include "matchcoordinator.h"
#include "matchruntimequeryservice.h"
#include "josekiwindowwiring.h"
#include "josekiwindow.h"

MainWindowServiceRegistry::MainWindowServiceRegistry(MainWindow& mw, QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    // Lifetime: owned by MainWindowServiceRegistry (QObject parent=this)
    // Created: once at MainWindow startup, never recreated
    , m_foundation(new MainWindowFoundationRegistry(mw, this, this))
    , m_kifu(new KifuSubRegistry(mw, this, m_foundation, this))
{
}

MainWindowServiceRegistry::~MainWindowServiceRegistry() = default;

// --- Kifu convenience wrappers ---

bool MainWindowServiceRegistry::confirmDiscardUnsavedKifu()
{
    m_kifu->ensureKifuFileController();
    return m_mw.m_kifuFileController->confirmDiscardUnsaved();
}

bool MainWindowServiceRegistry::confirmCloseJoseki()
{
    const auto *wiring = m_mw.m_josekiWiring.get();
    return !wiring || !wiring->josekiWindow() || wiring->josekiWindow()->confirmClose();
}

void MainWindowServiceRegistry::prepareGameRecordLoadService()
{
    if (m_mw.m_isShuttingDown) return;
    m_kifu->ensureGameRecordLoadService();
}

void MainWindowServiceRegistry::updateJosekiWindow()
{
    m_kifu->updateJosekiWindow();
}

void MainWindowServiceRegistry::startGameSession()
{
    auto* match = m_mw.m_match;
    if (!match) {
        initMatchCoordinator();
        match = m_mw.m_match;
    }
    if (!match) return;

    QStringList* sfenRecord = m_mw.m_queryService ? m_mw.m_queryService->sfenRecord() : nullptr;
    match->prepareAndStartGame(
        m_mw.m_state.playMode,
        m_mw.m_state.startSfenStr,
        sfenRecord,
        nullptr,
        m_mw.m_player.bottomIsP1);
}
