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

    QList<BranchLine> lines = m_refs.tree->allLines();
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

int KifuDisplayPresenter::populateRecordModelFromPath(const QList<KifuBranchNode*>& path, int highlightPly)
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
        const auto treeLineIndex = m_refs.tree->findLineIndexForNode(path.last());
        qCDebug(lcUi).noquote() << "populateRecordModelFromPath: DEBUG"
                           << "nodeLineIndex=" << nodeLineIndex
                           << "treeLineIndex=" << treeLineIndex.value_or(-1)
                           << "pathSize=" << path.size()
                           << "lastNodePly=" << path.last()->ply();
        m_lastModelLineIndex = treeLineIndex.value_or(0);
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

    QList<BranchLine> lines = m_refs.tree->allLines();
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

QList<BranchTreeManager::ResolvedRowLite> KifuDisplayPresenter::buildBranchTreeRows(KifuBranchTree* tree)
{
    QList<BranchTreeManager::ResolvedRowLite> rows;
    if (tree == nullptr) {
        return rows;
    }

    const QList<BranchLine> lines = tree->allLines();

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
