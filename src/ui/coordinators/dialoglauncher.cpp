/// @file dialoglauncher.cpp
/// @brief メニューから開くダイアログの起動の実装

#include "dialoglauncher.h"
#include "piecesoundplayer.h"
#include "piecesoundsettingsdialog.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QMessageBox>
#include <utility>

#include "dialogcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "appactivitystate.h"
#include "matchcoordinator.h"
#include "jishogiscoredialogcontroller.h"
#include "nyugyokudeclarationhandler.h"
#include "nyugyokujudgement.h"
#include "csagamewiring.h"
#include "csagamedialog.h"
#include "csagamecoordinator.h"
#include "dialogutils.h"
#include "boardsetupcontroller.h"
#include "analysisresultspresenter.h"
#include "engineanalysistab.h"
#include "sfencollectiondialog.h"
#include "tsumeshogigeneratordialog.h"
#include "tsumecollectiondialog.h"
#include "evaluationchartwidget.h"
#include "kifufilecontroller.h"
#include "logcategories.h"

DialogLauncher::DialogLauncher(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
}

void DialogLauncher::displayVersionInformation()
{
    auto* dc = m_deps.dialogCoordinator;
    if (dc) {
        dc->showVersionInformation();
    }
}

void DialogLauncher::openProjectWebsite()
{
    auto* dc = m_deps.dialogCoordinator;
    if (dc) {
        dc->openProjectWebsite();
    }
}

void DialogLauncher::displayEngineSettingsDialog()
{
    auto* dc = m_deps.dialogCoordinator;
    if (dc) {
        dc->showEngineSettingsDialog();
    }
}

bool DialogLauncher::choosePromotion()
{
    auto* dc = m_deps.dialogCoordinator;
    return dc && dc->showPromotionDialog();
}

void DialogLauncher::displayJishogiScoreDialog()
{
    auto* sv = m_deps.shogiView;
    if (!sv || !sv->board()) {
        QMessageBox::warning(m_deps.parentWidget,
                             QCoreApplication::translate("MainWindow", "エラー"),
                             QCoreApplication::translate("MainWindow", "盤面データがありません。"));
        return;
    }

    auto* jc = m_deps.jishogiController;
    if (jc) {
        jc->showDialog(m_deps.parentWidget, sv->board());
    }
}

void DialogLauncher::handleNyugyokuDeclaration()
{
    CsaGameCoordinator* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr;
    if (m_deps.activity && m_deps.activity->is(AppActivityState::Activity::DuringCsaGame) && csa) {
        // 成否はサーバーが判定する。誤操作で負けないよう、ローカル対局と同じく確認する
        if (csa->isMyTurn()) {
            // 駒落ちはローカル対局と同じく、対局情報の見出しに合わせて下手・上手と呼ぶ
            const QString declarer = NyugyokuJudgement::declarerName(
                csa->isBlackSide(), m_deps.usesHandicapNames && m_deps.usesHandicapNames());
            if (!DialogUtils::confirmAction(
                    m_deps.parentWidget,
                    QCoreApplication::translate("NyugyokuDeclarationHandler", "入玉宣言確認"),
                    QCoreApplication::translate("NyugyokuDeclarationHandler",
                                                "%1が入玉宣言を行います。\n\n"
                                                "宣言条件を満たさない場合は宣言側の負けとなります。\n"
                                                "本当に宣言しますか？").arg(declarer),
                    QCoreApplication::translate("NyugyokuDeclarationHandler", "宣言する"))) {
                return;
            }
        }
        csa->declareWin();
        return;
    }
    auto* sv = m_deps.shogiView;
    if (!sv || !sv->board()) {
        QMessageBox::warning(m_deps.parentWidget,
                             QCoreApplication::translate("MainWindow", "エラー"),
                             QCoreApplication::translate("MainWindow", "盤面データがありません。"));
        return;
    }

    auto* nh = m_deps.nyugyokuHandler;
    if (nh) {
        auto* gc = m_deps.gameController;
        auto* match = m_deps.match;
        nh->setGameController(gc);
        nh->setMatchCoordinator(match);
        nh->handleDeclaration(m_deps.parentWidget, sv->board(),
                              m_deps.activity && m_deps.activity->is(AppActivityState::Activity::DuringGame));
    }
}

void DialogLauncher::displayTsumeShogiSearchDialog()
{
    if (auto* dc = m_deps.dialogCoordinator) dc->showTsumeSearchDialogFromContext();
}

