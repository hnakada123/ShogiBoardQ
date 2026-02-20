/// @file kifudisplaycoordinator.cpp
/// @brief 棋譜表示コーディネータクラスの実装

#include "kifudisplaycoordinator.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "recordpane.h"
#include "branchtreewidget.h"
#include "branchtreemanager.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifudisplay.h"
#include "kifdisplayitem.h"
#include "livegamesession.h"

#include "loggingcategory.h"

#include <QTableView>
#include <QModelIndex>
#include <QTextStream>
#include <QStringList>
#include <utility>

Q_LOGGING_CATEGORY(lcUi, "shogi.ui")

namespace {
QString normalizeSfenForCompare(const QString& sfen)
{
    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        return trimmed;
    }
    // 盤面・手番・持ち駒までを比較対象にする（手数は除外）
    return QStringLiteral("%1 %2 %3").arg(parts.at(0), parts.at(1), parts.at(2));
}
}

KifuDisplayCoordinator::KifuDisplayCoordinator(
    KifuBranchTree* tree,
    KifuNavigationState* state,
    KifuNavigationController* navController,
    QObject* parent)
    : QObject(parent)
    , m_tree(tree)
    , m_state(state)
    , m_navController(navController)
{
}

void KifuDisplayCoordinator::setBoardSfenProvider(BoardSfenProvider provider)
{
    m_boardSfenProvider = std::move(provider);
}

void KifuDisplayCoordinator::resetTracking()
{
    m_lastLineIndex = 0;
    m_lastModelLineIndex = -1;
    m_expectedTreeLineIndex = 0;
    m_expectedTreePly = 0;
}

void KifuDisplayCoordinator::setRecordPane(RecordPane* pane)
{
    m_recordPane = pane;
}

void KifuDisplayCoordinator::setBranchTreeWidget(BranchTreeWidget* widget)
{
    m_branchTreeWidget = widget;
    if (m_branchTreeWidget != nullptr && m_tree != nullptr) {
        m_branchTreeWidget->setTree(m_tree);
    }
}

void KifuDisplayCoordinator::setRecordModel(KifuRecordListModel* model)
{
    m_recordModel = model;
}

void KifuDisplayCoordinator::setBranchModel(KifuBranchListModel* model)
{
    m_branchModel = model;
}

void KifuDisplayCoordinator::setBranchTreeManager(BranchTreeManager* manager)
{
    m_branchTreeManager = manager;
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

void KifuDisplayCoordinator::onBranchTreeNodeClicked(int lineIndex, int ply)
{
    qCDebug(lcUi).noquote() << "onBranchTreeNodeClicked ENTER lineIndex=" << lineIndex << "ply=" << ply
                            << "preferredLineIndex(before)=" << (m_state ? m_state->preferredLineIndex() : -99)
                            << "m_lastModelLineIndex=" << m_lastModelLineIndex;

    // ラインとplyからノードを探して移動
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
    // goToNode() は preferredLineIndex を変更しないため、ここで設定しないと
    // 以前の分岐選択の古い preferredLineIndex が残り、誤ったラインが表示される
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
                                    << "m_lastModelLineIndex(after)=" << m_lastModelLineIndex;
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
                           << "m_lastModelLineIndex=" << m_lastModelLineIndex;

        // 重要: 棋譜モデルが実際に表示しているラインと一致しているかも確認
        // onPositionChanged() でm_lastLineIndexが先に更新されてしまう場合があるため、
        // m_lastModelLineIndex を使って実際のモデル内容との不一致を検出する
        const bool lineIndexChanged = (newLineIndex != m_lastLineIndex);
        const bool modelNeedsUpdate = (m_lastModelLineIndex >= 0 && m_lastModelLineIndex != newLineIndex);

        if (lineIndexChanged || modelNeedsUpdate) {
            qCDebug(lcUi).noquote() << "onNavigationCompleted: updating record view"
                               << "lineIndexChanged=" << lineIndexChanged
                               << "modelNeedsUpdate=" << modelNeedsUpdate;
            m_lastLineIndex = newLineIndex;
            updateRecordView();  // 棋譜欄の内容を再構築
        } else {
            qCDebug(lcUi).noquote() << "onNavigationCompleted: line NOT changed, skipping updateRecordView";
        }
    }

    // 盤面とハイライトを更新（新システム用シグナル）
    if (node != nullptr) {
        QString currentSfen = node->sfen();
        QString prevSfen;
        if (node->parent() != nullptr) {
            prevSfen = node->parent()->sfen();
        }
        qCDebug(lcUi).noquote() << "onNavigationCompleted: emitting boardWithHighlightsRequired"
                           << "currentSfen=" << currentSfen.left(40)
                           << "prevSfen=" << (prevSfen.isEmpty() ? "(empty)" : prevSfen.left(40));
        emit boardWithHighlightsRequired(currentSfen, prevSfen);
    }

    highlightCurrentPosition();

    // ナビゲーション完了時の一致性チェック
    if (!verifyDisplayConsistency()) {
        qCWarning(lcUi).noquote() << "POST-NAVIGATION INCONSISTENCY:";
        qCDebug(lcUi).noquote() << getConsistencyReport();
    }
}

