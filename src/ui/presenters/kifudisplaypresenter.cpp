/// @file kifudisplaypresenter.cpp
/// @brief 棋譜表示データ構築・一致性検証プレゼンタの実装

#include "kifudisplaypresenter.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifudisplay.h"
#include "kifdisplayitem.h"
#include "branchtreemanager.h"

#include "logcategories.h"

#include <QStringList>
#include <QTextStream>


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

KifuDisplayPresenter::KifuDisplayPresenter() = default;

void KifuDisplayPresenter::updateRefs(const Refs& refs)
{
    m_refs = refs;
}

// ============================================================
// 棋譜欄モデル構築
// ============================================================

void KifuDisplayPresenter::populateRecordModel()
{
    qCDebug(lcUi).noquote() << "populateRecordModel: ENTER"
                       << "m_recordModel=" << (m_refs.recordModel ? "yes" : "null")
                       << "m_recordModel ptr=" << static_cast<void*>(m_refs.recordModel)
                       << "m_tree=" << (m_refs.tree ? "yes" : "null");

    if (m_refs.recordModel == nullptr || m_refs.tree == nullptr) {
        qCDebug(lcUi).noquote() << "populateRecordModel: EARLY RETURN (null model or tree)";
        return;
    }

    const int oldRowCount = m_refs.recordModel->rowCount();
    m_refs.recordModel->clearAllItems();
    qCDebug(lcUi).noquote() << "populateRecordModel: cleared model, old rowCount=" << oldRowCount
                       << "new rowCount=" << m_refs.recordModel->rowCount();

    // 現在のラインを取得
    int currentLineIndex = 0;
    if (m_refs.state != nullptr) {
        currentLineIndex = m_refs.state->currentLineIndex();
    }

    qCDebug(lcUi).noquote() << "populateRecordModel: currentLineIndex=" << currentLineIndex;

    QVector<BranchLine> lines = m_refs.tree->allLines();
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
        QObject::tr("=== 開始局面 ==="),
        QObject::tr("（１手 / 合計）"),
        QString(),
        openingBookmark,
        m_refs.recordModel);
    m_refs.recordModel->appendItem(startItem);

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
            m_refs.recordModel
        );
        m_refs.recordModel->appendItem(item);
    }

    // 重要: 棋譜モデルが実際に表示しているラインインデックスを記録
    m_lastModelLineIndex = currentLineIndex;

    qCDebug(lcUi).noquote() << "populateRecordModel: DONE, final rowCount=" << m_refs.recordModel->rowCount()
                       << "m_lastModelLineIndex=" << m_lastModelLineIndex;

    // デバッグ: 棋譜欄の3手目の内容を出力（不一致検出用）
    if (m_refs.recordModel->rowCount() > 3) {
        KifuDisplay* item = m_refs.recordModel->item(3);
        if (item != nullptr) {
            qCDebug(lcUi).noquote() << "populateRecordModel DEBUG:"
                               << "currentLineIndex=" << currentLineIndex
                               << "ply3_move=" << item->currentMove();
        }
    }
}

int KifuDisplayPresenter::populateRecordModelFromPath(const QVector<KifuBranchNode*>& path, int highlightPly)
{
    if (m_refs.recordModel == nullptr) {
        return 0;
    }

    m_refs.recordModel->clearAllItems();

    // 開始局面のしおりを取得（ply==0 のノード）
    QString openingBookmark;
    for (KifuBranchNode* node : std::as_const(path)) {
        if (node != nullptr && node->ply() == 0) {
            openingBookmark = node->bookmark();
            break;
        }
    }
    auto* startItem = new KifuDisplay(
        QObject::tr("=== 開始局面 ==="),
        QObject::tr("（１手 / 合計）"),
        QString(),
        openingBookmark,
        m_refs.recordModel);
    m_refs.recordModel->appendItem(startItem);

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
            m_refs.recordModel
        );
        m_refs.recordModel->appendItem(item);
    }

    m_refs.recordModel->setBranchPlyMarks(branchPlys);

    // 重要: 棋譜モデルが実際に表示しているラインインデックスを記録
    if (!path.isEmpty() && path.last() != nullptr && m_refs.tree != nullptr) {
        const int nodeLineIndex = path.last()->lineIndex();
        const int treeLineIndex = m_refs.tree->findLineIndexForNode(path.last());
        qCDebug(lcUi).noquote() << "populateRecordModelFromPath: DEBUG"
                           << "nodeLineIndex=" << nodeLineIndex
                           << "treeLineIndex=" << treeLineIndex
                           << "pathSize=" << path.size()
                           << "lastNodePly=" << path.last()->ply();
        m_lastModelLineIndex = (treeLineIndex >= 0) ? treeLineIndex : 0;
    } else {
        m_lastModelLineIndex = 0;
    }
    qCDebug(lcUi).noquote() << "populateRecordModelFromPath: m_lastModelLineIndex=" << m_lastModelLineIndex;

    const int maxRow = m_refs.recordModel->rowCount() - 1;
    return qBound(0, highlightPly, maxRow);
}

