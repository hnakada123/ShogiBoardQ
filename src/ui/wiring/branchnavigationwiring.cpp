/// @file branchnavigationwiring.cpp
/// @brief 分岐ナビゲーション配線クラスの実装

#include "branchnavigationwiring.h"
#include "branchtreeeditcontroller.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifudisplaycoordinator.h"
#include "livegamesession.h"
#include "gamerecordmodel.h"
#include "recordpane.h"
#include "engineanalysistab.h"
#include "commentcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogitypes.h"
#include "logcategories.h"

BranchNavigationWiring::BranchNavigationWiring(QObject* parent)
    : QObject(parent)
{
}

void BranchNavigationWiring::updateDeps(const Deps& deps)
{
    m_deps = deps;
    wireBranchEditing();
}

void BranchNavigationWiring::initialize()
{
    if (m_initialized || m_deps.displayCoordinator == nullptr || m_deps.kifuNavController == nullptr) {
        return;
    }
    m_initialized = true;

    connectNavigationButtons();
    configureDisplayCoordinator();
    connectDisplaySignals();
    wireBranchEditing();
}

void BranchNavigationWiring::wireBranchEditing()
{
    if (m_deps.branchTree == nullptr || m_deps.kifuNavController == nullptr) return;

    if (m_editController == nullptr) {
        m_editController = new BranchTreeEditController(this);
    }
    BranchTreeEditController::Deps deps;
    deps.tree = m_deps.branchTree;
    deps.navState = m_deps.navState;
    deps.navController = m_deps.kifuNavController;
    deps.liveSession = m_deps.liveGameSession;
    deps.parentWidget = m_deps.analysisTab;
    deps.markGameRecordDirty = m_deps.markGameRecordDirty;
    QString* startSfen = m_deps.startSfenStr;
    deps.startPositionSfen = [startSfen]() { return startSfen ? *startSfen : QString(); };
    deps.treeRootCreated = std::bind(&BranchNavigationWiring::onBranchTreeBuilt, this);
    m_editController->updateDeps(deps);
    if (m_deps.analysisTab != nullptr) {
        m_editController->attach(m_deps.analysisTab->branchTreeManager());
    }
    if (m_deps.recordPane != nullptr) {
        connect(m_deps.recordPane, &RecordPane::branchContextMenuRequested,
                m_editController, &BranchTreeEditController::onCandidateContextMenuRequested, Qt::UniqueConnection);
    }
}

void BranchNavigationWiring::connectNavigationButtons()
{
    if (m_deps.recordPane == nullptr) return;

    KifuNavigationController::Buttons buttons;
    buttons.first = m_deps.recordPane->firstButton();
    buttons.back10 = m_deps.recordPane->back10Button();
    buttons.prev = m_deps.recordPane->prevButton();
    buttons.next = m_deps.recordPane->nextButton();
    buttons.fwd10 = m_deps.recordPane->fwd10Button();
    buttons.last = m_deps.recordPane->lastButton();
    m_deps.kifuNavController->connectButtons(buttons);
    qCDebug(lcApp).noquote() << "BranchNavigationWiring::connectNavigationButtons: buttons connected";
}

void BranchNavigationWiring::configureDisplayCoordinator()
{
    KifuDisplayCoordinator* dc = m_deps.displayCoordinator;

    dc->setRecordPane(m_deps.recordPane);
    dc->setRecordModel(m_deps.kifuRecordModel);
    dc->setBranchModel(m_deps.kifuBranchModel);

    // BranchTreeManager を設定（分岐ツリーハイライト用）
    if (m_deps.analysisTab != nullptr) {
        dc->setBranchTreeManager(m_deps.analysisTab->branchTreeManager());
    }

    dc->setBoardSfenProvider([this]() {
        if (m_deps.gameController == nullptr || m_deps.gameController->board() == nullptr) {
            return QString();
        }
        ShogiBoard* board = m_deps.gameController->board();
        QString standPart = board->convertStandToSfen();
        if (standPart.isEmpty()) {
            standPart = QStringLiteral("-");
        }
        const QString turnPart = turnToSfen(board->currentPlayer());
        return QStringLiteral("%1 %2 %3 1")
            .arg(board->convertBoardToSfen(), turnPart, standPart);
    });

    dc->wireSignals();

    if (m_deps.liveGameSession != nullptr) {
        dc->setLiveGameSession(m_deps.liveGameSession);
    }
}