void KifuDisplayCoordinator::onBoardUpdateRequired(const QString& sfen)
{
    // 注意: boardWithHighlightsRequired を使用するため、このスロットは現在未使用
    Q_UNUSED(sfen);
}

void KifuDisplayCoordinator::onRecordHighlightRequired(int ply)
{
    if (m_recordModel == nullptr) {
        return;
    }

    const int stateLineIndex = m_state ? m_state->currentLineIndex() : -1;
    qCDebug(lcUi).noquote() << "onRecordHighlightRequired: ply=" << ply
                       << "modelRowCount=" << m_recordModel->rowCount()
                       << "lastModelLineIndex=" << m_lastModelLineIndex
                       << "stateLineIndex=" << stateLineIndex
                       << "preferredLineIndex=" << (m_state ? m_state->preferredLineIndex() : -99);

    // 重要: モデルのラインとナビゲーション状態のラインが一致しない場合、モデルを再構築
    if (m_state != nullptr && m_lastModelLineIndex >= 0 && m_lastModelLineIndex != stateLineIndex) {
        qCDebug(lcUi).noquote() << "onRecordHighlightRequired: Line mismatch detected ("
                           << m_lastModelLineIndex << "!=" << stateLineIndex << "), rebuilding model";
        updateRecordView();
        qCDebug(lcUi).noquote() << "onRecordHighlightRequired: after rebuild, m_lastModelLineIndex=" << m_lastModelLineIndex;
    } else {
        qCDebug(lcUi).noquote() << "onRecordHighlightRequired: NO rebuild (lastModel=" << m_lastModelLineIndex
                           << " state=" << stateLineIndex << ")";
    }

    // ply=0は開始局面で、モデルの行0に対応
    // ply=Nはモデルの行Nに対応
    m_recordModel->setCurrentHighlightRow(ply);

    // 棋譜欄のビューで該当行を選択
    // シグナルブロック: ナビゲーション起因の行変更でonRecordRowChangedByPresenterが
    //    再トリガーされ、本譜の局面でエンジン位置が上書きされるのを防止
    if (m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        QTableView* view = m_recordPane->kifuView();
        QModelIndex idx = m_recordModel->index(ply, 0);
        {
            QSignalBlocker blocker(view->selectionModel());
            view->setCurrentIndex(idx);
        }
        view->scrollTo(idx);
    }
}

void KifuDisplayCoordinator::onBranchTreeHighlightRequired(int lineIndex, int ply)
{
    // 期待値を追跡
    m_expectedTreeLineIndex = lineIndex;
    m_expectedTreePly = ply;

    qCDebug(lcUi).noquote() << "onBranchTreeHighlightRequired:"
                       << "lineIndex=" << lineIndex
                       << "ply=" << ply
                       << "state->currentLineIndex()=" << (m_state != nullptr ? m_state->currentLineIndex() : -1)
                       << "m_lastLineIndex=" << m_lastLineIndex;

    // 即時不一致検出
    if (m_state != nullptr && lineIndex != m_state->currentLineIndex()) {
        qCWarning(lcUi).noquote() << "INCONSISTENCY DETECTED:"
                             << "Tree will highlight line" << lineIndex
                             << "but state says current line is" << m_state->currentLineIndex();
        qCDebug(lcUi).noquote() << getConsistencyReport();
    }

    // BranchTreeWidget がある場合はそちらを使用
    if (m_branchTreeWidget != nullptr) {
        m_branchTreeWidget->highlightNode(lineIndex, ply);
    }
    // BranchTreeManager がある場合はそちらを使用（通常はこちら）
    if (m_branchTreeManager != nullptr) {
        m_branchTreeManager->highlightBranchTreeAt(lineIndex, ply, /*centerOn=*/false);
    }
}

void KifuDisplayCoordinator::onBranchCandidatesUpdateRequired(const QVector<KifuBranchNode*>& candidates)
{
    if (m_branchModel == nullptr) {
        return;
    }

    m_branchModel->resetBranchCandidates();

    if (candidates.isEmpty()) {
        // 分岐がない場合は「本譜に戻る」ボタンも非表示
        m_branchModel->setHasBackToMainRow(false);
        // 分岐がない場合もビューを有効化（薄く表示されないように）
        if (m_recordPane != nullptr && m_recordPane->branchView() != nullptr) {
            m_recordPane->branchView()->setEnabled(true);
        }
        return;
    }

    // 分岐候補をKifDisplayItemに変換してモデルに設定
    QList<KifDisplayItem> items;
    for (KifuBranchNode* node : std::as_const(candidates)) {
        KifDisplayItem item;
        item.prettyMove = node->displayText();
        item.timeText = node->timeText();
        items.append(item);
    }
    m_branchModel->updateBranchCandidates(items);

    // 本譜にいない場合は「本譜に戻る」を表示
    // currentLineIndex() が 0 でなければ分岐ライン上
    bool isOnMainLine = (m_state != nullptr) ? (m_state->currentLineIndex() == 0) : true;
    m_branchModel->setHasBackToMainRow(!isOnMainLine);

    // 現在選択されている分岐をハイライト
    if (m_state != nullptr && m_state->currentNode() != nullptr) {
        KifuBranchNode* current = m_state->currentNode();
        KifuBranchNode* parent = current->parent();
        if (parent != nullptr) {
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->childAt(i) == current) {
                    m_branchModel->setCurrentHighlightRow(i);
                    break;
                }
            }
        }
    }

    // 分岐候補ビューを有効化（ナビゲーションボタン経由でも有効にする）
    if (m_recordPane != nullptr && m_recordPane->branchView() != nullptr) {
        m_recordPane->branchView()->setEnabled(true);
    }
}

