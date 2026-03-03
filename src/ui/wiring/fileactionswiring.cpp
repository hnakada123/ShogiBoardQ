/// @file fileactionswiring.cpp
/// @brief ファイル/アプリ関連アクションの配線クラスの実装

#include "fileactionswiring.h"
#include "ui_mainwindow.h"
#include "gamesessionorchestrator.h"
#include "mainwindow.h"
#include "dialoglaunchwiring.h"
#include "kifufilecontroller.h"
#include "logcategories.h"
#include <QApplication>

void FileActionsWiring::wire()
{
    auto* ui  = m_d.ui;
    auto* mw  = m_d.mw;
    auto* dlw = m_d.dlw;
    auto* kfc = m_d.kfc;
    auto* gso = m_d.gso;

    if (Q_UNLIKELY(!ui || !mw)) {
        qCWarning(lcUi, "FileActionsWiring::wire(): ui or mw is null");
        return;
    }
    if (Q_UNLIKELY(!dlw || !kfc || !gso)) {
        qCWarning(lcUi, "FileActionsWiring::wire(): required wiring dependency is null "
                  "(dlw=%p, kfc=%p, gso=%p)",
                  static_cast<void*>(dlw), static_cast<void*>(kfc), static_cast<void*>(gso));
        return;
    }

    // ファイル/アプリ
    QObject::connect(ui->actionQuit,         &QAction::triggered, mw,  &MainWindow::saveSettingsAndClose,              Qt::UniqueConnection);
    QObject::connect(ui->actionSaveAs,       &QAction::triggered, kfc, &KifuFileController::saveKifuToFile,            Qt::UniqueConnection);
    QObject::connect(ui->actionSave,         &QAction::triggered, kfc, &KifuFileController::overwriteKifuFile,         Qt::UniqueConnection);
    QObject::connect(ui->actionOpenKifuFile, &QAction::triggered, kfc, &KifuFileController::chooseAndLoadKifuFile,     Qt::UniqueConnection);
    QObject::connect(ui->actionVersionInfo,  &QAction::triggered, dlw, &DialogLaunchWiring::displayVersionInformation, Qt::UniqueConnection);
    QObject::connect(ui->actionOpenWebsite,  &QAction::triggered, gso, &GameSessionOrchestrator::openWebsiteInExternalBrowser, Qt::UniqueConnection);
    QObject::connect(ui->actionAboutQt,      &QAction::triggered, qApp, &QApplication::aboutQt);
}
