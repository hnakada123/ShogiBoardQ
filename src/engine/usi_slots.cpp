/// @file usi_slots.cpp
/// @brief Usi クラスのモデル更新スロット実装

#include "usi.h"
#include "usimatchhandler.h"
#include "shogiinforecord.h"

namespace {
constexpr int kMaxThinkingRows = 500;

[[nodiscard]] bool isSameThinkingPayload(const ShogiInfoRecord* record,
                                         const ThinkingInfoRow& row,
                                         const QString& lastUsiMove)
{
    if (!record) {
        return false;
    }

    // 頻繁に変化するフィールドを先に比較して早期リターン
    return record->scoreCp() == row.scoreCp
        && record->multipv() == row.multipv
        && record->depth() == row.depth
        && record->pv() == row.pvKanji
        && record->time() == row.time
        && record->nodes() == row.nodes
        && record->score() == row.score
        && record->usiPv() == row.usiPv
        && record->enginePv() == row.enginePv
        && record->baseSfen() == row.baseSfen
        && record->lastUsiMove() == lastUsiMove;
}

ShogiInfoRecord* makeRecord(const ThinkingInfoRow& row, const QString& lastUsiMove)
{
    auto* record = new ShogiInfoRecord(row.time, row.depth, row.nodes, row.score, row.pvKanji);
    record->setUsiPv(row.usiPv);
    record->setEnginePv(row.enginePv);
    record->setBaseSfen(row.baseSfen);
    record->setLastUsiMove(lastUsiMove);
    record->setMultipv(row.multipv);
    record->setScoreCp(row.scoreCp);
    return record;
}
} // anonymous namespace

// ============================================================
// モデル更新スロット実装（Presenter → CommLogModel/ThinkingModel転送）
// ============================================================

void Usi::onSearchedMoveUpdated(const QString& move)
{
    if (m_commLogModel) {
        m_commLogModel->setSearchedMove(move);
    }
}

void Usi::onSearchDepthUpdated(const QString& depth)
{
    if (m_commLogModel) {
        m_commLogModel->setSearchDepth(depth);
    }
}

void Usi::onNodeCountUpdated(const QString& nodes)
{
    if (m_commLogModel) {
        m_commLogModel->setNodeCount(nodes);
    }
}

void Usi::onNpsUpdated(const QString& nps)
{
    if (m_commLogModel) {
        m_commLogModel->setNodesPerSecond(nps);
    }
}

void Usi::onHashUsageUpdated(const QString& hashUsage)
{
    if (m_commLogModel) {
        m_commLogModel->setHashUsage(hashUsage);
    }
}

void Usi::onCommLogAppended(const QString& log)
{
    if (m_commLogModel) {
        m_commLogModel->appendUsiCommLog(log);
    }
}

void Usi::onConsiderationStopTimeout()
{
    if (m_processManager && m_processManager->isRunning()) {
        qCDebug(lcEngine) << "検討停止タイマー発火";
        m_protocolHandler->sendStop();
    }
    resetAnalysisStopTimer();
}

void Usi::onClearThinkingInfoRequested()
{
    qCDebug(lcEngine) << "思考情報クリア要求";
    if (m_thinkingModel) {
        qCDebug(lcEngine) << "思考モデルクリア実行";
        m_thinkingModel->clearAllItems();
    }
}

void Usi::onThinkingInfoUpdated(const ThinkingInfoRow& row)
{
    // 処理フロー:
    // 1. ShogiInfoRecordを生成して思考タブへ追記（先頭に追加）
    // 2. 検討タブへ追記（MultiPVモードで行を更新/挿入）
    // 3. 外部へシグナルで通知
    const QString lastUsiMove = m_matchHandler->lastUsiMove();
    qCDebug(lcEngine) << "思考情報更新: m_lastUsiMove=" << lastUsiMove
                      << "baseSfen=" << row.baseSfen.left(50)
                      << "multipv=" << row.multipv << "scoreCp=" << row.scoreCp;

    // 思考タブへ追記（通常モード: 先頭に追加）
    // 読み筋（PV）が空の行は表示しない（詰み探索の中間結果など）
    if (m_thinkingModel && !row.pvKanji.isEmpty()) {
        const ShogiInfoRecord* topRecord =
            (m_thinkingModel->rowCount() > 0) ? m_thinkingModel->recordAt(0) : nullptr;
        if (!isSameThinkingPayload(topRecord, row, lastUsiMove)) {
            m_thinkingModel->prependItem(makeRecord(row, lastUsiMove));
            m_thinkingModel->trimToMaxRows(kMaxThinkingRows);
        }
    }

    // 検討タブへ追記（MultiPVモード: multipv値に基づいて行を更新/挿入）
    // PV のない info（停止時の time/nodes だけの行など）は multipv 1 扱いになり、
    // 最善手の行を空の読み筋で上書きしてしまうため反映しない
    if (m_considerationModel && !row.pvKanji.isEmpty()) {
        const auto existingRow = m_considerationModel->findRowByMultipv(row.multipv);
        const ShogiInfoRecord* existingMultipvRecord = existingRow.has_value()
            ? m_considerationModel->recordAt(*existingRow) : nullptr;

        if (!isSameThinkingPayload(existingMultipvRecord, row, lastUsiMove)) {
            m_considerationModel->updateByMultipv(makeRecord(row, lastUsiMove), m_considerationMaxMultiPV);
        }
    }

    // 外部への通知
    emit thinkingInfoUpdated(row);
}