void KifuDisplayCoordinator::onTreeChanged()
{
    // ツリー変更時にラインインデックスをリセット
    m_lastLineIndex = (m_state != nullptr) ? m_state->currentLineIndex() : 0;

    updateRecordView();
    updateBranchTreeView();
    updateBranchCandidatesView();
    highlightCurrentPosition();
}

void KifuDisplayCoordinator::updateRecordView()
{
    qCDebug(lcUi).noquote() << "updateRecordView: CALLED";
    populateRecordModel();
    populateBranchMarks();

    // ビューの明示的な更新を強制（モデル変更後にビューが更新されない問題の対策）
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

    // BranchTreeManager も更新（分岐ツリードックの表示）
    if (m_branchTreeManager != nullptr && m_tree != nullptr) {
        QVector<BranchTreeManager::ResolvedRowLite> rows;
        const QVector<BranchLine> lines = m_tree->allLines();

        for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            BranchTreeManager::ResolvedRowLite row;
            row.startPly = (line.branchPly > 0) ? line.branchPly : 1;

            row.parent = -1;
            if (line.branchPoint != nullptr) {
                for (int j = 0; j < lines.size(); ++j) {
                    if (j == lineIdx) continue;
                    if (lines.at(j).nodes.contains(line.branchPoint)) {
                        row.parent = j;
                        break;
                    }
                }
            }

            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                KifDisplayItem item;
                if (node->ply() == 0) {
                    item.prettyMove = QString();
                } else {
                    item.prettyMove = node->displayText();
                }
                item.timeText = node->timeText();
                item.comment = node->comment();
                row.disp.append(item);
                row.sfen.append(node->sfen());
            }

            rows.append(row);
        }

        m_branchTreeManager->setBranchTreeRows(rows);
    } else if (m_branchTreeManager != nullptr) {
        // ツリーが null の場合は空にする
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
    if (m_state == nullptr) {
        return;
    }

    const int ply = m_state->currentPly();
    const int lineIndex = m_state->currentLineIndex();

    // 棋譜欄のハイライト
    onRecordHighlightRequired(ply);

    // 分岐ツリーのハイライト
    onBranchTreeHighlightRequired(lineIndex, ply);

    // コメント表示の更新
    if (m_state->currentNode() != nullptr) {
        const QString comment = m_state->currentNode()->comment().trimmed();
        emit commentUpdateRequired(ply, comment.isEmpty() ? tr("コメントなし") : comment, true);
    }
}

void KifuDisplayCoordinator::populateRecordModel()
{
    qCDebug(lcUi).noquote() << "populateRecordModel: ENTER"
                       << "m_recordModel=" << (m_recordModel ? "yes" : "null")
                       << "m_recordModel ptr=" << static_cast<void*>(m_recordModel)
                       << "m_tree=" << (m_tree ? "yes" : "null");

    if (m_recordModel == nullptr || m_tree == nullptr) {
        qCDebug(lcUi).noquote() << "populateRecordModel: EARLY RETURN (null model or tree)";
        return;
    }

    const int oldRowCount = m_recordModel->rowCount();
    m_recordModel->clearAllItems();
    qCDebug(lcUi).noquote() << "populateRecordModel: cleared model, old rowCount=" << oldRowCount
                       << "new rowCount=" << m_recordModel->rowCount();

    // 現在のラインを取得
    int currentLineIndex = 0;
    if (m_state != nullptr) {
        currentLineIndex = m_state->currentLineIndex();
    }

    qCDebug(lcUi).noquote() << "populateRecordModel: currentLineIndex=" << currentLineIndex;

    QVector<BranchLine> lines = m_tree->allLines();
    if (currentLineIndex < 0 || currentLineIndex >= lines.size()) {
        currentLineIndex = 0;  // フォールバック: 本譜
    }

    // 表示するラインが無い場合は終了
    if (lines.isEmpty()) {
        return;
    }

    const BranchLine& line = lines.at(currentLineIndex);

    // 開始局面（ply=0）を追加
    // ルートノード（ply==0）がある場合はしおりを取得
    QString openingBookmark;
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == 0) {
            openingBookmark = node->bookmark();
            break;
        }
    }
    auto* startItem = new KifuDisplay(
        tr("=== 開始局面 ==="),
        tr("（１手 / 合計）"),
        QString(),
        openingBookmark,
        m_recordModel);
    m_recordModel->appendItem(startItem);

    // 各指し手を追加
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == 0) {
            continue;  // 開始局面はスキップ（既に追加済み）
        }

        // 手数番号を追加（4桁右寄せ）
        const QString moveNumberStr = QString::number(node->ply());
        const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
        QString displayText = spaces + moveNumberStr + QLatin1Char(' ') + node->displayText();

        // 分岐マーク: 分岐がある手には '+' を付ける
        if (node->parent() != nullptr && node->parent()->hasBranch()) {
            if (!displayText.endsWith(QLatin1Char('+'))) {
                displayText += QLatin1Char('+');
            }
        }

        auto* item = new KifuDisplay(
            displayText,
            node->timeText(),
            node->comment(),
            node->bookmark(),
            m_recordModel
        );
        m_recordModel->appendItem(item);
    }

    // 重要: 棋譜モデルが実際に表示しているラインインデックスを記録
    m_lastModelLineIndex = currentLineIndex;

    qCDebug(lcUi).noquote() << "populateRecordModel: DONE, final rowCount=" << m_recordModel->rowCount()
                       << "m_lastModelLineIndex=" << m_lastModelLineIndex;

    // デバッグ: 棋譜欄の3手目の内容を出力（不一致検出用）
    if (m_recordModel->rowCount() > 3) {
        KifuDisplay* item = m_recordModel->item(3);
        if (item) {
            qCDebug(lcUi).noquote() << "populateRecordModel DEBUG:"
                               << "currentLineIndex=" << currentLineIndex
                               << "ply3_move=" << item->currentMove();
        }
    }
}

