/// @file kifudisplaycoordinator.cpp
/// @brief 棋譜表示コーディネータクラスの実装

#include "kifudisplaycoordinator.h"
#include "kifuselectionsync.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "recordpane.h"
#include "branchtreemanager.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "livegamesession.h"

#include "logcategories.h"

#include <QTableView>
#include <QModelIndex>
#include <utility>
#include "kifudisplaypresenter.h"


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
        connect(m_liveSession, &LiveGameSession::movesUndone,
                this, &KifuDisplayCoordinator::onLiveGameMovesUndone);
        connect(m_liveSession, &LiveGameSession::sessionStarted,
                this, &KifuDisplayCoordinator::onLiveGameSessionStarted);
        connect(m_liveSession, &LiveGameSession::sessionCommitted,
                this, &KifuDisplayCoordinator::onLiveGameCommitted);
        connect(m_liveSession, &LiveGameSession::sessionDiscarded,
                this, &KifuDisplayCoordinator::onLiveGameDiscarded);
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
    refs.recordPane = m_recordPane;
    m_selectionSync->updateRefs(refs);
}

KifuDisplayPresenter::TrackingState KifuDisplayCoordinator::trackingState() const
{
    return {m_lastLineIndex,
            m_selectionSync->expectedTreeLineIndex(),
            m_selectionSync->expectedTreePly(),
            (m_liveSession != nullptr && m_liveSession->canAddMove())};
}

void KifuDisplayCoordinator::wireSignals()
{
    if (m_signalsWired)
        return;
    m_signalsWired = true;

    if (m_navController != nullptr) {
        connect(m_navController, &KifuNavigationController::navigationCompleted,
                this, &KifuDisplayCoordinator::onNavigationCompleted);
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

    // 分岐ツリー（BranchTreeManager）のクリックは BranchNavigationWiring::onBranchNodeActivated 経由で
    // KifuNavigationController::handleBranchNodeActivated に届く

    // 分岐候補のクリックをナビゲーションに接続
    if (m_recordPane != nullptr && m_navController != nullptr) {
        connect(m_recordPane, &RecordPane::branchActivated,
                this, &KifuDisplayCoordinator::onBranchCandidateActivated);
    }
}

// ============================================================
// イベントハンドラ
// ============================================================

void KifuDisplayCoordinator::onBranchCandidateActivated(const QModelIndex& index)
{
    qCDebug(lcNavTrace).noquote() << "onBranchCandidateActivated ENTER"
                       << "index.valid=" << index.isValid()
                       << "row=" << index.row()
                       << "navController=" << (m_navController ? "yes" : "null")
                       << "branchModel=" << (m_branchModel ? "yes" : "null");

    if (!index.isValid() || m_navController == nullptr || m_branchModel == nullptr) {
        qCDebug(lcNavTrace).noquote() << "onBranchCandidateActivated: guard failed, returning";
        return;
    }

    // 未更新のコメントの破棄をやめたときは移動せず、クリックした候補の強調を戻す
    if (!m_navController->canLeaveCurrentPosition()) {
        restoreCurrentPositionHighlights();
        return;
    }

    const int row = index.row();
    if (m_branchModel->isBackToMainRow(row)) {
        qCDebug(lcNavTrace).noquote() << "onBranchCandidateActivated: back to main row, calling goToMainLineAtCurrentPly";
        m_navController->goToMainLineAtCurrentPly();
        return;
    }

    qCDebug(lcNavTrace).noquote() << "onBranchCandidateActivated: calling selectBranchCandidate(" << row << ")";
    m_navController->selectBranchCandidate(row);
}

void KifuDisplayCoordinator::restoreCurrentPositionHighlights()
{
    if (m_state == nullptr) {
        return;
    }
    m_selectionSync->applyRecordHighlight(m_state->currentPly());
    updateBranchCandidatesView();
    onBranchTreeHighlightRequired(m_state->currentLineIndex(), m_state->currentPly());
}

void KifuDisplayCoordinator::onTreeChanged()
{
    if (m_liveSession && m_liveSession->isActive() && m_liveSession->liveNode()) {
        return;
    }
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
    qCDebug(lcNavTrace).noquote() << "updateRecordView: CALLED";
    const bool rebuilt = m_presenter->populateRecordModel();
    m_presenter->populateBranchMarks();

    // 全行を作り直したときはビューの明示的な更新を強制する
    // （末尾に手を足しただけなら、足した行の描き直しで足りる）
    if (rebuilt && m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        QTableView* view = m_recordPane->kifuView();
        view->viewport()->update();
        qCDebug(lcNavTrace).noquote() << "updateRecordView: forced view update, model rowCount=" << m_recordModel->rowCount();
    }
}

void KifuDisplayCoordinator::updateBranchTreeView()
{
    if (m_branchTreeManager != nullptr && m_tree != nullptr) {
        m_branchTreeManager->setBranchTree(m_tree);
        m_branchTreeNodeCount = m_tree->nodeCount();
        m_branchTreeRevision = m_tree->revision();
    } else if (m_branchTreeManager != nullptr) {
        m_branchTreeManager->setBranchTree(nullptr);
        m_branchTreeNodeCount = -1;
    }
}

void KifuDisplayCoordinator::updateBranchCandidatesView()
{
    if (m_state == nullptr) {
        return;
    }

    QList<KifuBranchNode*> candidates = m_state->branchCandidatesAtCurrent();
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
                                   comment,
                                   commentInfo.asHtml);
    }
}

