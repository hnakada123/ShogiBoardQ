/// @file shogiinforecord.cpp
/// @brief 思考の表の1行の実装

#include "shogiinforecord.h"

#include "kifupresentation.h"
#include "pvtext.h"

#include <utility>

ShogiInfoRecord::ShogiInfoRecord(ThinkingInfoRow row)
    : m_row(std::move(row))
{
}

bool ShogiInfoRecord::hasSameContent(const ThinkingInfoRow& row) const
{
    // 頻繁に変わる項目を先に比べる
    return m_row.score == row.score && m_row.multipv == row.multipv && m_row.depth == row.depth
        && m_row.timeMs == row.timeMs && m_row.nodes == row.nodes && m_row.pv == row.pv
        && m_row.lastUsiMove == row.lastUsiMove;
}

// 表の描画のたびに呼ばれる。表記の設定が変わらなければ前に作った文字列を返す。
// ツールチップ用（移動元をすべて付ける）は使うときに作る。
QString ShogiInfoRecord::displayPv(bool fullOrigin) const
{
    return m_display.text(0, fullOrigin, [this, fullOrigin] { return KifuPresentation::pv(m_row.pv, fullOrigin); });
}