void BranchNavigationWiring::connectDisplaySignals()
{
    KifuDisplayCoordinator* dc = m_deps.displayCoordinator;
    KifuNavigationController* nc = m_deps.kifuNavController;

    // 盤面とハイライト更新シグナルを転送
    connect(dc, &KifuDisplayCoordinator::boardWithHighlightsRequired,
            this, &BranchNavigationWiring::boardWithHighlightsRequired);

    // 盤面のみ更新シグナルを転送
    connect(dc, &KifuDisplayCoordinator::boardSfenChanged,
            this, &BranchNavigationWiring::boardSfenChanged);

    // コメント表示シグナルを接続
    if (m_deps.ensureCommentCoordinator) {
        m_deps.ensureCommentCoordinator();
    }
    if (m_deps.commentCoordinator) {
        connect(dc, &KifuDisplayCoordinator::commentUpdateRequired,
                m_deps.commentCoordinator, &CommentCoordinator::onNavigationCommentUpdate);
        // 利用者の操作で別の手へ移る前に、未更新のコメントの破棄を確認する
        nc->setLeaveGuard(std::bind(&CommentCoordinator::confirmLeaveEditingMove, m_deps.commentCoordinator));
    }

    connect(nc, &KifuNavigationController::recordHighlightRequired,
            this, &BranchNavigationWiring::currentPlyChanged);

    // 分岐ノード処理完了シグナルを転送
    connect(nc, &KifuNavigationController::branchNodeHandled,
            this, &BranchNavigationWiring::branchNodeHandled);

    // lineSelectionChanged: KifuNavigationState が currentLineIndex() を管理するため接続不要
}

void BranchNavigationWiring::onBranchTreeBuilt()
{
    qCDebug(lcApp) << "BranchNavigationWiring::onBranchTreeBuilt: updating navigation system";

    // 新しいナビゲーションシステムを更新
    if (m_deps.navState != nullptr && m_deps.branchTree != nullptr) {
        m_deps.navState->setTree(m_deps.branchTree);
    }

    // 表示コーディネーターにツリー変更を通知
    if (m_deps.displayCoordinator != nullptr) {
        m_deps.displayCoordinator->onTreeChanged();
    }

    if (m_deps.gameRecordModel != nullptr && m_deps.branchTree != nullptr) {
        m_deps.gameRecordModel->setBranchTree(m_deps.branchTree);
        if (m_deps.navState != nullptr) {
            m_deps.gameRecordModel->setNavigationState(m_deps.navState);
        }
    }
}

void BranchNavigationWiring::onBranchNodeActivated(int row, int ply)
{
    qCDebug(lcApp).noquote() << "BranchNavigationWiring::onBranchNodeActivated ENTER row=" << row << "ply=" << ply;
    if (m_deps.kifuNavController == nullptr) {
        qCDebug(lcApp).noquote() << "BranchNavigationWiring::onBranchNodeActivated: kifuNavController is null";
        return;
    }

    m_deps.kifuNavController->handleBranchNodeActivated(row, ply);
    qCDebug(lcApp).noquote() << "BranchNavigationWiring::onBranchNodeActivated LEAVE";
}

void BranchNavigationWiring::onBranchTreeResetForNewGame()
{
    LiveGameSession* ls = m_deps.liveGameSession;
    KifuBranchTree* bt = m_deps.branchTree;
    KifuNavigationState* ns = m_deps.navState;

    // 前の対局のライブゲームセッションが残っていれば破棄する（新しいセッションは、
    // この後の開始前の整理 performCleanup() が新しい根から始める）
    if (ls && ls->isActive()) {
        ls->discard();
    }
    // resetToInitialState() と同じパターンで分岐ツリーを直接リセット
    if (bt) {
        if (ns) {
            ns->setCurrentNode(nullptr);
            ns->resetPreferredLineIndex();
        }
        if (m_deps.startSfenStr) {
            bt->setRootSfen(*m_deps.startSfenStr);
        }
        if (ns) {
            ns->goToRoot();
        }
    }
}