void KifuDisplayCoordinator::populateBranchMarks()
{
    if (m_recordModel == nullptr || m_tree == nullptr) {
        return;
    }

    QSet<int> branchPlys;

    // 現在表示中のラインで分岐がある手を収集
    int currentLineIndex = 0;
    if (m_state != nullptr) {
        currentLineIndex = m_state->currentLineIndex();
    }

    QVector<BranchLine> lines = m_tree->allLines();
    if (currentLineIndex >= 0 && currentLineIndex < lines.size()) {
        const BranchLine& line = lines.at(currentLineIndex);
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->parent() != nullptr && node->parent()->hasBranch()) {
                branchPlys.insert(node->ply());
            }
        }
    }

    m_recordModel->setBranchPlyMarks(branchPlys);
}

void KifuDisplayCoordinator::populateRecordModelFromPath(const QVector<KifuBranchNode*>& path, int highlightPly)
{
    if (m_recordModel == nullptr) {
        return;
    }

    m_recordModel->clearAllItems();

    // 開始局面のしおりを取得（ply==0 のノード）
    QString openingBookmark;
    for (KifuBranchNode* node : std::as_const(path)) {
        if (node != nullptr && node->ply() == 0) {
            openingBookmark = node->bookmark();
            break;
        }
    }
    auto* startItem = new KifuDisplay(
        tr("=== 開始局面 ==="),
        tr("（１手 / 合計）"),
        QString(),
        openingBookmark,
        m_recordModel);
    m_recordModel->appendItem(startItem);

    QSet<int> branchPlys;

    for (KifuBranchNode* node : std::as_const(path)) {
        if (node == nullptr || node->ply() == 0) {
            continue;
        }

        const QString moveNumberStr = QString::number(node->ply());
        const QString spaces = QString(qMax(0, 4 - moveNumberStr.length()), QLatin1Char(' '));
        QString displayText = spaces + moveNumberStr + QLatin1Char(' ') + node->displayText();

        if (node->parent() != nullptr && node->parent()->hasBranch()) {
            if (!displayText.endsWith(QLatin1Char('+'))) {
                displayText += QLatin1Char('+');
            }
            branchPlys.insert(node->ply());
        }

        auto* item = new KifuDisplay(
            displayText,
            node->timeText(),
            node->comment(),
            node->bookmark(),
            m_recordModel
        );
        m_recordModel->appendItem(item);
    }

    m_recordModel->setBranchPlyMarks(branchPlys);

    // 重要: 棋譜モデルが実際に表示しているラインインデックスを記録
    // パスの最後のノードからラインインデックスを取得
    // 注意: node->lineIndex() は最初の分岐点での選択インデックスのみを返すため、
    // allLines() の DFS順と一致しない。findLineIndexForNode() を使用する必要がある。
    if (!path.isEmpty() && path.last() != nullptr && m_tree != nullptr) {
        const int nodeLineIndex = path.last()->lineIndex();
        const int treeLineIndex = m_tree->findLineIndexForNode(path.last());
        qCDebug(lcUi).noquote() << "populateRecordModelFromPath: DEBUG"
                           << "nodeLineIndex=" << nodeLineIndex
                           << "treeLineIndex=" << treeLineIndex
                           << "pathSize=" << path.size()
                           << "lastNodePly=" << path.last()->ply();
        // findLineIndexForNode() を使用して正確なラインインデックスを取得
        m_lastModelLineIndex = (treeLineIndex >= 0) ? treeLineIndex : 0;
    } else {
        m_lastModelLineIndex = 0;  // 空のパスは本譜として扱う
    }
    qCDebug(lcUi).noquote() << "populateRecordModelFromPath: m_lastModelLineIndex=" << m_lastModelLineIndex;

    const int maxRow = m_recordModel->rowCount() - 1;
    const int rowToHighlight = qBound(0, highlightPly, maxRow);
    onRecordHighlightRequired(rowToHighlight);
}

