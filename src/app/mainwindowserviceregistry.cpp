/// @file mainwindowserviceregistry.cpp
/// @brief サービスレジストリのコンストラクタ

#include "mainwindowserviceregistry.h"

MainWindowServiceRegistry::MainWindowServiceRegistry(MainWindow& mw, QObject* parent)
    : QObject(parent)
    , m_mw(mw)
{
}
