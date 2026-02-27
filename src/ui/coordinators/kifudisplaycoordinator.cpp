/// @file kifudisplaycoordinator.cpp
/// @brief 棋譜表示コーディネータクラスの実装

#include "kifudisplaycoordinator.h"
#include "kifuselectionsync.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "recordpane.h"
#include "branchtreewidget.h"
#include "branchtreemanager.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "livegamesession.h"

#include "logcategories.h"

#include <QTableView>
#include <QModelIndex>
#include <utility>


KifuDisplayCoordinator::KifuDisplayCoordinator(
    KifuBranchTree* tree,
    KifuNavigationState* state,
    KifuNavigationController* navController,
    QObject* parent)
    : QObject(parent)
    , m_tree(tree)
    , m_state(state)
    , m_navController(navController)
    , m_presenter(std::make_unique<KifuDisplayPresenter>())
    , m_selectionSync(std::make_unique<KifuSelectionSync>())
{
}

KifuDisplayCoordinator::~KifuDisplayCoordinator() = default;

void KifuDisplayCoordinator::setBoardSfenProvider(BoardSfenProvider provider)
{
    m_boardSfenProvider = std::move(provider);
    syncPresenterRefs();
}

void KifuDisplayCoordinator::resetTracking()
{
    m_lastLineIndex = 0;
    m_presenter->setLastModelLineIndex(-1);
    m_pendingNavResultCheck = false;
}

