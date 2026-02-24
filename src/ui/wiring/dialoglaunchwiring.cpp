/// @file dialoglaunchwiring.cpp
/// @brief ダイアログ起動配線クラスの実装

#include "dialoglaunchwiring.h"

#include <QCoreApplication>
#include <QMessageBox>

#include "dialogcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "matchcoordinator.h"
#include "jishogiscoredialogcontroller.h"
#include "nyugyokudeclarationhandler.h"
#include "csagamewiring.h"
#include "csagamedialog.h"
#include "csagamecoordinator.h"
#include "boardsetupcontroller.h"
#include "playerinfowiring.h"
#include "analysisresultspresenter.h"
#include "engineanalysistab.h"
#include "kifuanalysislistmodel.h"
#include "sfencollectiondialog.h"
#include "tsumeshogigeneratordialog.h"
#include "logcategories.h"

DialogLaunchWiring::DialogLaunchWiring(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
}

void DialogLaunchWiring::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void DialogLaunchWiring::displayVersionInformation()
{
    auto* dc = m_deps.getDialogCoordinator ? m_deps.getDialogCoordinator() : nullptr;
    if (dc) {
        dc->showVersionInformation();
    }
}

void DialogLaunchWiring::displayEngineSettingsDialog()
{
    auto* dc = m_deps.getDialogCoordinator ? m_deps.getDialogCoordinator() : nullptr;
    if (dc) {
        dc->showEngineSettingsDialog();
    }
}

void DialogLaunchWiring::displayPromotionDialog()
{
    auto* gc = m_deps.getGameController ? m_deps.getGameController() : nullptr;
    if (!gc) return;
    auto* dc = m_deps.getDialogCoordinator ? m_deps.getDialogCoordinator() : nullptr;
    if (dc) {
        const bool promote = dc->showPromotionDialog();
        gc->setPromote(promote);
    }
}

void DialogLaunchWiring::displayJishogiScoreDialog()
{
    auto* sv = m_deps.getShogiView ? m_deps.getShogiView() : nullptr;
    if (!sv || !sv->board()) {
        QMessageBox::warning(m_deps.parentWidget,
                             QCoreApplication::translate("MainWindow", "エラー"),
                             QCoreApplication::translate("MainWindow", "盤面データがありません。"));
        return;
    }

    auto* jc = m_deps.getJishogiController ? m_deps.getJishogiController() : nullptr;
    if (jc) {
        jc->showDialog(m_deps.parentWidget, sv->board());
    }
}

void DialogLaunchWiring::handleNyugyokuDeclaration()
{
    auto* sv = m_deps.getShogiView ? m_deps.getShogiView() : nullptr;
    if (!sv || !sv->board()) {
        QMessageBox::warning(m_deps.parentWidget,
                             QCoreApplication::translate("MainWindow", "エラー"),
                             QCoreApplication::translate("MainWindow", "盤面データがありません。"));
        return;
    }

    auto* nh = m_deps.getNyugyokuHandler ? m_deps.getNyugyokuHandler() : nullptr;
    if (nh) {
        auto* gc = m_deps.getGameController ? m_deps.getGameController() : nullptr;
        auto* match = m_deps.getMatch ? m_deps.getMatch() : nullptr;
        nh->setGameController(gc);
        nh->setMatchCoordinator(match);
        nh->handleDeclaration(m_deps.parentWidget, sv->board(),
                              static_cast<int>(*m_deps.playMode));
    }
}

void DialogLaunchWiring::displayTsumeShogiSearchDialog()
{
    *m_deps.playMode = PlayMode::TsumiSearchMode;

    auto* dc = m_deps.getDialogCoordinator ? m_deps.getDialogCoordinator() : nullptr;
    if (dc) {
        dc->showTsumeSearchDialogFromContext();
    }
}

void DialogLaunchWiring::displayTsumeshogiGeneratorDialog()
{
    TsumeshogiGeneratorDialog dialog(m_deps.parentWidget);
    dialog.exec();
}

void DialogLaunchWiring::displayMenuWindow()
{
    // ドックが未作成の場合は作成
    if (m_deps.menuWindowDock && !*m_deps.menuWindowDock) {
        if (m_deps.createMenuWindowDock) {
            m_deps.createMenuWindowDock();
        }
    }

    // ドックの表示/非表示をトグル
    if (m_deps.menuWindowDock && *m_deps.menuWindowDock) {
        QDockWidget* dock = *m_deps.menuWindowDock;
        if (dock->isVisible()) {
            dock->hide();
        } else {
            dock->show();
            dock->raise();
        }
    }
}

