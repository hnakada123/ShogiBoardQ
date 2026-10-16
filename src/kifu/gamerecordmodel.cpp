/// @file gamerecordmodel.cpp
/// @brief 棋譜データ中央管理クラスの実装

#include "gamerecordmodel.h"
#include "csaexporter.h"
#include "jkfexporter.h"
#include "ki2exporter.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifexporter.h"

#include "usenexporter.h"
#include "usiexporter.h"

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
// 初期化
// ========================================

void GameRecordModel::setBranchTree(KifuBranchTree* tree)
{
    m_branchTree = tree;
    qCDebug(lcKifu).noquote() << "setBranchTree:" << (tree ? "valid" : "null");
}

void GameRecordModel::setComment(KifuBranchNode* node, const QString& text)
{
    if (!node || node->comment() == text) return;
    node->record().comment = text;
    m_isDirty = true;
    emit commentChanged(node->nodeId(), text);
}

void GameRecordModel::setBookmark(KifuBranchNode* node, const QString& text)
{
    if (!node || node->bookmark() == text) return;
    node->record().bookmark = text;
    m_isDirty = true;
    emit bookmarkChanged(node->nodeId(), text);
}

// ========================================
// KIF形式出力（KifExporter へ委譲）
// ========================================

QStringList GameRecordModel::toKifLines(const ExportContext& ctx) const
{
    return KifExporter::exportLines(*this, ctx);
}

RecordLine GameRecordModel::collectMainlineForExport() const
{
    return m_branchTree ? m_branchTree->exportLine(0) : RecordLine{};
}

QStringList GameRecordModel::collectMainlineUsiForExport() const
{
    if (!m_branchTree || m_branchTree->isEmpty()) return {};
    return m_branchTree->mainRecordLine().usiMoves();
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

QStringList GameRecordModel::toCsaLines(const ExportContext& ctx) const
{
    return CsaExporter::exportLines(*this, ctx);
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

QStringList GameRecordModel::toUsenLines(const ExportContext& ctx) const
{
    return UsenExporter::exportLines(*this, ctx);
}

// ========================================
// USI形式出力（UsiExporter へ委譲）
// ========================================

QStringList GameRecordModel::toUsiLines(const ExportContext& ctx) const
{
    return UsiExporter::exportLines(*this, ctx);
}