void KifuDisplayCoordinator::setRecordPane(RecordPane* pane)
{
    m_recordPane = pane;
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setBranchTreeWidget(BranchTreeWidget* widget)
{
    m_branchTreeWidget = widget;
    if (m_branchTreeWidget != nullptr && m_tree != nullptr) {
        m_branchTreeWidget->setTree(m_tree);
    }
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setRecordModel(KifuRecordListModel* model)
{
    m_recordModel = model;
    syncPresenterRefs();
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setBranchModel(KifuBranchListModel* model)
{
    m_branchModel = model;
    syncPresenterRefs();
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setBranchTreeManager(BranchTreeManager* manager)
{
    m_branchTreeManager = manager;
    syncPresenterRefs();
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setLiveGameSession(LiveGameSession* session)
{
    // 古いセッションからの接続を解除
    if (m_liveSession != nullptr) {
        disconnect(m_liveSession, nullptr, this, nullptr);
    }

    m_liveSession = session;

    // 新しいセッションへの接続
    if (m_liveSession != nullptr) {
        connect(m_liveSession, &LiveGameSession::moveAdded,
                this, &KifuDisplayCoordinator::onLiveGameMoveAdded);
        connect(m_liveSession, &LiveGameSession::sessionStarted,
                this, &KifuDisplayCoordinator::onLiveGameSessionStarted);
        connect(m_liveSession, &LiveGameSession::branchMarksUpdated,
                this, &KifuDisplayCoordinator::onLiveGameBranchMarksUpdated);
        connect(m_liveSession, &LiveGameSession::sessionCommitted,
                this, &KifuDisplayCoordinator::onLiveGameCommitted);
        connect(m_liveSession, &LiveGameSession::sessionDiscarded,
                this, &KifuDisplayCoordinator::onLiveGameDiscarded);
        connect(m_liveSession, &LiveGameSession::recordModelUpdateRequired,
                this, &KifuDisplayCoordinator::onLiveGameRecordModelUpdateRequired);
    }
}

void KifuDisplayCoordinator::syncPresenterRefs()
{
    KifuDisplayPresenter::Refs refs;
    refs.tree = m_tree;
    refs.state = m_state;
    refs.recordModel = m_recordModel;
    refs.branchModel = m_branchModel;
    refs.branchTreeManager = m_branchTreeManager;
    refs.boardSfenProvider = m_boardSfenProvider;
    m_presenter->updateRefs(refs);
}

void KifuDisplayCoordinator::syncSelectionSyncRefs()
{
    KifuSelectionSync::Refs refs;
    refs.tree = m_tree;
    refs.state = m_state;
    refs.recordModel = m_recordModel;
    refs.branchModel = m_branchModel;
    refs.branchTreeManager = m_branchTreeManager;
    refs.branchTreeWidget = m_branchTreeWidget;
    refs.recordPane = m_recordPane;
    m_selectionSync->updateRefs(refs);
}

KifuDisplayPresenter::TrackingState KifuDisplayCoordinator::trackingState() const
{
    return {m_lastLineIndex, m_selectionSync->expectedTreeLineIndex(), m_selectionSync->expectedTreePly()};
}

void KifuDisplayCoordinator::wireSignals()
{
    if (m_navController != nullptr) {
        connect(m_navController, &KifuNavigationController::navigationCompleted,
                this, &KifuDisplayCoordinator::onNavigationCompleted);
        connect(m_navController, &KifuNavigationController::boardUpdateRequired,
                this, &KifuDisplayCoordinator::onBoardUpdateRequired);
        connect(m_navController, &KifuNavigationController::recordHighlightRequired,
                this, &KifuDisplayCoordinator::onRecordHighlightRequired);
        connect(m_navController, &KifuNavigationController::branchTreeHighlightRequired,
                this, &KifuDisplayCoordinator::onBranchTreeHighlightRequired);
        connect(m_navController, &KifuNavigationController::branchCandidatesUpdateRequired,
                this, &KifuDisplayCoordinator::onBranchCandidatesUpdateRequired);
    }

    if (m_tree != nullptr) {
        connect(m_tree, &KifuBranchTree::treeChanged,
                this, &KifuDisplayCoordinator::onTreeChanged);
    }

    // 分岐ツリーのクリックをナビゲーションに接続
    if (m_branchTreeWidget != nullptr && m_navController != nullptr) {
        connect(m_branchTreeWidget, &BranchTreeWidget::nodeClicked,
                this, &KifuDisplayCoordinator::onBranchTreeNodeClicked);
    }

    // 分岐候補のクリックをナビゲーションに接続
    if (m_recordPane != nullptr && m_navController != nullptr) {
        connect(m_recordPane, &RecordPane::branchActivated,
                this, &KifuDisplayCoordinator::onBranchCandidateActivated);
    }
}

// ============================================================
// イベントハンドラ
// ============================================================

void KifuDisplayCoordinator::onBranchTreeNodeClicked(int lineIndex, int ply)
{
    qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked ENTER lineIndex=" << lineIndex << "ply=" << ply
                            << "preferredLineIndex(before)=" << (m_state ? m_state->preferredLineIndex() : -99)
                            << "m_lastModelLineIndex=" << m_presenter->lastModelLineIndex();

    if (m_tree == nullptr || m_navController == nullptr) {
        qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked: tree or navController is null";
        return;
    }

    QVector<BranchLine> lines = m_tree->allLines();
    qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked: allLines().size()=" << lines.size();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked: lineIndex out of range";
        return;
    }

    // 分岐ツリーでクリックしたラインを優先ラインとして設定
    if (m_state != nullptr) {
        if (lineIndex > 0) {
            m_state->setPreferredLineIndex(lineIndex);
        } else {
            m_state->resetPreferredLineIndex();
        }
        qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked: preferredLineIndex(after)=" << m_state->preferredLineIndex();
    }

    const BranchLine& line = lines.at(lineIndex);
    qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked: line.nodes.size()=" << line.nodes.size()
                            << "line.branchPly=" << line.branchPly
                            << "line.name=" << line.name;
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == ply) {
            qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked: found node nodeId=" << node->nodeId()
                                    << "displayText=" << node->displayText()
                                    << "sfen=" << node->sfen();
            m_navController->goToNode(node);
            qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked LEAVE (goToNode done)"
                                    << "currentLineIndex(after)=" << (m_state ? m_state->currentLineIndex() : -99)
                                    << "m_lastModelLineIndex(after)=" << m_presenter->lastModelLineIndex();
            return;
        }
    }

    // plyが見つからない場合（開始局面）
    if (ply == 0 && m_tree->root() != nullptr) {
        qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked: using root node";
        m_navController->goToNode(m_tree->root());
    }
    qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked LEAVE (node not found for ply=" << ply << ")";
}

void KifuDisplayCoordinator::onBranchCandidateActivated(const QModelIndex& index)
{
    qCDebug(lcUi).noquote() << "onBranchCandidateActivated ENTER"
                       << "index.valid=" << index.isValid()
                       << "row=" << index.row()
                       << "navController=" << (m_navController ? "yes" : "null")
                       << "branchModel=" << (m_branchModel ? "yes" : "null");

    if (!index.isValid() || m_navController == nullptr || m_branchModel == nullptr) {
        qCDebug(lcUi).noquote() << "onBranchCandidateActivated: guard failed, returning";
        return;
    }

    const int row = index.row();
    if (m_branchModel->isBackToMainRow(row)) {
        qCDebug(lcUi).noquote() << "onBranchCandidateActivated: back to main row, calling goToMainLineAtCurrentPly";
        m_navController->goToMainLineAtCurrentPly();
        return;
    }

    qCDebug(lcUi).noquote() << "onBranchCandidateActivated: calling selectBranchCandidate(" << row << ")";
    m_navController->selectBranchCandidate(row);
}

void KifuDisplayCoordinator::onNavigationCompleted(KifuBranchNode* node)
{
    qCDebug(lcUi).noquote() << "onNavigationCompleted ENTER node="
                       << (node ? QString("ply=%1").arg(node->ply()) : "null")
                       << "preferredLineIndex=" << (m_state ? m_state->preferredLineIndex() : -999);

    // ラインが変更された場合は棋譜欄の内容を更新
    if (m_state != nullptr) {
        const int newLineIndex = m_state->currentLineIndex();
        qCDebug(lcUi).noquote() << "onNavigationCompleted: newLineIndex=" << newLineIndex
                           << "m_lastLineIndex=" << m_lastLineIndex
                           << "m_lastModelLineIndex=" << m_presenter->lastModelLineIndex();

        const bool lineIndexChanged = (newLineIndex != m_lastLineIndex);
        const bool modelNeedsUpdate = (m_presenter->lastModelLineIndex() >= 0
                                       && m_presenter->lastModelLineIndex() != newLineIndex);

        if (lineIndexChanged || modelNeedsUpdate) {
            qCDebug(lcUi).noquote() << "onNavigationCompleted: updating record view"
                               << "lineIndexChanged=" << lineIndexChanged
                               << "modelNeedsUpdate=" << modelNeedsUpdate;
            m_lastLineIndex = newLineIndex;
            updateRecordView();
        } else {
            qCDebug(lcUi).noquote() << "onNavigationCompleted: line NOT changed, skipping updateRecordView";
        }
    }

    // 盤面とハイライトを更新
    if (node != nullptr) {
        QString currentSfen = node->sfen();
        QString prevSfen;
        if (node->parent() != nullptr) {
            prevSfen = node->parent()->sfen();
        }
        if (currentSfen.isEmpty() && !prevSfen.isEmpty()) {
            currentSfen = prevSfen;
            qCDebug(lcUi).noquote() << "onNavigationCompleted: terminal node (empty SFEN),"
                               << "using parent's SFEN";
        }
        qCDebug(lcUi).noquote() << "onNavigationCompleted: emitting boardWithHighlightsRequired"
                           << "currentSfen=" << currentSfen.left(40)
                           << "prevSfen=" << (prevSfen.isEmpty() ? "(empty)" : prevSfen.left(40));
        emit boardWithHighlightsRequired(currentSfen, prevSfen);
    }

    highlightCurrentPosition();

    // 一致性チェックは onBranchCandidatesUpdateRequired 完了後に遅延実行する
    m_pendingNavResultCheck = true;
}

void KifuDisplayCoordinator::onBoardUpdateRequired(const QString& sfen)
{
    Q_UNUSED(sfen);
}

void KifuDisplayCoordinator::onRecordHighlightRequired(int ply)
{
    if (m_recordModel == nullptr) {
        return;
    }

    qCDebug(lcUi).noquote() << "onRecordHighlightRequired: ply=" << ply
                       << "modelRowCount=" << m_recordModel->rowCount()
                       << "lastModelLineIndex=" << m_presenter->lastModelLineIndex()
                       << "preferredLineIndex=" << (m_state ? m_state->preferredLineIndex() : -99);

    // モデルのラインとナビゲーション状態のラインが一致しない場合、モデルを再構築
    if (m_selectionSync->checkModelLineMismatch(m_presenter->lastModelLineIndex())) {
        updateRecordView();
        qCDebug(lcUi).noquote() << "onRecordHighlightRequired: after rebuild, m_lastModelLineIndex="
                           << m_presenter->lastModelLineIndex();
    }

    m_selectionSync->applyRecordHighlight(ply);
}

void KifuDisplayCoordinator::onBranchTreeHighlightRequired(int lineIndex, int ply)
{
    const bool inconsistent = m_selectionSync->applyBranchTreeHighlight(lineIndex, ply);
    if (inconsistent) {
        qCDebug(lcUi).noquote() << getConsistencyReport();
    }
}

void KifuDisplayCoordinator::onBranchCandidatesUpdateRequired(const QList<KifuBranchNode*>& candidates)
{
    m_selectionSync->applyBranchCandidates(candidates);
    runPendingNavResultCheck();
}

void KifuDisplayCoordinator::onTreeChanged()
{
    m_lastLineIndex = (m_state != nullptr) ? m_state->currentLineIndex() : 0;

    updateRecordView();
    updateBranchTreeView();
    updateBranchCandidatesView();
    highlightCurrentPosition();
}

// ============================================================
// 内部更新メソッド
// ============================================================

void KifuDisplayCoordinator::updateRecordView()
{
    qCDebug(lcUi).noquote() << "updateRecordView: CALLED";
    m_presenter->populateRecordModel();
    m_presenter->populateBranchMarks();

    // ビューの明示的な更新を強制
    if (m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        QTableView* view = m_recordPane->kifuView();
        view->viewport()->update();
        qCDebug(lcUi).noquote() << "updateRecordView: forced view update, model rowCount=" << m_recordModel->rowCount();
    }
}

void KifuDisplayCoordinator::updateBranchTreeView()
{
    if (m_branchTreeWidget != nullptr) {
        m_branchTreeWidget->setTree(m_tree);
    }

    if (m_branchTreeManager != nullptr && m_tree != nullptr) {
        m_branchTreeManager->setBranchTreeRows(KifuDisplayPresenter::buildBranchTreeRows(m_tree));
    } else if (m_branchTreeManager != nullptr) {
        m_branchTreeManager->setBranchTreeRows({});
    }
}

void KifuDisplayCoordinator::updateBranchCandidatesView()
{
    if (m_state == nullptr) {
        return;
    }

    QVector<KifuBranchNode*> candidates = m_state->branchCandidatesAtCurrent();
    onBranchCandidatesUpdateRequired(candidates);
}

void KifuDisplayCoordinator::highlightCurrentPosition()
{
    const auto commentInfo = m_selectionSync->highlightCurrentPosition(m_presenter->lastModelLineIndex());

    // モデル不一致が検出された場合の再構築
    if (m_selectionSync->checkModelLineMismatch(m_presenter->lastModelLineIndex())) {
        updateRecordView();
        if (m_state != nullptr) {
            m_selectionSync->applyRecordHighlight(m_state->currentPly());
        }
    }

    // コメント更新シグナル発火
    if (commentInfo.ply >= 0) {
        const QString& comment = commentInfo.comment;
        emit commentUpdateRequired(commentInfo.ply,
                                   comment.isEmpty() ? tr("コメントなし") : comment,
                                   commentInfo.asHtml);
    }
}

void KifuDisplayCoordinator::runPendingNavResultCheck()
{
    if (!m_pendingNavResultCheck) {
        return;
    }
    m_pendingNavResultCheck = false;

    const bool consistent = verifyDisplayConsistency();
    if (!consistent) {
        qCWarning(lcUi).noquote() << "POST-NAVIGATION INCONSISTENCY:";
        qCDebug(lcUi).noquote() << getConsistencyReport();
    }

    const DisplaySnapshot snap = captureDisplaySnapshot();
    qCDebug(lcUi).noquote() << "NAV-RESULT: ply=" << snap.statePly
                        << "line=" << snap.stateLineIndex
                        << "recordHighlight=" << snap.modelHighlightRow
                        << "treeHL=(" << snap.treeHighlightLineIndex << "," << snap.treeHighlightPly << ")"
                        << "candidates=" << snap.branchCandidateCount
                        << "consistent=" << (consistent ? "YES" : "NO");
}

// ============================================================
// onPositionChanged
// ============================================================

void KifuDisplayCoordinator::onPositionChanged(int lineIndex, int ply, const QString& sfen)
{
    Q_UNUSED(sfen)
    Q_UNUSED(lineIndex)

    qCDebug(lcUi).noquote() << "onPositionChanged ENTER lineIndex=" << lineIndex << "ply=" << ply;

    if (m_tree == nullptr || m_state == nullptr) {
        qCDebug(lcUi).noquote() << "onPositionChanged LEAVE (no tree or state)";
        return;
    }

    KifuBranchNode* targetNode = nullptr;
    QVector<BranchLine> lines = m_tree->allLines();

    // ply=0は開始局面（ルートノード）
    if (ply == 0) {
        targetNode = m_tree->root();
        qCDebug(lcUi).noquote() << "ply=0: using root node";
    } else if (!lines.isEmpty()) {
        int currentLine = m_state->currentLineIndex();
        if (currentLine < 0 || currentLine >= lines.size()) {
            currentLine = 0;
        }

        qCDebug(lcUi).noquote() << "searching on line" << currentLine << "for ply" << ply;

        const BranchLine& line = lines.at(currentLine);
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->ply() == ply) {
                targetNode = node;
                qCDebug(lcUi).noquote() << "found node on line" << currentLine
                                   << "ply=" << ply << "displayText=" << node->displayText();
                break;
            }
        }

        // 現在のラインで見つからない場合、全ラインを検索
        if (targetNode == nullptr) {
            for (int li = 0; li < lines.size(); ++li) {
                if (li == currentLine) continue;
                for (KifuBranchNode* node : std::as_const(lines.at(li).nodes)) {
                    if (node->ply() == ply) {
                        targetNode = node;
                        qCDebug(lcUi).noquote() << "found node on line" << li
                                           << "(fallback) ply=" << ply;
                        break;
                    }
                }
                if (targetNode != nullptr) break;
            }
        }
    }

    if (targetNode == nullptr) {
        qCDebug(lcUi).noquote() << "onPositionChanged LEAVE (target node not found for ply=" << ply << ")";
        return;
    }

    qCDebug(lcUi).noquote() << "onPositionChanged: found node ply=" << targetNode->ply()
                       << "displayText=" << targetNode->displayText()
                       << "childCount=" << targetNode->childCount()
                       << "parent=" << (targetNode->parent() ? "yes" : "null");

    // 分岐点での選択を記憶
    KifuBranchNode* nodeParent = targetNode->parent();
    if (nodeParent != nullptr && nodeParent->childCount() > 1) {
        for (int i = 0; i < nodeParent->childCount(); ++i) {
            if (nodeParent->childAt(i) == targetNode) {
                m_state->rememberLineSelection(nodeParent, i);
                qCDebug(lcUi).noquote() << "onPositionChanged: remembered line selection at parent ply="
                                   << nodeParent->ply() << "index=" << i;
                break;
            }
        }
    }

    m_state->setCurrentNode(targetNode);

    // 分岐候補の同期（KifuSelectionSyncに委譲）
    m_selectionSync->syncBranchCandidatesForNode(targetNode);

    // 分岐ツリーハイライトの同期（KifuSelectionSyncに委譲）
    const int highlightLineIndex = m_selectionSync->syncBranchTreeHighlightForNode(
        targetNode, ply, m_state->currentLineIndex());

    // m_lastLineIndex を同期
    const int oldLastLineIndex = m_lastLineIndex;
    m_lastLineIndex = highlightLineIndex;
    qCDebug(lcUi).noquote() << "onPositionChanged: synced m_lastLineIndex from" << oldLastLineIndex
                       << "to" << m_lastLineIndex
                       << "(model rowCount=" << (m_recordModel ? m_recordModel->rowCount() : -1) << ")";

    // コメント表示の更新
    {
        const QString comment = targetNode->comment().trimmed();
        emit commentUpdateRequired(ply, comment.isEmpty() ? tr("コメントなし") : comment, true);
    }

    // 位置変更完了時の一致性チェック
    const bool consistent = verifyDisplayConsistency();
    if (!consistent) {
        qCWarning(lcUi).noquote() << "POST-NAVIGATION INCONSISTENCY:";
        qCDebug(lcUi).noquote() << getConsistencyReport();
    }

    // NAV-RESULT サマリー
    {
        const DisplaySnapshot snap = captureDisplaySnapshot();
        qCDebug(lcUi).noquote() << "NAV-RESULT: ply=" << snap.statePly
                            << "line=" << snap.stateLineIndex
                            << "recordHighlight=" << snap.modelHighlightRow
                            << "treeHL=(" << snap.treeHighlightLineIndex << "," << snap.treeHighlightPly << ")"
                            << "candidates=" << snap.branchCandidateCount
                            << "consistent=" << (consistent ? "YES" : "NO");
    }

    qCDebug(lcUi).noquote() << "onPositionChanged LEAVE";
}

