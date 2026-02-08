#ifndef SHOGIINFORECORD_H
#define SHOGIINFORECORD_H

/// @file shogiinforecord.h
/// @brief USIエンジン思考情報レコードクラスの定義


#include <QObject>
#include <QString>

// GUIの思考タブの表に「時間」「深さ」「ノード数」「評価値」「読み筋」をセットするためのクラス
class ShogiInfoRecord : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit ShogiInfoRecord(QObject *parent = nullptr);

    // コンストラクタ
    ShogiInfoRecord(const QString &time, const QString &depth, const QString &nodes,
               const QString &score, const QString &pv, QObject *parent = nullptr);

    // コンストラクタ（USI形式のPV付き）
    ShogiInfoRecord(const QString &time, const QString &depth, const QString &nodes,
               const QString &score, const QString &pv, const QString &usiPv,
               QObject *parent = nullptr);


    // 思考時間を取得する。
    QString time() const;

    // 現在思考中の手の探索深さを取得する。
    QString depth() const;

    // 思考開始から探索したノード数を取得する。
    QString nodes() const;

    // 現在の評価値を取得する。
    QString score() const;

    // 現在の読み筋を取得する。
    QString pv() const;

    // USI形式の読み筋を取得する。
    QString usiPv() const;

    // USI形式の読み筋を設定する。
    void setUsiPv(const QString& usiPv);

    // 読み筋の開始局面SFENを取得する。
    QString baseSfen() const;

    // 読み筋の開始局面SFENを設定する。
    void setBaseSfen(const QString& sfen);

    // 開始局面に至った最後の指し手（USI形式）を取得する。
    QString lastUsiMove() const;

    // 開始局面に至った最後の指し手（USI形式）を設定する。
    void setLastUsiMove(const QString& move);

    // MultiPV番号を取得する（1から始まる、1が最良）
    int multipv() const;

    // MultiPV番号を設定する
    void setMultipv(int multipv);

    // 評価値（整数）を取得する（ソート用）
    int scoreCp() const;

    // 評価値（整数）を設定する
    void setScoreCp(int scoreCp);

private:
    // 思考時間
    QString m_time;

    // 現在思考中の手の探索深さ
    QString m_depth;

    // 思考開始から探索したノード数
    QString m_nodes;

    // エンジンによる現在の評価値
    QString m_score;

    // 現在の読み筋（漢字表記）
    QString m_pv;

    // 現在の読み筋（USI形式）
    QString m_usiPv;

    // 読み筋の開始局面SFEN
    QString m_baseSfen;

    // 開始局面に至った最後の指し手（USI形式）
    QString m_lastUsiMove;

    // MultiPV番号（1から始まる、1が最良）
    int m_multipv = 1;

    // 評価値（整数、ソート用）
    int m_scoreCp = 0;
};

#endif // SHOGIINFORECORD_H
