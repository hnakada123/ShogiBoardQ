/// @file branchnavigationwiring.cpp
/// @brief 分岐ナビゲーション配線クラスの実装

#include "branchnavigationwiring.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifudisplaycoordinator.h"
#include "branchtreewidget.h"
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
}

void BranchNavigationWiring::initialize()
{
    createModels();
    wireSignals();
}

// --- アクセサヘルパー（ポインタのポインタを安全に逆参照）---

static KifuBranchTree* tree(const BranchNavigationWiring::Deps& d) { return d.branchTree ? *d.branchTree : nullptr; }
static KifuNavigationState* navSt(const BranchNavigationWiring::Deps& d) { return d.navState ? *d.navState : nullptr; }
static KifuNavigationController* navCtl(const BranchNavigationWiring::Deps& d) { return d.kifuNavController ? *d.kifuNavController : nullptr; }
static KifuDisplayCoordinator* dispCo(const BranchNavigationWiring::Deps& d) { return d.displayCoordinator ? *d.displayCoordinator : nullptr; }
static BranchTreeWidget* treeW(const BranchNavigationWiring::Deps& d) { return d.branchTreeWidget ? *d.branchTreeWidget : nullptr; }
static LiveGameSession* liveSess(const BranchNavigationWiring::Deps& d) { return d.liveGameSession ? *d.liveGameSession : nullptr; }

void BranchNavigationWiring::createModels()
{
    if (!m_deps.branchTree) return;

    // ツリーの作成
    if (*m_deps.branchTree == nullptr) {
        *m_deps.branchTree = new KifuBranchTree(parent());
    }

    // ナビゲーション状態の作成
    if (*m_deps.navState == nullptr) {
        *m_deps.navState = new KifuNavigationState(parent());
        (*m_deps.navState)->setTree(*m_deps.branchTree);
    }

    // ナビゲーションコントローラの作成
    if (*m_deps.kifuNavController == nullptr) {
        *m_deps.kifuNavController = new KifuNavigationController(parent());
        (*m_deps.kifuNavController)->setTreeAndState(*m_deps.branchTree, *m_deps.navState);

        // ボタン接続（RecordPaneが存在する場合）
        if (m_deps.recordPane != nullptr) {
            KifuNavigationController::Buttons buttons;
            buttons.first = m_deps.recordPane->firstButton();
            buttons.back10 = m_deps.recordPane->back10Button();
            buttons.prev = m_deps.recordPane->prevButton();
            buttons.next = m_deps.recordPane->nextButton();
            buttons.fwd10 = m_deps.recordPane->fwd10Button();
            buttons.last = m_deps.recordPane->lastButton();
            (*m_deps.kifuNavController)->connectButtons(buttons);
            qCDebug(lcApp).noquote() << "BranchNavigationWiring::createModels: buttons connected";
        }
    }

    // ライブゲームセッションの作成
    if (*m_deps.liveGameSession == nullptr) {
        *m_deps.liveGameSession = new LiveGameSession(parent());
        (*m_deps.liveGameSession)->setTree(*m_deps.branchTree);
    }
}

void BranchNavigationWiring::wireSignals()
{
    if (!m_deps.displayCoordinator) return;

    qCDebug(lcApp).noquote() << "BranchNavigationWiring::wireSignals: displayCoordinator="
                             << (dispCo(m_deps) ? "exists" : "null");
    if (dispCo(m_deps) != nullptr) {
        return;
    }

    qCDebug(lcApp).noquote() << "BranchNavigationWiring::wireSignals: creating KifuDisplayCoordinator";
    *m_deps.displayCoordinator = new KifuDisplayCoordinator(
        tree(m_deps), navSt(m_deps), navCtl(m_deps), parent());

    configureDisplayCoordinator();
    connectDisplaySignals();
}

void BranchNavigationWiring::configureDisplayCoordinator()
{
    auto* dc = dispCo(m_deps);
    if (!dc) return;

    dc->setRecordPane(m_deps.recordPane);
    dc->setRecordModel(m_deps.kifuRecordModel);
    dc->setBranchModel(m_deps.kifuBranchModel);

    // 分岐ツリーウィジェットを設定（EngineAnalysisTabから取得）
    if (treeW(m_deps) != nullptr) {
        dc->setBranchTreeWidget(treeW(m_deps));
    }

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

    if (liveSess(m_deps) != nullptr) {
        dc->setLiveGameSession(liveSess(m_deps));
    }
}

void BranchNavigationWiring::connectDisplaySignals()
{
    auto* dc = dispCo(m_deps);
    auto* nc = navCtl(m_deps);
    if (!dc || !nc) return;

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
    }

    // 分岐ノード処理完了シグナルを転送
    connect(nc, &KifuNavigationController::branchNodeHandled,
            this, &BranchNavigationWiring::branchNodeHandled);

    // lineSelectionChanged: KifuNavigationState が currentLineIndex() を管理するため接続不要
}

void BranchNavigationWiring::onBranchTreeBuilt()
{
    qCDebug(lcApp) << "BranchNavigationWiring::onBranchTreeBuilt: updating navigation system";

    // 新しいナビゲーションシステムを更新
    if (navSt(m_deps) != nullptr && tree(m_deps) != nullptr) {
        navSt(m_deps)->setTree(tree(m_deps));
    }

    // 分岐ツリーウィジェットを更新
    if (treeW(m_deps) != nullptr && tree(m_deps) != nullptr) {
        treeW(m_deps)->setTree(tree(m_deps));
    }

    // 表示コーディネーターにツリー変更を通知
    if (dispCo(m_deps) != nullptr) {
        dispCo(m_deps)->onTreeChanged();
    }

    if (m_deps.gameRecordModel != nullptr && tree(m_deps) != nullptr) {
        m_deps.gameRecordModel->setBranchTree(tree(m_deps));
        if (navSt(m_deps) != nullptr) {
            m_deps.gameRecordModel->setNavigationState(navSt(m_deps));
        }
    }
}

void BranchNavigationWiring::onBranchNodeActivated(int row, int ply)
{
    qCDebug(lcApp).noquote() << "BranchNavigationWiring::onBranchNodeActivated ENTER row=" << row << "ply=" << ply;
    if (navCtl(m_deps) == nullptr) {
        qCDebug(lcApp).noquote() << "BranchNavigationWiring::onBranchNodeActivated: kifuNavController is null";
        return;
    }

    navCtl(m_deps)->handleBranchNodeActivated(row, ply);
    qCDebug(lcApp).noquote() << "BranchNavigationWiring::onBranchNodeActivated LEAVE";
}

void BranchNavigationWiring::onBranchTreeResetForNewGame()
{
    auto* ls = liveSess(m_deps);
    auto* bt = tree(m_deps);
    auto* ns = navSt(m_deps);

    // performCleanup() が開始したライブゲームセッションを破棄
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