void KifuDisplayCoordinator::onPositionChanged(int lineIndex, int ply, const QString& sfen)
{
    Q_UNUSED(sfen)
    Q_UNUSED(lineIndex)  // 現在は m_state->currentLineIndex() を使用

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
        // 現在表示中のラインからplyで探す
        // m_state->currentLineIndex() を使用して、棋譜欄が表示中のラインを特定
        int currentLine = m_state->currentLineIndex();
        if (currentLine < 0 || currentLine >= lines.size()) {
            currentLine = 0;  // フォールバック: 本譜
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
                if (li == currentLine) continue;  // 既に検索済み
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

    // 分岐点での選択を記憶（次回のナビゲーションで正しい子を選択するため）
    // goToNodeと同様のロジックで、現在ノードが分岐の子である場合に記憶
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

    // 状態を更新（次回の検索で正しいラインを使用するため）
    // この呼び出しはシグナルを発火するが、UI側での二重ナビゲーションは発生しない
    // （MainWindowが既にUIを管理しているため）
    m_state->setCurrentNode(targetNode);

    // 分岐候補を直接計算（m_state を経由しない）
    // 新システム専用のメソッドを使用し、設定後はロックする
    if (m_branchModel != nullptr) {
        m_branchModel->resetBranchCandidates();

        // 分岐候補は「現在位置の親」の子（つまり現在位置の兄弟）を表示する
        // これにより、分岐点で選択可能な手が表示される
        KifuBranchNode* parentNode = targetNode->parent();

        qCDebug(lcUi).noquote() << "parentNode=" << (parentNode ? "yes" : "null")
                           << "parentNode->childCount()=" << (parentNode ? parentNode->childCount() : -1);

        if (parentNode != nullptr && parentNode->childCount() > 1) {
            QList<KifDisplayItem> items;
            for (int i = 0; i < parentNode->childCount(); ++i) {
                KifuBranchNode* sibling = parentNode->childAt(i);
                KifDisplayItem item;
                item.prettyMove = sibling->displayText();
                item.timeText = sibling->timeText();
                items.append(item);
            }
            // 新システム専用メソッド（設定後に自動でロック）
            m_branchModel->updateBranchCandidates(items);

            // 本譜にいるかどうかを確認（targetNode が本譜上かどうか）
            bool isOnMainLine = targetNode->isMainLine();
            m_branchModel->setHasBackToMainRow(!isOnMainLine);

            // 分岐候補ビューを有効化
            if (m_recordPane != nullptr && m_recordPane->branchView() != nullptr) {
                m_recordPane->branchView()->setEnabled(true);
            }

            qCDebug(lcUi).noquote() << "onPositionChanged: set" << items.size()
                               << "branch candidates (siblings at ply" << targetNode->ply() << ")";
        } else {
            m_branchModel->setHasBackToMainRow(false);
            // ロックして外部からの変更を防ぐ
            m_branchModel->setLocked(true);

            // 分岐候補がない場合もビューを有効化（薄く表示されないように）
            if (m_recordPane != nullptr && m_recordPane->branchView() != nullptr) {
                m_recordPane->branchView()->setEnabled(true);
            }

            qCDebug(lcUi).noquote() << "onPositionChanged: no branch candidates (parent has"
                               << (parentNode ? parentNode->childCount() : 0) << "children)";
        }
    }

    // 分岐ツリーのハイライト（グラフィカルのみ）
    // targetNodeが含まれるラインを探す
    // 現在のラインインデックスを優先して使用（分岐選択後の状態を維持）
    int highlightLineIndex = m_state->currentLineIndex();
    QVector<BranchLine> allLines = m_tree->allLines();

    // 重要: m_lastLineIndex を同期しておく（新システムとの整合性を保つため）
    // 分岐ツリーから直接クリックした場合など、onNavigationCompleted を経由しない
    // 場合でもラインインデックスを追跡する
    const int oldLastLineIndex = m_lastLineIndex;
    m_lastLineIndex = highlightLineIndex;
    qCDebug(lcUi).noquote() << "onPositionChanged: synced m_lastLineIndex from" << oldLastLineIndex
                       << "to" << m_lastLineIndex
                       << "(model rowCount=" << (m_recordModel ? m_recordModel->rowCount() : -1) << ")";

    // 現在のラインにノードが存在するか確認
    bool foundInCurrentLine = false;
    if (highlightLineIndex >= 0 && highlightLineIndex < allLines.size()) {
        for (KifuBranchNode* node : std::as_const(allLines.at(highlightLineIndex).nodes)) {
            if (node == targetNode) {
                foundInCurrentLine = true;
                break;
            }
        }
    }

    // 現在のラインになければ、本譜（line 0）を使用
    if (!foundInCurrentLine) {
        highlightLineIndex = 0;
    }

    qCDebug(lcUi).noquote() << "onPositionChanged: highlighting branch tree at line="
                       << highlightLineIndex << "ply=" << ply;

    // BranchTreeWidget がある場合はそちらを使用
    if (m_branchTreeWidget != nullptr) {
        m_branchTreeWidget->highlightNode(highlightLineIndex, ply);
    }
    // BranchTreeManager がある場合はそちらも使用（通常はこちらが表示される）
    if (m_branchTreeManager != nullptr) {
        m_branchTreeManager->highlightBranchTreeAt(highlightLineIndex, ply, /*centerOn=*/false);
    }

    // 重要: onRecordHighlightRequired() は呼び出さない
    // MainWindowが既にビューの選択を管理しているため

    // 位置変更完了時の一致性チェック
    if (!verifyDisplayConsistency()) {
        qCWarning(lcUi).noquote() << "POST-NAVIGATION INCONSISTENCY:";
        qCDebug(lcUi).noquote() << getConsistencyReport();
    }

    qCDebug(lcUi).noquote() << "onPositionChanged LEAVE";
}

KifuDisplayCoordinator::DisplaySnapshot KifuDisplayCoordinator::captureDisplaySnapshot() const
{
    DisplaySnapshot snapshot;
    snapshot.trackedLineIndex = m_lastLineIndex;
    snapshot.modelLineIndex = m_lastModelLineIndex;
    snapshot.expectedTreeLineIndex = m_expectedTreeLineIndex;
    snapshot.expectedTreePly = m_expectedTreePly;

    if (m_state != nullptr) {
        snapshot.stateLineIndex = m_state->currentLineIndex();
        snapshot.statePly = m_state->currentPly();
        snapshot.stateOnMainLine = m_state->isOnMainLine();
        snapshot.stateSfen = m_state->currentSfen();
        snapshot.stateSfenNormalized = normalizeSfenForCompare(snapshot.stateSfen);
    }

    if (m_recordModel != nullptr) {
        snapshot.modelRowCount = m_recordModel->rowCount();
        snapshot.modelHighlightRow = m_recordModel->currentHighlightRow();

        if (snapshot.statePly >= 0 && snapshot.statePly < m_recordModel->rowCount()) {
            if (KifuDisplay* item = m_recordModel->item(snapshot.statePly)) {
                snapshot.displayedMoveAtPly = item->currentMove();
            }
        }
    }

    if (m_tree != nullptr && snapshot.stateLineIndex >= 0) {
        const QVector<BranchLine> lines = m_tree->allLines();
        if (snapshot.stateLineIndex < lines.size()) {
            const BranchLine& line = lines.at(snapshot.stateLineIndex);
            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                if (node != nullptr && node->ply() == snapshot.statePly) {
                    snapshot.expectedMoveAtPly = node->displayText();
                    break;
                }
            }
        }
    }

    if (m_branchTreeManager != nullptr && m_branchTreeManager->hasHighlightedNode()) {
        snapshot.treeHighlightLineIndex = m_branchTreeManager->lastHighlightedRow();
        snapshot.treeHighlightPly = m_branchTreeManager->lastHighlightedPly();
    }

    if (m_branchModel != nullptr) {
        snapshot.branchCandidateCount = m_branchModel->branchCandidateCount();
        snapshot.hasBackToMainRow = m_branchModel->hasBackToMainRow();
    }

    if (m_boardSfenProvider) {
        snapshot.boardSfen = m_boardSfenProvider();
        snapshot.boardSfenNormalized = normalizeSfenForCompare(snapshot.boardSfen);
    }

    return snapshot;
}

