/// @file dialoglaunchwiring.cpp
/// @brief ダイアログ起動配線クラスの実装

#include "dialoglaunchwiring.h"
#include "piecesoundplayer.h"
#include "piecesoundsettingsdialog.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QMessageBox>
#include <utility>

#include "dialogcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
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

DialogLaunchWiring::DialogLaunchWiring(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
}

void DialogLaunchWiring::displayVersionInformation()
{
    auto* dc = m_deps.dialogCoordinator;
    if (dc) {
        dc->showVersionInformation();
    }
}

void DialogLaunchWiring::displayEngineSettingsDialog()
{
    auto* dc = m_deps.dialogCoordinator;
    if (dc) {
        dc->showEngineSettingsDialog();
    }
}

bool DialogLaunchWiring::choosePromotion()
{
    auto* dc = m_deps.dialogCoordinator;
    return dc && dc->showPromotionDialog();
}

void DialogLaunchWiring::displayJishogiScoreDialog()
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

void DialogLaunchWiring::handleNyugyokuDeclaration()
{
    CsaGameCoordinator* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr;
    if (m_deps.playMode && *m_deps.playMode == PlayMode::CsaNetworkMode && csa) {
        // 成否はサーバーが判定する。誤操作で負けないよう、ローカル対局と同じく確認する
        if (csa->isMyTurn()) {
            // 駒落ちはローカル対局と同じく、対局情報の見出しに合わせて下手・上手と呼ぶ
            const MatchCoordinator* match = m_deps.match;
            const QString declarer = NyugyokuJudgement::declarerName(
                csa->isBlackSide(), match && match->usesHandicapNames());
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
                              static_cast<int>(*m_deps.playMode));
    }
}

void DialogLaunchWiring::displayTsumeShogiSearchDialog()
{
    *m_deps.playMode = PlayMode::TsumiSearchMode;

    auto* dc = m_deps.dialogCoordinator;
    if (!dc) return;

    // 詰み探索開始直前に DialogCoordinator の依存（モデル・解析タブ・司令塔）を渡す。
    // DialogCoordinator は司令塔より先に作られることがあるため、ここで最新の値にそろえる。
    auto* analysisTab = m_deps.analysisTab;
    DialogCoordinator::Deps dcDeps;
    dcDeps.matchCoordinator = m_deps.match;
    dcDeps.considerationTabManager = analysisTab ? analysisTab->considerationTabManager() : nullptr;
    dcDeps.logModel = m_deps.lineEditModel1;
    dcDeps.thinkingModel = m_deps.modelThinking1;
    dc->updateDeps(dcDeps);

    dc->showTsumeSearchDialogFromContext();
}

void DialogLaunchWiring::displayTsumePlayDialog()
{
    TsumeCollectionDialog dialog(m_deps.parentWidget);
    // 詰将棋対局の指し手でも、本譜の対局と同じ駒音を鳴らす（オフなら PieceSoundPlayer 側で鳴らない）
    if (auto* player = m_deps.pieceSoundPlayer)
        connect(&dialog, &TsumeCollectionDialog::movePlayed, player, &PieceSoundPlayer::playMoveSound);
    dialog.exec();
}

void DialogLaunchWiring::displayTsumeshogiGeneratorDialog()
{
    TsumeshogiGeneratorDialog dialog(m_deps.parentWidget);
    dialog.exec();
}

void DialogLaunchWiring::displayMenuWindow()
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

void DialogLaunchWiring::displayCsaGameDialog()
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
                    this, &DialogLaunchWiring::onCsaEngineScoreUpdatedInternal,
                    Qt::UniqueConnection);
        }
    }
}

void DialogLaunchWiring::displayKifuAnalysisDialog()
{
    qCDebug(lcUi).noquote() << "displayKifuAnalysisDialog START";

    // 解析モードに遷移
    *m_deps.playMode = PlayMode::AnalysisMode;

    auto* dc = m_deps.dialogCoordinator;
    if (!dc) return;

    // 依存オブジェクトを確保
    auto* gameInfo = m_deps.gameInfoController;
    auto* ap = m_deps.analysisPresenter;

    // 解析に必要な依存オブジェクトを設定
    KifuAnalysisListModel* analysisModel = m_deps.analysisModel;
    auto* analysisTab = m_deps.analysisTab;
    DialogCoordinator::Deps dcDeps;
    dcDeps.matchCoordinator = m_deps.match;
    dcDeps.analysisModel = analysisModel;
    dcDeps.considerationTabManager = analysisTab ? analysisTab->considerationTabManager() : nullptr;
    dcDeps.logModel = m_deps.lineEditModel1;
    dcDeps.thinkingModel = m_deps.modelThinking1;
    dc->updateDeps(dcDeps);

    // 棋譜解析コンテキストを更新（遅延初期化されたオブジェクトを反映）
    DialogCoordinator::KifuAnalysisContext kifuCtx;
    kifuCtx.recordModel = m_deps.kifuRecordModel;
    kifuCtx.navState = m_deps.navState;
    kifuCtx.gameController = m_deps.gameController;
    kifuCtx.gameInfoController = gameInfo;
    kifuCtx.evalChart = m_deps.evalChart;
    kifuCtx.presenter = ap;
    auto* sv = m_deps.shogiView;
    kifuCtx.shogiView = sv;
    dc->setKifuAnalysisContext(kifuCtx);

    // コンテキストから自動パラメータ構築してダイアログを表示
    dc->showKifuAnalysisDialogFromContext();
}

void DialogLaunchWiring::displaySfenCollectionViewer()
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
            this, &DialogLaunchWiring::sfenCollectionPositionSelected);
    dlg->show();
}

void DialogLaunchWiring::onCsaEngineScoreUpdatedInternal(int scoreCp, int ply, const QString& mate)
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

void DialogLaunchWiring::displayPieceSoundSettingsDialog()
{
    auto* player = m_deps.pieceSoundPlayer;
    if (!player) return;

    PieceSoundSettingsDialog dialog(player, m_deps.parentWidget);
    dialog.exec();
}