void KifuDisplayPresenter::populateBranchMarks()
{
    if (m_refs.recordModel == nullptr || m_refs.tree == nullptr) {
        return;
    }

    QSet<int> branchPlys;

    // 現在表示中のラインで分岐がある手を収集
    int currentLineIndex = 0;
    if (m_refs.state != nullptr) {
        currentLineIndex = m_refs.state->currentLineIndex();
    }

    QVector<BranchLine> lines = m_refs.tree->allLines();
    if (currentLineIndex >= 0 && currentLineIndex < lines.size()) {
        const BranchLine& line = lines.at(currentLineIndex);
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->parent() != nullptr && node->parent()->hasBranch()) {
                branchPlys.insert(node->ply());
            }
        }
    }

    m_refs.recordModel->setBranchPlyMarks(branchPlys);
}

// ============================================================
// 分岐ツリー行データ構築
// ============================================================

QVector<BranchTreeManager::ResolvedRowLite> KifuDisplayPresenter::buildBranchTreeRows(KifuBranchTree* tree)
{
    QVector<BranchTreeManager::ResolvedRowLite> rows;
    if (tree == nullptr) {
        return rows;
    }

    const QVector<BranchLine> lines = tree->allLines();

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

    return rows;
}

// ============================================================
// 一致性検証
// ============================================================

