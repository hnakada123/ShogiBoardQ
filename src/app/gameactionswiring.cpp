/// @file gameactionswiring.cpp
/// @brief 対局/解析/検討/詰み関連アクションの配線クラスの実装

#include "gameactionswiring.h"
#include "ui_mainwindow.h"
#include "analysissession.h"
#include "gamesession.h"
#include "mainwindowactions.h"
#include "dialoglauncher.h"
#include "dialogcoordinatorwiring.h"
#include "logcategories.h"

void GameActionsWiring::wire()
{
    auto* ui  = m_d.ui;
    auto* mw  = m_d.actions;
    auto* dlw = m_d.dlw;
    auto* dcw = m_d.dcw;
    auto* session = m_d.session;
    auto* analysis = m_d.analysis;

    if (Q_UNLIKELY(!ui || !mw)) {
        qCWarning(lcUi, "GameActionsWiring::wire(): ui or mw is null");
        return;
    }
    if (Q_UNLIKELY(!dlw || !dcw || !session || !analysis)) {
        qCWarning(lcUi, "GameActionsWiring::wire(): required wiring dependency is null "
                  "(dlw=%p, dcw=%p, session=%p, analysis=%p)",
                  static_cast<void*>(dlw), static_cast<void*>(dcw), static_cast<void*>(session),
                  static_cast<void*>(analysis));
        return;
    }

    // 対局
    QObject::connect(ui->actionNewGame,      &QAction::triggered, mw,  &MainWindowActions::resetToInitialState,  Qt::UniqueConnection);
    QObject::connect(ui->actionStartGame,    &QAction::triggered, session, &GameSession::startGameFromDialog, Qt::UniqueConnection);
    QObject::connect(ui->actionCSA,          &QAction::triggered, dlw, &DialogLauncher::displayCsaGameDialog, Qt::UniqueConnection);
    QObject::connect(ui->actionResign,       &QAction::triggered, session, &GameSession::resign,              Qt::UniqueConnection);
    QObject::connect(ui->actionBreakOffGame, &QAction::triggered, session, &GameSession::breakOff,            Qt::UniqueConnection);
    QObject::connect(ui->actionResumeGame,   &QAction::triggered, session, &GameSession::resumeInterruptedGame, Qt::UniqueConnection);

    QObject::connect(ui->actionTsumePlay, &QAction::triggered, dlw, &DialogLauncher::displayTsumePlayDialog, Qt::UniqueConnection);

    // 解析/検討/詰み・エンジン設定
    QObject::connect(ui->actionEngineSettings,       &QAction::triggered, dlw, &DialogLauncher::displayEngineSettingsDialog,       Qt::UniqueConnection);
    QObject::connect(ui->actionPieceSoundSettings,   &QAction::triggered, dlw, &DialogLauncher::displayPieceSoundSettingsDialog,   Qt::UniqueConnection);
    QObject::connect(ui->actionAnalyzeKifu,          &QAction::triggered, dlw, &DialogLauncher::displayKifuAnalysisDialog,         Qt::UniqueConnection);
    QObject::connect(ui->actionCancelAnalyzeKifu,    &QAction::triggered, dcw, &DialogCoordinatorWiring::cancelKifuAnalysis,           Qt::UniqueConnection);
    QObject::connect(ui->actionStartEditPosition,    &QAction::triggered, mw,  &MainWindowActions::beginPositionEditing,                     Qt::UniqueConnection);
    QObject::connect(ui->actionEndEditPosition,      &QAction::triggered, mw,  &MainWindowActions::finishPositionEditing,                    Qt::UniqueConnection);
    QObject::connect(ui->actionTsumeShogiSearch,     &QAction::triggered, dlw, &DialogLauncher::displayTsumeShogiSearchDialog,    Qt::UniqueConnection);
    QObject::connect(ui->actionStopTsumeSearch,      &QAction::triggered, analysis, &AnalysisSession::stop,                        Qt::UniqueConnection);
    QObject::connect(ui->actionTsumeshogiGenerator,  &QAction::triggered, dlw, &DialogLauncher::displayTsumeshogiGeneratorDialog, Qt::UniqueConnection);
    QObject::connect(ui->actionJishogiScore,         &QAction::triggered, dlw, &DialogLauncher::displayJishogiScoreDialog,        Qt::UniqueConnection);
    QObject::connect(ui->actionNyugyokuDeclaration,  &QAction::triggered, dlw, &DialogLauncher::handleNyugyokuDeclaration,        Qt::UniqueConnection);
}
