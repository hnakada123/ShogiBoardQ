/// @file usi_slots.cpp
/// @brief Usi クラスのモデル更新スロット実装

#include "usi.h"
#include "usimatchhandler.h"
#include "shogiinforecord.h"

namespace {
constexpr int kMaxThinkingRows = 500;

[[nodiscard]] bool isSameThinkingPayload(const ShogiInfoRecord* record,
                                         const QString& time,
                                         const QString& depth,
                                         const QString& nodes,
                                         const QString& score,
                                         const QString& pvKanjiStr,
                                         const QString& usiPv,
                                         const QString& baseSfen,
                                         const QString& lastUsiMove,
                                         int multipv,
                                         int scoreCp)
{
    if (!record) {
        return false;
    }

    // 頻繁に変化するフィールドを先に比較して早期リターン
    return record->scoreCp() == scoreCp
        && record->multipv() == multipv
        && record->depth() == depth
        && record->pv() == pvKanjiStr
        && record->time() == time
        && record->nodes() == nodes
        && record->score() == score
        && record->usiPv() == usiPv
        && record->baseSfen() == baseSfen
        && record->lastUsiMove() == lastUsiMove;
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

void Usi::onAnalysisStopTimeout()
{
    qCDebug(lcEngine) << "解析停止タイマー発火";
    m_protocolHandler->sendStop();
    m_analysisStopTimer = nullptr;
}

void Usi::onConsiderationStopTimeout()
{
    if (m_processManager && m_processManager->isRunning()) {
        qCDebug(lcEngine) << "検討停止タイマー発火";
        m_protocolHandler->sendStop();
    }
    m_analysisStopTimer = nullptr;
}

void Usi::onClearThinkingInfoRequested()
{
    qCDebug(lcEngine) << "思考情報クリア要求";
    if (m_thinkingModel) {
        qCDebug(lcEngine) << "思考モデルクリア実行";
        m_thinkingModel->clearAllItems();
    }
}

void Usi::onThinkingInfoUpdated(const QString& time, const QString& depth,
                                const QString& nodes, const QString& score,
                                const QString& pvKanjiStr, const QString& usiPv,
                                const QString& baseSfen, int multipv, int scoreCp)
{
    // 処理フロー:
    // 1. ShogiInfoRecordを生成して思考タブへ追記（先頭に追加）
    // 2. 検討タブへ追記（MultiPVモードで行を更新/挿入）
    // 3. 外部へシグナルで通知
    const QString lastUsiMove = m_matchHandler->lastUsiMove();
    qCDebug(lcEngine) << "思考情報更新: m_lastUsiMove=" << lastUsiMove
                      << "baseSfen=" << baseSfen.left(50)
                      << "multipv=" << multipv << "scoreCp=" << scoreCp;

    // 思考タブへ追記（通常モード: 先頭に追加）
    // 読み筋（PV）が空の行は表示しない（詰み探索の中間結果など）
    if (m_thinkingModel && !pvKanjiStr.isEmpty()) {
        const ShogiInfoRecord* topRecord =
            (m_thinkingModel->rowCount() > 0) ? m_thinkingModel->recordAt(0) : nullptr;
        if (!isSameThinkingPayload(topRecord, time, depth, nodes, score, pvKanjiStr, usiPv,
                                   baseSfen, lastUsiMove, multipv, scoreCp)) {
            ShogiInfoRecord* record = new ShogiInfoRecord(time, depth, nodes, score, pvKanjiStr);
            record->setUsiPv(usiPv);
            record->setBaseSfen(baseSfen);
            record->setLastUsiMove(lastUsiMove);
            record->setMultipv(multipv);
            record->setScoreCp(scoreCp);
            qCDebug(lcEngine) << "record->lastUsiMove()=" << record->lastUsiMove();
            m_thinkingModel->prependItem(record);
            m_thinkingModel->trimToMaxRows(kMaxThinkingRows);
        }
    }

    // 検討タブへ追記（MultiPVモード: multipv値に基づいて行を更新/挿入）
    if (m_considerationModel) {
        const int existingRow = m_considerationModel->findRowByMultipv(multipv);
        const ShogiInfoRecord* existingMultipvRecord = (existingRow >= 0)
            ? m_considerationModel->recordAt(existingRow) : nullptr;

        if (!isSameThinkingPayload(existingMultipvRecord, time, depth, nodes, score, pvKanjiStr, usiPv,
                                   baseSfen, lastUsiMove, multipv, scoreCp)) {
            ShogiInfoRecord* record = new ShogiInfoRecord(time, depth, nodes, score, pvKanjiStr);
            record->setUsiPv(usiPv);
            record->setBaseSfen(baseSfen);
            record->setLastUsiMove(lastUsiMove);
            record->setMultipv(multipv);
            record->setScoreCp(scoreCp);
            m_considerationModel->updateByMultipv(record, m_considerationMaxMultiPV);
        }
    }

    // 外部への通知
    emit thinkingInfoUpdated(time, depth, nodes, score, pvKanjiStr, usiPv, baseSfen, multipv, scoreCp);
}
