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
#include "kifupresentation.h"
#include "branchtreemanager.h"

#include "logcategories.h"




namespace {
/// 棋譜欄の1行分の内容
struct RecordRow {
    QString move;
    QString time;
    QString comment;
    QString bookmark;
    QString beforeSfen;
    QString usiMove;
};

bool sameRow(const KifuDisplay* item, const RecordRow& row)
{
    return item != nullptr && item->currentMove() == row.move && item->timeSpent() == row.time
        && item->comment() == row.comment && item->bookmark() == row.bookmark
        && item->beforeSfen == row.beforeSfen && item->usiMove == row.usiMove;
}

RecordRow startRow(const QString& comment, const QString& bookmark)
{
    return {QObject::tr("=== 開始局面 ==="), QObject::tr("（１手 / 合計）"), comment, bookmark, {}, {}};
}

RecordRow moveRow(const KifuBranchNode* node, QString displayText)
{
    return {std::move(displayText), node->timeText(), node->comment(), node->bookmark(),
            node->parent() ? node->parent()->sfen() : QString(),
            node->isTerminal() ? QString() : KifuPresentation::usiMove(node->move())};
}

/// 棋譜欄の行を rows に揃える。今の行の後ろに手が増えただけなら増えた行だけを足し、
/// それ以外は全行を作り直す。1手指すたびに全行を作り直すと、棋譜欄が全行を並べ直して
/// 幅を測り直す。行を削る差分更新はしない（現在行が消えると選択の変更通知が出て、
/// 棋譜の移動が起きるため）。全行を作り直したときは true を返す。
bool setRecordRows(KifuRecordListModel* model, const QList<RecordRow>& rows)
{
    const int current = model->rowCount();
    bool extends = current > 0 && current <= rows.size();
    for (int i = 0; extends && i < current; ++i) extends = sameRow(model->item(i), rows.at(i));
    if (!extends) model->clearAllItems();

    // 行は最後にまとめて追加する。1行ずつ追加すると、そのたびに棋譜欄が
    // 全行の文字幅を測り直すため、手数の2乗に比例して遅くなる。
    QList<KifuDisplay*> items;
    items.reserve(rows.size());
    for (qsizetype i = extends ? current : 0; i < rows.size(); ++i) {
        const RecordRow& row = rows.at(i);
        auto* item = new KifuDisplay(row.move, row.time, row.comment, row.bookmark, model);
        item->beforeSfen = row.beforeSfen;
        item->usiMove = row.usiMove;
        items.append(item);
    }
    model->appendItems(items);
    return !extends;
}
} // namespace

KifuDisplayPresenter::KifuDisplayPresenter() = default;

void KifuDisplayPresenter::updateRefs(const Refs& refs)
{
    m_refs = refs;
}

// ============================================================
// 棋譜欄モデル構築
// ============================================================

bool KifuDisplayPresenter::populateRecordModel()
{
    qCDebug(lcUi).noquote() << "populateRecordModel: ENTER"
                       << "m_recordModel=" << (m_refs.recordModel ? "yes" : "null")
                       << "m_recordModel ptr=" << static_cast<void*>(m_refs.recordModel)
                       << "m_tree=" << (m_refs.tree ? "yes" : "null");

    if (m_refs.recordModel == nullptr || m_refs.tree == nullptr) {
        qCDebug(lcUi).noquote() << "populateRecordModel: EARLY RETURN (null model or tree)";
        return false;
    }

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

    // 表示するラインが無い場合は空にして終了
    if (lines.isEmpty()) {
        m_refs.recordModel->clearAllItems();
        return true;
    }

    const BranchLine& line = lines.at(currentLineIndex);

    // 開始局面（ply=0）を追加
    // ルートノード（ply==0）のコメントとしおりを取得
    QString openingComment;
    QString openingBookmark;
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == 0) {
            openingComment = node->comment();
            openingBookmark = node->bookmark();
            break;
        }
    }
    QList<RecordRow> rows{startRow(openingComment, openingBookmark)};
    rows.reserve(line.nodes.size() + 1);

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

        rows.append(moveRow(node, displayText));
    }
    const bool rebuilt = setRecordRows(m_refs.recordModel, rows);

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
    return rebuilt;
}

int KifuDisplayPresenter::populateRecordModelFromPath(const QList<KifuBranchNode*>& path, int highlightPly)
{
    if (m_refs.recordModel == nullptr) {
        return 0;
    }

    // 開始局面のコメントとしおりを取得（ply==0 のノード）
    QString openingComment;
    QString openingBookmark;
    for (KifuBranchNode* node : std::as_const(path)) {
        if (node != nullptr && node->ply() == 0) {
            openingComment = node->comment();
            openingBookmark = node->bookmark();
            break;
        }
    }
    QList<RecordRow> rows{startRow(openingComment, openingBookmark)};
    rows.reserve(path.size() + 1);

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

        rows.append(moveRow(node, displayText));
    }
    setRecordRows(m_refs.recordModel, rows);

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
        // 変化の先頭の手（折りたたみ状態をツリーの再構築後も保つためのキー）
        if (lineIdx > 0 && row.startPly < line.nodes.size()) {
            row.headNodeId = line.nodes.at(row.startPly)->nodeId();
        }

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
                item.beforeSfen = node->parent() ? node->parent()->sfen() : QString();
                item.usiMove = node->isTerminal() ? QString() : KifuPresentation::usiMove(node->move());
                item.terminal = node->isTerminal();
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
