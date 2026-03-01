/// @file mainwindowserviceregistry.cpp
/// @brief サービスレジストリのコンストラクタ

#include "mainwindowserviceregistry.h"
#include "mainwindowfoundationregistry.h"
#include "gamesubregistry.h"
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