bool KifuDisplayCoordinator::verifyDisplayConsistencyDetailed(QString* reason) const
{
    if (reason != nullptr) {
        reason->clear();
    }

    // m_state がない場合は検証不可
    if (m_state == nullptr || m_tree == nullptr) {
        return true;
    }

    const DisplaySnapshot snapshot = captureDisplaySnapshot();
    auto fail = [&](const QString& msg) {
        if (reason != nullptr) {
            *reason = msg;
        }
        return false;
    };

    // 1. コーディネータ追跡ラインと状態ライン
    if (snapshot.trackedLineIndex != snapshot.stateLineIndex) {
        return fail(QStringLiteral("ライン不一致: tracked=%1 state=%2")
                    .arg(snapshot.trackedLineIndex)
                    .arg(snapshot.stateLineIndex));
    }

    // 2. 棋譜欄モデルのライン/行/ハイライト
    if (m_recordModel != nullptr) {
        if (snapshot.modelLineIndex >= 0 && snapshot.modelLineIndex != snapshot.stateLineIndex) {
            return fail(QStringLiteral("棋譜欄ライン不一致: model=%1 state=%2")
                        .arg(snapshot.modelLineIndex)
                        .arg(snapshot.stateLineIndex));
        }
        if (snapshot.modelHighlightRow != snapshot.statePly) {
            return fail(QStringLiteral("棋譜欄ハイライト不一致: modelHighlight=%1 statePly=%2")
                        .arg(snapshot.modelHighlightRow)
                        .arg(snapshot.statePly));
        }

        const QVector<BranchLine> lines = m_tree->allLines();
        if (snapshot.stateLineIndex >= 0 && snapshot.stateLineIndex < lines.size()) {
            const BranchLine& line = lines.at(snapshot.stateLineIndex);
            const int expectedRowCount = line.nodes.isEmpty() ? 1 : static_cast<int>(line.nodes.size());
            if (qAbs(expectedRowCount - snapshot.modelRowCount) > 1) {
                return fail(QStringLiteral("棋譜欄行数不一致: expected~=%1 actual=%2")
                            .arg(expectedRowCount)
                            .arg(snapshot.modelRowCount));
            }
        }

        if (snapshot.statePly == 0) {
            if (!snapshot.displayedMoveAtPly.contains(QStringLiteral("開始局面"))) {
                return fail(QStringLiteral("棋譜欄0行が開始局面ではありません: [%1]")
                            .arg(snapshot.displayedMoveAtPly));
            }
        } else if (!snapshot.expectedMoveAtPly.isEmpty()
                   && !snapshot.displayedMoveAtPly.contains(snapshot.expectedMoveAtPly)) {
            return fail(QStringLiteral("棋譜欄指し手不一致: expected contains [%1], actual [%2]")
                        .arg(snapshot.expectedMoveAtPly, snapshot.displayedMoveAtPly));
        }
    }

    // 3. 分岐ツリーハイライト（BranchTreeManager）
    if (m_branchTreeManager != nullptr && snapshot.treeHighlightLineIndex >= 0 && snapshot.treeHighlightPly >= 0) {
        if (snapshot.treeHighlightLineIndex != snapshot.stateLineIndex
            || snapshot.treeHighlightPly != snapshot.statePly) {
            return fail(QStringLiteral("分岐ツリーハイライト不一致: tree=(%1,%2) state=(%3,%4)")
                        .arg(snapshot.treeHighlightLineIndex)
                        .arg(snapshot.treeHighlightPly)
                        .arg(snapshot.stateLineIndex)
                        .arg(snapshot.statePly));
        }
    }

    // 4. 分岐候補欄
    if (m_branchModel != nullptr && m_state->currentNode() != nullptr) {
        int expectedCandidateCount = 0;
        if (KifuBranchNode* parent = m_state->currentNode()->parent();
            parent != nullptr && parent->childCount() > 1) {
            expectedCandidateCount = parent->childCount();
            for (int i = 0; i < expectedCandidateCount; ++i) {
                if (m_branchModel->labelAt(i) != parent->childAt(i)->displayText()) {
                    return fail(QStringLiteral("分岐候補指し手不一致: row=%1 expected=[%2] actual=[%3]")
                                .arg(i)
                                .arg(parent->childAt(i)->displayText(), m_branchModel->labelAt(i)));
                }
            }
        }
        if (snapshot.branchCandidateCount != expectedCandidateCount) {
            return fail(QStringLiteral("分岐候補件数不一致: expected=%1 actual=%2")
                        .arg(expectedCandidateCount)
                        .arg(snapshot.branchCandidateCount));
        }
        const bool expectedBackToMain = (expectedCandidateCount > 0 && !snapshot.stateOnMainLine);
        if (snapshot.hasBackToMainRow != expectedBackToMain) {
            return fail(QStringLiteral("本譜に戻る表示不一致: expected=%1 actual=%2")
                        .arg(expectedBackToMain ? QStringLiteral("true") : QStringLiteral("false"),
                             snapshot.hasBackToMainRow ? QStringLiteral("true") : QStringLiteral("false")));
        }
    }

    // 5. 盤面SFEN（任意）
    if (!snapshot.boardSfenNormalized.isEmpty() && !snapshot.stateSfenNormalized.isEmpty()
        && snapshot.boardSfenNormalized != snapshot.stateSfenNormalized) {
        return fail(QStringLiteral("盤面SFEN不一致: board=[%1] state=[%2]")
                    .arg(snapshot.boardSfenNormalized, snapshot.stateSfenNormalized));
    }

    return true;
}

