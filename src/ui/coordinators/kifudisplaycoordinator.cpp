#include "kifudisplaycoordinator.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "recordpane.h"
#include "branchtreewidget.h"
#include "engineanalysistab.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifudisplay.h"
#include "kifdisplayitem.h"
#include "livegamesession.h"

#include <QTableView>
#include <QModelIndex>
#include <QDebug>
#include <QTextStream>

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

void KifuDisplayCoordinator::setAnalysisTab(EngineAnalysisTab* tab)
{
    m_analysisTab = tab;
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
    // ラインとplyからノードを探して移動
    if (m_tree == nullptr || m_navController == nullptr) {
        return;
    }

    QVector<BranchLine> lines = m_tree->allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return;
    }

    const BranchLine& line = lines.at(lineIndex);
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == ply) {
            m_navController->goToNode(node);
            return;
        }
    }

    // plyが見つからない場合（開始局面）
    if (ply == 0 && m_tree->root() != nullptr) {
        m_navController->goToNode(m_tree->root());
    }
}

void KifuDisplayCoordinator::onBranchCandidateActivated(const QModelIndex& index)
{
    qDebug().noquote() << "[KDC] onBranchCandidateActivated ENTER"
                       << "index.valid=" << index.isValid()
                       << "row=" << index.row()
                       << "navController=" << (m_navController ? "yes" : "null")
                       << "branchModel=" << (m_branchModel ? "yes" : "null");

    if (!index.isValid() || m_navController == nullptr || m_branchModel == nullptr) {
        qDebug().noquote() << "[KDC] onBranchCandidateActivated: guard failed, returning";
        return;
    }

    const int row = index.row();
    if (m_branchModel->isBackToMainRow(row)) {
        qDebug().noquote() << "[KDC] onBranchCandidateActivated: back to main row, calling goToMainLineAtCurrentPly";
        m_navController->goToMainLineAtCurrentPly();
        return;
    }

    qDebug().noquote() << "[KDC] onBranchCandidateActivated: calling selectBranchCandidate(" << row << ")";
    m_navController->selectBranchCandidate(row);
}