// ============================================================
// 棋譜表示コーディネータ - ライブ対局セッション処理
// ============================================================

// ============================================================
// ライブ対局セッション
// ============================================================

void KifuDisplayCoordinator::onLiveGameMoveAdded(int ply, const QString& displayText)
{
    Q_UNUSED(ply);
    Q_UNUSED(displayText);

    const int liveLineIndex = (m_liveSession != nullptr) ? m_liveSession->currentLineIndex() : 0;
    KifuBranchNode* liveNode = (m_liveSession != nullptr) ? m_liveSession->liveNode() : nullptr;

    // 分岐ツリーは毎手の全再構築を避け、可能なら末尾ノードの差分追加だけ行う
    if (!appendLiveNodeToBranchTree(liveNode)) {
        updateBranchTreeView();
    }

    if (liveNode == nullptr || m_state == nullptr) {
        return;
    }

    // 対局中は盤面が対局ロジックによって既に更新されているため、
    // KifuNavigationController::goToNode() は使わない。goToNode() は棋譜閲覧用の経路で、
    // 盤面を SFEN から再設定し、手番同期も行う。
    // ここではナビゲーション状態と各表示のハイライトだけを最新手に同期する。
    if (liveLineIndex > 0) {
        m_state->setPreferredLineIndex(liveLineIndex);
    } else {
        m_state->resetPreferredLineIndex();
    }
    m_state->rememberPathSelections(liveNode);   // 対局後の「戻る→進む」で最新手の経路を辿れるように
    m_state->setCurrentNode(liveNode);

    // 正本を更新し終えた後に表示を作る。同じ経路で通常手・終局・既存手の再利用を扱う。
    m_presenter->presentLiveNode(liveNode);
    m_lastLineIndex = m_state->currentLineIndex();
    highlightCurrentPosition();
    m_pendingNavResultCheck = true;
    updateBranchCandidatesView();
}

void KifuDisplayCoordinator::onLiveGameMovesUndone()
{
    if (m_tree == nullptr || m_state == nullptr || m_liveSession == nullptr) {
        return;
    }
    KifuBranchNode* node = m_liveSession->liveNode();
    m_state->setCurrentNode(node);
    m_state->clearLineSelectionMemory();
    m_lastLineIndex = m_liveSession->currentLineIndex();
    m_state->setPreferredLineIndex(m_lastLineIndex);
    m_state->rememberPathSelections(node);

    // 既存の継続手を再利用した場合も、対局中の棋譜欄は現在手までにする。
    m_presenter->populateRecordModelFromPath(m_tree->pathToNode(node), m_state->currentPly());
    updateBranchTreeView();
    highlightCurrentPosition();
    m_pendingNavResultCheck = true;
    updateBranchCandidatesView();
}

bool KifuDisplayCoordinator::appendLiveNodeToBranchTree(KifuBranchNode* liveNode)
{
    if (m_branchTreeManager == nullptr || m_tree == nullptr || liveNode == nullptr) {
        return false;
    }

    const int nodeCount = m_tree->nodeCount();
    const quint64 revision = m_tree->revision();
    if (m_branchTreeNodeCount < 0) {
        return false;   // まだ一度も反映していない
    }
    if (nodeCount == m_branchTreeNodeCount && revision == m_branchTreeRevision) {
        return true;    // 既存ノードの再利用（指し直し）: 描画済みなので何もしない
    }
    // 1ノードの追加以外（待ったで残した手順との並べ替えなど）は全再構築に任せる
    if (nodeCount != m_branchTreeNodeCount + 1 || revision != m_branchTreeRevision + 1
        || liveNode->childCount() != 0) {
        return false;
    }

    // 新しいラインができた場合は行の並びが変わり得るため全再構築に任せる
    if (m_tree->lineCount() != m_branchTreeManager->rowCount()) {
        return false;
    }
    const auto lineIndex = m_tree->findLineIndexForNode(liveNode);
    if (!lineIndex.has_value()) {
        return false;
    }

    if (!m_branchTreeManager->appendNode(liveNode)) {
        return false;
    }
    m_branchTreeNodeCount = nodeCount;
    m_branchTreeRevision = revision;
    return true;
}