void DialogLauncher::displayTsumePlayDialog()
{
    TsumeCollectionDialog dialog(m_deps.parentWidget);
    // 詰将棋対局の指し手でも、本譜の対局と同じ駒音を鳴らす（オフなら PieceSoundPlayer 側で鳴らない）
    if (auto* player = m_deps.pieceSoundPlayer)
        connect(&dialog, &TsumeCollectionDialog::movePlayed, player, &PieceSoundPlayer::playMoveSound);
    dialog.exec();
}

void DialogLauncher::displayTsumeshogiGeneratorDialog()
{
    TsumeshogiGeneratorDialog dialog(m_deps.parentWidget);
    dialog.exec();
}

void DialogLauncher::displayMenuWindow()
{
    // ドックの表示/非表示をトグル（ドックが未作成ならゲッターが作る）
    QDockWidget* dock = m_deps.menuWindowDock;
    if (!dock) return;
    if (dock->isVisible()) {
        dock->hide();
    } else {
        dock->show();
        dock->raise();
    }
}

void DialogLauncher::displayCsaGameDialog()
{
    // ダイアログが未作成の場合は作成する
    if (!m_csaGameDialog) {
        m_csaGameDialog = new CsaGameDialog(m_deps.parentWidget);
    }

    CsaGameDialog* dlg = m_csaGameDialog;
    dlg->refreshChoices();

    // ダイアログを表示する
    if (dlg->exec() == QDialog::Accepted) {
        if (m_deps.kifuFileController && !m_deps.kifuFileController->confirmDiscardUnsaved()) return;
        // CsaGameWiringを確保
        auto* csaWiring = m_deps.csaGameWiring;
        if (!csaWiring) {
            qCWarning(lcUi).noquote() << "displayCsaGameDialog: CsaGameWiring is null";
            return;
        }

        // BoardSetupControllerを確保して設定
        auto* bsc = m_deps.boardSetupController;
        csaWiring->setBoardSetupController(bsc);
        auto* analysisTab = m_deps.analysisTab;
        csaWiring->setAnalysisTab(analysisTab);

        // CSA対局を開始（ロジックはCsaGameWiringに委譲）。司令塔は CsaGameWiring が作って持つ
        if (!csaWiring->startCsaGame(dlg, m_deps.parentWidget)) return;

        // CSA対局でエンジンを使用する場合、評価値グラフ更新用のシグナル接続
        CsaGameCoordinator* coord = csaWiring->coordinator();
        if (coord) {
            connect(coord, &CsaGameCoordinator::engineScoreUpdated,
                    this, &DialogLauncher::onCsaEngineScoreUpdatedInternal,
                    Qt::UniqueConnection);
        }
    }
}

void DialogLauncher::displayKifuAnalysisDialog()
{
    qCDebug(lcUi).noquote() << "displayKifuAnalysisDialog START";


    // コンテキスト（DialogCoordinatorWiring が起動時に設定）からパラメータを作ってダイアログを表示
    if (auto* dc = m_deps.dialogCoordinator) dc->showKifuAnalysisDialogFromContext();
}

void DialogLauncher::displaySfenCollectionViewer()
{
    // 既にダイアログが開いている場合はアクティブにする
    if (m_sfenCollectionDialog) {
        m_sfenCollectionDialog->raise();
        m_sfenCollectionDialog->activateWindow();
        return;
    }

    auto* dlg = new SfenCollectionDialog(m_deps.parentWidget);
    m_sfenCollectionDialog = dlg;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SfenCollectionDialog::positionSelected,
            this, &DialogLauncher::sfenCollectionPositionSelected);
    dlg->show();
}

void DialogLauncher::onCsaEngineScoreUpdatedInternal(int scoreCp, int ply, const QString& mate)
{
    qCDebug(lcUi).noquote() << "onCsaEngineScoreUpdatedInternal: scoreCp=" << scoreCp << "ply=" << ply;

    auto* evalChart = m_deps.evalChart;
    if (!evalChart) {
        qCDebug(lcUi).noquote() << "onCsaEngineScoreUpdatedInternal: evalChart is NULL";
        return;
    }

    auto* csaCoord = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr;
    if (csaCoord) {
        if (csaCoord->isBlackSide()) {
            evalChart->appendScoreP1(ply, scoreCp, false, mate);
        } else {
            evalChart->appendScoreP2(ply, scoreCp, true, mate);
        }
    }
}

void DialogLauncher::displayPieceSoundSettingsDialog()
{
    auto* player = m_deps.pieceSoundPlayer;
    if (!player) return;

    PieceSoundSettingsDialog dialog(player, m_deps.parentWidget);
    dialog.exec();
}
