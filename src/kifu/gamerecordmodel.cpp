/// @file gamerecordmodel.cpp
/// @brief 棋譜データ中央管理クラスの実装

#include "gamerecordmodel.h"
#include "csaexporter.h"
#include "jkfexporter.h"
#include "ki2exporter.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifexporter.h"
#include "usenexporter.h"
#include "usiexporter.h"
#include "usimoveconverter.h"

#include <QDateTime>
#include "logcategories.h"

// ========================================
// コンストラクタ・デストラクタ
// ========================================

GameRecordModel::GameRecordModel(QObject* parent)
    : QObject(parent)
{
}

// ========================================
// 初期化・バインド
// ========================================

void GameRecordModel::bind(QList<KifDisplayItem>* liveDisp)
{
    m_liveDisp = liveDisp;

    qCDebug(lcKifu).noquote() << "bind:"
                              << " liveDisp=" << (liveDisp ? "valid" : "null");
}

void GameRecordModel::setBranchTree(KifuBranchTree* tree)
{
    m_branchTree = tree;
    qCDebug(lcKifu).noquote() << "setBranchTree:" << (tree ? "valid" : "null");
}

void GameRecordModel::setNavigationState(KifuNavigationState* state)
{
    m_navState = state;
    qCDebug(lcKifu).noquote() << "setNavigationState:" << (state ? "valid" : "null");
}

void GameRecordModel::initializeFromDisplayItems(const QList<KifDisplayItem>& disp, int rowCount)
{
    m_comments.clear();
    m_bookmarks.clear();
    const auto mainline = m_branchTree ? m_branchTree->mainLine() : QList<KifuBranchNode*>();
    if (!hasBranchTree()) {
        m_comments.resize(qMax(0, rowCount));
        m_bookmarks.resize(qMax(0, rowCount));
    }
    for (qsizetype i = 0; i < disp.size() && i < rowCount; ++i) {
        if (i < mainline.size()) {
            mainline[i]->setComment(disp[i].comment);
            mainline[i]->setBookmark(disp[i].bookmark);
        } else if (!hasBranchTree()) {
            m_comments[i] = disp[i].comment;
            m_bookmarks[i] = disp[i].bookmark;
        }
    }

    m_isDirty = false;

    qCDebug(lcKifu).noquote() << "initializeFromDisplayItems:"
                              << " disp.size=" << disp.size()
                              << " rowCount=" << rowCount
                              << " m_comments.size=" << m_comments.size();
}

void GameRecordModel::clear()
{
    m_comments.clear();
    m_bookmarks.clear();
    m_isDirty = false;

    qCDebug(lcKifu).noquote() << "clear";
}

// ========================================
// コメント操作
// ========================================

void GameRecordModel::setComment(int ply, const QString& text)
{
    if (ply < 0) return;
    auto* node = nodeForCurrentLine(ply);
    if (hasBranchTree() && !node) return;
    const QString old = comment(ply);
    if (node) {
        node->setComment(text);
    } else {
        if (m_comments.size() <= ply) m_comments.resize(ply + 1);
        m_comments[ply] = text;
    }
    // ライブ履歴は本譜の投影。分岐だけの編集で同じ手数の本譜を上書きしない。
    if (m_liveDisp && ply < m_liveDisp->size() && isMainlineNode(ply, node)) {
        (*m_liveDisp)[ply].comment = text;
    }
    if (old != text) {
        m_isDirty = true;
        emit commentChanged(ply, text);
    }
}

QString GameRecordModel::comment(int ply) const
{
    if (const auto* node = nodeForCurrentLine(ply)) return node->comment();
    if (!hasBranchTree() && ply >= 0 && ply < m_comments.size()) return m_comments[ply];
    return {};
}

void GameRecordModel::setBookmark(int ply, const QString& text)
{
    if (ply < 0) return;
    auto* node = nodeForCurrentLine(ply);
    if (hasBranchTree() && !node) return;
    const QString old = bookmark(ply);
    if (node) {
        node->setBookmark(text);
    } else {
        if (m_bookmarks.size() <= ply) m_bookmarks.resize(ply + 1);
        m_bookmarks[ply] = text;
    }
    if (m_liveDisp && ply < m_liveDisp->size() && isMainlineNode(ply, node)) {
        (*m_liveDisp)[ply].bookmark = text;
    }
    if (old != text) {
        m_isDirty = true;
        emit bookmarkChanged(ply, text);
    }
}

QString GameRecordModel::bookmark(int ply) const
{
    if (const auto* node = nodeForCurrentLine(ply)) return node->bookmark();
    if (!hasBranchTree() && ply >= 0 && ply < m_bookmarks.size()) return m_bookmarks[ply];
    return {};
}

bool GameRecordModel::hasBranchTree() const
{
    return m_branchTree && !m_branchTree->isEmpty();
}

