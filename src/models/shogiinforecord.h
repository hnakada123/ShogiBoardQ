#ifndef SHOGIINFORECORD_H
#define SHOGIINFORECORD_H

/// @file shogiinforecord.h
/// @brief 思考の表の1行（エンジンの info 1件）

#include <QString>
#include <QStringList>

#include "thinkinginforow.h"

/**
 * @brief 思考の表・検討の表の1行
 *
 * 値は ThinkingInfoRow のまま持つ（時間・深さ・ノード数は数、評価値は UsiScore）。
 * 表示する文字列は表のモデル（ShogiEngineThinkingModel）が作る。読み筋の表示だけは
 * 表記の設定ごとに作った文字列を覚えておく（英語表記では局面をたどって作るので重い）。
 */
class ShogiInfoRecord
{
public:
    explicit ShogiInfoRecord(ThinkingInfoRow row);

    const ThinkingInfoRow& row() const { return m_row; }
    qint64 timeMs() const { return m_row.timeMs; }
    int depth() const { return m_row.depth; }
    qint64 nodes() const { return m_row.nodes; }
    const UsiScore& score() const { return m_row.score; }
    /// 漢字表記の読み筋
    QString pv() const { return m_row.pvKanji; }
    /// 表に出す読み筋（表記の設定に合わせる。fullOrigin なら移動元をすべて付ける）
    QString displayPv(bool fullOrigin = false) const;
    QString usiPv() const { return m_row.usiPv; }
    /// エンジンが送った読み筋（局面との照合前）。usiPv() が空のとき（局面と合わない手があったとき）に使う
    QStringList enginePv() const { return m_row.enginePv; }
    QString baseSfen() const { return m_row.baseSfen; }
    QString lastUsiMove() const { return m_row.lastUsiMove; }
    int multipv() const { return m_row.multipv; }

    /// 同じ内容の行か（同じ info が続けて届いたときに行を足さない）
    bool hasSameContent(const ThinkingInfoRow& row) const;

private:
    /// 表記の設定が変わったら覚えた文字列を捨てる
    void invalidateDisplayIfStyleChanged() const;

    ThinkingInfoRow m_row;
    mutable QString m_displayCache;
    mutable QString m_tooltipCache;
    mutable bool m_displayValid = false;
    mutable bool m_tooltipValid = false;
    mutable int m_cacheRevision = -1;   ///< 覚えた文字列を作ったときの表記の設定（KifuPresentation::revision）
};

#endif // SHOGIINFORECORD_H