void KifuDisplayCoordinator::onNavigationCompleted(KifuBranchNode* node)
{
    qDebug().noquote() << "[KDC] onNavigationCompleted ENTER node="
                       << (node ? QString("ply=%1").arg(node->ply()) : "null")
                       << "preferredLineIndex=" << (m_state ? m_state->preferredLineIndex() : -999);

    // ラインが変更された場合は棋譜欄の内容を更新
    if (m_state != nullptr) {
        const int newLineIndex = m_state->currentLineIndex();
        qDebug().noquote() << "[KDC] onNavigationCompleted: newLineIndex=" << newLineIndex
                           << "m_lastLineIndex=" << m_lastLineIndex
                           << "m_lastModelLineIndex=" << m_lastModelLineIndex;

        // ★ 重要: 棋譜モデルが実際に表示しているラインと一致しているかも確認
        // onPositionChanged() でm_lastLineIndexが先に更新されてしまう場合があるため、
        // m_lastModelLineIndex を使って実際のモデル内容との不一致を検出する
        const bool lineIndexChanged = (newLineIndex != m_lastLineIndex);
        const bool modelNeedsUpdate = (m_lastModelLineIndex >= 0 && m_lastModelLineIndex != newLineIndex);

        if (lineIndexChanged || modelNeedsUpdate) {
            qDebug().noquote() << "[KDC] onNavigationCompleted: updating record view"
                               << "lineIndexChanged=" << lineIndexChanged
                               << "modelNeedsUpdate=" << modelNeedsUpdate;
            m_lastLineIndex = newLineIndex;
            updateRecordView();  // 棋譜欄の内容を再構築
        } else {
            qDebug().noquote() << "[KDC] onNavigationCompleted: line NOT changed, skipping updateRecordView";
        }
    }

    // 盤面とハイライトを更新（新システム用シグナル）
    if (node != nullptr) {
        QString currentSfen = node->sfen();
        QString prevSfen;
        if (node->parent() != nullptr) {
            prevSfen = node->parent()->sfen();
        }
        qDebug().noquote() << "[KDC] onNavigationCompleted: emitting boardWithHighlightsRequired"
                           << "currentSfen=" << currentSfen.left(40)
                           << "prevSfen=" << (prevSfen.isEmpty() ? "(empty)" : prevSfen.left(40));
        emit boardWithHighlightsRequired(currentSfen, prevSfen);
    }

    highlightCurrentPosition();

    // ナビゲーション完了時の一致性チェック
    if (!verifyDisplayConsistency()) {
        qWarning().noquote() << "[KDC] POST-NAVIGATION INCONSISTENCY:";
        qDebug().noquote() << getConsistencyReport();
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

    // ply=0は開始局面で、モデルの行0に対応
    // ply=Nはモデルの行Nに対応
    m_recordModel->setCurrentHighlightRow(ply);

    // 棋譜欄のビューで該当行を選択
    if (m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        QTableView* view = m_recordPane->kifuView();
        QModelIndex idx = m_recordModel->index(ply, 0);
        view->setCurrentIndex(idx);
        view->scrollTo(idx);
    }
}

void KifuDisplayCoordinator::onBranchTreeHighlightRequired(int lineIndex, int ply)
{
    // 期待値を追跡
    m_expectedTreeLineIndex = lineIndex;
    m_expectedTreePly = ply;

    qDebug().noquote() << "[KDC] onBranchTreeHighlightRequired:"
                       << "lineIndex=" << lineIndex
                       << "ply=" << ply
                       << "state->currentLineIndex()=" << (m_state != nullptr ? m_state->currentLineIndex() : -1)
                       << "m_lastLineIndex=" << m_lastLineIndex;

    // 即時不一致検出
    if (m_state != nullptr && lineIndex != m_state->currentLineIndex()) {
        qWarning().noquote() << "[KDC] INCONSISTENCY DETECTED:"
                             << "Tree will highlight line" << lineIndex
                             << "but state says current line is" << m_state->currentLineIndex();
        qDebug().noquote() << getConsistencyReport();
    }

    // BranchTreeWidget がある場合はそちらを使用
    if (m_branchTreeWidget != nullptr) {
        m_branchTreeWidget->highlightNode(lineIndex, ply);
    }
    // EngineAnalysisTab がある場合はそちらを使用（通常はこちら）
    if (m_analysisTab != nullptr) {
        m_analysisTab->highlightBranchTreeAt(lineIndex, ply, /*centerOn=*/false);
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
    qDebug().noquote() << "[KDC] updateRecordView: CALLED";
    populateRecordModel();
    populateBranchMarks();

    // ★ 追加: ビューの明示的な更新を強制（モデル変更後にビューが更新されない問題の対策）
    if (m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        QTableView* view = m_recordPane->kifuView();
        view->viewport()->update();
        qDebug().noquote() << "[KDC] updateRecordView: forced view update, model rowCount=" << m_recordModel->rowCount();
    }
}

void KifuDisplayCoordinator::updateBranchTreeView()
{
    if (m_branchTreeWidget != nullptr) {
        m_branchTreeWidget->setTree(m_tree);
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
    if (m_recordPane != nullptr && m_state->currentNode() != nullptr) {
        QString comment = m_state->currentNode()->comment();
        m_recordPane->setBranchCommentText(comment);
    }
}

void KifuDisplayCoordinator::populateRecordModel()
{
    qDebug().noquote() << "[KDC] populateRecordModel: ENTER"
                       << "m_recordModel=" << (m_recordModel ? "yes" : "null")
                       << "m_recordModel ptr=" << static_cast<void*>(m_recordModel)
                       << "m_tree=" << (m_tree ? "yes" : "null");

    if (m_recordModel == nullptr || m_tree == nullptr) {
        qDebug().noquote() << "[KDC] populateRecordModel: EARLY RETURN (null model or tree)";
        return;
    }

    const int oldRowCount = m_recordModel->rowCount();
    m_recordModel->clearAllItems();
    qDebug().noquote() << "[KDC] populateRecordModel: cleared model, old rowCount=" << oldRowCount
                       << "new rowCount=" << m_recordModel->rowCount();

    // 現在のラインを取得
    int currentLineIndex = 0;
    if (m_state != nullptr) {
        currentLineIndex = m_state->currentLineIndex();
    }

    qDebug().noquote() << "[KDC] populateRecordModel: currentLineIndex=" << currentLineIndex;

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
    auto* startItem = new KifuDisplay(
        QStringLiteral("=== 開始局面 ==="),
        QStringLiteral("（１手 / 合計）"),
        this);
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
            this
        );
        m_recordModel->appendItem(item);
    }

    // ★ 重要: 棋譜モデルが実際に表示しているラインインデックスを記録
    m_lastModelLineIndex = currentLineIndex;

    qDebug().noquote() << "[KDC] populateRecordModel: DONE, final rowCount=" << m_recordModel->rowCount()
                       << "m_lastModelLineIndex=" << m_lastModelLineIndex;

    // ★ デバッグ: 棋譜欄の3手目の内容を出力（不一致検出用）
    if (m_recordModel->rowCount() > 3) {
        KifuDisplay* item = m_recordModel->item(3);
        if (item) {
            qDebug().noquote() << "[KDC] populateRecordModel DEBUG:"
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

    auto* startItem = new KifuDisplay(
        QStringLiteral("=== 開始局面 ==="),
        QStringLiteral("（１手 / 合計）"),
        this);
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
            this
        );
        m_recordModel->appendItem(item);
    }

    m_recordModel->setBranchPlyMarks(branchPlys);

    // ★ 重要: 棋譜モデルが実際に表示しているラインインデックスを記録
    // パスの最後のノードからラインインデックスを取得
    if (!path.isEmpty() && path.last() != nullptr) {
        m_lastModelLineIndex = path.last()->lineIndex();
    } else {
        m_lastModelLineIndex = 0;  // 空のパスは本譜として扱う
    }
    qDebug().noquote() << "[KDC] populateRecordModelFromPath: m_lastModelLineIndex=" << m_lastModelLineIndex;

    const int maxRow = m_recordModel->rowCount() - 1;
    const int rowToHighlight = qBound(0, highlightPly, maxRow);
    onRecordHighlightRequired(rowToHighlight);
}

void KifuDisplayCoordinator::onPositionChanged(int lineIndex, int ply, const QString& sfen)
{
    Q_UNUSED(sfen)
    Q_UNUSED(lineIndex)  // 現在は m_state->currentLineIndex() を使用

    qDebug().noquote() << "[KDC] onPositionChanged ENTER lineIndex=" << lineIndex << "ply=" << ply;

    if (m_tree == nullptr || m_state == nullptr) {
        qDebug().noquote() << "[KDC] onPositionChanged LEAVE (no tree or state)";
        return;
    }

    KifuBranchNode* targetNode = nullptr;
    QVector<BranchLine> lines = m_tree->allLines();

    // ply=0は開始局面（ルートノード）
    if (ply == 0) {
        targetNode = m_tree->root();
        qDebug().noquote() << "[KDC] ply=0: using root node";
    } else if (!lines.isEmpty()) {
        // ★ 現在表示中のラインからplyで探す
        // m_state->currentLineIndex() を使用して、棋譜欄が表示中のラインを特定
        int currentLine = m_state->currentLineIndex();
        if (currentLine < 0 || currentLine >= lines.size()) {
            currentLine = 0;  // フォールバック: 本譜
        }

        qDebug().noquote() << "[KDC] searching on line" << currentLine << "for ply" << ply;

        const BranchLine& line = lines.at(currentLine);
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->ply() == ply) {
                targetNode = node;
                qDebug().noquote() << "[KDC] found node on line" << currentLine
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
                        qDebug().noquote() << "[KDC] found node on line" << li
                                           << "(fallback) ply=" << ply;
                        break;
                    }
                }
                if (targetNode != nullptr) break;
            }
        }
    }

    if (targetNode == nullptr) {
        qDebug().noquote() << "[KDC] onPositionChanged LEAVE (target node not found for ply=" << ply << ")";
        return;
    }

    qDebug().noquote() << "[KDC] onPositionChanged: found node ply=" << targetNode->ply()
                       << "displayText=" << targetNode->displayText()
                       << "childCount=" << targetNode->childCount()
                       << "parent=" << (targetNode->parent() ? "yes" : "null");

    // ★ 分岐点での選択を記憶（次回のナビゲーションで正しい子を選択するため）
    // goToNodeと同様のロジックで、現在ノードが分岐の子である場合に記憶
    KifuBranchNode* nodeParent = targetNode->parent();
    if (nodeParent != nullptr && nodeParent->childCount() > 1) {
        for (int i = 0; i < nodeParent->childCount(); ++i) {
            if (nodeParent->childAt(i) == targetNode) {
                m_state->rememberLineSelection(nodeParent, i);
                qDebug().noquote() << "[KDC] onPositionChanged: remembered line selection at parent ply="
                                   << nodeParent->ply() << "index=" << i;
                break;
            }
        }
    }

    // ★ 状態を更新（次回の検索で正しいラインを使用するため）
    // この呼び出しはシグナルを発火するが、UI側での二重ナビゲーションは発生しない
    // （MainWindowが既にUIを管理しているため）
    m_state->setCurrentNode(targetNode);

    // 分岐候補を直接計算（m_state を経由しない）
    // ★ 新システム専用のメソッドを使用し、設定後はロックする
    if (m_branchModel != nullptr) {
        m_branchModel->resetBranchCandidates();

        // 分岐候補は「現在位置の親」の子（つまり現在位置の兄弟）を表示する
        // これにより、分岐点で選択可能な手が表示される
        KifuBranchNode* parentNode = targetNode->parent();

        qDebug().noquote() << "[KDC] parentNode=" << (parentNode ? "yes" : "null")
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

            qDebug().noquote() << "[KDC] onPositionChanged: set" << items.size()
                               << "branch candidates (siblings at ply" << targetNode->ply() << ")";
        } else {
            m_branchModel->setHasBackToMainRow(false);
            // ロックして外部からの変更を防ぐ
            m_branchModel->setLocked(true);

            // 分岐候補がない場合もビューを有効化（薄く表示されないように）
            if (m_recordPane != nullptr && m_recordPane->branchView() != nullptr) {
                m_recordPane->branchView()->setEnabled(true);
            }

            qDebug().noquote() << "[KDC] onPositionChanged: no branch candidates (parent has"
                               << (parentNode ? parentNode->childCount() : 0) << "children)";
        }
    }

    // 分岐ツリーのハイライト（グラフィカルのみ）
    // targetNodeが含まれるラインを探す
    // 現在のラインインデックスを優先して使用（分岐選択後の状態を維持）
    int highlightLineIndex = m_state->currentLineIndex();
    QVector<BranchLine> allLines = m_tree->allLines();

    // ★ 重要: m_lastLineIndex を同期しておく（新システムとの整合性を保つため）
    // 分岐ツリーから直接クリックした場合など、onNavigationCompleted を経由しない
    // 場合でもラインインデックスを追跡する
    const int oldLastLineIndex = m_lastLineIndex;
    m_lastLineIndex = highlightLineIndex;
    qDebug().noquote() << "[KDC] onPositionChanged: synced m_lastLineIndex from" << oldLastLineIndex
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

    qDebug().noquote() << "[KDC] onPositionChanged: highlighting branch tree at line="
                       << highlightLineIndex << "ply=" << ply;

    // BranchTreeWidget がある場合はそちらを使用
    if (m_branchTreeWidget != nullptr) {
        m_branchTreeWidget->highlightNode(highlightLineIndex, ply);
    }
    // EngineAnalysisTab がある場合はそちらも使用（通常はこちらが表示される）
    if (m_analysisTab != nullptr) {
        m_analysisTab->highlightBranchTreeAt(highlightLineIndex, ply, /*centerOn=*/false);
    }

    // ★ 重要: onRecordHighlightRequired() は呼び出さない
    // MainWindowが既にビューの選択を管理しているため

    // 位置変更完了時の一致性チェック
    if (!verifyDisplayConsistency()) {
        qWarning().noquote() << "[KDC] POST-NAVIGATION INCONSISTENCY:";
        qDebug().noquote() << getConsistencyReport();
    }

    qDebug().noquote() << "[KDC] onPositionChanged LEAVE";
}

