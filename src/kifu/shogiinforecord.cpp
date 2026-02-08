/// @file shogiinforecord.cpp
/// @brief USIエンジン思考情報レコードクラスの実装

#include "shogiinforecord.h"

// GUIの思考タブの表に「時間」「深さ」「ノード数」「評価値」「読み筋」をセットするためのクラス
ShogiInfoRecord::ShogiInfoRecord(QObject* parent) : QObject(parent)
{
}

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

// コンストラクタ（USI形式のPV付き）
ShogiInfoRecord::ShogiInfoRecord(const QString& time, const QString& depth, const QString& nodes,
                                const QString& score, const QString& pv, const QString& usiPv,
                                QObject* parent)
    : ShogiInfoRecord(time, depth, nodes, score, pv, parent)
{
    m_usiPv = usiPv;
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

// USI形式の読み筋を取得する。
QString ShogiInfoRecord::usiPv() const
{
    return m_usiPv;
}

// USI形式の読み筋を設定する。
void ShogiInfoRecord::setUsiPv(const QString& usiPv)
{
    m_usiPv = usiPv;
}

// 読み筋の開始局面SFENを取得する。
QString ShogiInfoRecord::baseSfen() const
{
    return m_baseSfen;
}

// 読み筋の開始局面SFENを設定する。
void ShogiInfoRecord::setBaseSfen(const QString& sfen)
{
    m_baseSfen = sfen;
}

// 開始局面に至った最後の指し手（USI形式）を取得する。
QString ShogiInfoRecord::lastUsiMove() const
{
    return m_lastUsiMove;
}

// 開始局面に至った最後の指し手（USI形式）を設定する。
void ShogiInfoRecord::setLastUsiMove(const QString& move)
{
    m_lastUsiMove = move;
}

// MultiPV番号を取得する（1から始まる、1が最良）
int ShogiInfoRecord::multipv() const
{
    return m_multipv;
}

// MultiPV番号を設定する
void ShogiInfoRecord::setMultipv(int multipv)
{
    m_multipv = multipv;
}

// 評価値（整数）を取得する（ソート用）
int ShogiInfoRecord::scoreCp() const
{
    return m_scoreCp;
}

// 評価値（整数）を設定する
void ShogiInfoRecord::setScoreCp(int scoreCp)
{
    m_scoreCp = scoreCp;
}