bool KifuDisplayCoordinator::verifyDisplayConsistency() const
{
    return verifyDisplayConsistencyDetailed(nullptr);
}

QString KifuDisplayCoordinator::getConsistencyReport() const
{
    QString report;
    QTextStream out(&report);

    out << "=== Consistency Report ===" << Qt::endl;

    QString reason;
    const DisplaySnapshot snapshot = captureDisplaySnapshot();
    const bool consistent = verifyDisplayConsistencyDetailed(&reason);

    // State 情報
    out << "State: lineIndex=" << snapshot.stateLineIndex
        << ", ply=" << snapshot.statePly
        << ", onMainLine=" << (snapshot.stateOnMainLine ? "true" : "false")
        << ", sfen=" << snapshot.stateSfenNormalized
        << Qt::endl;

    // Coordinator 情報
    out << "Coordinator: trackedLine=" << snapshot.trackedLineIndex
        << ", modelLine=" << snapshot.modelLineIndex
        << ", expectedTree=(" << snapshot.expectedTreeLineIndex << "," << snapshot.expectedTreePly << ")"
        << Qt::endl;

    // Model 情報
    if (m_recordModel != nullptr) {
        out << "Model: rowCount=" << snapshot.modelRowCount
            << ", highlightRow=" << snapshot.modelHighlightRow
            << Qt::endl;
        out << "ModelMoveAtPly: actual=[" << snapshot.displayedMoveAtPly
            << "] expected=[" << snapshot.expectedMoveAtPly << "]" << Qt::endl;
    } else {
        out << "Model: null" << Qt::endl;
    }

    out << "TreeHighlight(actual): lineIndex=" << snapshot.treeHighlightLineIndex
        << ", ply=" << snapshot.treeHighlightPly << Qt::endl;

    out << "BranchCandidates: count=" << snapshot.branchCandidateCount
        << ", hasBackToMain=" << (snapshot.hasBackToMainRow ? "true" : "false") << Qt::endl;

    out << "BoardSfen(normalized): " << snapshot.boardSfenNormalized << Qt::endl;

    // 一致性判定結果
    out << "Consistent: " << (consistent ? "YES" : "NO") << Qt::endl;
    if (!reason.isEmpty()) {
        out << "Reason: " << reason << Qt::endl;
    }

    return report;
}

