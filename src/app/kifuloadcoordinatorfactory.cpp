/// @file kifuloadcoordinatorfactory.cpp
/// @brief KifuLoadCoordinator の生成・配線ロジック

#include "kifuloadcoordinatorfactory.h"
#include "mainwindow.h"
#include "kifuloadcoordinator.h"
#include "playerinfowiring.h"
#include "gameinfopanecontroller.h"
#include "engineanalysistab.h"
#include "branchnavigationwiring.h"
#include "kifunavigationcoordinator.h"
#include "uistatepolicymanager.h"
#include "uinotificationservice.h"

KifuLoadCoordinator* KifuLoadCoordinatorFactory::createAndWire(const Params& p)
{
    auto* coordinator = new KifuLoadCoordinator(
        /* gameMoves           */ *p.gameMoves,
        /* positionStrList     */ *p.positionStrList,
        /* activePly           */ *p.activePly,
        /* currentSelectedPly  */ *p.currentSelectedPly,
        /* currentMoveIndex    */ *p.currentMoveIndex,
        /* sfenRecord          */ p.sfenRecord,
        /* gameInfoTable       */ p.gameInfoController ? p.gameInfoController->tableWidget() : nullptr,
        /* gameInfoDock        */ nullptr,  // GameInfoPaneControllerに移行済み
        /* tab                 */ p.tab,
        /* recordPane          */ p.recordPane,
        /* kifuRecordModel     */ p.kifuRecordModel,
        /* kifuBranchModel     */ p.kifuBranchModel,
        /* parent              */ p.parent
        );

    // 分岐ツリーとナビゲーション状態を設定
    if (p.branchTree) {
        coordinator->setBranchTree(p.branchTree);
    }
    if (p.navState) {
        coordinator->setNavigationState(p.navState);
    }

    // 分岐ツリー構築完了シグナルを BranchNavigationWiring へ直接接続
    if (p.branchNavWiring) {
        QObject::connect(coordinator, &KifuLoadCoordinator::branchTreeBuilt,
                         p.branchNavWiring, &BranchNavigationWiring::onBranchTreeBuilt, Qt::UniqueConnection);
    }

    // Analysisタブ・ShogiViewとの配線
    if (p.analysisTab) {
        coordinator->setBranchTreeManager(p.analysisTab->branchTreeManager());
    }
    coordinator->setShogiView(p.shogiView);

    // UI更新通知
    if (p.mainWindow) {
        QObject::connect(coordinator, &KifuLoadCoordinator::displayGameRecord,
                         p.mainWindow, &MainWindow::displayGameRecord, Qt::UniqueConnection);
    }
    if (p.kifuNavCoordinator) {
        QObject::connect(coordinator, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow,
                         p.kifuNavCoordinator, &KifuNavigationCoordinator::syncBoardAndHighlightsAtRow, Qt::UniqueConnection);
    }
    if (p.uiStatePolicy) {
        QObject::connect(coordinator, &KifuLoadCoordinator::enableArrowButtons,
                         p.uiStatePolicy, &UiStatePolicyManager::enableNavigationIfAllowed, Qt::UniqueConnection);
    }

    // 対局情報の元データを保存（PlayerInfoWiring経由）
    if (p.playerInfoWiring) {
        QObject::connect(coordinator, &KifuLoadCoordinator::gameInfoPopulated,
                         p.playerInfoWiring, &PlayerInfoWiring::setOriginalGameInfo, Qt::UniqueConnection);
    }
    if (p.notificationService) {
        QObject::connect(coordinator, &KifuLoadCoordinator::errorOccurred,
                         p.notificationService, &UiNotificationService::displayErrorMessage, Qt::UniqueConnection);
    }

    return coordinator;
}
