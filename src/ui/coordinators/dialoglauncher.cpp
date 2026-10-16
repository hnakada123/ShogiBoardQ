/// @file dialoglauncher.cpp
/// @brief メニューから開くダイアログの起動の実装

#include "dialoglauncher.h"
#include "piecesoundplayer.h"
#include "piecesoundsettingsdialog.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QMessageBox>
#include <utility>

#include "aboutcoordinator.h"
#include "boardcolordialog.h"
#include "fontsettingsdialog.h"
#include "engineregistrationdialog.h"
#include "promotionflow.h"
#include "shogiview.h"
#include "appactivitystate.h"
#include "jishogiscoredialogcontroller.h"
#include "nyugyokudeclarationhandler.h"
#include "nyugyokujudgement.h"
#include "csagamesession.h"
#include "csagamedialog.h"
#include "csagamecoordinator.h"
#include "dialogutils.h"
#include "sfencollectiondialog.h"
#include "tsumeshogigeneratordialog.h"
#include "tsumecollectiondialog.h"
#include "kifufilecontroller.h"
#include "logcategories.h"

DialogLauncher::DialogLauncher(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
}

void DialogLauncher::displayVersionInformation()
{
    AboutCoordinator::showVersionDialog(m_deps.parentWidget);
}

void DialogLauncher::openProjectWebsite()
{
    AboutCoordinator::openProjectWebsite();
}

void DialogLauncher::displayEngineSettingsDialog()
{
    EngineRegistrationDialog dialog(m_deps.parentWidget);
    dialog.exec();
}

bool DialogLauncher::choosePromotion()
{
    return PromotionFlow::askPromote(m_deps.parentWidget);
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
    CsaGameCoordinator* csa = m_deps.csaGame;
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

    if (auto* nh = m_deps.nyugyokuHandler) {
        nh->handleDeclaration(m_deps.parentWidget, sv->board(),
                              m_deps.activity && m_deps.activity->is(AppActivityState::Activity::DuringGame));
    }
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
    // ドックの表示/非表示をトグル
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
        // 待機ダイアログを出して対局が始まるまで待つ（始まった後は CsaGameSession が進める）
        if (m_deps.csaSession) m_deps.csaSession->startGame(dlg->selection());
    }
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

void DialogLauncher::displayPieceSoundSettingsDialog()
{
    auto* player = m_deps.pieceSoundPlayer;
    if (!player) return;

    PieceSoundSettingsDialog dialog(player, m_deps.parentWidget);
    dialog.exec();
}

void DialogLauncher::displayBoardAppearanceDialog()
{
    if (!m_boardColorDialog) {
        m_boardColorDialog = new BoardColorDialog(m_deps.parentWidget);
        m_boardColorDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_boardColorDialog->show();
    m_boardColorDialog->raise();
    m_boardColorDialog->activateWindow();
}

void DialogLauncher::displayFontSettingsDialog()
{
    FontSettingsDialog dialog(m_deps.parentWidget);
    dialog.exec();
}