// === ライブ対局セッションからのシグナル ===

void KifuDisplayCoordinator::onLiveGameMoveAdded(int ply, const QString& displayText)
{
    Q_UNUSED(ply);
    Q_UNUSED(displayText);

    const int liveLineIndex = (m_liveSession != nullptr) ? m_liveSession->currentLineIndex() : 0;
    KifuBranchNode* liveNode = (m_liveSession != nullptr) ? m_liveSession->liveNode() : nullptr;

    // BranchTreeManager の分岐ツリーを更新
    // addMoveQuiet() は treeChanged() を発火しないため、ここで明示的に更新する
    if (m_branchTreeManager != nullptr && m_tree != nullptr) {
        // KifuBranchTree から ResolvedRowLite 形式に変換
        QVector<BranchTreeManager::ResolvedRowLite> rows;
        QVector<BranchLine> lines = m_tree->allLines();

        for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            BranchTreeManager::ResolvedRowLite row;
            row.startPly = (line.branchPly > 0) ? line.branchPly : 1;

            // branchPointから親行インデックスを正しく計算する
            // branchPointが含まれる他のラインのインデックスを探す
            row.parent = -1;
            if (line.branchPoint != nullptr) {
                for (int j = 0; j < lines.size(); ++j) {
                    if (j == lineIdx) continue;  // 自分自身はスキップ
                    if (lines.at(j).nodes.contains(line.branchPoint)) {
                        row.parent = j;
                        break;
                    }
                }
            }

            // rebuildBranchTree() は disp[0] が開始局面エントリ（prettyMove 空）、
            // disp[i] (i>=1) が i 手目に対応することを期待する。
            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                KifDisplayItem item;
                if (node->ply() == 0) {
                    // 開始局面: prettyMove を空にして追加
                    item.prettyMove = QString();
                } else {
                    item.prettyMove = node->displayText();
                }
                item.timeText = node->timeText();
                item.comment = node->comment();
                row.disp.append(item);
                row.sfen.append(node->sfen());
            }

            rows.append(row);
        }

        m_branchTreeManager->setBranchTreeRows(rows);

        // ハイライト更新
        if (m_liveSession != nullptr) {
            const int totalPly = m_liveSession->totalPly();
            m_branchTreeManager->highlightBranchTreeAt(liveLineIndex, totalPly, false);
        }
    }

    // BranchTreeWidget も更新（存在する場合）
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

    // branchPoint のラインインデックスを取得して設定
    // 再対局開始時点でユーザーが選択したラインは確定しているため、
    // リセットではなく正しいラインインデックスを設定する
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

    // m_lastLineIndex を branchPoint のラインインデックスに設定
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
    // これにより、開始局面から1手ずつ進む際に正しい子を選択できる
    // goToNode() と同様のロジックで、各分岐点での選択をm_lastSelectedLineAtBranch に記憶する
    if (m_state != nullptr && !path.isEmpty()) {
        for (int i = 0; i < path.size(); ++i) {
            KifuBranchNode* node = path.at(i);
            KifuBranchNode* parent = node->parent();
            if (parent != nullptr && parent->childCount() > 1) {
                // 何番目の子かを探す
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

    populateRecordModelFromPath(path, branchPoint->ply());
}

void KifuDisplayCoordinator::onLiveGameBranchMarksUpdated(const QSet<int>& branchPlys)
{
    if (m_recordModel != nullptr) {
        m_recordModel->setBranchPlyMarks(branchPlys);
    }
}

void KifuDisplayCoordinator::onLiveGameCommitted(KifuBranchNode* newLineEnd)
{
    // ライブ対局がツリーに確定されたので、ビューを更新
    updateRecordView();
    updateBranchTreeView();

    // 確定後の新ラインの最終ノードにナビゲート
    if (newLineEnd != nullptr && m_navController != nullptr) {
        m_navController->goToNode(newLineEnd);
    }
}

void KifuDisplayCoordinator::onLiveGameDiscarded()
{
    // ライブ対局が破棄されたので、元の状態に戻す
    updateRecordView();
    updateBranchTreeView();
}

void KifuDisplayCoordinator::onLiveGameRecordModelUpdateRequired()
{
    // 棋譜欄ビューの更新を強制
    if (m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        m_recordPane->kifuView()->viewport()->update();
    }
}
