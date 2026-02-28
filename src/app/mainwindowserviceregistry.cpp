/// @file mainwindowserviceregistry.cpp
/// @brief サービスレジストリのコンストラクタ

#include "mainwindowserviceregistry.h"
#include "mainwindowfoundationregistry.h"

MainWindowServiceRegistry::MainWindowServiceRegistry(MainWindow& mw, QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_foundation(new MainWindowFoundationRegistry(mw, this, this))
{
}