void KifuDisplayCoordinator::onLiveGameSessionStarted(KifuBranchNode* branchPoint)
{
    qCDebug(lcUi).noquote() << "onLiveGameSessionStarted: branchPoint="
                       << (branchPoint ? QString("ply=%1").arg(branchPoint->ply()) : "null");

    int branchLineIndex = 0;
    if (branchPoint != nullptr && m_tree != nullptr) {
        branchLineIndex = m_tree->findLineIndexForNode(branchPoint).value_or(0);
    }

    if (m_state != nullptr) {
        if (branchLineIndex > 0) {
            m_state->setPreferredLineIndex(branchLineIndex);
        } else {
            m_state->resetPreferredLineIndex();
        }
        m_state->clearLineSelectionMemory();
        // 対局は分岐点（途中局面）か開始局面から始まる。表示中の手数（カーソル）をそこに合わせる
        KifuBranchNode* startNode = branchPoint != nullptr ? branchPoint
                                                            : (m_tree != nullptr ? m_tree->root() : nullptr);
        if (startNode != nullptr) {
            m_state->setCurrentNode(startNode);
        }
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

    const QList<KifuBranchNode*> path = m_tree->pathToNode(branchPoint);

    // branchPoint までのパスの分岐選択を記憶
    if (m_state != nullptr) {
        m_state->rememberPathSelections(branchPoint);
    }

    const int highlightRow = m_presenter->populateRecordModelFromPath(path, branchPoint->ply());
    onRecordHighlightRequired(highlightRow);
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

// ============================================================
// 棋譜表示コーディネータ - ナビゲーションイベント処理
// ============================================================

// ============================================================
// ナビゲーション完了イベントハンドラ
// ============================================================

void KifuDisplayCoordinator::onNavigationCompleted(KifuBranchNode* node)
{
    qCDebug(lcUi).noquote() << "onNavigationCompleted ENTER node="
                       << (node ? QString("ply=%1").arg(node->ply()) : "null")
                       << "preferredLineIndex=" << (m_state ? m_state->preferredLineIndex() : -999);

    // ラインが変更された場合は棋譜欄の内容を更新
    syncRecordViewToCurrentLine();

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

bool KifuDisplayCoordinator::syncRecordViewToCurrentLine()
{
    if (m_state == nullptr) {
        return false;
    }

    const int newLineIndex = m_state->currentLineIndex();
    const bool lineIndexChanged = (newLineIndex != m_lastLineIndex);
    const bool modelNeedsUpdate = (m_presenter->lastModelLineIndex() >= 0
                                   && m_presenter->lastModelLineIndex() != newLineIndex);
    qCDebug(lcUi).noquote() << "syncRecordViewToCurrentLine: newLineIndex=" << newLineIndex
                       << "m_lastLineIndex=" << m_lastLineIndex
                       << "m_lastModelLineIndex=" << m_presenter->lastModelLineIndex();

    if (!lineIndexChanged && !modelNeedsUpdate) {
        return false;
    }

    m_lastLineIndex = newLineIndex;
    updateRecordView();
    return true;
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
        qCDebug(lcUi).noquote() << consistencyReport();
    }
}

void KifuDisplayCoordinator::onBranchCandidatesUpdateRequired(const QList<KifuBranchNode*>& candidates)
{
    m_selectionSync->applyBranchCandidates(candidates);
    runPendingNavResultCheck();
}

void KifuDisplayCoordinator::runPendingNavResultCheck()
{
    if (!m_pendingNavResultCheck) {
        return;
    }
    m_pendingNavResultCheck = false;

    QString reason;
    const bool consistent = verifyDisplayConsistencyDetailed(&reason);
    if (!consistent) {
        qCWarning(lcUi).noquote() << "POST-NAVIGATION INCONSISTENCY:" << reason;
        qCDebug(lcUi).noquote() << consistencyReport();
    }

    // 表示の状態の取得（盤の SFEN の生成を含む）は、ログを出すときだけ
    if (lcUi().isDebugEnabled()) {
        const DisplaySnapshot snap = captureDisplaySnapshot();
        qCDebug(lcUi).noquote() << "NAV-RESULT: ply=" << snap.statePly
                            << "line=" << snap.stateLineIndex
                            << "recordHighlight=" << snap.modelHighlightRow
                            << "treeHL=(" << snap.treeHighlightLineIndex << "," << snap.treeHighlightPly << ")"
                            << "candidates=" << snap.branchCandidateCount
                            << "consistent=" << (consistent ? "YES" : "NO");
    }
}

// ============================================================
// onPositionChanged
// ============================================================

void KifuDisplayCoordinator::onPositionChanged(int lineIndex, int ply, const QString& sfen)
{
    Q_UNUSED(sfen)
    if (!m_navController || !m_tree || !m_state) return;
    const auto lines = m_tree->allLines();
    if (lineIndex < 0 || lineIndex >= lines.size() || ply < 0) return;
    const auto& nodes = lines.at(lineIndex).nodes;
    for (auto* node : nodes) {
        if (node->ply() != ply) continue;
        // 同手数でも分岐先が違えば移動する。範囲外を別ラインで補わない。
        if (m_state->currentNode() != node) m_navController->goToLine(lineIndex, ply);
        return;
    }
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

QString KifuDisplayCoordinator::consistencyReport() const
{
    return m_presenter->consistencyReport(trackingState());
}
