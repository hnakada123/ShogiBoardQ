/// @file shogiinforecord.cpp
/// @brief 思考の表の1行の実装

#include "shogiinforecord.h"

#include "kifupresentation.h"

#include <utility>

ShogiInfoRecord::ShogiInfoRecord(ThinkingInfoRow row)
    : m_row(std::move(row))
{
}

bool ShogiInfoRecord::hasSameContent(const ThinkingInfoRow& row) const
{
    // 頻繁に変わる項目を先に比べる
    return m_row.score == row.score && m_row.multipv == row.multipv && m_row.depth == row.depth
        && m_row.pvKanji == row.pvKanji && m_row.timeMs == row.timeMs && m_row.nodes == row.nodes
        && m_row.usiPv == row.usiPv && m_row.enginePv == row.enginePv && m_row.baseSfen == row.baseSfen
        && m_row.lastUsiMove == row.lastUsiMove;
}

void ShogiInfoRecord::invalidateDisplayIfStyleChanged() const
{
    const int revision = KifuPresentation::revision();
    if (m_cacheRevision == revision) return;
    m_cacheRevision = revision;
    m_displayValid = m_tooltipValid = false;
}

// 表の描画のたびに呼ばれる。表記の設定が変わらなければ前に作った文字列を返す。
// ツールチップ用（移動元をすべて付ける）は使うときに作る。
QString ShogiInfoRecord::displayPv(bool fullOrigin) const
{
    invalidateDisplayIfStyleChanged();
    if (fullOrigin) {
        if (!m_tooltipValid) {
            m_tooltipCache = KifuPresentation::pv(m_row.baseSfen, m_row.usiPv, m_row.pvKanji, true);
            m_tooltipValid = true;
        }
        return m_tooltipCache;
    }
    if (!m_displayValid) {
        m_displayCache = KifuPresentation::pv(m_row.baseSfen, m_row.usiPv, m_row.pvKanji);
        m_displayValid = true;
    }
    return m_displayCache;
}