bool KifuDisplayCoordinator::verifyDisplayConsistency() const
{
    // m_state がない場合は検証不可
    if (m_state == nullptr || m_tree == nullptr) {
        return true;  // 検証対象がないので true を返す
    }

    bool consistent = true;

    // 1. m_lastLineIndex と m_state->currentLineIndex() の一致を検証
    const int stateLineIndex = m_state->currentLineIndex();
    if (m_lastLineIndex != stateLineIndex) {
        consistent = false;
    }

    // 2. 棋譜欄の内容が期待するラインの内容と一致するかを検証
    if (m_recordModel != nullptr) {
        QVector<BranchLine> lines = m_tree->allLines();
        if (stateLineIndex >= 0 && stateLineIndex < lines.size()) {
            const BranchLine& line = lines.at(stateLineIndex);
            const int expectedRowCount = line.nodes.isEmpty() ? 1 : static_cast<int>(line.nodes.size());
            const int actualRowCount = m_recordModel->rowCount();

            // 行数が大きく異なる場合は不一致
            if (qAbs(expectedRowCount - actualRowCount) > 1) {
                consistent = false;
            }
        }
    }

    // 3. ツリーハイライトの期待値と実際の状態を比較
    // ハイライト要求後の lineIndex と ply が state と一致しているか
    if (m_expectedTreeLineIndex != stateLineIndex ||
        m_expectedTreePly != m_state->currentPly()) {
        // 注: これは状態遷移中に一時的に発生することがあるため、
        // ここでは警告ログを出すが consistent には影響させない
        // 本当の不一致は m_lastLineIndex と stateLineIndex の比較で検出
    }

    return consistent;
}