bool GameRecordModel::isMainlineNode(int ply, const KifuBranchNode* node) const
{
    if (!hasBranchTree()) return true;
    const auto nodes = m_branchTree->mainLine();
    return ply >= 0 && ply < nodes.size() && nodes[ply] == node;
}

int GameRecordModel::activeRow() const
{
    // 新システム: KifuNavigationState から現在のラインインデックスを取得
    if (m_navState != nullptr) {
        return m_navState->currentLineIndex();
    }
    return 0;
}

// ========================================
// 内部ヘルパ：表示中の分岐ノードの取得
// ========================================

KifuBranchNode* GameRecordModel::nodeForCurrentLine(int ply) const
{
    if (!m_branchTree || m_branchTree->isEmpty() || ply < 0) return nullptr;
    const auto lines = m_branchTree->allLines();
    const int lineIndex = activeRow();
    if (lineIndex < 0 || lineIndex >= lines.size()) return nullptr;
    const auto& nodes = lines[lineIndex].nodes;
    return ply < nodes.size() ? nodes[ply] : nullptr;
}

// ========================================
// KIF形式出力（KifExporter へ委譲）
// ========================================

QStringList GameRecordModel::toKifLines(const ExportContext& ctx) const
{
    return KifExporter::exportLines(*this, ctx);
}

QList<KifDisplayItem> GameRecordModel::collectMainlineForExport() const
{
    QList<KifDisplayItem> result;

    // 優先0: KifuBranchTree から取得（新システム）
    if (m_branchTree != nullptr && !m_branchTree->isEmpty()) {
        // 本譜は常にlineIndex=0（ナビゲーション状態に依存しない）
        constexpr int lineIndex = 0;

        result = m_branchTree->displayItemsForLine(lineIndex);
        qCDebug(lcKifu).noquote() << "collectMainlineForExport: from BranchTree"
                                  << "lineIndex=" << lineIndex << "items=" << result.size();

        // 空でない結果が得られた場合は返す
        if (!result.isEmpty()) {
            return result;
        }
    }

    // フォールバック: liveDisp から取得
    if (m_liveDisp && !m_liveDisp->isEmpty()) {
        result = *m_liveDisp;
    }

    // ツリー未構築時の編集を反映
    for (qsizetype i = 0; i < result.size(); ++i) {
        if (i < m_comments.size() && !m_comments[i].isEmpty()) {
            result[i].comment = m_comments[i];
        }
        if (i < m_bookmarks.size() && !m_bookmarks[i].isEmpty()) {
            result[i].bookmark = m_bookmarks[i];
        }
    }

    return result;
}

QStringList GameRecordModel::collectMainlineUsiForExport() const
{
    if (!m_branchTree || m_branchTree->isEmpty()) return {};
    QStringList positions;
    const auto nodes = m_branchTree->mainLine();
    for (const KifuBranchNode* node : nodes) {
        if (node->isTerminal()) break;
        positions.append(node->sfen());
    }
    return UsiMoveConverter::fromSfenRecord(positions);
}

QString GameRecordModel::initialSfenForExport(const QString& fallback) const
{
    return m_branchTree && m_branchTree->root() ? m_branchTree->root()->sfen() : fallback;
}

QList<KifGameInfoItem> GameRecordModel::collectGameInfo(const ExportContext& ctx)
{
    return KifuExportMetadataBuilder::collect(ctx);
}

void GameRecordModel::resolvePlayerNames(const ExportContext& ctx, QString& black, QString& white)
{
    KifuExportMetadataBuilder::resolvePlayerNames(ctx, black, white);
}

// ========================================
// KI2形式出力（Ki2Exporter へ委譲）
// ========================================

QStringList GameRecordModel::toKi2Lines(const ExportContext& ctx) const
{
    return Ki2Exporter::exportLines(*this, ctx);
}

// ========================================
// CSA形式出力（CsaExporter へ委譲）
// ========================================

QStringList GameRecordModel::toCsaLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    return CsaExporter::exportLines(*this, ctx, usiMoves);
}

// ========================================
// JKF形式出力（JkfExporter へ委譲）
// ========================================

QStringList GameRecordModel::toJkfLines(const ExportContext& ctx) const
{
    return JkfExporter::exportLines(*this, ctx);
}


// ========================================
// USEN形式出力（UsenExporter へ委譲）
// ========================================

QStringList GameRecordModel::toUsenLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    return UsenExporter::exportLines(*this, ctx, usiMoves);
}

// ========================================
// USI形式出力（UsiExporter へ委譲）
// ========================================

QStringList GameRecordModel::toUsiLines(const ExportContext& ctx, const QStringList& usiMoves) const
{
    return UsiExporter::exportLines(*this, ctx, usiMoves);
}