void DialogLaunchWiring::displayCsaGameDialog()
{
    // ダイアログが未作成の場合は作成する
    if (m_deps.csaGameDialog && !*m_deps.csaGameDialog) {
        *m_deps.csaGameDialog = new CsaGameDialog(m_deps.parentWidget);
    }

    CsaGameDialog* dlg = m_deps.csaGameDialog ? *m_deps.csaGameDialog : nullptr;
    if (!dlg) return;

    // ダイアログを表示する
    if (dlg->exec() == QDialog::Accepted) {
        // CsaGameWiringを確保
        auto* csaWiring = m_deps.getCsaGameWiring ? m_deps.getCsaGameWiring() : nullptr;
        if (!csaWiring) {
            qCWarning(lcUi).noquote() << "displayCsaGameDialog: CsaGameWiring is null";
            return;
        }

        // BoardSetupControllerを確保して設定
        auto* bsc = m_deps.getBoardSetupController ? m_deps.getBoardSetupController() : nullptr;
        csaWiring->setBoardSetupController(bsc);
        auto* analysisTab = m_deps.getAnalysisTab ? m_deps.getAnalysisTab() : nullptr;
        csaWiring->setAnalysisTab(analysisTab);

        // プレイモードをCSA通信対局に設定
        *m_deps.playMode = PlayMode::CsaNetworkMode;

        // CSA対局を開始（ロジックはCsaGameWiringに委譲）
        csaWiring->startCsaGame(dlg, m_deps.parentWidget);

        // CsaGameCoordinatorの参照を保持（他のコードとの互換性のため）
        if (m_deps.csaGameCoordinator) {
            *m_deps.csaGameCoordinator = csaWiring->coordinator();
        }

        // CSA対局でエンジンを使用する場合、評価値グラフ更新用のシグナル接続
        CsaGameCoordinator* coord = csaWiring->coordinator();
        if (coord) {
            connect(coord, &CsaGameCoordinator::engineScoreUpdated,
                    this, &DialogLaunchWiring::csaEngineScoreUpdated,
                    Qt::UniqueConnection);
        }
    }
}

void DialogLaunchWiring::displayKifuAnalysisDialog()
{
    qCDebug(lcUi).noquote() << "displayKifuAnalysisDialog START";

    // 解析モードに遷移
    *m_deps.playMode = PlayMode::AnalysisMode;

    // 解析モデルが未生成ならここで作成
    if (m_deps.analysisModel && !*m_deps.analysisModel) {
        *m_deps.analysisModel = new KifuAnalysisListModel(m_deps.parentWidget);
    }

    auto* dc = m_deps.getDialogCoordinator ? m_deps.getDialogCoordinator() : nullptr;
    if (!dc) return;

    // 依存オブジェクトを確保
    auto* piw = m_deps.getPlayerInfoWiring ? m_deps.getPlayerInfoWiring() : nullptr;
    if (piw) {
        piw->ensureGameInfoController();
        if (m_deps.gameInfoController) {
            *m_deps.gameInfoController = piw->gameInfoController();
        }
    }
    auto* ap = m_deps.getAnalysisPresenter ? m_deps.getAnalysisPresenter() : nullptr;

    // 解析に必要な依存オブジェクトを設定
    KifuAnalysisListModel* analysisModel = m_deps.analysisModel ? *m_deps.analysisModel : nullptr;
    dc->setAnalysisModel(analysisModel);
    auto* analysisTab = m_deps.getAnalysisTab ? m_deps.getAnalysisTab() : nullptr;
    if (analysisTab) {
        dc->setConsiderationTabManager(analysisTab->considerationTabManager());
    }
    dc->setUsiEngine(m_deps.getUsi1 ? m_deps.getUsi1() : nullptr);
    dc->setLogModel(m_deps.getLineEditModel1 ? m_deps.getLineEditModel1() : nullptr);
    dc->setThinkingModel(m_deps.getModelThinking1 ? m_deps.getModelThinking1() : nullptr);

    // 棋譜解析コンテキストを更新（遅延初期化されたオブジェクトを反映）
    DialogCoordinator::KifuAnalysisContext kifuCtx;
    kifuCtx.sfenRecord = m_deps.getSfenRecord ? m_deps.getSfenRecord() : nullptr;
    kifuCtx.moveRecords = m_deps.moveRecords;
    kifuCtx.recordModel = m_deps.getKifuRecordModel ? m_deps.getKifuRecordModel() : nullptr;
    kifuCtx.activePly = m_deps.activePly;
    kifuCtx.gameController = m_deps.getGameController ? m_deps.getGameController() : nullptr;
    kifuCtx.gameInfoController = m_deps.gameInfoController ? *m_deps.gameInfoController : nullptr;
    kifuCtx.kifuLoadCoordinator = m_deps.getKifuLoadCoordinator ? m_deps.getKifuLoadCoordinator() : nullptr;
    kifuCtx.evalChart = m_deps.getEvalChart ? m_deps.getEvalChart() : nullptr;
    kifuCtx.gameUsiMoves = m_deps.gameUsiMoves;
    kifuCtx.presenter = ap;
    auto* sv = m_deps.getShogiView ? m_deps.getShogiView() : nullptr;
    kifuCtx.getBoardFlipped = [sv]() { return sv ? sv->getFlipMode() : false; };
    dc->setKifuAnalysisContext(kifuCtx);

    // コンテキストから自動パラメータ構築してダイアログを表示
    dc->showKifuAnalysisDialogFromContext();
}

void DialogLaunchWiring::displaySfenCollectionViewer()
{
    if (!m_deps.sfenCollectionDialog) return;

    // 既にダイアログが開いている場合はアクティブにする
    if (*m_deps.sfenCollectionDialog) {
        (*m_deps.sfenCollectionDialog)->raise();
        (*m_deps.sfenCollectionDialog)->activateWindow();
        return;
    }

    auto* dlg = new SfenCollectionDialog(m_deps.parentWidget);
    *m_deps.sfenCollectionDialog = dlg;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SfenCollectionDialog::positionSelected,
            this, &DialogLaunchWiring::sfenCollectionPositionSelected);
    dlg->show();
}
