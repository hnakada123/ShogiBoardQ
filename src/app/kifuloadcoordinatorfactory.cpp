/// @file kifuloadcoordinatorfactory.cpp
/// @brief KifuLoadCoordinator の生成・配線ロジック

#include "kifuloadcoordinatorfactory.h"
#include "mainwindow.h"
#include "matchruntimequeryservice.h"
#include "kifuloadcoordinator.h"
#include "playerinfowiring.h"
#include "gameinfopanecontroller.h"
#include "engineanalysistab.h"
#include "branchnavigationwiring.h"
#include "kifunavigationcoordinator.h"
#include "uistatepolicymanager.h"
#include "mainwindowserviceregistry.h"
#include "uinotificationservice.h"

void KifuLoadCoordinatorFactory::createAndWire(MainWindow& mw)
{
    // 既存があれば遅延破棄（イベントキュー内の保留シグナルによるdangling pointer防止）
    if (mw.m_kifuLoadCoordinator) {
        mw.m_kifuLoadCoordinator->deleteLater();
        mw.m_kifuLoadCoordinator = nullptr;
    }

    mw.m_kifuLoadCoordinator = new KifuLoadCoordinator(
        /* gameMoves           */ mw.m_kifu.gameMoves,
        /* positionStrList     */ mw.m_kifu.positionStrList,
        /* activePly           */ mw.m_kifu.activePly,
        /* currentSelectedPly  */ mw.m_kifu.currentSelectedPly,
        /* currentMoveIndex    */ mw.m_state.currentMoveIndex,
        /* sfenRecord          */ mw.m_queryService->sfenRecord(),
        /* gameInfoTable       */ mw.m_gameInfoController ? mw.m_gameInfoController->tableWidget() : nullptr,
        /* gameInfoDock        */ nullptr,  // GameInfoPaneControllerに移行済み
        /* tab                 */ mw.m_tab,
        /* recordPane          */ mw.m_recordPane,
        /* kifuRecordModel     */ mw.m_models.kifuRecord,
        /* kifuBranchModel     */ mw.m_models.kifuBranch,
        /* parent              */ &mw
        );

    // 分岐ツリーとナビゲーション状態を設定
    if (mw.m_branchNav.branchTree != nullptr) {
        mw.m_kifuLoadCoordinator->setBranchTree(mw.m_branchNav.branchTree);
    }
    if (mw.m_branchNav.navState != nullptr) {
        mw.m_kifuLoadCoordinator->setNavigationState(mw.m_branchNav.navState);
    }

    // 分岐ツリー構築完了シグナルを BranchNavigationWiring へ直接接続
    mw.ensureBranchNavigationWiring();
    QObject::connect(mw.m_kifuLoadCoordinator, &KifuLoadCoordinator::branchTreeBuilt,
                     mw.m_branchNavWiring, &BranchNavigationWiring::onBranchTreeBuilt, Qt::UniqueConnection);

    // Analysisタブ・ShogiViewとの配線
    if (mw.m_analysisTab) {
        mw.m_kifuLoadCoordinator->setBranchTreeManager(mw.m_analysisTab->branchTreeManager());
    }
    mw.m_kifuLoadCoordinator->setShogiView(mw.m_shogiView);

    // UI更新通知
    QObject::connect(mw.m_kifuLoadCoordinator, &KifuLoadCoordinator::displayGameRecord,
                     &mw, &MainWindow::displayGameRecord, Qt::UniqueConnection);
    mw.ensureKifuNavigationCoordinator();
    QObject::connect(mw.m_kifuLoadCoordinator, &KifuLoadCoordinator::syncBoardAndHighlightsAtRow,
                     mw.m_kifuNavCoordinator, &KifuNavigationCoordinator::syncBoardAndHighlightsAtRow, Qt::UniqueConnection);
    mw.m_registry->ensureUiStatePolicyManager();
    QObject::connect(mw.m_kifuLoadCoordinator, &KifuLoadCoordinator::enableArrowButtons,
                     mw.m_uiStatePolicy, &UiStatePolicyManager::enableNavigationIfAllowed, Qt::UniqueConnection);

    // 対局情報の元データを保存（PlayerInfoWiring経由）
    mw.ensurePlayerInfoWiring();
    QObject::connect(mw.m_kifuLoadCoordinator, &KifuLoadCoordinator::gameInfoPopulated,
                     mw.m_playerInfoWiring, &PlayerInfoWiring::setOriginalGameInfo, Qt::UniqueConnection);
    mw.m_registry->ensureUiNotificationService();
    QObject::connect(mw.m_kifuLoadCoordinator, &KifuLoadCoordinator::errorOccurred,
                     mw.m_notificationService, &UiNotificationService::displayErrorMessage, Qt::UniqueConnection);
}
