#include "shogiinforecord.h"

// GUIの思考タブの表に「時間」「深さ」「ノード数」「評価値」「読み筋」をセットするためのクラス
// コンストラクタ
ShogiInfoRecord::ShogiInfoRecord(QObject* parent) : QObject(parent)
{
}

// コンストラクタ
ShogiInfoRecord::ShogiInfoRecord(const QString& time, const QString& depth, const QString& nodes,
                                const QString& score, const QString& pv, QObject* parent)
    : ShogiInfoRecord(parent)
{
    // 時間、深さ、ノード数、評価値、読み筋をセットする。
    m_time = time;
    m_depth = depth;
    m_nodes = nodes;
    m_score = score;
    m_pv = pv;
}

// 思考時間を取得する。
QString ShogiInfoRecord::time() const
{
    return m_time;
}

// 現在思考探索中の手の探索深さを取得する。
QString ShogiInfoRecord::depth() const
{
    return m_depth;
}

// 思考開始から探索したノード数を取得する。
QString ShogiInfoRecord::nodes() const
{
    return m_nodes;
}

// 現在の評価値を取得する。
QString ShogiInfoRecord::score() const
{
    return m_score;
}

// 現在の読み筋を取得する。
QString ShogiInfoRecord::pv() const
{
    return m_pv;
}
