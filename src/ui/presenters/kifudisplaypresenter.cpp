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
#include <QStringList>
#include <QTextStream>




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

void KifuDisplayPresenter::presentLiveNode(KifuBranchNode* node)
{
    if (!node || !m_refs.tree || !m_refs.recordModel) return;
    auto* model = m_refs.recordModel;
    const int line = m_refs.state ? m_refs.state->currentLineIndex() : 0;
    // 通常の末尾追加は一行だけ作る。分岐の変更や指し直しは経路から照合する。
    if (node->ply() == model->rowCount() && m_lastModelLineIndex == line
        && node->parent() && !node->parent()->hasBranch()) {
        const QString label = QString::number(node->ply()).rightJustified(4, QLatin1Char(' '))
                              + QLatin1Char(' ') + node->displayText();
        const auto row = moveRow(node, label);
        auto* item = new KifuDisplay(row.move, row.time, row.comment, row.bookmark, model);
        item->beforeSfen = row.beforeSfen;
        item->usiMove = row.usiMove;
        model->appendItem(item);
    } else {
        populateRecordModelFromPath(m_refs.tree->pathToNode(node), node->ply());
    }
    m_lastModelLineIndex = line;
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
// 棋譜表示プレゼンタ - 表示一致性の検証とレポート
// ============================================================

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

// ============================================================
// 一致性検証
// ============================================================

KifuDisplayPresenter::DisplaySnapshot KifuDisplayPresenter::captureDisplaySnapshot(const TrackingState& tracking) const
{
    DisplaySnapshot snapshot;
    snapshot.trackedLineIndex = tracking.lastLineIndex;
    snapshot.liveSessionActive = tracking.liveSessionActive;
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
        const QList<BranchLine> lines = m_refs.tree->allLines();
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

        const QList<BranchLine> lines = m_refs.tree->allLines();
        if (snapshot.stateLineIndex >= 0 && snapshot.stateLineIndex < lines.size()) {
            const BranchLine& line = lines.at(snapshot.stateLineIndex);
            // 対局中の棋譜欄は「開始局面〜現在の手」だけを表示し、同じライン上にある
            // 既存の続き（指し直しで再利用した手の先）は隠す。そのため対局中は
            // ラインの長さではなく現在手数+1 を期待値にする。
            const int expectedRowCount = snapshot.liveSessionActive
                ? snapshot.statePly + 1
                : (line.nodes.isEmpty() ? 1 : static_cast<int>(line.nodes.size()));
            if (qAbs(expectedRowCount - snapshot.modelRowCount) > 1) {
                return fail(QStringLiteral("棋譜欄行数不一致: expected~=%1 actual=%2")
                            .arg(expectedRowCount)
                            .arg(snapshot.modelRowCount));
            }
        }

        if (snapshot.statePly == 0) {
            if (snapshot.displayedMoveAtPly != QObject::tr("=== 開始局面 ===")) {
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
                const QList<BranchLine> allLines = m_refs.tree->allLines();
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

QString KifuDisplayPresenter::consistencyReport(const TrackingState& tracking) const
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
        << ", liveSession=" << (snapshot.liveSessionActive ? "true" : "false")
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
