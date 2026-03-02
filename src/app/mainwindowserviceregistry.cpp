/// @file mainwindowserviceregistry.cpp
/// @brief サービスレジストリのコンストラクタ

#include "mainwindowserviceregistry.h"
#include "mainwindowfoundationregistry.h"
#include "gamesubregistry.h"
#include "gamesessionsubregistry.h"
#include "kifusubregistry.h"

MainWindowServiceRegistry::MainWindowServiceRegistry(MainWindow& mw, QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    // Lifetime: owned by MainWindowServiceRegistry (QObject parent=this)
    // Created: once at MainWindow startup, never recreated
    , m_foundation(new MainWindowFoundationRegistry(mw, this, this))
    , m_game(new GameSubRegistry(mw, this, m_foundation, this))
    , m_kifu(new KifuSubRegistry(mw, this, m_foundation, this))
{
}

// --- Sub-registry convenience wrappers ---

void MainWindowServiceRegistry::ensureUndoFlowService()
{
    m_game->ensureUndoFlowService();
}

void MainWindowServiceRegistry::ensureTurnStateSyncService()
{
    m_game->ensureTurnStateSyncService();
}

void MainWindowServiceRegistry::ensureGameRecordLoadService()
{
    m_kifu->ensureGameRecordLoadService();
}

void MainWindowServiceRegistry::updateJosekiWindow()
{
    m_kifu->updateJosekiWindow();
}

void MainWindowServiceRegistry::resetToInitialState()
{
    m_game->session()->resetToInitialState();
}

void MainWindowServiceRegistry::resetGameState()
{
    m_game->session()->resetGameState();
}

void MainWindowServiceRegistry::setReplayMode(bool on)
{
    m_game->setReplayMode(on);
}
