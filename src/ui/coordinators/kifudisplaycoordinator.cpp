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

#include <QTableView>
#include <QModelIndex>
#include <QDebug>

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
                           << "m_lastLineIndex=" << m_lastLineIndex;
        if (newLineIndex != m_lastLineIndex) {
            qDebug().noquote() << "[KDC] onNavigationCompleted: line changed from"
                               << m_lastLineIndex << "to" << newLineIndex;
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
}

void KifuDisplayCoordinator::onBoardUpdateRequired(const QString& sfen)
{
    // 注意: 新システムでは boardWithHighlightsRequired を使用するため、
    // このシグナルは旧システムとの互換性のために残している
    Q_UNUSED(sfen);
    // emit boardSfenChanged(sfen);  // 新システムでは onNavigationCompleted で処理
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

    m_branchModel->clearBranchCandidatesByNewSystem();

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
    m_branchModel->setBranchCandidatesByNewSystem(items);

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

    qDebug().noquote() << "[KDC] populateRecordModel: DONE, final rowCount=" << m_recordModel->rowCount();
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

void KifuDisplayCoordinator::onLegacyPositionChanged(int row, int ply, const QString& sfen)
{
    Q_UNUSED(sfen)  // SFENは旧システムでは本譜のみなので使用しない

    qDebug().noquote() << "[KDC] onLegacyPositionChanged ENTER row=" << row << "ply=" << ply;

    if (m_tree == nullptr || m_state == nullptr) {
        qDebug().noquote() << "[KDC] onLegacyPositionChanged LEAVE (no tree or state)";
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
        qDebug().noquote() << "[KDC] onLegacyPositionChanged LEAVE (target node not found for ply=" << ply << ")";
        return;
    }

    qDebug().noquote() << "[KDC] onLegacyPositionChanged: found node ply=" << targetNode->ply()
                       << "displayText=" << targetNode->displayText()
                       << "childCount=" << targetNode->childCount()
                       << "parent=" << (targetNode->parent() ? "yes" : "null");

    // ★ 状態を更新（次回の検索で正しいラインを使用するため）
    // この呼び出しはシグナルを発火するが、UI側での二重ナビゲーションは発生しない
    // （旧システムが既にUIを管理しているため）
    m_state->setCurrentNode(targetNode);

    // 分岐候補を直接計算（m_state を経由しない）
    // ★ 新システム専用のメソッドを使用し、設定後はロックする
    if (m_branchModel != nullptr) {
        m_branchModel->clearBranchCandidatesByNewSystem();

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
            m_branchModel->setBranchCandidatesByNewSystem(items);

            // 本譜にいるかどうかを確認（targetNode が本譜上かどうか）
            bool isOnMainLine = targetNode->isMainLine();
            m_branchModel->setHasBackToMainRow(!isOnMainLine);

            // 分岐候補ビューを有効化（旧システムが無効化している可能性があるため）
            if (m_recordPane != nullptr && m_recordPane->branchView() != nullptr) {
                m_recordPane->branchView()->setEnabled(true);
            }

            qDebug().noquote() << "[KDC] onLegacyPositionChanged: set" << items.size()
                               << "branch candidates (siblings at ply" << targetNode->ply() << ")";
        } else {
            m_branchModel->setHasBackToMainRow(false);
            // ロックして旧システムからの変更を防ぐ
            m_branchModel->setLockedByNewSystem(true);

            // 分岐候補がない場合もビューを有効化（薄く表示されないように）
            if (m_recordPane != nullptr && m_recordPane->branchView() != nullptr) {
                m_recordPane->branchView()->setEnabled(true);
            }

            qDebug().noquote() << "[KDC] onLegacyPositionChanged: no branch candidates (parent has"
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
    qDebug().noquote() << "[KDC] onLegacyPositionChanged: synced m_lastLineIndex from" << oldLastLineIndex
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

    qDebug().noquote() << "[KDC] onLegacyPositionChanged: highlighting branch tree at line="
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
    // 旧システムが既にビューの選択を管理しているため

    qDebug().noquote() << "[KDC] onLegacyPositionChanged LEAVE";
}