QString KifuDisplayCoordinator::getConsistencyReport() const
{
    QString report;
    QTextStream out(&report);

    out << "=== Consistency Report ===" << Qt::endl;

    // State 情報
    if (m_state != nullptr) {
        out << "State: lineIndex=" << m_state->currentLineIndex()
            << ", ply=" << m_state->currentPly()
            << ", preferredLineIndex=" << m_state->preferredLineIndex()
            << Qt::endl;
    } else {
        out << "State: null" << Qt::endl;
    }

    // Coordinator 情報
    out << "Coordinator: m_lastLineIndex=" << m_lastLineIndex << Qt::endl;

    // Model 情報
    if (m_recordModel != nullptr) {
        out << "Model: rowCount=" << m_recordModel->rowCount()
            << ", highlightRow=" << m_recordModel->currentHighlightRow()
            << Qt::endl;

        // 棋譜欄の内容を出力（最大5行）
        const int maxRows = qMin(m_recordModel->rowCount(), 5);
        for (int i = 1; i <= maxRows && i < m_recordModel->rowCount(); ++i) {
            KifuDisplay* item = m_recordModel->item(i);
            if (item != nullptr) {
                out << "  kifu[" << i << "]: " << item->currentMove() << Qt::endl;
            }
        }
    } else {
        out << "Model: null" << Qt::endl;
    }

    // ツリーハイライト期待値
    out << "ExpectedTreeHighlight: lineIndex=" << m_expectedTreeLineIndex
        << ", ply=" << m_expectedTreePly << Qt::endl;

    // 一致性判定結果
    const bool consistent = verifyDisplayConsistency();
    out << "Consistent: " << (consistent ? "YES" : "NO") << Qt::endl;

    return report;
}

