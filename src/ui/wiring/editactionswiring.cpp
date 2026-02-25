/// @file editactionswiring.cpp
/// @brief 編集メニュー関連アクションの配線クラスの実装

#include "editactionswiring.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "kifuexportcontroller.h"
#include "kifufilecontroller.h"
#include "dialoglaunchwiring.h"

void EditActionsWiring::wire()
{
    auto* ui  = m_d.ui;
    auto* kec = m_d.kec;
    auto* dlw = m_d.dlw;
    auto* kfc = m_d.kfc;

    // 棋譜コピー
    QObject::connect(ui->actionCopyKIF,        &QAction::triggered, kec, &KifuExportController::copyKifToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyKI2,        &QAction::triggered, kec, &KifuExportController::copyKi2ToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyCSA,        &QAction::triggered, kec, &KifuExportController::copyCsaToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyUSICurrent, &QAction::triggered, kec, &KifuExportController::copyUsiCurrentToClipboard, Qt::UniqueConnection);
    QObject::connect(ui->actionCopyUSIAll,     &QAction::triggered, kec, &KifuExportController::copyUsiToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyJKF,        &QAction::triggered, kec, &KifuExportController::copyJkfToClipboard,        Qt::UniqueConnection);
    QObject::connect(ui->actionCopyUSEN,       &QAction::triggered, kec, &KifuExportController::copyUsenToClipboard,       Qt::UniqueConnection);

    // 局面コピー
    QObject::connect(ui->actionCopySFEN,       &QAction::triggered, kec, &KifuExportController::copySfenToClipboard,       Qt::UniqueConnection);
    QObject::connect(ui->actionCopyBOD,        &QAction::triggered, kec, &KifuExportController::copyBodToClipboard,        Qt::UniqueConnection);

    // 棋譜貼り付け
    QObject::connect(ui->actionPasteKifu,            &QAction::triggered, kfc, &KifuFileController::pasteKifuFromClipboard, Qt::UniqueConnection);

    // 局面集ビューア
    QObject::connect(ui->actionSfenCollectionViewer, &QAction::triggered, dlw, &DialogLaunchWiring::displaySfenCollectionViewer, Qt::UniqueConnection);
}