KifuDisplayPresenter::DisplaySnapshot KifuDisplayPresenter::captureDisplaySnapshot(const TrackingState& tracking) const
{
    DisplaySnapshot snapshot;
    snapshot.trackedLineIndex = tracking.lastLineIndex;
    snapshot.modelLineIndex = m_lastModelLineIndex;
    snapshot.expectedTreeLineIndex = tracking.expectedTreeLineIndex;
    snapshot.expectedTreePly = tracking.expectedTreePly;

    if (m_refs.state != nullptr) {
        snapshot.stateLineIndex = m_refs.state->currentLineIndex();
        snapshot.statePly = m_refs.state->currentPly();
        snapshot.stateOnMainLine = m_refs.state->isOnMainLine();
        snapshot.stateSfen = m_refs.state->currentSfen();
        snapshot.stateSfenNormalized = normalizeSfenForCompare(snapshot.stateSfen);
    }

    if (m_refs.recordModel != nullptr) {
        snapshot.modelRowCount = m_refs.recordModel->rowCount();
        snapshot.modelHighlightRow = m_refs.recordModel->currentHighlightRow();

        if (snapshot.statePly >= 0 && snapshot.statePly < m_refs.recordModel->rowCount()) {
            if (KifuDisplay* item = m_refs.recordModel->item(snapshot.statePly)) {
                snapshot.displayedMoveAtPly = item->currentMove();
            }
        }
    }

    if (m_refs.tree != nullptr && snapshot.stateLineIndex >= 0) {
        const QVector<BranchLine> lines = m_refs.tree->allLines();
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

    if (m_refs.branchTreeManager != nullptr && m_refs.branchTreeManager->hasHighlightedNode()) {
        snapshot.treeHighlightLineIndex = m_refs.branchTreeManager->lastHighlightedRow();
        snapshot.treeHighlightPly = m_refs.branchTreeManager->lastHighlightedPly();
    }

    if (m_refs.branchModel != nullptr) {
        snapshot.branchCandidateCount = m_refs.branchModel->branchCandidateCount();
        snapshot.hasBackToMainRow = m_refs.branchModel->hasBackToMainRow();
    }

    if (m_refs.boardSfenProvider) {
        snapshot.boardSfen = m_refs.boardSfenProvider();
        snapshot.boardSfenNormalized = normalizeSfenForCompare(snapshot.boardSfen);
    }

    return snapshot;
}

bool KifuDisplayPresenter::verifyDisplayConsistencyDetailed(const TrackingState& tracking, QString* reason) const
{
    if (reason != nullptr) {
        reason->clear();
    }

    // m_state がない場合は検証不可
    if (m_refs.state == nullptr || m_refs.tree == nullptr) {
        return true;
    }

    const DisplaySnapshot snapshot = captureDisplaySnapshot(tracking);
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
    if (m_refs.recordModel != nullptr) {
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

        const QVector<BranchLine> lines = m_refs.tree->allLines();
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
    if (m_refs.branchTreeManager != nullptr && snapshot.treeHighlightLineIndex >= 0 && snapshot.treeHighlightPly >= 0) {
        if (snapshot.treeHighlightLineIndex != snapshot.stateLineIndex
            || snapshot.treeHighlightPly != snapshot.statePly) {
            // 共有ノードチェック: 分岐点より前のノードは複数ラインで共有されるため、
            // ツリーウィジェット上ではライン0（または親ライン）にハイライトされる。
            bool isSharedNode = false;
            if (snapshot.treeHighlightPly == snapshot.statePly && m_refs.tree != nullptr) {
                const QVector<BranchLine> allLines = m_refs.tree->allLines();
                if (snapshot.treeHighlightLineIndex < allLines.size()
                    && snapshot.stateLineIndex < allLines.size()) {
                    auto findAtPly = [](const BranchLine& ln, int p) -> KifuBranchNode* {
                        for (KifuBranchNode* n : std::as_const(ln.nodes)) {
                            if (n->ply() == p) return n;
                        }
                        return nullptr;
                    };
                    KifuBranchNode* treeNode = findAtPly(allLines.at(snapshot.treeHighlightLineIndex),
                                                         snapshot.treeHighlightPly);
                    KifuBranchNode* stateNode = findAtPly(allLines.at(snapshot.stateLineIndex),
                                                          snapshot.statePly);
                    isSharedNode = (treeNode != nullptr && treeNode == stateNode);
                }
            }
            if (!isSharedNode) {
                return fail(QStringLiteral("分岐ツリーハイライト不一致: tree=(%1,%2) state=(%3,%4)")
                            .arg(snapshot.treeHighlightLineIndex)
                            .arg(snapshot.treeHighlightPly)
                            .arg(snapshot.stateLineIndex)
                            .arg(snapshot.statePly));
            }
        }
    }

    // 4. 分岐候補欄
    if (m_refs.branchModel != nullptr && m_refs.state->currentNode() != nullptr) {
        int expectedCandidateCount = 0;
        if (KifuBranchNode* parent = m_refs.state->currentNode()->parent();
            parent != nullptr && parent->childCount() > 1) {
            expectedCandidateCount = parent->childCount();
            for (int i = 0; i < expectedCandidateCount; ++i) {
                if (m_refs.branchModel->labelAt(i) != parent->childAt(i)->displayText()) {
                    return fail(QStringLiteral("分岐候補指し手不一致: row=%1 expected=[%2] actual=[%3]")
                                .arg(i)
                                .arg(parent->childAt(i)->displayText(), m_refs.branchModel->labelAt(i)));
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

bool KifuDisplayPresenter::verifyDisplayConsistency(const TrackingState& tracking) const
{
    return verifyDisplayConsistencyDetailed(tracking, nullptr);
}

QString KifuDisplayPresenter::getConsistencyReport(const TrackingState& tracking) const
{
    QString report;
    QTextStream out(&report);

    out << "=== Consistency Report ===" << Qt::endl;

    QString reason;
    const DisplaySnapshot snapshot = captureDisplaySnapshot(tracking);
    const bool consistent = verifyDisplayConsistencyDetailed(tracking, &reason);

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
    if (m_refs.recordModel != nullptr) {
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