// ============================================================
// 一致性検証（プレゼンタに委譲）
// ============================================================

KifuDisplayCoordinator::DisplaySnapshot KifuDisplayCoordinator::captureDisplaySnapshot() const
{
    return m_presenter->captureDisplaySnapshot(trackingState());
}

bool KifuDisplayCoordinator::verifyDisplayConsistencyDetailed(QString* reason) const
{
    return m_presenter->verifyDisplayConsistencyDetailed(trackingState(), reason);
}

bool KifuDisplayCoordinator::verifyDisplayConsistency() const
{
    return m_presenter->verifyDisplayConsistency(trackingState());
}

QString KifuDisplayCoordinator::getConsistencyReport() const
{
    return m_presenter->getConsistencyReport(trackingState());
}

// ============================================================
// ライブ対局セッション
// ============================================================

void KifuDisplayCoordinator::onLiveGameMoveAdded(int ply, const QString& displayText)
{
    Q_UNUSED(ply);
    Q_UNUSED(displayText);

    const int liveLineIndex = (m_liveSession != nullptr) ? m_liveSession->currentLineIndex() : 0;
    KifuBranchNode* liveNode = (m_liveSession != nullptr) ? m_liveSession->liveNode() : nullptr;

    // BranchTreeManager の分岐ツリーを更新
    if (m_branchTreeManager != nullptr && m_tree != nullptr) {
        m_branchTreeManager->setBranchTreeRows(KifuDisplayPresenter::buildBranchTreeRows(m_tree));

        // ハイライト更新
        if (m_liveSession != nullptr) {
            const int totalPly = m_liveSession->totalPly();
            m_branchTreeManager->highlightBranchTreeAt(liveLineIndex, totalPly, false);
        }
    }

    // BranchTreeWidget も更新
    if (m_branchTreeWidget != nullptr && m_tree != nullptr) {
        m_branchTreeWidget->setTree(m_tree);
    }

    // ライブ対局中は最新ノードへナビゲートし、分岐ラインを同期
    if (liveNode != nullptr && m_navController != nullptr && m_state != nullptr) {
        if (liveLineIndex > 0) {
            m_state->setPreferredLineIndex(liveLineIndex);
        } else {
            m_state->resetPreferredLineIndex();
        }
        m_navController->goToNode(liveNode);
    }
}