// === ライブ対局セッションからのシグナル ===

void KifuDisplayCoordinator::onLiveGameMoveAdded(int ply, const QString& displayText)
{
    Q_UNUSED(ply);
    Q_UNUSED(displayText);

    const int liveLineIndex = (m_liveSession != nullptr) ? m_liveSession->currentLineIndex() : 0;
    KifuBranchNode* liveNode = (m_liveSession != nullptr) ? m_liveSession->liveNode() : nullptr;

    // EngineAnalysisTab の分岐ツリーを更新
    // addMoveQuiet() は treeChanged() を発火しないため、ここで明示的に更新する
    if (m_analysisTab != nullptr && m_tree != nullptr) {
        // KifuBranchTree から ResolvedRowLite 形式に変換
        QVector<EngineAnalysisTab::ResolvedRowLite> rows;
        QVector<BranchLine> lines = m_tree->allLines();

        for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);
            EngineAnalysisTab::ResolvedRowLite row;
            row.startPly = (line.branchPly > 0) ? line.branchPly : 1;

            // ★ 修正: branchPointから親行インデックスを正しく計算する
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

        m_analysisTab->setBranchTreeRows(rows);

        // ハイライト更新
        if (m_liveSession != nullptr) {
            const int totalPly = m_liveSession->totalPly();
            m_analysisTab->highlightBranchTreeAt(liveLineIndex, totalPly, false);
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
    qDebug().noquote() << "[KDC] onLiveGameSessionStarted: branchPoint="
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
        qDebug().noquote() << "[KDC] onLiveGameSessionStarted: set preferredLineIndex=" << branchLineIndex
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