void KifuDisplayCoordinator::onLiveGameSessionStarted(KifuBranchNode* branchPoint)
{
    qCDebug(lcUi).noquote() << "onLiveGameSessionStarted: branchPoint="
                       << (branchPoint ? QString("ply=%1").arg(branchPoint->ply()) : "null");

    int branchLineIndex = 0;
    if (branchPoint != nullptr && m_tree != nullptr) {
        branchLineIndex = m_tree->findLineIndexForNode(branchPoint);
        if (branchLineIndex < 0) {
            branchLineIndex = 0;
        }
    }

    if (m_state != nullptr) {
        if (branchLineIndex > 0) {
            m_state->setPreferredLineIndex(branchLineIndex);
        } else {
            m_state->resetPreferredLineIndex();
        }
        m_state->clearLineSelectionMemory();
        qCDebug(lcUi).noquote() << "onLiveGameSessionStarted: set preferredLineIndex=" << branchLineIndex
                           << "and clearLineSelectionMemory";
    }

    m_lastLineIndex = branchLineIndex;

    if (m_recordModel == nullptr || m_tree == nullptr) {
        return;
    }

    if (branchPoint == nullptr) {
        m_recordModel->setBranchPlyMarks(QSet<int>());
        onRecordHighlightRequired(0);
        return;
    }

    const QVector<KifuBranchNode*> path = m_tree->pathToNode(branchPoint);

    // branchPoint までのパスの分岐選択を記憶
    if (m_state != nullptr && !path.isEmpty()) {
        for (int i = 0; i < path.size(); ++i) {
            KifuBranchNode* node = path.at(i);
            KifuBranchNode* parent = node->parent();
            if (parent != nullptr && parent->childCount() > 1) {
                for (int j = 0; j < parent->childCount(); ++j) {
                    if (parent->childAt(j) == node) {
                        m_state->rememberLineSelection(parent, j);
                        qCDebug(lcUi).noquote() << "onLiveGameSessionStarted: remembered line selection"
                                           << "parentPly=" << parent->ply() << "childIndex=" << j;
                        break;
                    }
                }
            }
        }
    }

    const int highlightRow = m_presenter->populateRecordModelFromPath(path, branchPoint->ply());
    onRecordHighlightRequired(highlightRow);
}

void KifuDisplayCoordinator::onLiveGameBranchMarksUpdated(const QSet<int>& branchPlys)
{
    if (m_recordModel != nullptr) {
        m_recordModel->setBranchPlyMarks(branchPlys);
    }
}

void KifuDisplayCoordinator::onLiveGameCommitted(KifuBranchNode* newLineEnd)
{
    updateRecordView();
    updateBranchTreeView();

    if (newLineEnd != nullptr && m_navController != nullptr) {
        m_navController->goToNode(newLineEnd);
    }
}

void KifuDisplayCoordinator::onLiveGameDiscarded()
{
    updateRecordView();
    updateBranchTreeView();
}

void KifuDisplayCoordinator::onLiveGameRecordModelUpdateRequired()
{
    if (m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        m_recordPane->kifuView()->viewport()->update();
    }
}
